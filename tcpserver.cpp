#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
// Директива линковщику: использовать библиотеку сокетов
#pragma comment(lib, "ws2_32.lib")
#else // LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>
#include <time.h>

int count = 0;
#define MAX_CLIENTS 20
#define BUF_SIZE 350000
#define PHONE_NUMBER_LENGTH 13

bool stop_flag = false;

uint32_t containsSubstring(const char *mainStr, const char *subStr) {
    int main_str_len = strlen(mainStr);
    int second_str_len = strlen(subStr);

    for (int i = 0; i <= main_str_len - second_str_len; ++i) {
        int j;
        for (j = 0; j < second_str_len; ++j) {
            if (mainStr[i + j] != subStr[j]) {
                break; 
            }
        }
        if (j == second_str_len) {
            return 1; 
        }
    }

    return 0;
}

char* unix_timestamp_to_datetime(uint32_t unix_time) 
{
    time_t rawtime = unix_time;
    struct tm * timeinfo = localtime(&rawtime);
    char* formatted_time = (char*)malloc(20 * sizeof(char));
    if (formatted_time == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    strftime(formatted_time, 20, "%d.%m.%Y %H:%M:%S", timeinfo);
    
    return formatted_time;
}

void getClientAddress(struct sockaddr_in *addr) {
    char ip[INET_ADDRSTRLEN];
    printf("%s:%d\n", inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN), ntohs(addr->sin_port));
}


void writeResponse(int sock, char* buffer, struct sockaddr_in *addr) {
    FILE* file = fopen("msg.txt", "a");

    uint32_t msg_number;
    memcpy(&msg_number, buffer, sizeof(uint32_t));
    msg_number = ntohl(msg_number);

    uint32_t unix_timestamp;
    memcpy(&unix_timestamp, buffer + sizeof(uint32_t), sizeof(uint32_t));
    unix_timestamp = ntohl(unix_timestamp);
    char* formatted_time = unix_timestamp_to_datetime((time_t)unix_timestamp);   

    char phone_number[PHONE_NUMBER_LENGTH]; 
    memcpy(phone_number, buffer + sizeof(uint32_t) * 2, PHONE_NUMBER_LENGTH - 1);
    phone_number[PHONE_NUMBER_LENGTH - 1] = '\0';

    uint32_t message_length;
    memcpy(&message_length, buffer + sizeof(uint32_t) * 2 + 12, sizeof(uint32_t));
    message_length = ntohl(message_length);

    char* message_content = (char*)malloc((message_length + 1) * sizeof(char));
    if (message_content == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    memcpy(message_content, buffer + sizeof(uint32_t) * 2 + 12 + sizeof(uint32_t), message_length);
    message_content[message_length] = '\0';

    fprintf(file, "%s:%d ", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    fprintf(file, "%s ", formatted_time);
    fprintf(file, "%s ", phone_number);
    fprintf(file, "%s\n", message_content);
    fclose(file);

    send(sock, "ok", 2, 0);

    if (strcmp(message_content, "stop") == 0){
        printf("'stop' founded, terminating...\n");
        stop_flag = true;
        exit(0);
    }

    free(message_content);
}


void readSocket(int sock, struct sockaddr_in *addr) {
    char* buffer = (char*) malloc (BUF_SIZE * sizeof(int));
    bool msg_flag = false;
    int received;
    static char accumulated_data[BUF_SIZE];
    static size_t accumulated_len = 0;

    while ((received = recv(sock, buffer, BUF_SIZE, 0)) > 0) {
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Ошибка EAGAIN или EWOULDBLOCK: нет данных для чтения
                continue;
            } else {
                perror("Error reading from socket\n");
                break;
            }
        }
        if (msg_flag == false)
        {
            memcpy(accumulated_data + accumulated_len, buffer, received);
            accumulated_len += received;
            if (containsSubstring(accumulated_data, "put")) {
                msg_flag = true;
                memset(accumulated_data, 0, BUF_SIZE);
                accumulated_len = 0;
            } 
            else 
            {
                ;
            }
        }
        else 
        {
            writeResponse(sock, buffer, addr);
            memset(buffer, 0, BUF_SIZE);
        }  
    }
    free(buffer);
}


int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: ./tcpserver <port>\n");
        return 0;
    }

    uint32_t port = atoi(argv[1]);
    struct sockaddr_in addr;

    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);  

    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    int fl = fcntl(listen_sock, F_GETFL, 0);
    fcntl(listen_sock, F_SETFL, fl | O_NONBLOCK);

    if (listen_sock < 0) {
        perror("Error: socket wasn't found");
        exit(0);
    }

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Error: bind");
        exit(0);
    }

    if (listen(listen_sock, 1) < 0) {
        perror("Error: listening");
        exit(0);
    }

    printf("Listening on port %d\n", port);


    uint32_t sock_count = 0;
    uint32_t connect_status[MAX_CLIENTS]; 
    struct pollfd pfd[MAX_CLIENTS + 1];
    int i;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        pfd[i].fd = connect_status[i];
        pfd[i].events = POLLIN | POLLOUT;
    }

    pfd[MAX_CLIENTS].fd = listen_sock;
    pfd[MAX_CLIENTS].events = POLLIN;

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        connect_status[i] = -1;
    }

    while (!stop_flag){
        uint32_t ev_cnt = poll(pfd, sizeof(pfd) / sizeof(pfd[0]), 1000);
        if (ev_cnt > 0) {
            for (size_t i = 0; i < MAX_CLIENTS; i++) {
                
                if (pfd[i].revents & POLLHUP) 
                {
                    printf("Peer disconnected: ");
                    getClientAddress(&addr);
                    connect_status[i] = -1;
                    close(pfd[i].fd);
                    continue;
                }
                
                if (pfd[i].revents & POLLERR) 
                {
                    printf("Peer disconnected: ");
                    getClientAddress(&addr);
                    connect_status[i] = -1;
                    close(pfd[i].fd);
                    continue;
                }
                
                if (pfd[i].revents & POLLIN)
                {             

                    readSocket(pfd[i].fd, &addr);
                }
                
                /*if (pfd[i].revents & POLLOUT)
                {
                    printf("444\n");
                    send(pfd[i].fd, "ok", 2, 0);
                }*/
            }
            if (pfd[MAX_CLIENTS].revents & POLLIN) {
                uint32_t socklen = sizeof(addr);
                int new_cli = accept(pfd[MAX_CLIENTS].fd, (struct sockaddr *)&addr, &socklen);
                if (new_cli < 0) {
                    perror("accept");
                    continue;
                }
                printf("Peer connected: ");
                getClientAddress(&addr);

                if (sock_count < MAX_CLIENTS) {
                    for (size_t i = 0; i < MAX_CLIENTS; i++) {
                        if (connect_status[i] == -1) {
                            connect_status[i] = new_cli;
                            pfd[i].fd = new_cli;
                            pfd[i].events = POLLIN | POLLOUT;
                            break;
                        }
                    }
                } else {
                    close(new_cli);
                    printf("No free slots for new connection. Connection closed.\n");
                }
            }
        }
    }


    for (size_t i = 0; i < sock_count; i++) {
        close(pfd[i].fd);
    }
    close(listen_sock);
    return 0;
}