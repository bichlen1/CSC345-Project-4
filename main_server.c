#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT_NUM 1004

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

typedef struct _USR {
	int clisockfd;		// socket file descriptor
    char username[50];
    char ip[INET_ADDRSTRLEN];
	struct _USR* next;	// for linked list queue
} USR;

USR *head = NULL;
USR *tail = NULL;

USR* find_user(int sockfd)
{
    USR* cur = head;

    while (cur != NULL) {
        if (cur->clisockfd == sockfd) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

void add_tail(int newclisockfd, char* username, char* ip)
{
	USR* newUser = (USR*) malloc(sizeof(USR));

    newUser->clisockfd = newclisockfd;

    strcpy(newUser->username, username);
    strcpy(newUser->ip, ip);

    newUser->next = NULL;

    if (head == NULL) {
        head = newUser;
        tail = newUser;
    } else {
        tail->next = newUser;
        tail = newUser;
    }
}

void remove_user(int sockfd)
{
    USR* cur = head;
    USR* prev = NULL;

    while (cur != NULL) {
        if (cur->clisockfd == sockfd) {
            if (prev == NULL) {
                head = cur->next;
            } else {
                prev->next = cur->next;
            }
            if (cur == tail) {
                tail = prev;
            }
            free(cur);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

void broadcast_message(char* message) 
{
    USR* cur = head;

    while (cur != NULL) {
        send(cur->clisockfd, message, strlen(message), 0);
        cur = cur->next;
    }
}

void broadcast(int fromfd, char* message)
{
	USR* sender = find_user(fromfd);

    if (sender == NULL) {
        return;
    }

	// traverse through all connected clients
	USR* cur = head;
	while (cur != NULL) {
		// check if cur is not the one who sent the message
		if (cur->clisockfd != fromfd) {
			char buffer[512];

			// prepare message
			sprintf(buffer, "[%s]:%s", sender->username, message);
			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() failed");
		}

		cur = cur->next;
	}
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	int nsen, nrcv;

	nrcv = recv(clisockfd, buffer, 255, 0);
    buffer[nrcv] = '\0';
	if (nrcv < 0) error("ERROR recv() failed");

	while (nrcv > 0) {
		// we send the message to everyone except the sender
		broadcast(clisockfd, buffer);

		nrcv = recv(clisockfd, buffer, 255, 0);
		if (nrcv < 0) buffer[nrcv] = '\0';
	}

    USR* user = find_user(clisockfd);

    if (user != NULL) {
        char leaveMsg[256];
        sprintf(leaveMsg, "%s left", user->username);
        broadcast_message(leaveMsg);
    }
    
    remove_user(clisockfd);

	close(clisockfd);
	//-------------------------------

	return NULL;
}

int main(int argc, char *argv[])
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	//serv_addr.sin_addr.s_addr = inet_addr("192.168.1.171");	
	serv_addr.sin_port = htons(PORT_NUM);

	int status = bind(sockfd, 
			(struct sockaddr*) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");

	listen(sockfd, 5); // maximum number of connections = 5

	while(1) {
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");

        char username[50];

        memset(username, 0, sizeof(username));

        int n = recv(newsockfd, username, sizeof(username) -1, 0);

        if (n <= 0) {
            close(newsockfd);
            continue;
        }

        username[n] = '\0';

		printf("%s connected: %s\n", username, inet_ntoa(cli_addr.sin_addr));
		add_tail(newsockfd, username, inet_ntoa(cli_addr.sin_addr)); // add this new client to the client list

        char joinMsg[256];
        sprintf(joinMsg, "%s joined", username);
        broadcast_message(joinMsg);

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	return 0; 
}

	return 0; 
}
