#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <semaphore.h>

#define PORT 12345
#define SERVER_ADDR "127.0.0.1"

int sockfd = -1;
struct sockaddr_in server_addr;

void* send_function(void* arg){
    char buffer[64];
    ssize_t bytes_read;

    while(1){

        bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; 
            write(sockfd, buffer, strlen(buffer));
            //printf("\rClient: %s\n", buffer);
            // tin nhắn ngắt kết nối với server
            if (strncmp(buffer, "exit", 4) == 0) {
                printf("Client: Connection close.\n");
                close(sockfd);
                break;
            }
        } else {
            perror("read");
        }
    }

    return NULL;
}

void* receive_function(void* arg){
    char buffer[64];
    ssize_t bytes_read;
    while(1){
        bytes_read = read(sockfd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("%s\n", buffer);
        } else {
            perror("read từ server");
            break;
        }
    }
    return NULL;
}

int main(){
    memset(&server_addr, 0, sizeof(server_addr));

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);
    server_addr.sin_port = htons(PORT);

    //bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0){
        pthread_t send_thread, receive_thread;
        pthread_create(&send_thread, &attr, send_function, NULL);
        pthread_create(&receive_thread, &attr, receive_function, NULL);

        pthread_join(send_thread, NULL);
        pthread_join(receive_thread, NULL);
    }
    return 0;
}
