#include <stdio.h>       // printf
#include <string.h>      // memset
#include <netdb.h>       // addrinfo
#include <stdlib.h>      // exit
#include <sys/socket.h>  // socket
#include <unistd.h>      // close


#define PORT "5656"
#define ADDRESS "localhost"
#define MAXDATASIZE 100     // number of bytes can get at once
#define LINES 10            // number of past messages to save

int setup_socket(char *address, char *port);
void update_screen(char *lines[], int count);
void append_screen(char *lines[], int count, char *line);

void handle_send(int socket_fd);
void handle_recv(int socket_fd);


/*
\33[2K  erases the entire line curror is on
\33[A   moves cursor up one line
\r      returns cursor to beginning of line

text color
\33[30m black
\33[31m red
\33[32m green
\33[33m yellow
\33[34m blue
\33[35m magenta
\33[36m cyan
\33[37m white
\33[39m default

\33[0m revert - use after color to go back
 */






int main(int argc, char *argv[]) {
  char *port;
  char *address;

  if(argc != 2) {
    printf("Usage %s [port]\n", argv[0]);
    printf("Using default PORT 5656 for server\n");
    port = PORT;
  }else{
    port = argv[1];
  }

  //idk error checking or something
  address = ADDRESS;


  struct addrinfo hints, *server_info;
  int socket_fd; // socket file descriptor
  int num_bytes;
  char buf[MAXDATASIZE];
  int len;


  socket_fd = setup_socket(address, port);

  
  // Accept incoming connections
  printf("client: connected to %s\n\n\n", address);

  // get username
  char username[30];
  printf("Enter a username: ");
  scanf("%s", username);
  getchar(); // to remove the trailing \n
  // recv "Whats your username?"
  num_bytes = recv(socket_fd, buf, MAXDATASIZE-1, 0);
  if(num_bytes <= 0){
    printf("recv error\n");
    exit(1);
  }
  // send "<username>"
  len = strlen(username);
  if(send(socket_fd, username, len, 0) == -1){
    printf("send error\n");
  }
  // recv "ok"
  num_bytes = recv(socket_fd, buf, MAXDATASIZE-1, 0);
  if(num_bytes <= 0){
    printf("recv error\n");
    exit(1);
  }


  // start send and recv processes
  pid_t p;
  p = fork();
  if(p < 0){
    printf("fork fail\n");
  }else if(p == 0){
    handle_send(socket_fd);
  }else{
    handle_recv(socket_fd);
  }

  // Close Socket
  close(socket_fd);

  return 0;
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









void handle_send(int socket_fd){
  char buf[MAXDATASIZE];
  int len;

  printf("client: starting send loop\n");
  // start loop of sending msg
  while(1){
    //printf("> ");
    fgets(buf, MAXDATASIZE, stdin);
    buf[strlen(buf)-1] = '\0'; // remove newline char at end
    printf("\33[A\33[2K\r");   // clear line
    len = strlen(buf);
    if(len > 0){
      if(buf[0] == 'X'){
        // exit
        break;
      }
      if(send(socket_fd, buf, len, 0) == -1){
        printf("send error\n");
      }
    }
  }
}





void handle_recv(int socket_fd){
  printf("client: starting recv loop\n");
  int i;
  char buf[MAXDATASIZE];
  int num_bytes;
  char *lines[LINES];
  // init lines
  for(i = 0; i < LINES; i++){
    lines[i] = (char *)malloc(100);
  }
  printf("\033[H\033[2J"); // clear screen
  update_screen(lines, LINES);

  num_bytes = 1;
  while(num_bytes){
    num_bytes = recv(socket_fd, buf, MAXDATASIZE-1, 0);
    if(num_bytes <= 0){
      printf("recv error\n");
      exit(1);
    }

    // null out the bytes
    buf[num_bytes] = '\0';

    //printf("client: recieved %s\n", buf);
    //printf("\33[1A\33[2K\r%s\n> ", buf);
    append_screen(lines, LINES, buf);
    update_screen(lines, LINES);
  }
  printf("client: Server closed connection\n");
  exit(0);
}




void update_screen(char *lines[], int count){
  int i;
  printf("\033[0;0H");
  for(i = 0; i < count; i++){
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
