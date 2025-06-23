/* Common file for server & client */

#include "tftp.h"
#include "tftp_client.h"
#include "unistd.h"
#include <string.h>
#include <stdio.h>

void send_file(int sockfd, struct sockaddr_in server_addr, socklen_t server_len, int fd)
{
    // Implement file sending logic here
    uint16_t block_no = 1;
    size_t bytes_read;
    tftp_packet packet, recv_packet;
    packet.opcode = htons(3);
    printf("File sending...\n");
    while ((bytes_read = read(fd, packet.body.data_packet.data, sizeof(packet.body.data_packet.data))) > 0)
    {
        packet.body.data_packet.block_number = htons(block_no);
    resend:
        // send_request(sockfd,server_addr, packet, 3);
        size_t packet_len = sizeof(uint16_t) + sizeof(uint16_t) + bytes_read;
        ssize_t sent_bytes = sendto(sockfd, &packet, packet_len, 0,
                                    (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (sent_bytes < 0)
        {
            perror("sendto failed");
            // return -1;
        }
        else
        {
            printf("sent: %zd bytes\n", sent_bytes);
        }
        // receive_request(sockfd,server_addr,&recv_packet,4);
        int recv_len = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, NULL, NULL);
        if (recv_len > 0)
        {
            if (ntohs(recv_packet.body.ack_packet.block_number) == block_no)
            {
                printf("Ack Received for block %d\n", ntohs(recv_packet.body.ack_packet.block_number));
                block_no++;
            }
            else
            {
                printf("Ack not Received for block %d\n", ntohs(recv_packet.body.ack_packet.block_number));
                goto resend;
            }
        }
        else
        {
            printf("Ack not Received for block\n");
        }
    }
    printf("Send Operation over\n");
    return;
}

void receive_file(int sockfd, struct sockaddr_in client_addr, socklen_t client_len, int fd)
{
    ssize_t bytes_received;
    tftp_packet recv_packet, ack_packet;
    int block_no = 1;
    ssize_t packet_len = sizeof(uint16_t) + sizeof(uint16_t) + strlen(recv_packet.body.data_packet.data) + 1;
    ssize_t ack_packet_len = sizeof(uint16_t) + sizeof(uint16_t);
    while (1)
    {
        bytes_received = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0,
                                  (struct sockaddr *)&client_addr, &client_len);

        if (bytes_received < 0)
        {
            printf("End of file received.\n");
            perror("recvfrom");
            break;
        }
        else
        {
            // Write data to file
            size_t data_len = bytes_received - 4;
            ssize_t bytes_written = write(fd, recv_packet.body.data_packet.data, data_len);

            //Send ACK
            ack_packet.opcode = htons(4);
            ack_packet.body.ack_packet.block_number = recv_packet.body.data_packet.block_number;
            ssize_t sent_bytes = sendto(sockfd, &ack_packet, ack_packet_len, 0,
                                        (struct sockaddr *)&client_addr, client_len);
            if (sent_bytes > 0)
            {
                printf("Ack sent for block %d\n", ntohs(recv_packet.body.data_packet.block_number));
            }
            else
            {
                printf("Ack not sent for block %d\n", ntohs(recv_packet.body.data_packet.block_number));
            }
        }
    }
    printf("Receive Operation Completed\n");
    return;
}