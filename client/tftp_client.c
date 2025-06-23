#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "tftp.h"
#include "tftp_client.h"

int connected_flag = 0;
void flush_stdin()
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

int main()
{
    char command[256];
    tftp_client_t client;
    memset(&client, 0, sizeof(client)); // Initialize client structure

    // Main loop for command-line interface
    while (1)
    {
        printf("tftp> ");
        fflush(stdout);
        fgets(command, sizeof(command), stdin);

        // Remove newline character
        command[strcspn(command, "\n")] = 0;
        // printf("%zu", strlen(command));
        if (strlen(command) == 0)
            continue;
        // Process the command
        process_command(&client, command);
    }

    return 0;
}

// Function to process commands
void process_command(tftp_client_t *client, char *command)
{
    if (strcmp(command, "connect") == 0)
    {
        connect_to_server(client, SERVER_IP_ADDRESS, 5001);
        connected_flag = 1;
    }
    else if (strcmp(command, "put") == 0)
    {
        if (connected_flag)
        {
            char file_name[20];
            printf("Enter the filename:");
            scanf("%s", file_name);
            put_file(client, file_name);
        }
        else
        {
            printf("Please use the 'connect' command first.\n");
        }
    }
    else if (strcmp(command, "get") == 0)
    {
        if (connected_flag)
        {
            char file_name[20];
            printf("Enter the filename:");
            scanf("%s", file_name);
            get_file(client, file_name);
        }
        else
        {
            printf("Please use the 'connect' command first.\n");
        }
    }
    else if (strcmp(command, "disconnect") == 0)
    {
        disconnect(client);
        connected_flag = 0;
    }
}

// This function is to initialize socket with given server IP, no packets sent to server in this function
void connect_to_server(tftp_client_t *client, char *ip, int port)
{
    // Create UDP socket
    if ((client->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Error: Could not create socket");
        return;
    }

    // Set socket timeout option
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    if (setsockopt(client->sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("Error: Could not set socket timeout");
        return;
    }

    // Set up server address
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port = htons(port);
    client->server_addr.sin_addr.s_addr = inet_addr(ip);

    printf("Connected\n");
}

void put_file(tftp_client_t *client, char *filename)
{
    // Send WRQ request and send file
    // open file for reading
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        perror("Error opening file");
        // close(client->sockfd);
        // exit(EXIT_FAILURE);
        return;
    }

    // Build the WRQ packet
    tftp_packet packet;
    memset(&packet, 0, sizeof(packet));
    packet.opcode = htons(2);
    // Copy filename (e.g., "file.txt") and mode (e.g., "octet")
    strncpy(packet.body.request.filename, filename, sizeof(packet.body.request.filename) - 1);
    strncpy(packet.body.request.mode, "octet", sizeof(packet.body.request.mode) - 1);

    // Send WRQ (Write Request) packet
    send_request(client->sockfd, client->server_addr, packet, 2);

    // Wait for ACK for block 0
    tftp_packet recv_packet;
    int recv_len = receive_request(client->sockfd, client->server_addr, &packet, 4);
    if (recv_len > 0)
    {
        if (ntohs(packet.body.ack_packet.block_number) == 0)
        {
            printf("ACK received for WRQ\n");
            send_file(client->sockfd, client->server_addr, client->server_len, fd);
        }
    }
    else
    {
        perror("Timeout or error receiving ACK for WRQ");
    }
}

void get_file(tftp_client_t *client, char *filename)
{
    // Send RRQ and recive file
    tftp_packet packet;
    memset(&packet, 0, sizeof(packet));
    packet.opcode = htons(1);
    // Copy filename (e.g., "file.txt") and mode (e.g., "octet")
    strncpy(packet.body.request.filename, filename, sizeof(packet.body.request.filename) - 1);
    strncpy(packet.body.request.mode, "octet", sizeof(packet.body.request.mode) - 1);
    send_request(client->sockfd, client->server_addr, packet, 1);
    memset(&packet, 0, sizeof(packet));
    int recv_len = receive_request(client->sockfd, client->server_addr, &packet, 1);
    if (recv_len > -1)
    {
        if (ntohs(packet.opcode) == 4) //ACK
        {
            if (ntohs(packet.body.ack_packet.block_number) == 0)
            {
                // Create file to write
                int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                if (fd < 0)
                {
                    perror("File open failed");
                    return;
                }

                //Send ACK for file transfer and Receive file
                tftp_packet ack_packet;
                memset(&ack_packet, 0, sizeof(ack_packet));
                ack_packet.opcode = htons(4);
                ack_packet.body.ack_packet.block_number = htons(0);
                send_request(client->sockfd, client->server_addr, ack_packet, 4);
                receive_file(client->sockfd, client->server_addr, client->server_len, fd);
            }
        }
        else if (ntohs(packet.opcode) == 5)  // ERROR
        {
            if (ntohs(packet.body.error_packet.error_code) == 1)
            {
                printf("File not found\n");
            }
        }
    }
}

void disconnect(tftp_client_t *client)
{
    // close fd
    if (client->sockfd >= 0)
    {
        close(client->sockfd);
        client->sockfd = -1; // mark socket as closed
        printf("Disconnected from server.\n");
    }
    else
    {
        printf("Socket already closed or not connected.\n");
    }
}

int send_request(int sockfd, struct sockaddr_in server_addr, tftp_packet packet, int opcode)
{
    if (opcode == 1)//RRQ
    {
        size_t packet_len = sizeof(uint16_t) + strlen(packet.body.request.filename) + 1 + strlen(packet.body.request.mode) + 1;
        ssize_t sent_bytes = sendto(sockfd, &packet, packet_len, 0,
                                    (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (sent_bytes < 0)
        {
            perror("sendto failed");
        }
        else
        {
            printf("RRQ sent: %zd bytes\n", sent_bytes);
        }
    }
    else if (opcode == 2)//WRQ
    {

        size_t packet_len = sizeof(uint16_t) + strlen(packet.body.request.filename) + 1 + strlen(packet.body.request.mode) + 1;
        ssize_t sent_bytes = sendto(sockfd, &packet, packet_len, 0,
                                    (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (sent_bytes < 0)
        {
            perror("sendto failed");
        }
        else
        {
            printf("WRQ sent: %zd bytes\n", sent_bytes);
        }
    }
    else if (opcode == 3)//DATA
    {
        size_t packet_len = sizeof(uint16_t) + sizeof(uint16_t) + strlen(packet.body.data_packet.data) + 1;
        ssize_t sent_bytes = sendto(sockfd, &packet, packet_len, 0,
                                    (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (sent_bytes < 0)
        {
            perror("sendto failed");
            return -1;
        }
        else
        {
            printf("Data sent: %zd bytes\n", sent_bytes);
        }
    }
}

int receive_request(int sockfd, struct sockaddr_in server_addr, tftp_packet *packet, int opcode)
{
    if (opcode == 1)
    {
        int recv_len = recvfrom(sockfd, packet, sizeof(packet), 0, NULL, NULL);

        if (recv_len > 0)
        {
            return recv_len;
        }
        else
        {
            return -1;
        }
    }
    else if (opcode == 4)
    {
        int recv_len = recvfrom(sockfd, packet, sizeof(packet), 0, NULL, NULL);

        if (recv_len > 0)
        {
            return recv_len;
        }
        else
        {
            return -1;
        }
    }
}