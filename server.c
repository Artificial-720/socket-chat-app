#include <netinet/in.h>
#include <stdio.h>       // printf
#include <string.h>      // memset
#include <netdb.h>       // addrinfo
#include <stdlib.h>      // exit
#include <sys/socket.h>  // socket
#include <unistd.h>      // close
#include <arpa/inet.h>   // inet_ntop
#include <pthread.h>     // pthreads



#define PORT "5656"
#define BACKLOG 10
#define MAXDATASIZE 100
#define MAXQUEUE 100
#define MAXCLIENTS 10
#define MAXLIST 10

typedef struct {
  char *items[MAXQUEUE];
  int rear;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} Queue;

void init_queue(Queue *q){
  q->rear = -1;
  memset(q, 0, sizeof(char)*MAXQUEUE);
  pthread_mutex_init(&(q->mutex), NULL);
  pthread_cond_init(&(q->cond), NULL);
}
void enqueue(Queue *q, char *value){
  // append to the end of the queue
  pthread_mutex_lock(&(q->mutex));
  if(q->rear == MAXQUEUE - 1){
    printf("Queue is full\n");
  }else{
    q->items[++q->rear] = value;
    //printf("adding item to queue '%s'\n", value);
  }
  pthread_mutex_unlock(&(q->mutex));
}
char *dequeue(Queue *q){
  // pop from the end of the queue
  char *value;
  value = NULL;
  pthread_mutex_lock(&(q->mutex));
  if(q->rear < 0){
  }else{
    value = q->items[0];
    // move all items down in array
    for(int i = 0; i <= q->rear; i++){
      q->items[i] = q->items[i+1];
    }
    q->rear = q->rear - 1;
    //printf("removing item from queue '%s'\n", value);
  }
  pthread_mutex_unlock(&(q->mutex));
  return value;
}



int list_add(int *list, int item){
  int i;
  for(i = 0; i < MAXLIST; i++){
    if(!list[i]){
      list[i] = item;
      return i;
    }
  }
  return -1;
}
void list_remove(int *list, int index){

}



void *handle_client( void *ptr );
void *handle_send( void *ptr );

int setup_socket(char *port);

typedef struct {
  Queue *q;
  int socket_fd;
} arg_struct;

typedef struct {
  Queue *q;
  int *list;
} arg_struct2;

char *build_message(char *username, char *msg, int color){
  char *colors[] = {
    "\33[30m[", // black
    "\33[31m[", // red
    "\33[32m[", // green
    "\33[33m[", // yellow
    "\33[34m[", // blue
    "\33[35m[", // magenta
    "\33[36m[", // cyan
    "\33[37m[", // white
    "\33[39m["  // default
  };
  // \33[<username>]:\33 <message>
  int len;
  char *start = colors[color];
  char *end = "]:\33[0m ";
  char *message;
  // start + username + end + msg

  len = strlen(username);
  len += strlen(msg);
  len += strlen(start);
  len += strlen(end);
  message = (char *)malloc(len + 1);
  strcpy(message, start);
  strcat(message, username);
  strcat(message, end);
  strcat(message, msg);

  return message;
}


int main(int argc, char *argv[]){
  char *port;
  if(argc != 2) {
    printf("Usage %s [port]\n", argv[0]);
    printf("Using default PORT 5656 for server\n\n\n");
    port = PORT;
  }else{
    port = argv[1];
  }

  int socket_fd;
  struct sockaddr_storage their_addr; // incoming address info
  socklen_t their_addr_size;
  char s[INET_ADDRSTRLEN];
  int socket_in;
  int clients[MAXCLIENTS];
  int list[MAXLIST];
  memset(list, 0, sizeof(int)*MAXLIST);

  // setup socket
  socket_fd = setup_socket(port);

  // setup Queue
  Queue q;
  init_queue(&q);

  // setup thread for sending
  pthread_t send_thread;
  arg_struct2 *args2 = (arg_struct2*)malloc(sizeof(arg_struct2));
  args2->q = &q;
  args2->list = list;
  pthread_create(&send_thread, NULL, handle_send, (void*)args2);

  while(1){
    // accept connection
    socket_in = accept(socket_fd, (struct sockaddr *)&their_addr, &their_addr_size);
    if(socket_in == -1){
      printf("accept error\n");
      continue;
    }

    // convert from network to presentable
    inet_ntop(their_addr.ss_family,
        &(((struct sockaddr_in *)&their_addr)->sin_addr),
        s, sizeof s);
    printf("server: new connection from %s\n", s);

    // create thread to handle connection
    pthread_t thread;
    arg_struct *args = (arg_struct*)malloc(sizeof(arg_struct));
    args->q = &q;
    args->socket_fd = socket_in;
    pthread_create(&thread, NULL, handle_client, (void*)args);
    printf("server: Thread created to handle connection from %s\n", s);

    // add new socket to list
    list_add(list, socket_in);
  }

  // Close Socket
  close(socket_fd);
  
  return 0;
}










