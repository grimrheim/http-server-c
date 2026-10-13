// POSIX не является стандартом C. 
// Чтобы подключить расширения для сети, добавляем _GNU_SOURCE.
// Иначе Makefile не увидит сетевые либы
#define _GNU_SOURCE
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "server.h"

// Это необходимо, чтобы сервер не зависел от версии адреса
void *get_in_addr(struct sockaddr *sa) {
    // Смотрим, какая структура лежит по адресу
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

int create_server_socket() {
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
        // gai_strerror превращает код ошибки в текст
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
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

int accept_client(int server_socket, struct sockaddr_storage *their_addr, socklen_t *sin_size) {
    /*
     * server_socket - сокет, который слушаем
     * sockaddr_storage - указатель на структуру, куда запишем адрес подключившегося клиента
     * sin_size - указатель на переменную с размером структуры.
     *
     * Хранилище должно быть универсальный, под любой тип адреса.
     * Размер передается в accept как указатель, потому что это и вход в функцию
     * и выход, т.е. сколько байт записано.
     * Разыменование, что бы явно взять размер
     */
    *sin_size = sizeof(*their_addr);

    int new_fd = accept(server_socket, (struct sockaddr *)their_addr, sin_size);

    return new_fd;
}

void handle_client_with_fork(int server_socket, int client_socket) {
    char buf[4096];
    ssize_t bytes_read;

    // fork клонирует текущий процесс: память и дескрипторы, стек.
    // После этой строки два процесса продолжают выполнение с этой же строки.
    //   в родителе  pid > 0 (это PID созданного child)
    //   в child     pid == 0
    //   при ошибке  pid < 0

    pid_t pid = fork();

    if (pid < 0) {
        // Ветка child, процесс не создан.
        // поэтому закрываем client_socket
        perror("fork");
        close(client_socket);
        return;
    }
    else if (pid > 0) {
        // Процесс у РОДИТЕЛЯ.
        // У родителя уже есть копия client_socket, но она ему не нужна:
        // обрабатывать этого клиента будет child, а родитель вернется
        // в accept-loop и принимать новых клиентов.
        // Поэтому родитель закрывает СВОЮ копию дескриптора.
        // Важно: закрытие дескриптора в родителе НЕ закрывает соединение —
        // в child-е есть своя копия того же дескриптора, она остаётся открытой.
        printf("Spawned child PID: %d\n", pid);
        close(client_socket);


        // Родитель закончил всё, что должен был сделать с этим клиентом.
        // Выходим из функции — управление вернётся в main(), в accept-loop.
        return;
    }

    // Если дошли до этой точки, PID == 0
    // Программа в child процессе.
    //
    // Так как child не принимает новые соединения, он может закрыть
    // свою копию server_socket
    close(server_socket);

    // Ребенок читает общую память из stats.c
    mem_stats_t *shared_mem = stats_init();
    read_mem(shared_mem);
    print_log(shared_mem);

    // Читаем запрос от клиента. recv() возвращает:
    //   > 0  — сколько байт реально прочитано
    //   == 0 — клиент закрыл соединение корректно (например, FIN)
    //   < 0  — ошибка сети
    bytes_read = recv(client_socket, buf, sizeof(buf) - 1, 0);

    if (bytes_read > 0) {
        buf[bytes_read] = '\0';     // Сырые байты -> Си-строка

        // sscanf считает количество полей
        char method[8], path[256];
        if (sscanf(buf, "%7s %255s", method, path) != 2) {
            bad_request(client_socket);
            close(client_socket);
            _exit(0);   // child завершается
        }

        // strchr ищет первое вхождение символа в строке и возвращает указатель на него
        // Т.е. отсекаем все символы после '?'
        char *q = strchr(path, '?');
        if (q) *q = '\0';

        if (strcmp(method, "GET") != 0) {
            bad_request(client_socket);
            close(client_socket);
            _exit(0);
        }

        // Ищет первое вхождение строки в строку и также возвращает указатель
        // Защита от path traversal: не даём читать файлы вне www/ через "..".
        if (strstr(path, "..")) {
            bad_request(client_socket);
            close(client_socket);
            _exit(0);
        }

        char filepath[512];
        if (strcmp(path, "/") == 0)
            snprintf(filepath, sizeof(filepath), "www/index.html");
        else
            snprintf(filepath, sizeof(filepath), "www%s", path);

        send_file(client_socket, filepath);
    } else {
        // bytes_read <= 0: либо клиент ничего не прислал и закрыл соединение,
        // либо произошла ошибка recv(). perror() выведет текст ошибки,
        // если errno был установлен (при bytes_read == 0 текст будет
        // не особо информативным, это нормально).
        perror("recv");
    }
    // ОДИН запрос = ОДИН ответ. Поэтой по завершении работы закрывает сокет клиента.
    close(client_socket);

    // ВАЖНО: child — это отдельный процесс, у него своя копия всего,
    // включая стек вызовов main(). Если бы мы просто сделали return,
    // выполнение вернулось бы туда, откуда была вызвана эта функция —
    // то есть в копию accept-loop внутри child. Child начал бы
    // пытаться accept() на server_socket, который сам же закрыл выше —
    // это гарантированные ошибки в бесконечном цикле.
    // _exit(0) вместо exit(0) или return — завершает ИМЕННО этот
    // процесс немедленно, без вызова atexit()-хендлеров и без повторного
    // сброса (flush) буферов stdio, которые child мог унаследовать
    // от родителя в непустом виде (иначе возможен задвоенный вывод).
    _exit(0);
}
