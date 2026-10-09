// POSIX не является стандартом C. 
// Чтобы подключить расширения для сети, добавляем _GNU_SOURCE.
// Иначе Makefile не увидит сетевые либы
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>

#include <signal.h>     // Сигналы
#include <sys/stat.h>   // file status

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "server.h"


int main() {
    /*
     * SIGPIPE - сигнал, что сервер пишет в сокет, на котором клиент уже отключился
     * SIG_IGN - сигнал будет игнорироваться
     */
    signal(SIGPIPE, SIG_IGN);
    /*
     * SIGCHLD приходит родителю, когда child завершился. Если сигнал не обработать,
     * процесс станет зомби.
     * SIG_IGN - говорит ядру, завершать процесс без wait()
     */

    signal(SIGCHLD, SIG_IGN);
    int server_socket = create_server_socket();

    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int new_socket;

    char s[INET6_ADDRSTRLEN];

    while (1) {
        new_socket = accept_client(server_socket, &their_addr, &sin_size);
        if (new_socket == -1) {
            perror("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof(s));

        int *client_socket = malloc(sizeof(int));
        *client_socket = new_socket;

        handle_client_with_fork(server_socket, *client_socket);
    }
}
