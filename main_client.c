#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <pthread.h>

#define PORT_NUM 1004
#define RESET   "\033[0m"

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

char* color_assignment(char* username)
{
    static char* colors[] = {
        "\033[31m",     // red
        "\033[32m",     // green
        "\033[33m",     // yellow
        "\033[34m",      // blue
        "\033[35m",     // magenta
        "\033[36m"     // cyan
    };

    int sum = 0;

    for (int i = 0; username[i] != '\0'; i++) {
        sum += username[i];
    }
    return colors[sum % 6];
}

void* thread_main_recv(void* args)
{
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);
	
	char buffer[1024];
	int n;
	while ((n = recv(sockfd, buffer, 1023, 0)) > 0) {

		buffer[n] = '\0';
		
		char* start = strchr(buffer, '[');
		char* end   = strchr(buffer, ']');

		if (start && end && end > start) {
            char name[50] = {0};
			int len = end - start - 1;
			//char username[50];
            if (len > 49) len = 49;
			strncpy(name, start + 1, len);
			name[len] = '\0';
			printf("\r%s%s%s\n", color_assignment(name), buffer, RESET);
		}
		else {
			printf("\r%s\n", buffer);
		}
        printf("Please enter the message: ");
        fflush(stdout);
	}
    printf("\nDisconnected\n");
    exit(0);
	
}

void* thread_main_send(void* args)
{
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);
	
	char buffer[256];
	while (1) {
		// You will need a bit of control on your terminal
		// console or GUI to have a nice input window.
		printf("\nPlease enter the message: ");
        fflush(stdout);
		if (fgets(buffer, 255, stdin) == NULL) break;
        buffer[strcspn(buffer, "\n")] = '\0';
		if (strlen(buffer) <= 1) break;
		if (send(sockfd, buffer, strlen(buffer), 0) < 0) break;
	}
	return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <IP> <room_number/new>\n", argv[0]);
        exit(1);
    }
	
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);

	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) error("ERROR connecting");

    char username[50];
    printf("What is your name? ");
    fgets(username, 50, stdin);
    send(sockfd, username, strlen(username), 0);
    sleep(1);
    send(sockfd, argv[2], strlen(argv[2]), 0);

	/* Threads */
    pthread_t tid1;
    pthread_t tid2;
	
    ThreadArgs* arg_send = malloc(sizeof(ThreadArgs));
    arg_send->clisockfd = sockfd;
	pthread_create(&tid1, NULL, thread_main_send, (void*) arg_send);

	ThreadArgs* arg_recv = malloc(sizeof(ThreadArgs));
	arg_recv->clisockfd = sockfd;
	pthread_create(&tid2, NULL, thread_main_recv, (void*) arg_recv);

	// parent will wait for sender to finish (= user stop sending message and disconnect from server)
	pthread_join(tid1, NULL);
	close(sockfd);
	return 0;
}
