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

    // SO_BINDTODEVICE ties the socket to one network interface veth-srv then only kernel receives packet here
    const char *ifname = "veth-srv";
    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, ifname, strlen(ifname) + 1) < 0)
    {
        perror("SO_BINDTODEVICE failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

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

    if (msg_type == DHCPACK)
    {
        r->ciaddr = req->ciaddr;
    }

    memcpy(r->chaddr, req->chaddr, 16);
    r->magic = htonl(DHCP_MAGIC);

    struct in_addr server, mask;
    inet_pton(AF_INET, SERVER_IP, &server);
    inet_pton(AF_INET, SUBNET_MASK, &mask);

    uint32_t lease = htonl(LEASE_SECS);

    uint8_t *p = out + DHCP_HDR_LEN;
    p = add_option(p, DHCP_OPT_MSG_TYPE, 1, &msg_type);
    p = add_option(p, DHCP_OPT_SERVER_ID, 4, &server);

    if (msg_type != DHCPNAK)
    {
        p = add_option(p, DHCP_OPT_LEASE_TIME, 4, &lease);
        p = add_option(p, DHCP_OPT_SUBNET_MASK, 4, &mask);
        p = add_option(p, DHCP_OPT_ROUTER, 4, &server);
    }

    *p = DHCP_OPT_END;
    p++;

    size_t len = (size_t)(p - out);
    return len < MIN_REPLY ? MIN_REPLY : len;
}

static void send_reply(int sockfd, const uint8_t *res, size_t len, uint32_t dest_ip)
{
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(DHCP_CLIENT_PORT);
    dest.sin_addr.s_addr = dest_ip;

    if (sendto(sockfd, res, len, 0, (struct sockaddr *)&dest, sizeof(dest)) < 0)
        perror("sendto error");
}

// prints the main fields of a received DHCP packet
static void print_packet_info(const struct dhcp_header *h, ssize_t total, size_t opts_len, int type)
{
    uint32_t ci_raw = h->ciaddr;
    uint32_t gi_raw = h->giaddr;
    char ci[INET_ADDRSTRLEN], gi[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ci_raw, ci, sizeof(ci));
    inet_ntop(AF_INET, &gi_raw, gi, sizeof(gi));

    printf("---------- packet ----------\n");
    printf("total length   : %zd bytes\n", total);
    printf("options length : %zu bytes\n", opts_len);
    printf("message type   : %d\n", type);
    printf("xid            : 0x%08x\n", ntohl(h->xid));
    printf("flags          : %s\n", (ntohs(h->flags) & 0x8000) ? "broadcast" : "unicast");
    printf("ciaddr         : %s\n", ci);
    printf("giaddr         : %s\n", gi);

    printf("client MAC     : ");
    for (int i = 0; i < h->hlen && i < 16; i++)
        printf("%02x%s", h->chaddr[i], (i < h->hlen - 1) ? ":" : "");
    printf("\n");
}

#endif