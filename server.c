#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

int const MAX_CONNECTIONS = 100;
int const PORT = 8000;
char connection_message[] = "connected";

int connection_count(int* connections) {
    int count = 0;
    for (size_t i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i] > 0) {
            count++;
        }
    }
    return count;
}

void add_connection(int* connections, int id) {
    for (size_t i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i] == 0) {
            connections[i] = id;
            return;
        }
    }
}

void update_connections(int* connections, int read_fd) {
    int pid;
    while (read(read_fd, &pid, 4) > 0) {
        for (size_t i = 0; i < MAX_CONNECTIONS; i++) {
            if (connections[i] == pid) {
                connections[i] = 0;
                return;
            }
        }
    } 
}

void handle_connection(int connection, int client_id) {
    if (send(connection, connection_message, 9, 0) < 0) {
        perror("Send error");
        exit(1);
    }

    while (1) {
        char buffer[100] = {0};
        size_t count = read(connection, buffer, 99);

        if (strcmp("close", buffer) == 0 || count <= 0)
            return;
        buffer[strlen(buffer)] = '\0';
        printf("Client #%d: %s\n", client_id, buffer);
    }
}

void close_connection(int connection, int write_fd) {
    close(connection);
    int pid = getpid();
    write(write_fd, &pid, 4);
}

int main(void) {
    int* connections = malloc(MAX_CONNECTIONS * sizeof(int));
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    int client_count = 0;

    // Create socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Socket creation error");
        exit(1);
    }

    // Forcefully attaching socket to the port 8000
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt error");
        exit(1);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Port bind failed");
        exit(1);
    }

    if (listen(socket_fd, MAX_CONNECTIONS) < 0) {
        perror("listen error");
        exit(1);
    }
    printf("Server listening on port %d\n", PORT);

    // Create a pipe
    int filedes[2] = {0}; // [0] read, [1] write
    if (pipe(filedes) < 0) {
        perror("Pipe error");
        exit(1);
    }
    int read_fd = filedes[0];
    int write_fd = filedes[1];
    fcntl(read_fd, F_SETFL, O_NONBLOCK);

    // Start listening for connections
    while (1) {
        update_connections(connections, read_fd);
        // Check for free connections
        int current_connections = connection_count(connections);
        if (current_connections >= MAX_CONNECTIONS)
            continue;
        printf("Current connections: %d\n", current_connections);
        // Wait for new connection
        int new_connection = accept(socket_fd, (struct sockaddr*)&address, &addrlen);
        if (new_connection < 0) {
            perror("accept error");
            exit(1);
        }
        client_count++;
        printf("New connection #%d\n", client_count);
        // Child fork handles the new connection, parent keeps listening for new connections
        int fork_id = fork();
        if (fork_id < 0) {
            perror("fork error");
            exit(1);
        // Parent waits for next new connection
        } else if (fork_id > 0) {
            add_connection(connections, fork_id);
        // Child handles new connection
        } else {
            free(connections);
            handle_connection(new_connection, client_count);
            close_connection(new_connection, write_fd);
            printf("Client #%d closed the connection\n", client_count);
            exit(0);
        }
    }
    
    return 0;
}