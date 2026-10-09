#pragma once

#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

void not_found(int client_socket);
void bad_request(int client_socket);
const char *get_content_type(const char *path);
void send_file(int client_socket, const char *filepath);
