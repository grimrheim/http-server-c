#include "http.h"

#define BUFFER_SIZE 4096


static int send_all(int client_socket, const char *buf, size_t len) {
    while (len > 0) {

        // MSG_NOSIGNAL - если соединение закрыто, вернуть -1 вместо SIGPIPE
        ssize_t n = send(client_socket, buf, len, MSG_NOSIGNAL);
        if (n == -1) {
            // EINTR - syscall прервал сигнал, пока он ждал
            if (errno == EINTR) continue;
            return -1;
        }
        buf += n;
        len -= n;
    }
    return 0;
}

void not_found(int client_socket) {
    FILE *file = fopen("www/not_found.html", "r");
    if (!file) {
        return;
    }

    // Чтение HTML-файла
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Выделение памяти под буфер файла
    char *file_content = malloc(file_size + 1);
    fread(file_content, 1, file_size, file);
    file_content[file_size] = '\0';
    fclose(file);

    // Формируем HTML-ответ
    char header[BUFFER_SIZE];
    int header_len = snprintf(header, sizeof(header),
                            "HTTP/1.1 404 Not Found\r\n"
                            "Content-Type: text/html; charset=utf-8\r\n"
                            "Connection: close\r\n"
                            "Content-Length: %ld\r\n\r\n",
                            file_size);

    // Отправка заголовка и следом файла
    send_all(client_socket, header, header_len);
    send_all(client_socket, file_content, file_size);

    free(file_content);
}

void bad_request(int client_socket) {
    FILE *file = fopen("www/bad_req.html", "r");
    if (!file) {
        not_found(client_socket);
        return;
    }

    // Чтение HTML-файла
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Выделение памяти под файл
    char *file_content = malloc(file_size + 1);
    fread(file_content, 1, file_size, file);
    file_content[file_size] = '\0';
    fclose(file);

    // Формируем HTML-ответ
    char header[BUFFER_SIZE];
    int header_len = snprintf(header, sizeof(header),
                            "HTTP/1.1 400 Bad Request\r\n"
                            "Content-Type: text/html; charset=utf-8\r\n"
                            "Connection: close\r\n"
                            "Content-Length: %ld\r\n\r\n",
                            file_size);

    // Отправка заголовка и следом файла
    send_all(client_socket, header, header_len);
    send_all(client_socket, file_content, file_size);

    free(file_content);
}

const char *get_content_type(const char *path) {
    // Находит последнее вхождение символа в строке 
    // и возвращает на него указатель
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";

    if (strcmp(dot, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(dot, ".css") == 0) return "text/css; charset=utf-8";
    if (strcmp(dot, ".js") == 0) return "application/javascript; charset=utf-8";
    if (strcmp(dot, ".json") == 0) return "application/json";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".jpg") == 0) return "image/jpeg";

    return "application/octet-stream";
}

void send_file(int client_socket, const char *filepath) {
    FILE *file = fopen(filepath, "rb");

    /*
     * stat заполняет st инфо о файле: размер, тип, права.
     * Если файла нет (-1) или необычный файл (!S_ISREG) -> 404
     */

    struct stat st;
    if (stat(filepath, &st) == -1 || !S_ISREG(st.st_mode)) {
        not_found(client_socket);
        return;
    }

    // Чтение HTML-файла
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Выделение памяти под файл
    char *file_content = malloc(file_size + 1);
    fread(file_content, 1, file_size, file);
    file_content[file_size] = '\0';
    fclose(file);

    // Формируем HTML-ответ
    char header[BUFFER_SIZE];
    int header_len = snprintf(header, sizeof(header),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: %s\r\n"
                            "Connection: close\r\n"
                            "Content-Length: %ld\r\n\r\n",
                            get_content_type(filepath), file_size);

    // Отправка заголовка и следом файла
    send_all(client_socket, header, header_len);
    send_all(client_socket, file_content, file_size);

    free(file_content);
}


