#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "tftp.h"

void handle_client(int sockfd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet);
int main()
{
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    tftp_packet packet;

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket timeout option
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("Error setting socket timeout");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind socket to the address
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("TFTP Server listening on port %d...\n", PORT);

    // Main loop to handle incoming requests
    while (1)
    {
        int n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&client_addr, &client_len);
        if (n < 0)
        {
            perror("Receive failed or timeout occurred");
            continue;
        }
        else
        {
            //printf("Packet Received\n");
            printf("Received packet from %s:%d\n",
                   inet_ntoa(client_addr.sin_addr),
                   ntohs(client_addr.sin_port));
            handle_client(sockfd, client_addr, client_len, &packet);
        }
    }

    close(sockfd);
    return 0;
}

void handle_client(int sockfd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet)
{
    if (ntohs(packet->opcode) == 2) // WRQ
    {
        int fd = open(packet->body.request.filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (fd < 0)
        {
            perror("File open failed");
            return;
        }

        printf("Write request received for file: %s\n", packet->body.request.filename);

        // Send ACK for block 0
        tftp_packet ack_packet;
        ack_packet.opcode = htons(4); // ACK
        ack_packet.body.ack_packet.block_number = htons(0);

        ssize_t packet_len = sizeof(uint16_t) + sizeof(ack_packet.body.ack_packet.block_number);
        ssize_t sent_bytes = sendto(sockfd, &ack_packet, packet_len, 0,
                                    (struct sockaddr *)&client_addr, client_len);
        if (sent_bytes > 0)
        {
            printf("ACK for block 0 sent to client.\n");
        }

        // ⬇️ Optional: start receiving DATA blocks in a loop here
        receive_file(sockfd, client_addr, client_len, fd);
    }
    else if (ntohs(packet->opcode) == 1) // RRQ
    {
        int fd = open(packet->body.request.filename, O_RDONLY);
        if (fd == -1)
        {
            printf("Error opening file\n");
            // Send error message
            tftp_packet error_packet;
            memset(&error_packet, 0, sizeof(error_packet));
            error_packet.opcode = htons(5); // Error
            error_packet.body.error_packet.error_code = htons(1);
            const char *errmsg = "File not found";
            strncpy(error_packet.body.error_packet.error_msg, errmsg, 15);
            printf("%s\n", error_packet.body.error_packet.error_msg);

            ssize_t packet_len = sizeof(uint16_t) + sizeof(uint16_t) + strlen(error_packet.body.error_packet.error_msg) + 1;
            ssize_t sent_bytes = sendto(sockfd, &error_packet, packet_len, 0,
                                        (struct sockaddr *)&client_addr, client_len);
            if (sent_bytes > 0)
            {
                printf("Error msg sent to client.\n");
            }
        }
        else
        {
            // Send ACK for block 0
            tftp_packet ack_packet;
            ack_packet.opcode = htons(4); // ACK
            ack_packet.body.ack_packet.block_number = htons(0);

            ssize_t packet_len = sizeof(uint16_t) + sizeof(ack_packet.body.ack_packet.block_number);
            ssize_t sent_bytes = sendto(sockfd, &ack_packet, packet_len, 0,
                                        (struct sockaddr *)&client_addr, client_len);
            if (sent_bytes > 0)
            {
                printf("ACK for block 0 sent to client.\n");
            }

            // Receiving ack for starting file transfer
            ssize_t bytes_received = recvfrom(sockfd, &ack_packet, sizeof(ack_packet), 0,
                                              (struct sockaddr *)&client_addr, &client_len);
            if (bytes_received)
            {
                printf("Ack packet received\n");
                if (ntohs(ack_packet.opcode) == 4)
                {
                    if (ntohs(ack_packet.body.ack_packet.block_number) == 0)
                    {
                        printf("File Transfer starting...\n");
                        send_file(sockfd, client_addr, client_len, fd);
                    }
                }
            }
            else
            {
                printf("Ack file packet not received\n");
            }
        }
    }
}

// void receive_file(int sockfd, struct sockaddr_in client_addr, socklen_t client_len, int fd)
// {
//     ssize_t bytes_received;
//     tftp_packet recv_packet, ack_packet;
//     int block_no = 1;
//     ssize_t packet_len = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(recv_packet.body.data_packet.data);
//     ssize_t ack_packet_len = sizeof(uint16_t) + sizeof(uint16_t);
//     while (1)
//     {
//         memset(&recv_packet, 0, sizeof(recv_packet));
//         bytes_received = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0,
//                                   (struct sockaddr *)&client_addr, &client_len);

//         if (bytes_received < 0)
//         {
//             printf("End of file received.\n");
//             perror("recvfrom");
//             break;
//         }
//         else
//         {
//             // Write data to file
//             size_t data_len = bytes_received - 4;
//             ssize_t bytes_written = write(fd, recv_packet.body.data_packet.data, data_len);

//             //  Send ACK
//             ack_packet.opcode = htons(4);
//             ack_packet.body.ack_packet.block_number = recv_packet.body.data_packet.block_number;
//             ssize_t sent_bytes = sendto(sockfd, &ack_packet, ack_packet_len, 0,
//                                         (struct sockaddr *)&client_addr, client_len);
//             if (sent_bytes > 0)
//             {
//                 printf("Ack sent for block %d\n", ntohs(recv_packet.body.data_packet.block_number));
//             }
//             else
//             {
//                 printf("Ack not sent for block %d\n", ntohs(recv_packet.body.data_packet.block_number));
//             }
//         }
//     }
//     printf("Receive Operation completed\n");
// }
