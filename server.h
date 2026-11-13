#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "dhcp.h"

int create_socket()
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // enabling broadcast as DHCP clients send a request to 255.255.255.255
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt SO_BROADCAST failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DHCP_SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind to port 67 failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

// https://datatracker.ietf.org/doc/html/rfc1533
static const uint8_t *find_option(const uint8_t *option, size_t len, uint8_t code, uint8_t *option_len)
{
    size_t i = 0;
    while (i < len)
    {
        uint8_t c = option[i];
        if (c == DHCP_OPT_PAD)
        {
            i++; // code 0 is for padding
            continue;
        }

        if (c == DHCP_OPT_END)
            // code 255 is for end of options
            break;

        if (i + 1 >= len)
            break;

        uint8_t olen = option[i + 1];

        if (i + 2 + olen > len) // value goes past the packet
            break;

        if (c == code)
        {
            *option_len = olen;
            return &option[i + 2];
        }

        i += 2 + (size_t)olen; // 2 because 1 for code and 1 for code length
    }

    return NULL; // error
}

#endif