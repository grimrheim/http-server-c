
#include <stdio.h>
#include <string.h>

#include <signal.h>     // Сигналы

#include "server.h"
#include "stats.h"

typedef enum {
    MODE_SINGLE,
    MODE_FORK,
    MODE_THREADS,
    MODE_POOL,
    MODE_EPOLL,
    MODE_IO_URING
} concurrency_mode_t;

concurrency_mode_t parse_mode(const char *arg) {
    if (strcmp(arg, "single") == 0) return MODE_SINGLE;
    if (strcmp(arg, "fork") == 0) return MODE_FORK;
    if (strcmp(arg, "threads") == 0) return MODE_THREADS;
    if (strcmp(arg, "pool") == 0) return MODE_POOL;
    if (strcmp(arg, "epoll") == 0) return MODE_EPOLL;
    if (strcmp(arg, "io_uring") == 0) return MODE_IO_URING;
    fprintf(stderr, "Unknown mode: %s\n", arg);
    exit(1);
}


void print_usage(const char *prog) {
    printf("Usage: %s --mode <mode>\n", prog);
    printf("Available modes:\n");
    printf("  single     - sequential accept loop\n");
    printf("  fork       - process per client\n");
    printf("  threads    - thread per client\n");
    printf("  pool       - thread/process pool\n");
    printf("  epoll      - event-driven (epoll)\n");
    printf("  io_uring   - async I/O (io_uring)\n");
}

void run_fork(int server_socket) {
    /*
     * SIGCHLD приходит родителю, когда child завершился. Если сигнал не обработать,
     * процесс станет зомби.
     * SIG_IGN - говорит ядру, завершать процесс без wait()
     */

    signal(SIGCHLD, SIG_IGN);
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];

    while (1) {
        sin_size = sizeof(their_addr);
        int client_socket = accept_client(server_socket, &their_addr, &sin_size);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof(s));

        handle_client_with_fork(server_socket, client_socket);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3 || strcmp(argv[1], "--mode") != 0) {
        print_usage(argv[0]);
        return 1;
    }

    concurrency_mode_t mode = parse_mode(argv[2]);
    int server_socket = create_server_socket();
    mem_stats_t *shared_mem = stats_init();
    if (read_mem(shared_mem) != 0) {
        fprintf(stderr, "read_mem failed\n");
        return 1;
    }
    print_log(shared_mem);

    /*
     * SIGPIPE - сигнал, что сервер пишет в сокет, на котором клиент уже отключился
     * SIG_IGN - сигнал будет игнорироваться
     */
    signal(SIGPIPE, SIG_IGN);
    switch (mode) {
        case MODE_SINGLE:   printf("Running in single-threaded mode\n"); break;
        case MODE_FORK:
            printf("Running in fork mode\n");
            run_fork(server_socket);
            break;
        case MODE_THREADS:  printf("Running in threads mode\n"); break;
        case MODE_POOL:     printf("Running in pool mode\n"); break;
        case MODE_EPOLL:    printf("Running in epoll mode\n"); break;
        case MODE_IO_URING: printf("Running in io_uring mode\n"); break;
    }

    return 0;
}
