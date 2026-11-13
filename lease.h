#ifndef LEASE_H
#define LEASE_H

#include <hiredis/hiredis.h>
#include <arpa/inet.h>
#include <stdint.h>
#include "dhcp.h"

#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379
#define POOL_START "192.168.50.100"
#define POOL_END "192.168.50.200"
#define OFFER_TTL 60

#define KEY_POOL "dhcp:pool"

redisContext *lease_connect(void)
{
    redisContext *c = redisConnect(REDIS_HOST, REDIS_PORT);
    if (c == NULL || c->err)
    {
        if (c)
        {
            printf("Error: %s\n", c->errstr);
        }
        else
        {
            printf("Can't allocate redis context\n");
        }
    }

    return c;
}

int lease_seed_pool(redisContext *ctx)
{
    redisReply *r = redisCommand(ctx, "EXISTS %s", KEY_POOL);

    if (r == NULL)
        return -1;

    int exists = (r->type == REDIS_REPLY_INTEGER && r->integer == 1);
    freeReplyObject(r);

    if (exists)
        return 0;

    struct in_addr start, end;

    inet_pton(AF_INET, POOL_START, &start);
    inet_pton(AF_INET, POOL_END, &end);

    uint32_t start_ip = ntohl(start.s_addr);
    uint32_t end_ip = ntohl(end.s_addr);
    int added = 0;

    for (uint32_t ip = start_ip; ip <= end_ip; ip++)
    {
        struct in_addr current;
        current.s_addr = htonl(ip);

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &current, ip_str, sizeof(ip_str));

        redisReply *r = redisCommand(ctx, "SADD %s %s", KEY_POOL, ip_str);

        if (r == NULL)
            return -1;

        added++;
        freeReplyObject(r);
    }

    return added;
}

#endif