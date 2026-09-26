#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT "8888"
#define BACKLOG 10

// Это необходимо, чтобы сервер не зависел от версии адреса
static void *get_in_addr(struct sockaddr *sa) {
    // Смотрим, какая структура лежит по адресу
    sa_family_t family = sa->sa_family;

    if (sa->sa_family == AF_INET) {

        // Кастуем generic-указатель к нужному типу и сохраняем
        // его в отдельную переменную
        struct sockaddr_in *addr_ipv4 = (struct sockaddr_in *)sa;

        // Берем адрес из поля sin_addr
        struct in_addr *ip_field = &(addr_ipv4->sin_addr);

        return (void *)ip_field;
    }

    // Если не IPv4
    struct sockaddr_in6 *addr_ipv6 = (struct sockaddr_in6 *)sa;
    struct in6_addr *ip_field6 = &(addr_ipv6->sin6_addr);
    return (void *)ip_field6;
}

static int create_server_socket() {
    int sockfd;

    /*
     * addrinfo - струтура, которую использует getaddrinfo
     * hints - структура-подсказка для getaddrinfo
     * servinfo - указатель на связный список. Он заполняется данными. Чаще всего в нем один элемент.
     */
    struct addrinfo hints, *servinfo, *p;
    int yes = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    /*
     * getaddrinfo преобразует данные в бинарные структуры, готовые к использованию,
     * которые системные вызовы вроде socket(), bind(), connect() понимают напрямую.
     * В случае, если name = "google.com" или другой адрес, то servinfo заполнился бы
     * более чем одним значением - DNS может вернуть несколько ip адресов
     */
    int rv = getaddrinfo(NULL, PORT, &hints, &servinfo);
    if (rv != 0) {
        perror("getaddrinfo");
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (sockfd == -1) {
            perror("server: socket");
            continue;
        }

        /*
         * setsockopt - настройка опций сокетов
         */
        if (setsockopt(sockfd, SOL_SOCKET,
                       SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        /*
         * bind привязывает сокет к конкретному локальному порту и адресу
         * Клиенту нужно понимать, куда стучаться - на какие ip & port.
         * bind резервирует порт за процессом, чтобы процессы на PORT
         * были отданы этому сокету
         */
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        perror("servinfo: failed to build");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    printf("server: waiting for connections...\n");
    return sockfd;
}

/*
 * sockfd - сокет, который слушаем
 * sockaddr_storage - указатель на структуру, куда запишем адрес подключившегося клиента
 * sin_size - указатель на переменную с размером структуры.
 * Хранилище должно быть универсальный, под любой тип адреса.
 */
int accept_client(int sockfd, struct sockaddr_storage *their_addr, socklen_t *sin_size) {
    /*
     * Размер передается в accept как указатель, потому что это и вход в функцию
     * и выход, т.е. сколько байт записано.
     * Разыменование, что бы явно взять размер
     */
    *sin_size = sizeof(*their_addr);

    int new_fd = accept(sockfd, (struct sockaddr *)their_addr, sin_size);

    return new_fd;
}

void *handle_client(void *arg) {
    int *fd = (int *)arg;
    int client_fd = *fd;
    free(fd);

    char buf[512];

    while (1) {
        ssize_t r = recv(client_fd, buf, sizeof(buf), 0);

        if (r <= 0) {
            break;
        }

        send(client_fd, buf, r, 0);
    }
    close(client_fd);

    return NULL;
}

int main() {
    int sockfd = create_server_socket();

    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int new_fd;

    char s[INET6_ADDRSTRLEN];

    while (1) {
        new_fd = accept_client(sockfd, &their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof(s));

        int *client_fd = malloc(sizeof(int));
        *client_fd = new_fd;

        handle_client(client_fd);

    }

}
