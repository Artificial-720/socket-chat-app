#include <asm-generic/socket.h>
#include <stdio.h>       // printf
#include <string.h>      // memset
#include <netdb.h>       // addrinfo
#include <stdlib.h>      // exit
#include <sys/socket.h>  // socket
#include <unistd.h>      // close
#include <pthread.h>     // pthreads
#include <sys/time.h>    // timeval

#define PORT "5656"         // port number on server default
#define ADDRESS "localhost" // IPv4 Address of the server
#define MAXDATASIZE 100     // number of bytes can get at once
#define LINES 10            // number of past messages to save

int flag = 0;  // flag for exiting
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_send(void *ptr);
void *handle_recv(void *ptr);

int setup_socket(char *address, char *port);
void update_screen(char *lines[], int count);
void append_screen(char *lines[], int count, char *line);

int main(int argc, char *argv[]) {
  char *port, *address;
  int socket_fd, len;
  char username[30];
  char buf[MAXDATASIZE];
  pthread_t send_thread, recv_thread;
  if(argc != 2) {
    printf("Usage %s [port]\n", argv[0]);
    printf("Using default PORT 5656 for server\n");
    port = PORT;
  }else{
    port = argv[1];
  }

  // address default to localhost
  address = ADDRESS;
  socket_fd = setup_socket(address, port);

  // get username
  printf("Enter a username: ");
  scanf("%s", username);
  getchar(); // to remove the trailing \n
  // recv "Whats your username?"
  if(recv(socket_fd, buf, MAXDATASIZE-1, 0) <= 0){
    printf("recv error\n");
    exit(1);
  }
  // send "<username>"
  len = strlen(username);
  if(send(socket_fd, username, len, 0) == -1){
    printf("send error\n");
    exit(1);
  }
  // recv "ok"
  if(recv(socket_fd, buf, MAXDATASIZE-1, 0) <= 0){
    printf("recv error\n");
    exit(1);
  }

  // init mutex
  pthread_mutex_init(&mutex, NULL);

  // start send a recv threads
  len = socket_fd; // using len as temp to prevent both threads from accessing at the same time
  pthread_create(&send_thread, NULL, handle_send, (void*)&socket_fd);
  pthread_create(&recv_thread, NULL, handle_recv, (void*)&len);

  // join threads in before exiting
  pthread_join(send_thread, NULL);
  pthread_join(recv_thread, NULL);

  close(socket_fd);

  return 0;
}



void *handle_send(void *ptr){
  char buf[MAXDATASIZE];
  int len;
  int socket_fd = *((int *)ptr);

  // start loop of sending msg
  while(1){
    fgets(buf, MAXDATASIZE, stdin);
    buf[strlen(buf)-1] = '\0'; // remove newline char at end
    printf("\33[A\33[2K\r");   // clear line
    len = strlen(buf);
    if(len > 0){
      if(buf[0] == 'X'){
        // set flag and close thread
        pthread_mutex_lock(&mutex);
        flag = 1;
        pthread_mutex_unlock(&mutex);
        break;
      }
      if(send(socket_fd, buf, len, 0) == -1){
        printf("send error\n");
      }
    }
  }

  return NULL;
}


void *handle_recv(void *ptr){
  int i, num_bytes;
  char buf[MAXDATASIZE];
  char *lines[LINES];
  int socket_fd = *((int *)ptr);
  int local_flag = 0;
  struct timeval timeout = {5, 0}; // timeout set to 5 seconds
  // set timeout on socket
  if(setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0){
    printf("setsockopt failed");
    exit(1);
  }
  // init lines
  for(i = 0; i < LINES; i++){
    lines[i] = (char *)malloc(100);
  }
  printf("\033[H\033[2J"); // clear screen
  update_screen(lines, LINES);

  num_bytes = 1;
  while(num_bytes){
    pthread_mutex_lock(&mutex);
    if(flag){
      local_flag = flag; // flag set exit
    }
    pthread_mutex_unlock(&mutex);
    if(local_flag){
      break;
    }
    num_bytes = recv(socket_fd, buf, MAXDATASIZE-1, 0);
    if(num_bytes > 0){
      // null out the bytes
      buf[num_bytes] = '\0';

      append_screen(lines, LINES, buf);
      update_screen(lines, LINES);
    }
  }
  if(!num_bytes){
    printf("client: Server closed connection\n");
  }
  // free lines
  for(i = 0; i < LINES; i++){
    free(lines[i]);
  }

  return NULL;
}


int setup_socket(char *address, char *port){
  struct addrinfo hints, *server_info;
  int status;
  int socket_fd;

  // address struct for setting up port
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;       // IPv4
  hints.ai_socktype = SOCK_STREAM; // TCP stream
  hints.ai_flags = AI_PASSIVE;     // Fill in IP for me

  if((status = getaddrinfo(address, port, &hints, &server_info)) != 0) {
    printf("getaddrinfo error: %d\n", status);
    exit(1);
  }

  // Create Socket
  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(socket_fd == -1){
    printf("socket error\n");
    exit(1);
  }

  // Connect
  status = connect(socket_fd, server_info->ai_addr, server_info->ai_addrlen);
  if(status == -1){
    printf("connection failed\n");
    exit(1);
  }

  freeaddrinfo(server_info); // free linked-list
  
  return socket_fd;
}

void update_screen(char *lines[], int count){
  int i;
  printf("\033[0;0H"); // move cursor to 0,0
  for(i = 0; i < count; i++){
    /*
    \33[2K  erases the entire line curror is on
    \33[A   moves cursor up one line
    \r      returns cursor to beginning of line
     */
    printf("\33[2K\r%s\n", lines[i]);
  }
}

void append_screen(char *lines[], int count, char *line){
  // move all the lines up 1
  int i;
  for(i = 0; i < count - 1; i++){
    strcpy(lines[i], lines[i+1]);
  }
  strcpy(lines[count - 1], line);
}