int setup_socket(char *port){
  int socket_fd;
  struct addrinfo hints, *server_info;
  int status;

  // address struct for setting up port
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;     // IPv4
  hints.ai_socktype = SOCK_STREAM; // TCP stream
  hints.ai_flags = AI_PASSIVE;     // Fill in IP for me
  
  if((status = getaddrinfo(NULL, port, &hints, &server_info)) != 0) {
    printf("getaddrinfo error: %d\n", status);
    exit(1);
  }


  // Create Socket
  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(socket_fd == -1){
    printf("socket error\n");
    exit(1);
  }

  // bind socket to port
  status = bind(socket_fd, server_info->ai_addr, server_info->ai_addrlen);
  if(status == -1){
    printf("bind error\n");
    exit(1);
  }

  freeaddrinfo(server_info); // free linked-list

  // listen for incomming connections
  status = listen(socket_fd, BACKLOG);
  if(status == -1){
    printf("listen error\n");
    exit(1);
  }

  return socket_fd;
}









void *handle_client(void *ptr){
  int socket_in;
  Queue *q;
  arg_struct * args = (arg_struct *)(ptr);
  socket_in = args->socket_fd;
  q = args->q;
  free(args); // dont need args anymore free it

  char *tmp;
  char username[30];
  char *msg = "Whats you username?";
  int len, bytes_sent, num_bytes;
  char buf[MAXDATASIZE];
  len = strlen(msg);

  // send "Whats your username?"
  len = strlen(msg);
  if(send(socket_in, msg, len, 0) == -1){
    printf("send error\n");
  }
  // recv "<username>"
  num_bytes = recv(socket_in, buf, MAXDATASIZE-1, 0);
  if(num_bytes <= 0){
    printf("recv error\n");
    exit(1);
  }
  strcpy(username, buf); // NOT SAFE
  // send "ok"
  len = strlen("ok");
  if(send(socket_in, "ok", len, 0) == -1){
    printf("send error\n");
  }

  printf("server: welcomes %s\n", username);
  char *tmp2 = " joined chat";
  strcpy(buf, username);
  strcat(buf, tmp2);
  tmp = build_message("server", buf, 8);
  enqueue(q, tmp);

  // start loop to keep recv from client
  num_bytes = 1;
  while(num_bytes){
    num_bytes = recv(socket_in, buf, MAXDATASIZE-1, 0);
    if(num_bytes <= 0){
      printf("recv error %d\n", num_bytes);
      continue;
    }

    // null out the bytes
    buf[num_bytes] = '\0';

    printf("server: recieved %d bytes '%s'\n", num_bytes, buf);

    char *tmp = build_message(username, buf, socket_in);
    enqueue(q, tmp);
  }

  printf("server: client closed connection\n");
  char *tmp3 = " left chat";
  strcpy(buf, username);
  strcat(buf, tmp3);
  tmp = build_message("server", buf, 8);
  enqueue(q, tmp);
  close(socket_in);

  return NULL;
}







void *handle_send( void *ptr ){
  arg_struct2 * args = (arg_struct2 *)(ptr);
  Queue *q = args->q;
  int *list = args->list;
  char *item;
  int bytes_sent;
  int len;
  int i;

  // keep looping over queue and send out to all clients
  while(1){
    item = dequeue(q);
    if(item){
      len = strlen(item);
      for(i = 0; i < MAXLIST; i++){
        if(list[i]){
          bytes_sent = send(list[i], item, len, 0);
          printf("sending '%s' to %d\n", item, list[i]);
        }
      }
      free(item); // free item after sending
    }
  }
  return NULL;
}
