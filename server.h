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

static uint8_t *add_option(uint8_t *packet, uint8_t code, uint8_t len, const void *val) // had to make const void pointer so it accepts anything dynamically
{
    *packet = code;
    packet++;
    *packet = len;
    packet++;

    memcpy(packet, val, len);

    return packet + len;
}

// builds the response packet and returns the length
static size_t build_response(const struct dhcp_header *req, uint8_t msg_type, uint32_t yiaddr, uint8_t *out)
{
    memset(out, 0, MIN_REPLY); // 300 bytes of 0s
    struct dhcp_header *r = (struct dhcp_header *)out;

    // sname and hops are set to 0
    // secs is filled by client
    r->op = BOOTREPLY;
    r->htype = req->htype;
    r->hlen = req->hlen;
    r->xid = req->xid;
    r->flags = req->flags;
    r->giaddr = req->giaddr;
    r->yiaddr = yiaddr;
    memcpy(r->chaddr, req->chaddr, 16);
    r->magic = htonl(DHCP_MAGIC);

    struct in_addr server, mask;
    inet_pton(AF_INET, SERVER_IP, &server);
    inet_pton(AF_INET, SUBNET_MASK, &mask);

    uint32_t lease = htonl(LEASE_SECS);

    uint8_t *p = out + DHCP_HDR_LEN;
    p = add_option(p, DHCP_OPT_MSG_TYPE, 1, &msg_type);
    p = add_option(p, DHCP_OPT_SERVER_ID, 4, &server);
    p = add_option(p, DHCP_OPT_LEASE_TIME, 4, &lease);
    p = add_option(p, DHCP_OPT_SUBNET_MASK, 4, &mask);
    p = add_option(p, DHCP_OPT_ROUTER, 4, &server);

    *p = DHCP_OPT_END;
    p++;

    size_t len = (size_t)(p - out);
    return len < MIN_REPLY ? MIN_REPLY : len;
}

#endif