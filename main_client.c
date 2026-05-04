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

<<<<<<< HEAD
#define PORT_NUM 8080
=======
#define PORT_NUM 1004
>>>>>>> 21813196531fef80faa404a9cdb9ae2b8b41c683
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
        "\033[34m"      // blue
    };

    int sum = 0;

    for (int i = 0; username[i] != '\0'; i++) {
        sum += username[i];
    }

    return colors[sum % 4];
}

void* thread_main_recv(void* args)
{
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// keep receiving and displaying message from server
	char buffer[512];
	int n;

	while ((n = recv(sockfd, buffer, 512, 0)) > 0) {
		buffer[n] = '\0';
<<<<<<< HEAD
		char username[50];

		sscanf(buffer, "%49s", username);

		char *msg = (char *)strchr(buffer, ' ');
		
		if(msg != NULL)
			msg++;

		printf("%s%s: %s%s\n", color_assignment(username), username, msg, RESET);


        // if (sscanf(buffer, "[%49[^]]", username) == 1) {
        //     printf("%s%s%s\n", color_assignment(username), buffer, RESET);
        // } else {
        //     printf("%s\n", buffer);
        // }
=======

		char username[50];

        if (sscanf(buffer, "[%49[^]]", username) == 1) {
            printf("%s%s%s\n", color_assignment(username), buffer, RESET);
        } else {
            printf("%s\n", buffer);
        }
>>>>>>> 21813196531fef80faa404a9cdb9ae2b8b41c683
	}
	return NULL;
}

void* thread_main_send(void* args)
{
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// keep sending messages to the server
	char buffer[256];
	int n;

	while (1) {
		// You will need a bit of control on your terminal
		// console or GUI to have a nice input window.
		//printf("\nPlease enter the message: ");
		memset(buffer, 0, 256);
		fgets(buffer, 255, stdin);

		if (strlen(buffer) == 1) buffer[0] = '\0';

		n = send(sockfd, buffer, strlen(buffer), 0);
		if (n < 0) error("ERROR writing to socket");

		if (n == 0) break; // we stop transmission when user type empty string
	}

	return NULL;
}

int main(int argc, char *argv[])
{
<<<<<<< HEAD

	printf("CLIENT RUNNING\n");	//test
	fflush(stdout); //test

	if (argc < 2) error("Please specify hostname");
=======
	if (argc < 2) error("Please speicify hostname");
>>>>>>> 21813196531fef80faa404a9cdb9ae2b8b41c683

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);

	printf("Try connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));

	int status = connect(sockfd, 
			(struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR connecting");

    char username[50];

    printf("What is your name? ");
<<<<<<< HEAD
	
	fflush(stdout);	//test

    fgets(username, sizeof(username), stdin);


=======
    fgets(username, sizeof(username), stdin);

>>>>>>> 21813196531fef80faa404a9cdb9ae2b8b41c683
    username[strcspn(username, "\n")] = '\0';

    send(sockfd, username, strlen(username), 0);

	pthread_t tid1;
	pthread_t tid2;

	ThreadArgs* args;
	
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid1, NULL, thread_main_send, (void*) args);

	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid2, NULL, thread_main_recv, (void*) args);

	// parent will wait for sender to finish (= user stop sending message and disconnect from server)
	pthread_join(tid1, NULL);

	close(sockfd);

	return 0;
}
