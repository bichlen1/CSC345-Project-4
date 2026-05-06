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
#define MAX_ROOMS 10

// User struct
typedef struct _USR {
	int clisockfd;		// socket file descriptor
    char username[50];
    char ip[INET_ADDRSTRLEN];
    int room_id;
	struct _USR* next;	// for linked list queue
} USR;

USR *head = NULL;
pthread_mutex_t user_lock = PTHREAD_MUTEX_INITIALIZER;
int room_count = 0;

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

typedef struct _ThreadArgs {
    int clisockfd;
} ThreadArgs;

void print_client_list(void) 
{
    pthread_mutex_lock(&user_lock);
    printf("\nConnected Clients\n");
    USR* cur = head;
    int count = 0;
    while (cur) {
        printf("[Room %d] %s (%s)\n", cur->room_id, cur->username, cur->ip);
        count++;
        cur = cur->next;
    }
    if (count == 0) {
        printf("(none)\n");
    }
    pthread_mutex_unlock(&user_lock);
}

void add_user(int sockfd, char* user, char* ip, int room)
{
    pthread_mutex_lock(&user_lock);
    USR* newUser = (USR*) malloc(sizeof(USR));
    newUser->clisockfd = sockfd;
    strncpy(newUser->username, user, 49);
    newUser->username[49] = '\0';
    strncpy(newUser->ip, ip, INET_ADDRSTRLEN - 1);
    newUser->ip[INET_ADDRSTRLEN - 1] = '\0';
    newUser->room_id = room;
    newUser->next = head;
    head = newUser;
    pthread_mutex_unlock(&user_lock);
}

void remove_user(int sockfd)
{
    pthread_mutex_lock(&user_lock);
    USR* cur = head;
    USR* prev = NULL;
    while (cur) {
        if (cur->clisockfd == sockfd) {
            if (prev) {
                prev->next = cur->next;
            } else {
                head = cur->next;
            }
            free(cur);
            pthread_mutex_unlock(&user_lock);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
    pthread_mutex_unlock(&user_lock);
}

void broadcast(int fromfd, int room, char* message, int is_system)
{
    pthread_mutex_lock(&user_lock);
	USR* sender = NULL;
    USR* tmp = head;
    while (tmp) {
        if (tmp->clisockfd == fromfd) {
            sender = tmp;
            break;
        }
        tmp = tmp->next;
    }
        
    USR* cur = head;
    char final_msg[1024];
    while (cur) {
        if (cur->clisockfd != fromfd && cur->room_id == room) {
            if (is_system) {
                snprintf(final_msg, sizeof(final_msg), "%s\n", message);
            } else if (sender) {
                snprintf(final_msg, sizeof(final_msg), "[%s (%s)]: %s", sender->username, sender->ip, message);
            }
            send(cur->clisockfd, final_msg, strlen(final_msg), 0);
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&user_lock);
}

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// Now, we receive/send messages
	char buffer[256];
	int  n;

	while ((n = recv(clisockfd, buffer, 255, 0)) > 0) {
        buffer[n] = '\0';
        pthread_mutex_lock(&user_lock);
        USR* u = head;
        int rid = -1;
        while (u) {
            if (u->clisockfd == clisockfd) {
                rid = u->room_id;
                break;
            }
            u = u->next;
        }
        pthread_mutex_unlock(&user_lock);
        if (rid != -1) {
            broadcast(clisockfd, rid, buffer, 0);
        }
	}

    pthread_mutex_lock(&user_lock);
    USR* u = head;

    while (u) {
        if (u->clisockfd == clisockfd) {
            char leave_msg[128];
            sprintf(leave_msg, "%s (%s) left the room!", u->username, u->ip);
            int rid = u->room_id;
            pthread_mutex_unlock(&user_lock);
            broadcast(clisockfd, rid, leave_msg, 1);
            printf("%s\n", leave_msg);
            remove_user(clisockfd);
            close(clisockfd);
            print_client_list();
            return NULL;
        }
        u = u->next;
    }
    pthread_mutex_unlock(&user_lock);
    close(clisockfd);
    return NULL;
}

int main(int argc, char *argv[])
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	//if (sockfd < 0) error("ERROR opening socket");
	struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT_NUM);

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR on binding");
    }
	listen(sockfd, 5); // maximum number of connections = 5
    printf("Server running %d...\n", PORT_NUM);

	while(1) {
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clen);
        if (newsockfd < 0) continue;

        char username[50] = {0}, room_req[50] = {0};
        recv(newsockfd, username, 49, 0);
        username[strcspn(username, "\n\r")] = '\0';
        recv(newsockfd, room_req, 49, 0);
        room_req[strcspn(room_req, "\n\r")] = '\0';

        int rid;
        if (strcmp(room_req, "new") == 0) {
            rid = ++room_count;
        } else {
            rid = atoi(room_req);
            if (rid <= 0 || rid > room_count) {
                rid = ++room_count;
            }
        }

        add_user(newsockfd, username, inet_ntoa(cli_addr.sin_addr), rid);
        char joinMsg[256];
        snprintf(joinMsg, sizeof(joinMsg), "%s, (%s) joined room %d!", username, inet_ntoa(cli_addr.sin_addr), rid);
        broadcast(newsockfd, rid, joinMsg, 1);
        printf("%s\n", joinMsg);
        print_client_list();

        ThreadArgs* args = malloc(sizeof(ThreadArgs));
        args->clisockfd = newsockfd;
        pthread_t tid;
        pthread_create(&tid, NULL, thread_main, (void*) args);
	}
	return 0; 
}
