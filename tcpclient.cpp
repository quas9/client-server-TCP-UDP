#define _CRT_SECURE_NO_WARNINGS
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
// Директива линковщику: использовать библиотеку сокетов
#pragma comment(lib, "ws2_32.lib")
#else // LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SERVER_ADDR  "192.168.50.7" //"127.0.0.1"
#define SERVER_PORT 9000
#define MAX_ATTEMPTS 10
#define BUFFER_SIZE 341000
#define RESPONSE_SIZE 1024

int init()
{
#ifdef _WIN32
    // Для Windows следует вызвать WSAStartup перед началом использования сокетов
    WSADATA wsa_data;
    return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
#else
    return 1; // Для других ОС действий не требуется
#endif
}
void deinit()
{
#ifdef _WIN32
    // Для Windows следует вызвать WSACleanup в конце работы
    WSACleanup();
#else
    // Для других ОС действий не требуется
#endif
}
int sock_err(const char* function, int s)
{
    int err;
#ifdef _WIN32
    err = WSAGetLastError();
#else
    err = errno;
#endif
    fprintf(stderr, "%s: socket error: %d\n", function, err);
    return -1;
}
void s_close(int s)
{
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

void error(const char* message)
{
    fprintf(stderr, "Error: %s\n", message);
    exit(EXIT_FAILURE);
}

void done_message(const char* message)
{
    printf("[DONE] %s\n", message);
}

unsigned int datetime_to_unix_timestamp(int day, int month, int year, int hour, int minute, int second) 
{
    struct tm timeinfo;
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;

    unsigned int unix_time = mktime(&timeinfo);
    return unix_time;
}

void send_message(unsigned int clientSocket, unsigned int message_number, unsigned int timestamp, const char* phone_number, const char* message)
{
    unsigned int message_number_network_order = htonl(message_number);
    send(clientSocket, (char*)&message_number_network_order, sizeof(message_number_network_order), 0);

    unsigned int timestamp_network_order = htonl(timestamp);
    send(clientSocket, (char*)&timestamp_network_order, sizeof(timestamp_network_order), 0);

    send(clientSocket, phone_number, 12, 0);

    unsigned int message_length = strlen(message);

    unsigned int message_length_network_order = htonl(message_length);
    send(clientSocket, (char*)&message_length_network_order, sizeof(message_length_network_order), 0);

    send(clientSocket, message, message_length, 0);
}

int main(int argc, char* argv[]) 
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <server_address:port> <file_name>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char server_addr[256];
    int server_port;
    if (sscanf(argv[1], "%[^:]:%d", server_addr, &server_port) != 2) 
    {
        fprintf(stderr, "Invalid server address:port format\n");
        return EXIT_FAILURE;
    }

    const char* file_name = argv[2];

    done_message("Starting client...");

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        error("WSAStartup failed");
    }

    unsigned int clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET)
    {
        error("Socket creation failed");
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_addr, &serverAddr.sin_addr);

    done_message("Attempting to connect to server...");

    int attempt = 0;
    while (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) 
    {
        if (++attempt >= MAX_ATTEMPTS)
        {
            error("Connection failed after maximum attempts");
        }
        done_message("Connection failed, retrying in 100 ms...");
        Sleep(100);

        }

    done_message("Connection established.");

    FILE* file = fopen(file_name, "r");
    if (!file) 
    {
        fprintf(stderr, "Failed to open file: %s\n", file_name);
        return EXIT_FAILURE;
    }
    int messages_sent = 0; // счетчик отправленных сообщений
    int ok_received = 0;   // счетчик "ok" 

    const char signal[] = "put";
    send(clientSocket, signal, 3, 0); //отправили сокет с путом

    char line[BUFFER_SIZE];
    int message_number = 0;
    while (fgets(line, sizeof(line), file) != NULL) 
    {
        if (strlen(line) <= 1) 
        {
            continue;
        }


        char date[13], time[11], phone_number[13], message[BUFFER_SIZE];
        date[12] = '\0';
        time[10] = '\0';

        if (sscanf(line, "%10s %8s %12s %[^\n]", date, time, phone_number, message) != 4)
        {
            error("Invalid input format in the input file");    
            continue; 
        }

        int day, month, year, hour, minute, second;
        if (sscanf(date, "%2d.%2d.%4d", &day, &month, &year) != 3 ||
            sscanf(time, "%2d:%2d:%2d", &hour, &minute, &second) != 3) 
        {
            error("Invalid date or time format in the input file");
            continue;
        }
        //printf("%d\n", day);
        //printf("%d\n", month);
        //printf("%d\n", year);
        //printf("%d\n", hour);
        //printf("%d\n", minute);
        //printf("%d\n", second);

        unsigned int timestamp = datetime_to_unix_timestamp(day, month, year, hour, minute, second);

        //printf("%u\n", timestamp);

        send_message(clientSocket, message_number, timestamp, phone_number, message);
        messages_sent++;
        message_number++;
    }

    done_message("All messages sent successfully.");

    int received_ok_count = 0;
    char current_byte;

    while (received_ok_count < messages_sent * 2) {
        int received_bytes = recv(clientSocket, &current_byte, 1, 0);
        //printf("%c", current_byte);
        if (received_bytes > 0) {
            if (current_byte == 'o') {
                int received_bytes_k = recv(clientSocket, &current_byte, 1, MSG_PEEK);
                if (received_bytes_k > 0 && current_byte == 'k') {
                    recv(clientSocket, &current_byte, 1, 0);
                    //printf("%c\n", current_byte);
                    received_ok_count++;
                }
                else {
                    received_ok_count++;
                }
            }
            else if (current_byte == 'k') {
                received_ok_count++;
            }
            else {
                break;
            }
        }
        else if (received_bytes == 0) {
            break;
        }
        else {
            break;
        }
    }
    

    fclose(file);
    s_close(clientSocket);
    deinit();

    return EXIT_SUCCESS;
}