#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

int sock = -1;

void *receive_from_server(void *arg)
{
    char buffer[BUFFER_SIZE];

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t n = recv(sock, buffer, BUFFER_SIZE - 1, 0);

        if (n <= 0)
        {
            printf("\n[Disconnected from server]\n");
            break;
        }

        buffer[n] = '\0';
        printf("%s", buffer);
        fflush(stdout);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    char server_ip[50] = "127.0.0.1";
    int server_port = 9000;

    if (argc > 1)
    {
        strcpy(server_ip, argv[1]);
    }
    if (argc > 2)
    {
        server_port = atoi(argv[2]);
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid IP address");
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        return 1;
    }

    printf("✅ Connected to Game Hub Server at %s:%d\n\n", server_ip, server_port);

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_from_server, NULL);
    pthread_detach(recv_thread);

    char input[BUFFER_SIZE];

    while (1)
    {
        fflush(stdout);

        if (fgets(input, BUFFER_SIZE, stdin) == NULL)
        {
            break;
        }

        if (send(sock, input, strlen(input), 0) < 0)
        {
            perror("Send failed");
            break;
        }
    }

    close(sock);
    return 0;
}
