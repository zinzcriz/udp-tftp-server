#ifndef TFTP_CLIENT_H
#define TFTP_CLIENT_H

#define SERVER_IP_ADDRESS "127.0.0.1"
#define SERVER_PORT 5001
#define FILE_SIZE 50

typedef struct {
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t server_len;
    char server_ip[INET_ADDRSTRLEN];
} tftp_client_t;

// Function prototypes
void connect_to_server(tftp_client_t *client, char *ip, int port);
void put_file(tftp_client_t *client, char *filename);
void get_file(tftp_client_t *client, char *filename);
void disconnect(tftp_client_t *client);
void process_command(tftp_client_t *client, char *command);


int send_request(int sockfd, struct sockaddr_in server_addr, tftp_packet packet, int opcode);
int receive_request(int sockfd, struct sockaddr_in server_addr, tftp_packet *packet, int opcode);
#endif