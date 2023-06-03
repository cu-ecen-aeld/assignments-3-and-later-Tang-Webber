#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h> 
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <pthread.h>

#define BACKLOG 10
#define PORT "9000"
#define FILEPATH "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 2000000

int fd, sockfd, newsockfd;
bool connecting;

void signal_handler(int sig){
    if (sig == SIGTERM || sig == SIGINT) {
        syslog(LOG_USER, "Caught signal, exiting");
        connecting = false;
        close(newsockfd);
        close(sockfd);
        shutdown(sockfd, SHUT_RDWR);
        shutdown(newsockfd, SHUT_RDWR);
        remove(FILEPATH);
        exit(sig);
    }
}

char *read_file(char *filename, ssize_t count){
    FILE *fp = fopen(FILEPATH, "r");

    if (fp == NULL) {
        perror("fopen error");
	    exit(1);
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET); 

    char* read_buffer = malloc(fsize + 1);
    if (read_buffer == NULL) {
        perror("malloc error");
        exit(1);
    }

    fread(read_buffer, fsize, 1, fp);
    read_buffer[fsize] = 0;    
   
    fclose(fp);
    return read_buffer;

}

void write_file(char* filename, char* buffer, ssize_t count){
    int fd = open(filename, (O_RDWR | O_CREAT | O_APPEND | O_DSYNC), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));

    if (fd == -1) {
        perror("open file error");
	    exit(1);
    }
    int write_status = write(fd, buffer, count);

    if (write_status == -1) {
        perror("write error");
	    exit(1);
    }
    close(fd);
}

int main(int argc, char* argv[]) {
    // signal setup
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(struct sigaction));

    new_action.sa_handler = signal_handler;
    if (sigaction(SIGTERM, &new_action, NULL) != 0) {
        printf("Error %d (%s) registering for SIGTERM", errno, strerror(errno));
    }
    if (sigaction(SIGINT, &new_action, NULL) != 0) {
        printf("Error %d (%s) registering for SIGINT", errno, strerror(errno));
    }

    // daemon check
    bool daemon = false;
    if (argc >= 2 && strcmp(argv[1], "-d") == 0) {
        daemon = true; 
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct addrinfo* result, hints;
    int addrinfo_status;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;    

    addrinfo_status = getaddrinfo(NULL, PORT, &hints, &result);
    if (addrinfo_status != 0) // note that result is pointer to pointer
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(addrinfo_status));
        perror("error getaddrinfo");
        exit(1);
    }

    if (sockfd == -1) {
        perror("error open socket");
        exit(1);
    }

    // avoid Address already in use
    int optname = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optname, sizeof optname) == -1) {
        perror("setsockopt");
        exit(1);
    }

    int bind_status = bind(sockfd, result->ai_addr, result->ai_addrlen);
    if (bind_status == -1) {
        perror("Bind Error");
        exit(1);
    }

    freeaddrinfo(result);
    if (daemon) {
        printf("daemon mode\n");
        pid_t pid;
        pid = fork();

        if (pid < 0) {
            close(sockfd);
            perror("fork error");
            exit(EXIT_FAILURE);
        }

        if (pid > 0)
            exit(EXIT_SUCCESS);

        if (setsid() < 0) {
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        int chdir_status = chdir("/");
        if (chdir_status == -1) {
            close(sockfd);
            perror("chdir error");
            exit(EXIT_FAILURE);
        }
    }

    int listen_status = listen(sockfd, BACKLOG);
    if (listen_status == -1) {
        perror("listen error");
        exit(1);
    }

    char client_ip[INET_ADDRSTRLEN];
    connecting = true;
    char* buffer;
    while (connecting) {
        newsockfd = accept(sockfd, result->ai_addr, &(result->ai_addrlen));
        if (newsockfd != -1) {
            syslog(LOG_USER, "Accepted connection from %s", inet_ntop(AF_INET, &result, client_ip, INET_ADDRSTRLEN));
            printf("Accept connection from:  %s\n",  inet_ntop(AF_INET, &result, client_ip, INET_ADDRSTRLEN));
        }
        else{
            continue;
        }

        ssize_t read_val;

        buffer = malloc(BUFFER_SIZE * sizeof(char));
        memset(buffer, 0, BUFFER_SIZE);

        while ((read_val = recv(newsockfd, buffer, BUFFER_SIZE, 0)) > 0) {
            if (read_val == -1) {
                printf("read error\n");
                perror("read error");
                exit(1);
		    }
            write_file(FILEPATH, buffer, read_val);

            char *data_send;
            data_send = read_file(FILEPATH, read_val);
            int send_status = send(newsockfd, data_send, strlen(data_send), 0);
            free(data_send);

            if (send_status == -1) {
                perror("send_status error: ");
                exit(1);
            }
        }

        free(buffer);
        syslog(LOG_USER, "Closed connection from %s", inet_ntop(AF_INET, &result, client_ip, INET_ADDRSTRLEN));
        printf("Closed connectino from %s\n\n\nwaiting...\n", inet_ntop(AF_INET, &result, client_ip, INET_ADDRSTRLEN));
        close(newsockfd);
        shutdown(newsockfd, SHUT_RDWR);
    }
    remove(FILEPATH);
    close(sockfd);
    shutdown(sockfd, SHUT_RDWR);

    return 0;
}








