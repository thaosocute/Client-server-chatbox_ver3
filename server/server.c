#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#include <pthread.h>
#include <semaphore.h>

#include <sys/select.h>

#include <signal.h>

#define PORT 12345

int listenfd = -1;
struct sockaddr_in server_addr;

void *send_function(void* arg){
    char buffer[64];
    ssize_t bytes_read;
    int connfd = *(int *)arg;

    while(1){
        bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; 
            write(connfd, buffer, strlen(buffer));
            //printf("\rServer: %s\n", buffer);
            // tin nhắn ngắt kết nối với client
            if (strncmp(buffer, "exit", 4) == 0) {
                printf("Server: Connection close.\n");
                close(connfd);
                break;
            }
        } else {
            perror("read");
        }
    }

    return NULL;
}

void *receive_function(void* arg){
    char buffer[64];
    ssize_t bytes_read;
    int connfd = *(int *)arg;

    while(1){
        bytes_read = read(connfd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Client: %s\n", buffer);
        } else {
            perror("read từ client");
            break;
        }
    }
    return NULL;
}

void sigint_handle(){
    close(listenfd);
    _exit(0);
}

int main(){    
    memset(&server_addr, 0, sizeof(server_addr));

    //file description cho socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    //gắn listenfd vào địa chỉ IP và port được server lắng nghe
    bind(listenfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(listenfd, 3);

    signal(SIGINT, sigint_handle);

    // tạo các thread cho các client
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    while(1){
        int *connfd = malloc(sizeof(int));
        *connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

        pthread_t send_thread, receive_thread;
        pthread_create(&send_thread, &attr, send_function, connfd);
        pthread_detach(send_thread); 
        pthread_create(&receive_thread, &attr, receive_function, connfd);
        pthread_detach(receive_thread); 
        printf("CLient connect\n");
    }
    // pthread_t client_thread[16];
    // pthread_attr_t attr;
    // pthread_attr_init(&attr);

    // while(1){
    //     connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
    //     ticks = time(NULL);
    //     sprintf(send_buf, "Server reply %s", ctime(&ticks));
    //     write(connfd, send_buf, strlen(send_buf));
    //     close(connfd);
    // }
    return 0;
}
