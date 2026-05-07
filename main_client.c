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

#define PORT_NUM 1004           //Server port number
#define RESET   "\033[0m"       // Reset color

/* Error handling */
void error(const char *msg)
{
	perror(msg);
	exit(0);
}

/* Sturct  used to pass arguments into thread */
typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

/* Function to assigne colors to each username using ASCII sum */
char* color_assignment(char* username)
{
    static char* colors[] = {
        "\033[31m",     // red
        "\033[32m",     // green
        "\033[33m",     // yellow
        "\033[34m",     // blue
        "\033[35m",     // magenta
        "\033[36m"      // cyan
    };

    int sum = 0;

    /* Sum ACSII values of username chars */
    for (int i = 0; username[i] != '\0'; i++) {
        sum += username[i];
    }

    /* Pick a color based on modulo */
    return colors[sum % 6];
}

/* Recieves message and prints to terminal */
void* thread_main_recv(void* args)
{
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	char buffer[1024];
	int n;

	while ((n = recv(sockfd, buffer, 1023, 0)) > 0) {

		buffer[n] = '\0';

		/* Message format */
		char* start = strchr(buffer, '[');
		char* end   = strchr(buffer, ']');

		if (start && end && end > start) {

            char name[50] = {0};

			int len = end - start - 1;
			
            /* Prenvent overflow */
            if (len > 49) len = 49;

			strncpy(name, start + 1, len);
			name[len] = '\0';

            /* Print colored message */
			printf("\r%s%s%s\n", color_assignment(name), buffer, RESET);
		}
		else {
            /* System messages */
			printf("\r%s\n", buffer);
		}

        /* Reprint input prompt after receiving message */
        printf("Please enter the message: ");
        fflush(stdout);
	}

    /* If recv fails server disconnects */
    printf("\nDisconnected\n");
    exit(0);
	
}

/* Reads user input and sends it to server */
void* thread_main_send(void* args)
{
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	char buffer[256];

	while (1) {
		
        /* Prompt user for message */
		printf("\nPlease enter the message: ");
        fflush(stdout);
		
        /* Read input from user */
		if (fgets(buffer, 255, stdin) == NULL) break;

        /* Remove newline character */
        buffer[strcspn(buffer, "\n")] = '\0';

        /* Exit if empty input */
		if (strlen(buffer) <= 1) break;

        /* Send message to server */
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
	
    /* Create socket */
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in serv_addr;

	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);

    /* Connect to server */
	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) error("ERROR connecting");

    /* Username input asking "What is your name? "*/
    char username[50];
    printf("What is your name? ");
    fgets(username, 50, stdin);

    /* Send username to server */
    send(sockfd, username, strlen(username), 0);
    /* Small delay to ensure ordering of messages*/
    sleep(1);
    /* Send room request */
    send(sockfd, argv[2], strlen(argv[2]), 0);

    pthread_t tid1;     // sender thread
    pthread_t tid2;     // reciever thread

    /* Create sending thread */
    ThreadArgs* arg_send = malloc(sizeof(ThreadArgs));
    arg_send->clisockfd = sockfd;

	pthread_create(&tid1, NULL, thread_main_send, (void*) arg_send);

    /* Create receiving thread */
	ThreadArgs* arg_recv = malloc(sizeof(ThreadArgs));
	arg_recv->clisockfd = sockfd;
	pthread_create(&tid2, NULL, thread_main_recv, (void*) arg_recv);

	/* Wait for sender thread to finish */
	pthread_join(tid1, NULL);
    /* Close socket when done */
	close(sockfd);
    
	return 0;
}
