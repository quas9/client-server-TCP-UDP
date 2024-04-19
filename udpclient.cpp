#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_CONNECTIONS 20
#define MAX_MSG_SIZE 1024
#define RETRY_DELAY_US 100000

uint32_t message_lengths[MAX_CONNECTIONS];
int messages_processed[MAX_CONNECTIONS] = {0}; 
uint32_t messages_sent_from_server = 0; 

void doneMessage(const char *message) {
    printf("[DONE] %s\n", message);
}

void sockErr(const char *msg, int sockfd) {
    fprintf(stderr, "%s failed with error: %s\n", msg, strerror(errno));
    close(sockfd);
    exit(EXIT_FAILURE);
}

uint32_t datetimeToUnixTimestamp(int day, int month, int year, int hour, int minute, int second) {
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

uint32_t countNonEmptyLines(const char *file_name) {
    FILE *file = fopen(file_name, "r");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", file_name);
        return -1;
    }

    int count = 0;
    char line[1024]; 

    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] != '\n' && line[0] != '\0') { 
            count++;
        }
    }

    fclose(file);
    return count;
}

size_t myStrlen(char *str) {
    const char *s;
    for (s = str; *s; ++s)
        ;
    return (s - str );
}

int countOnes(int array[], int size) {
    int count = 0;
    for (int i = 0; i < size; i++) {
        if (array[i] == 1) {
            count++;
        }
    }
    return count;
}

char *createMsgForServer(const char *line, uint32_t index) {
    char *msg_for_server = (char *)malloc(MAX_MSG_SIZE + 1);
    if (msg_for_server == NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }

    char date[13], time[11], phone_number[13], message[MAX_MSG_SIZE];

    if (sscanf(line, "%10s %8s %12s %[^\n]", date, time, phone_number, message) != 4) {
        printf("Invalid input format\n");
        free(msg_for_server);
        return NULL;
    }
    uint32_t datagram_length = 0;
    uint32_t day, month, year, hour, minute, second;
    if (sscanf(date, "%2d.%2d.%4d", &day, &month, &year) != 3 ||
        sscanf(time, "%2d:%2d:%2d", &hour, &minute, &second) != 3) {
        printf("Invalid date or time format\n");
        free(msg_for_server);
        return NULL;
    }

    uint32_t timestamp = htonl(datetimeToUnixTimestamp(day, month, year, hour, minute, second));
    uint32_t message_length = htonl(myStrlen(message));

    memcpy(msg_for_server, &index, sizeof(uint32_t));
    memcpy(msg_for_server + sizeof(uint32_t), &timestamp, sizeof(uint32_t));
    memcpy(msg_for_server + 2 * sizeof(uint32_t), phone_number, 12);
    memcpy(msg_for_server + 2 * sizeof(uint32_t) + 12, &message_length, sizeof(uint32_t));
    memcpy(msg_for_server + 2 * sizeof(uint32_t) + 12 + sizeof(uint32_t), message, strlen(message) + 1);


    datagram_length = 3* sizeof(uint32_t) + 12 + myStrlen(message);
    message_lengths[index] = datagram_length;
    /*printf("myStrlen(message) : %d\n", myStrlen(message));
    printf("length : %d\n", datagram_length);
    printf("index : %d\n", index);*/
    return msg_for_server;
}

