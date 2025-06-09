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
#define MAX_CLIENT 5

//typedef define
typedef struct msg_node {
    char *msg;
    struct msg_node *next;
} msg_node;

typedef struct{
    int login_flag;
    int in_chat_flag;
    int client_fd;
    int client_id;
    int receiver_fd;
    int receiver_id;
    char username[32];
    char password[64];
    pthread_mutex_t msg_mutex;
} client_member;

pthread_attr_t attr;
int listenfd = -1;
int connfd = -1;
struct sockaddr_in server_addr;
client_member user[MAX_CLIENT];
char temp_username[32];
char temp_password[64];

//funcion prototybe
void send_msg(int client_fd, const char *msg);
ssize_t receive_msg(int client_fd, char *buffer, size_t buffer_size);
void send_menu(client_member *receiver);
void chat_session(client_member *member);

//handle function
void sigint_handle();

//thread function
void *client_thread_function(void *arg);
void *check_receive_thread_function(void *arg);
void *login_thread_function(void *arg);

int main(){  
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&temp_username, 0, sizeof(temp_username));
    memset(&temp_password, 0, sizeof(temp_password));

    //init users
    for(int i = 0; i < MAX_CLIENT; i++){
        user[i].login_flag = 0;
        user[i].in_chat_flag = 0;
        user[i]. receiver_id = -1;
        snprintf(user[i].username, sizeof(user[i].username), "Memberno%d", i + 1);
        snprintf(user[i].password, sizeof(user[i].password), "passwoRdofmember%d", i + 1);
        user[i].client_id = i + 1;
        pthread_mutex_init(&user[i].msg_mutex, NULL);
    }

    //file description for socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    //gắn listenfd vào địa chỉ IP và port được server lắng nghe
    bind(listenfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(listenfd, 3);

    signal(SIGINT, sigint_handle);

    // tạo các thread cho các client
    pthread_attr_init(&attr);
    if(pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN) != 0){
        perror("Failed to set stack size");
        exit(EXIT_FAILURE);
    }

    while(1){
        int connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
        pthread_t login_thread;
        pthread_create(&login_thread, &attr, login_thread_function, &connfd);
        pthread_detach(login_thread);
    }

    return 0;
}

void send_msg(int client_fd, const char *msg){
    if (write(client_fd, msg, strlen(msg)) < 0) {
        perror("Error sending message");
    }
}

ssize_t receive_msg(int client_fd, char *buffer, size_t buffer_size){
    ssize_t bytes_read = read(client_fd, buffer, buffer_size - 1);  

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0'; 
    } else if (bytes_read == 0) {
        fprintf(stderr, "Client disconnected\n");
    } else {
        perror("Error reading from client");
    }

    return bytes_read;
}

void send_menu(client_member *receiver){
    send_msg(receiver->client_fd, "These member are online:\n");
    char online_id[8];
    for(int i = 0; i < MAX_CLIENT; i++){
        if(user[i].login_flag == 1 && user[i].client_id != receiver->client_id){
            sprintf(online_id, "%d\n", user[i].client_id);
            send_msg(receiver->client_fd, online_id);
        }
    }
    send_msg(receiver->client_fd, "send \"exit\" to server to disconnect\n");
}

void sigint_handle(){
    close(listenfd);
    pthread_attr_destroy(&attr);
    _exit(0);
}

