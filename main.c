#include "dhcp.h"
#include "server.h"
#include <stdio.h>

int main()
{
    int sockfd = create_socket();
    uint8_t buf[1500];

    printf("DHCP Server listening on port %d\n", DHCP_SERVER_PORT);

    while (1)
    {
        struct sockaddr_in client;
        socklen_t c_len = sizeof(client);

        ssize_t n = recvfrom(sockfd, buf, sizeof(buf), 0,
                             (struct sockaddr *)&client, &c_len);

        if (n < 0)
        {
            perror("recvfrom error");
            continue;
        }

        if (n < DHCP_HDR_LEN)
            continue;

        const struct dhcp_header *req = (const struct dhcp_header *)buf;

        if (ntohl(req->magic) != DHCP_MAGIC)
            continue;

        if (req->op != BOOTREQUEST)
            continue;

        const uint8_t *opts = buf + DHCP_HDR_LEN;
        size_t opts_len = (size_t)n - DHCP_HDR_LEN;
        uint8_t l;

        const uint8_t *v = find_option(opts, opts_len, DHCP_OPT_MSG_TYPE, &l);
        int type = (v && l == 1) ? v[0] : -1;
        printf("message type = %d\n", type);

        v = find_option(opts, opts_len, DHCP_OPT_REQUESTED_IP, &l);
        if (v && l == 4)
        {
            struct in_addr requested_ip;
            memcpy(&requested_ip, v, 4);
            printf("requested IP = %s\n", inet_ntoa(requested_ip));
        }

        if (type == DHCPDISCOVER)
        {
            uint8_t res[1500];
            struct in_addr offer_addr;

            inet_pton(AF_INET, OFFER_IP, &offer_addr);
            size_t res_len = build_response(req, DHCPOFFER, offer_addr.s_addr, res);

            struct sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(DHCP_CLIENT_PORT);
            dest.sin_addr.s_addr = htonl(INADDR_BROADCAST);

            if (sendto(sockfd, res, res_len, 0,
                       (struct sockaddr *)&dest, sizeof(dest)) < 0)
                perror("sendto error");
            else
                printf("sent OFFER %s\n", OFFER_IP);
        }
    }

    return 0;
}