void sendMessageToServer(int sockfd, struct sockaddr_in *serv_addr, char* line, int message_index) {
    
    if (messages_processed[message_index]) {
        return;
    }
    

    char *msg_for_server = createMsgForServer(line, message_index);
    if (msg_for_server == NULL) {
        fprintf(stderr, "Failed to create message for server\n");
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_sent = sendto(sockfd, msg_for_server, message_lengths[message_index], 
                                MSG_NOSIGNAL, (struct sockaddr *)serv_addr, sizeof(*serv_addr));
    if (bytes_sent < 0) {
        fprintf(stderr, "Error sending message to server\n");
        free(msg_for_server);
        exit(EXIT_FAILURE);
    }

    free(msg_for_server);
    

}

void receiveMessageFromServer(int sockfd) {
    char buffer[MAX_MSG_SIZE];
    struct timeval tv = {0, 100 * 1000}; // 100 msec
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);
    uint32_t res = select(sockfd + 1, &fds, 0, 0, &tv);
    if (res > 0) {
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        uint32_t received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &addrlen);
        if (received <= 0) {
            sockErr("recvfrom", sockfd);
        }

        uint32_t count_indices = received / sizeof(uint32_t);  //amount of indices which was received
        printf("Returned: ");
        for (uint32_t i = 0; i < count_indices; ++i) {
            uint32_t index;
            index = ntohl(index);
            memcpy(&index, buffer + i * sizeof(uint32_t), sizeof(uint32_t));
            
            printf("%d ", index);
            messages_processed[index] = 1; 
            messages_sent_from_server++;
        }
        printf("\n"); 
    } else if (res == 0) {
        ;//printf("No data received from server.\n");
    } else {
        sockErr("select", sockfd);
    }
}

int main(int argc, char* argv[]) {
    uint32_t msg_index = 0;

    if (argc != 3){
        fprintf(stderr, "Usage: %s <server_address:port> <file_name>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char server_addr[256];
    int server_port;
    if (sscanf(argv[1], "%[^:]:%d", server_addr, &server_port) != 2) {
        fprintf(stderr, "Invalid server address:port format\n");
        return EXIT_FAILURE;
    }

    const char* file_name = argv[2];
    uint32_t count_of_msgs_in_single_file = countNonEmptyLines(file_name);

    if (count_of_msgs_in_single_file < 0) {
        fprintf(stderr, "Failed to count messages in the file.\n");
        return EXIT_FAILURE;
    }
    doneMessage("Starting client...");

    int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket < 0) {
        sockErr("socket", clientSocket);
    }

    int flags = fcntl(clientSocket, F_GETFL, 0);
    if (flags < 0) {
        sockErr("fcntl", clientSocket);
    }



    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_addr, &serverAddr.sin_addr);
    doneMessage("Connection established.");
    uint32_t file_processed = 0;
    FILE* file = fopen(file_name, "r");
    if (!file) 
    {
        fprintf(stderr, "Failed to open file: %s\n", file_name);
        close(clientSocket);
        return EXIT_FAILURE;
    }

   char line[1024];

    if (count_of_msgs_in_single_file < MAX_CONNECTIONS){
        while (msg_index <= count_of_msgs_in_single_file && 
                countOnes(messages_processed, count_of_msgs_in_single_file) < count_of_msgs_in_single_file) {
            fgets(line, sizeof(line), file);
            if (myStrlen(line) <= 1) {
                continue;
            }
            sendMessageToServer(clientSocket, &serverAddr, line, msg_index);
            receiveMessageFromServer(clientSocket);
            msg_index++;
            if (feof(file)) {
                rewind(file);
                msg_index = 0;
            }
        }
    }

    else {
        if (!file_processed) { 
            while (msg_index < count_of_msgs_in_single_file) {
                fgets(line, sizeof(line), file);
                if (myStrlen(line) <= 1) {
                    continue;
                }
                sendMessageToServer(clientSocket, &serverAddr, line, msg_index);
                receiveMessageFromServer(clientSocket);
                msg_index++;
                if (feof(file)) {
                    rewind(file); 
                }
            }
            msg_index = 0;
            file_processed = 1;
        }
        //printf("count ONES : %d\n", countOnes(messages_processed, count_of_msgs_in_single_file));
        while (countOnes(messages_processed, count_of_msgs_in_single_file) < 20) {
            printf("count ones : %d\n", countOnes(messages_processed, count_of_msgs_in_single_file));
            fgets(line, sizeof(line), file);
            if (myStrlen(line) <= 1) {
                continue;
            }
            sendMessageToServer(clientSocket, &serverAddr, line, msg_index);
            receiveMessageFromServer(clientSocket);
            msg_index++;
            if (feof(file)) {
                rewind(file);
                msg_index = 0;
            }
        }
    }
        
    doneMessage("All messages sent successfully.");
    fclose(file);
    close(clientSocket);

    return 0;
}