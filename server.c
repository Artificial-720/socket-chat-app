#include <stdio.h>       // printf
#include <string.h>      // memset
#include <netdb.h>       // addrinfo
#include <stdlib.h>      // exit
#include <sys/socket.h>  // socket
#include <unistd.h>      // close


#define PORT "5656"

int main(int argc, char *argv[]) {
  char *port;
  if(argc != 2) {
    printf("Usage %s [port]\n", argv[0]);
    printf("Using default PORT 5656 for server\n");
    port = PORT;
  }else{
    port = argv[1];
  }

  struct addrinfo hints, *server_info;
  int status;
  int socket_fd; // socket file descriptor


  // address struct for setting up port
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream
  hints.ai_flags = AI_PASSIVE;     // Fill in IP for me

  if((status = getaddrinfo(NULL, port, &hints, &server_info)) != 0) {
    printf("getaddrinfo error: %d\n", status);
    exit(1);
  }


  // Create Socket
  socket_fd = socket(AF_UNSPEC, SOCK_STREAM, 0);
  if(socket_fd == -1){
    printf("socket error");
    exit(1);
  }

  // bind socket to port
  status = bind(socket_fd, server_info->ai_addr, server_info->ai_addrlen);
  if(status == -1){
    printf("bind error");
    exit(1);
  }

  freeaddrinfo(server_info); // free linked-list
  
  // Accept incoming connections
  printf("Server started listening on port %s\n", port);
  while(1){

  }


  // Close Socket
  close(socket_fd);

  return 0;
}
