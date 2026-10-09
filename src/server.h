#pragma once

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "http.h"

#define PORT "8888"
#define BACKLOG 100000

void *get_in_addr(struct sockaddr *sa);
int create_server_socket();
int accept_client(int sockfd, struct sockaddr_storage *their_addr, socklen_t *sin_size);
void handle_client_with_fork(int server_socket, int client_socket);
