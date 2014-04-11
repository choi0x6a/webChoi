// Program written by: Joseph Choi
// Program written for: CSC524 Project 3 - Webserver Project

// Import necessary C libraries to implement the code
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>

#define TIMEOUT_SEC 10     // Timeout (seconds) if access limitation needed
#define DEFAULT_PORT 8888  // Default port number for use of this web-server
#define MAX_CONNECTIONS 10 // Max # of clients that can request at a time
#define MAXBUFLEN 512      // The maximum buffer length

// PROTOTYPE: Bind function of server: supplies protocol port number
int bindto_socket(int set_port);
// PROTOTYPE: Honor the request from client with proper action
void honor_request(int conn_sockfd);
//------------------------------------------------------------------------------
int main(void) {

  int init_sockfd = bindto_socket(DEFAULT_PORT);
  struct sockaddr_in client_addr;
  socklen_t addr_len;
  int index;
  int client[MAX_CONNECTIONS];
  int cindex = 0;

  for (index = 0; index < MAX_CONNECTIONS; index++) {
    client[index] = -1;
  }

  if (listen(init_sockfd, MAX_CONNECTIONS) == -1) {
    perror("listen");
    exit(1);
  }

  printf("Waiting for connection from client(s)...\n\n");
  printf("... ... ... ... ... ... ... ... ... ... ... ... ... ^.^\n\n");
  
  while (1) {
    addr_len = sizeof(client_addr);
    if ((client[cindex] = accept(init_sockfd, (struct sockaddr *)&client_addr,
				 &addr_len)) == -1) {
      perror("accept");
      exit(1);
    } else {
      printf("Connection established by: %s\n\n", 
	     inet_ntoa(client_addr.sin_addr));
      if (fork() == 0) {
	if (close(init_sockfd) == -1) {
	  perror("close initial socket");
	  exit(1);
	}
	honor_request(client[cindex]);
	shutdown(client[cindex], SHUT_RDWR);
	if (close(client[cindex]) == -1) {
	  perror("close new child socket");
	  exit(1);
	}
	exit(EXIT_SUCCESS);
      }
      if (close(client[cindex]) == -1) {
	perror("close new parent socket");
	exit(1);
      }
    }
    client[cindex] = -1;
    while (client[cindex] != -1) {
      cindex = (cindex + 1) % MAX_CONNECTIONS;
    }
  }
  return(EXIT_FAILURE);
}
//-----------------------------------------------------------------------------
int bindto_socket(int set_port) {

  int current_sockfd;
  struct sockaddr_in srv_addr;
  static const int yes = 1;
  static const struct timeval timeout = {.tv_sec = TIMEOUT_SEC, .tv_usec = 0};

  if ((current_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }
  printf("Success in obtaining TCP sockfd %d\n", current_sockfd);

  if (setsockopt(current_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
		 sizeof(int)) == -1) {
    perror("setsockopt");
    printf("Continuing without port reuse\n");
  } else {
    printf("Successfully set port reuse\n");
  }
  /*
  // The following code exists so server can limit access via timeouts
  if (setsockopt(current_sockfd, SOL_SOCKET, SO_RCVTIMEO, (void*)&timeout,
		 sizeof(struct timeval)) == -1) {
    perror("setsockopt");
    printf("Continuing without receive timeout\n");
  } else {
    printf("Successfully set receive timeout to %d seconds\n", TIMEOUT_SEC);
  }
  if (setsockopt(current_sockfd, SOL_SOCKET, SO_SNDTIMEO, (void*)&timeout,
		 sizeof(struct timeval)) == -1) {
    perror("setsockopt");
    printf("Continuing without send timeout\n");
  } else {
    printf("Successfully set send timeout to %d seconds\n", TIMEOUT_SEC);
  }
  */
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_port = htons(set_port);
  srv_addr.sin_addr.s_addr = INADDR_ANY;
  memset(&(srv_addr.sin_zero), '\0', 8);

  if (bind(current_sockfd, (struct sockaddr *)&srv_addr, 
	   sizeof(struct sockaddr)) == -1) {
    perror("bind");
    exit(1);
  }
  printf("Successfully bound to default port %d\n", ntohs(srv_addr.sin_port));

  return current_sockfd;
}
//-----------------------------------------------------------------------------
void honor_request(int conn_sockfd) {

  int num_bytes;
  int to_send;
  char recvbuf[MAXBUFLEN];
  char sendbuf[MAXBUFLEN];
  char * token_holder0;
  char * token_holder1;
  char * path = getenv("PWD");
  int file_descriptor;
  char * type_token;
  int simple = 0;
  int bad_request = 0;

  if ((num_bytes = recv(conn_sockfd, recvbuf, MAXBUFLEN, 0)) == -1) {
    perror("recv");
    exit(1);
  } else if (num_bytes == 0) {
    perror("recv");
    exit(1);
  } else {
    printf("%s\n", recvbuf);
    token_holder0 = strtok(recvbuf, " \t\n");
    if (strncmp(token_holder0, "GET\0", 4) == 0) {
      printf("The request contained command: GET\n");
      token_holder0 = strtok(NULL, " \t");
      token_holder1 = strtok(NULL, " \t\n");
      if (token_holder1 == NULL) {
	printf("The request contained a simple command!\n");
	simple = 1;
      }
      if (simple == 0) {
	if ((strncmp(token_holder1, "HTTP/1.0", 8) != 0) &&
	    (strncmp(token_holder1, "HTTP/1.1", 8) != 0)) {
	  write(conn_sockfd, "HTTP/1.0 400 Bad Request\n", 25);
	  bad_request = 1;
	}
      }
      if (simple == 1 || bad_request == 0) {
	strcpy(&path[strlen(path)], token_holder0);
	printf("Path of target file: %s\n", path);
	type_token = strtok(token_holder0, ".");
	type_token = strtok(NULL, " \t\n");
	if ((file_descriptor = open(path, O_RDONLY)) != -1) {
	  printf("File exists on server!\n");
	  send(conn_sockfd, "HTTP/1.0 200 OK\n", 16, 0);
	  send(conn_sockfd, "Server: JC-WebServer\n", 21, 0);
	  if (strncmp(type_token, "html", 4) == 0) {
	    send(conn_sockfd, 
		 "Content-Type: text/html; charset=utf-8\n\n", 40, 0);
	  } else {
	    send(conn_sockfd, 
		 "Content-Type: text/plain; charset=utf-8\n\n", 41, 0);
	  }
	  while ((to_send = read(file_descriptor, sendbuf, MAXBUFLEN)) > 0) {
	    write(conn_sockfd, sendbuf, to_send);
	  }
	} else {
	  printf("File does not exist on server!\n");
	  write(conn_sockfd, "HTTP/1.0 404 Not Found\n", 23);
	}
      }
    } else if (strncmp(token_holder0, "PUSH\0", 5) == 0) {
      printf("The request contained command: PUSH\n");
    } else {
      printf("Unsupported command; request will not be processed\n");
    }
  }
  printf("Successfully honored the client's request!\n\n");
  printf("... ... ... ... ... ... ... ... ... ... ... ... ... ^.^\n\n");
  return;
}
//============================================================================
