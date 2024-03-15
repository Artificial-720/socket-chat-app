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

typedef struct {
  int items[MAXLIST];
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} Set;

typedef struct {
  Queue *q;
  Set *s;
  int socket_fd;
} arg_struct;

void *handle_client( void *ptr);
void *handle_send( void *ptr);
int setup_socket(char *port);
char *build_message(char *username, char *msg, int color);
void init_queue(Queue *q);
void enqueue(Queue *q, char *value);
char *dequeue(Queue *q);
void init_set(Set *s);
int set_add(Set *s, int item);
int set_at(Set *s, int index);
int set_remove(Set *s, int item);


int main(int argc, char *argv[]){
  char *port;
  if(argc != 2) {
    printf("Usage %s [port]\n", argv[0]);
    printf("Using default PORT 5656 for server\n\n\n");
    port = PORT;
  }else{
    port = argv[1];
  }

  int socket_fd, socket_in;
  struct sockaddr_storage their_addr;
  socklen_t their_addr_size;
  char s_addr[INET_ADDRSTRLEN];
  Queue q;
  Set s;
  pthread_t send_thread, thread;
  arg_struct *args;

  // setup socket
  socket_fd = setup_socket(port);
  // setup queue and set
  init_queue(&q);
  init_set(&s);
  // setup thread for sending out messages from queue
  args = (arg_struct*)malloc(sizeof(arg_struct));
  args->q = &q;
  args->s = &s;
  pthread_create(&send_thread, NULL, handle_send, (void*)args);

  while(1){
    // accept connections
    socket_in = accept(socket_fd, (struct sockaddr *)&their_addr, &their_addr_size);
    if(socket_in == -1){
      printf("accept error\n");
      continue;
    }

    // convert from network to presentable
    inet_ntop(their_addr.ss_family,
        &(((struct sockaddr_in *)&their_addr)->sin_addr),
        s_addr, sizeof s_addr);
    printf("server: new connection from %s\n", s_addr);

    // create a new thread to handle connection 
    args = (arg_struct*)malloc(sizeof(arg_struct));
    args->q = &q;
    args->s = &s;
    args->socket_fd = socket_in;
    pthread_create(&thread, NULL, handle_client, (void*)args);
    printf("server: Thread created to handle connection from %s\n", s_addr);

    // add new socket to list
    set_add(&s, socket_in);
  }

  close(socket_fd);

  return 0;
}




void *handle_client(void *ptr){
  arg_struct *args;
  Queue *q;
  Set *s;
  int socket_in;
  int len, num_bytes;
  char *msg = "Whats you username?";
  char buf[MAXDATASIZE];
  char username[30];
  char *tmp;
  char *m_joined = " joined chat";
  char *m_left = " left chat";

  //unpack args
  args = (arg_struct*)ptr;
  q = args->q;
  s = args->s;
  socket_in = args->socket_fd;
  free(args);

  // send "Whats your username?"
  len = strlen(msg);
  if(send(socket_in, msg, len, 0) == -1){
    printf("send error\n");
  }
  // recv "<username>"
  if(recv(socket_in, buf, MAXDATASIZE-1, 0) <= 0){
    printf("recv error\n");
    exit(1);
  }
  len = sizeof(username);
  buf[len] = '\0'; // null out buf so dont copy to much
  strcpy(username, buf);
  // send "ok"
  if(send(socket_in, "ok", strlen("ok"), 0) == -1){
    printf("send error\n");
  }

  // welcome message
  printf("server: welcomes %s\n", username);
  strcpy(buf, username);
  strcat(buf, m_joined);
  tmp = build_message("server", buf, 8);
  enqueue(q, tmp);

  // start loop to recv from client
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
    tmp = build_message(username, buf, socket_in);
    enqueue(q, tmp);
  }

  // goodbye message
  printf("server: client closed connection\n");
  strcpy(buf, username);
  strcat(buf, m_left);
  tmp = build_message("server", buf, 8);
  enqueue(q, tmp);

  // remove from list and close socket
  set_remove(s, socket_in);
  close(socket_in);

  return NULL;
}



/*
 * thread function
 * will keep checking queue, when something is in the queue
 * it will send that message out to all clients in list
 */
void *handle_send(void *ptr){
  arg_struct *args;
  Queue *q;
  Set *s;
  char *item;
  int len, socket, i;

  //unpack args
  args = (arg_struct*)ptr;
  q = args->q;
  s = args->s;
  free(args);

  // keep looping over queue and send out to all clients
  while(1){
    item = dequeue(q);
    if(item){
      len = strlen(item);
      for(i = 0; i < MAXLIST; i++){
        socket = set_at(s, i);
        if(socket){
          send(socket, item, len, 0);
          printf("sending '%s' to %d\n", item, socket);
        }
      }
      free(item); // free item after sending
    }
  }
  return NULL;
}


int setup_socket(char *port){
  int socket_fd;
  int status;
  struct addrinfo hints, *server_info;

  // address struct for setting up port
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;       // IPv4
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


/*
 * build a message with a username and color text
 * returns a newly malloc c string
 */
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
  int len;
  char *start = colors[color % 9]; // prevent outofbounds
  char *end = "]:\33[0m ";
  char *message;
  // \33[<username>]:\33 <message>
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


/*
 * Thread safe Queue
 */
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
  }
  pthread_mutex_unlock(&(q->mutex));
  return value;
}



/*
 * Thread safe Set
 */
void init_set(Set *s){
  memset(s, 0, sizeof(int)*MAXLIST);
  pthread_mutex_init(&(s->mutex), NULL);
  pthread_cond_init(&(s->cond), NULL);
}
int set_add(Set *s, int item){
  int i;
  int index = -1;
  int found = 0;
  pthread_mutex_lock(&(s->mutex));
  for(i = 0; i < MAXLIST; i++){
    if(s->items[i] == item){
      found = 1;
      break;
    }
  }
  if(!found){
    for(i = 0; i < MAXLIST; i++){
      if(!s->items[i]){
        index = i;
        s->items[index] = item;
        break;
      }
    }
  }
  pthread_mutex_unlock(&(s->mutex));
  return index;
}
int set_at(Set *s, int index){
  int value;
  value = 0;
  pthread_mutex_lock(&(s->mutex));
  value = s->items[index];
  pthread_mutex_unlock(&(s->mutex));
  return value;
}
int set_remove(Set *s, int item){
  int i;
  int index = -1;
  pthread_mutex_lock(&(s->mutex));
  for(i = 0; i < MAXLIST; i++){
    if(s->items[i] == item){
      index = i;
    }
  }
  if(index > -1){
    for(i = index; i < MAXLIST - 1; i++){
      s->items[i] = s->items[i+1];
    }
    s->items[MAXLIST-1] = 0;
  }
  pthread_mutex_unlock(&(s->mutex));
  if(index > -1){
    return 1;
  }else{
    return 0;
  }
}
