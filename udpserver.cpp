#define _CRT_SECURE_NO_WARNINGS
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
#include <fcntl.h>
#include <time.h>

#define MAX_CONNECTIONS 20
#define BUF_SIZE 8192

int stop_flag = 0;
int last_processed_msg = -1;

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

struct structDBForClintID {
	int socket;					//find socket for every client
	struct sockaddr_in addr;
	unsigned int ip;
	unsigned int port;
	int detect_similar[20];		//find same msges
	int message_number;
} ;

int nonBlockMode(int s)
{
	unsigned long mode = 1;
	return ioctlsocket(s, FIONBIO, &mode);
}

void cleanupClientData(struct structDBForClintID* db, int index) {
	db[index].message_number = 0;
	for (int message_index = 0; message_index < 20; message_index++) {
		db[index].detect_similar[message_index] = -1;
	}
}

void myInetNtop(unsigned int ip, unsigned short port, char* ip_str) {
	sprintf(ip_str, "%u.%u.%u.%u:%u",
		(ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
		(ip >> 8) & 0xFF, ip & 0xFF,port);
}

int makeComparing(struct structDBForClintID* db, int i) {
	int min_index = 0;
	int min_value = INT_MAX;
	for (int j = 0; j < 20; j++) {
		int current_value = ntohl(db[i].detect_similar[j]); 
		if (current_value < min_value) {
			min_value = current_value;
			min_index = j;
		}
	}

	return min_index;
}


char* unixTimestamp(unsigned int unix_time)
{
	time_t rawtime = unix_time;
	struct tm* timeinfo = localtime(&rawtime);
	char* formatted_time = (char*)malloc(20 * sizeof(char));
	if (formatted_time == NULL) {
		perror("Memory allocation error");
		exit(EXIT_FAILURE);
	}
	strftime(formatted_time, 20, "%d.%m.%Y %H:%M:%S", timeinfo);
	return formatted_time;
}


void addTodb(struct structDBForClintID* db, int index, char* buffer) {
	unsigned int msg_number;
	memcpy(&msg_number, buffer, sizeof(unsigned int));
	//msg_number = ntohl(msg_number);

	for (int j = 0; j <= 20; j++) {
		if (db[index].detect_similar[j] == msg_number) {
			return;
		}
		if (db[index].detect_similar[j] == -1) {		// if we have similar
			db[index].detect_similar[j] = msg_number;
			break;
		}
		if (j == 20) {									// if count of msg > 19
			int k = makeComparing(db, index);
			db[index].detect_similar[k] = msg_number;
		}
	}
}


void parseMsg(char* buffer, unsigned int* msg_number, unsigned int* unix_time,
							char* phone_number, unsigned int* message_length, char* message_content) {
	memcpy(msg_number, buffer, sizeof(unsigned int));
	memcpy(unix_time, buffer + sizeof(unsigned int), sizeof(unsigned int));
	*unix_time = ntohl(*unix_time);
	memcpy(phone_number, buffer + sizeof(unsigned int) + sizeof(unsigned int), 12);
	memcpy(message_length, buffer + sizeof(unsigned int) + sizeof(unsigned int) + 12, sizeof(unsigned int));
	memcpy(message_content, buffer + sizeof(unsigned int) + sizeof(unsigned int) + 12 + sizeof(unsigned int), BUF_SIZE - 24);
}

void writeToFileMsg(FILE* file, unsigned int unix_time, unsigned int ip, 
							unsigned short port, char* phone_number, char* message_content) 
{
	char ip_str[20];
	myInetNtop(ip, port, ip_str); 
	fprintf(file, "%s %s %s %s\n", ip_str, unixTimestamp(unix_time), phone_number, message_content);
}

void getAndAnalyseDatagram(struct structDBForClintID* db, int msg_index) {
	FILE* file = fopen("msg.txt", "a+");
	int res;
	char buffer[BUF_SIZE];
	memset(buffer, 0, BUF_SIZE);
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	res = recvfrom(db[msg_index].socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, &addrlen);
	db[msg_index].ip = ntohl(addr.sin_addr.s_addr);
	db[msg_index].port = addr.sin_port;
	db[msg_index].addr = addr;
	db[msg_index].message_number++;

	char ip_str[20];
	myInetNtop(db[msg_index].ip, db[msg_index].port, ip_str);

	unsigned int msg_number;
	unsigned int unix_time;
	char phone_number[13];
	phone_number[12] = '\0';
	unsigned int message_length;
	char* message_content = (char*)malloc(BUF_SIZE * sizeof(char));
	parseMsg(buffer, &msg_number, &unix_time, phone_number, &message_length, message_content);


	if (last_processed_msg != ntohl(msg_number)) {
		//printf("%d\n", ntohl(msg_number));
		addTodb(db, msg_index, buffer);
		writeToFileMsg(file, unix_time, db[msg_index].ip, db[msg_index].port, phone_number, message_content);
		printf("Message %d from client %s was processed\n", ntohl(msg_number), ip_str);

	}
	
	last_processed_msg = ntohl(msg_number);

	if (strcmp(message_content, "stop") == 0)
	{
		stop_flag = 1;
	}

	free(message_content);
}

void sendResponseDatagram(struct structDBForClintID* db, int msg_index) {
	unsigned int last_processed_msg = 0;

	for (int j = 0; j < 20 && db[msg_index].detect_similar[j] != -1; j++) {
		last_processed_msg = db[msg_index].detect_similar[j];
	}

	int addrlen = sizeof(db[msg_index].addr);
	int bytes_sent = sendto(db[msg_index].socket, (char*)&last_processed_msg,
							sizeof(unsigned int), 0, (struct sockaddr*)&db[msg_index].addr, addrlen);
}

void initializeConnections(struct structDBForClintID db[MAX_CONNECTIONS], 
										int* active_connections, unsigned int start_port, unsigned int last_port) 
{
	struct sockaddr_in addr;
	int port_number = start_port;

	for (int connection_index = 0; connection_index < MAX_CONNECTIONS && port_number <= last_port; connection_index++) {
		db[connection_index].socket = socket(AF_INET, SOCK_DGRAM, 0);
		db[connection_index].message_number = 0;
		for (int message_index = 0; message_index < 20; message_index++) {
			db[connection_index].detect_similar[message_index] = -1;
		}
		if (db[connection_index].socket < 0) {
			sock_err("socket", db[connection_index].socket);
			continue;
		}
		nonBlockMode(db[connection_index].socket);

		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port_number);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (bind(db[connection_index].socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			sock_err("bind", db[connection_index].socket);
			closesocket(db[connection_index].socket); 
			continue; 
		}
		port_number++;
		(*active_connections)++;
	}
}

int main(int argc, const char* argv[])
{
	if (argc != 3) {
		printf("Usage: ./udpserver <port> <port>\n");
		return 0;
	}
	unsigned int start_port = atoi(argv[1]);
	unsigned int last_port = atoi(argv[2]);
		
	struct sockaddr_in addr;
	char ip_str[20];
	init();

	struct structDBForClintID db[MAX_CONNECTIONS];
	int active_connections = 0;
	int port_number = start_port;
	
	initializeConnections(db, &active_connections, start_port, last_port);
	printf("Listening on ports from %d to %d...", start_port, last_port);


	WSAEVENT events[2]; // Первое событие - прослушивающего сокета, второе - клиентских соединений

	int i;
	events[0] = WSACreateEvent();
	events[1] = WSACreateEvent();

	for (i = 0; i < active_connections; i++)
		WSAEventSelect(db[i].socket, events[1], FD_READ );

	while (!stop_flag)
	{
		WSANETWORKEVENTS ne;
		// Ожидание событий в течение секунды
		DWORD dw = WSAWaitForMultipleEvents(2, events, FALSE, 1, FALSE);
		WSAResetEvent(events[0]);
		WSAResetEvent(events[1]);


		for (i = 0; i < active_connections; i++)
		{
			if (0 == WSAEnumNetworkEvents(db[i].socket, events[1], &ne))
			{
				if (ne.lNetworkEvents & FD_READ)
				{
					getAndAnalyseDatagram(db, i);
					sendResponseDatagram(db, i);
					cleanupClientData(db, i);
				}
				//if (stop_flag)
					//goto close_sockets;
			}
		}
		if (stop_flag)
			break;
	}
	printf("Message with 'stop' content was processed");

	for (int i = 0; i < active_connections; i++) {
		closesocket(db[i].socket);
	}
	WSACleanup();
	return 0;
}
