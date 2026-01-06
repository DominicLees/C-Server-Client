#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

int const PORT = 8000;

int main(void) {
    int connection, valread, socket_fd;
    struct sockaddr_in serv_addr;
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error: ");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary
    // form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported: ");
        exit(1);
    }

    if ((connection = connect(socket_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0) {
        perror("Connection Failed: ");
        exit(1);
    }

    // Wait for connected message
    char message[10] = {0};
    size_t count = read(socket_fd, message, 9);
    if (count <= 0 || strcmp("connected", message) != 0)
        exit(1);

    // Parent sends to server, child receives 
    int fork_id = fork();
    if (fork_id < 0) {
        perror("Fork error");
        exit(1);
    }

    if (fork_id > 0) {
        while (1) {
            char buffer[100] = {0};
            printf("Enter message: ");
            fgets(buffer, 100, stdin);
            buffer[strlen(buffer) - 1] = '\000';
            size_t count = send(socket_fd, buffer, strlen(buffer), 0);
            if (count < 0) {
                perror("send error");
                exit(1);
            }

            if (strcmp("close", buffer) == 0) {
                kill(fork_id, SIGKILL);
                break;
            }
        }
    } else {
        while (1) {
            char buffer[100] = {0};
            size_t count = read(socket_fd, buffer, 99);
            if (count > 0)
                printf("Message from server: %s", buffer);
            else
                break;
        }
    }

    printf("Connection closed with server");
    close(socket_fd);
    exit(0);
}