void *client_thread_function(void *arg){
    client_member *member = (client_member *)arg;
    char recv_buffer[64];
    int target_id;
    int found_request;
    
    while (1) {
        found_request = 0;
        send_menu(member);
        send_msg(member->client_fd, "Enter member ID to chat with, or wait for incoming chat request:\n");

        // Vòng lặp chờ chọn người hoặc có người khác mời chat
        while (1) {
            fd_set read_fds;
            struct timeval timeout;

            FD_ZERO(&read_fds);
            FD_SET(member->client_fd, &read_fds);

            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            int activity = select(member->client_fd + 1, &read_fds, NULL, NULL, &timeout);

            if (activity > 0 && FD_ISSET(member->client_fd, &read_fds)) {
                // Client nhập vào gì đó
                ssize_t len = receive_msg(member->client_fd, recv_buffer, sizeof(recv_buffer));
                if (len <= 0) {
                    close(member->client_fd);
                    member->login_flag = 0;
                    pthread_exit(NULL);
                }

                recv_buffer[strcspn(recv_buffer, "\r\n")] = 0;

                if (strcmp(recv_buffer, "exit") == 0) {
                    send_msg(member->client_fd, "Goodbye!\n");
                    close(member->client_fd);
                    member->login_flag = 0;
                    pthread_exit(NULL);
                }

                target_id = atoi(recv_buffer);
                if (target_id <= 0 || target_id > MAX_CLIENT || target_id == member->client_id ||
                    !user[target_id - 1].login_flag || user[target_id - 1].in_chat_flag) {
                    send_msg(member->client_fd, "Invalid or busy member. Try again:\n");
                    continue;
                }

                // Gửi yêu cầu cho target
                pthread_mutex_lock(&user[target_id - 1].msg_mutex);
                user[target_id - 1].receiver_id = member->client_id;
                pthread_mutex_unlock(&user[target_id - 1].msg_mutex);

                send_msg(member->client_fd, "Waiting for response...\n");

                // đợi target phản hồi sẽ xử lý ở thread của target
                break;
            }

            // Kiểm tra có ai mời mình không
            pthread_mutex_lock(&member->msg_mutex);
            int requester_id = member->receiver_id;
            pthread_mutex_unlock(&member->msg_mutex);

            if (requester_id != -1) {
                char msg[128];
                snprintf(msg, sizeof(msg), "Member %d wants to chat with you. Accept? (y/n): ", requester_id);
                send_msg(member->client_fd, msg);

                receive_msg(member->client_fd, recv_buffer, sizeof(recv_buffer));
                recv_buffer[strcspn(recv_buffer, "\r\n")] = 0;

                if (strcmp(recv_buffer, "y") == 0) {
                    target_id = requester_id;
                    found_request = 1;
                    break;
                } else {
                    snprintf(msg, sizeof(msg), "Member %d refused your chat request.\n", member->client_id);
                    send_msg(user[requester_id - 1].client_fd, msg);

                    pthread_mutex_lock(&member->msg_mutex);
                    member->receiver_id = -1;
                    pthread_mutex_unlock(&member->msg_mutex);
                }
            }
        }

        // Cập nhật thông tin chat và vào phiên trò chuyện
        member->in_chat_flag = 1;
        member->receiver_fd = user[target_id - 1].client_fd;
        member->receiver_id = target_id;
        user[target_id - 1].in_chat_flag = 1;

        chat_session(member);

        // reset sau chat
        member->in_chat_flag = 0;
        member->receiver_id = -1;
        user[target_id - 1].in_chat_flag = 0;
        user[target_id - 1].receiver_id = -1;
    }
    return NULL;
}

void chat_session(client_member *member){
    char buffer[128];

    while(1){
        ssize_t bytes = receive_msg(member->client_fd, buffer, sizeof(buffer));
        if(bytes <= 0){
            send_msg(member->receiver_fd, "The other user has disconnected.\n");
            break;
        }

        // Kiểm tra nếu client muốn thoát phiên chat
        if(strncmp(buffer, "exit", 4) == 0){
            send_msg(member->receiver_fd, "The other user has left the chat.\n");
            break;
        }

        printf("Member %d to member %d: %s\n", member->client_id, member->receiver_id, buffer);
        send_msg(member->receiver_fd, buffer);
    }

    // Thoát chat: gỡ liên kết chat giữa 2 người
    int receiver_index = member->receiver_id - 1;

    if(receiver_index >= 0 && receiver_index < MAX_CLIENT){
        user[receiver_index].in_chat_flag = 0;
        user[receiver_index].receiver_id = 0;
        user[receiver_index].receiver_fd = 0;
    }

    member->in_chat_flag = 0;
    member->receiver_id = 0;
    member->receiver_fd = 0;
}

void *login_thread_function(void *arg){
    int connfd = *(int*)arg;
    int authenticated = 0;
    printf("CLient connect\n");
    while(1){
        authenticated = 0;
        send_msg(connfd, "Enter username: ");
        receive_msg(connfd, temp_username, sizeof(temp_username));
        for(int i = 0; i < MAX_CLIENT; i++){
            if(strncmp(temp_username, user[i].username, 9) == 0){
                if(!user[i].login_flag){
                    send_msg(connfd, "Enter password: ");
                    receive_msg(connfd, temp_password, sizeof(temp_password));

                    if(strncmp(temp_password, user[i].password, 17) == 0){
                        authenticated = 1;
                        user[i].login_flag = 1;
                        user[i].client_fd = connfd;

                        pthread_t client_thread;
                        pthread_create(&client_thread, &attr, client_thread_function, &user[i]);
                        pthread_detach(client_thread);
                        printf("Client %d connected\n", user[i].client_id);
                    }else{
                        send_msg(connfd, "Password is incorrect\n");
                        //close(connfd);
                    }
                } else {
                    send_msg(connfd, "This account is in use\n");
                    //close(connfd);
                }
                break;  
            }
        }
        if(!authenticated){
            send_msg(connfd, "Invalid Username\n");
            //close(connfd);
            continue;
        } else {
            send_msg(connfd, "Login success\n");
            break;
        }
    }
    return NULL;
}

