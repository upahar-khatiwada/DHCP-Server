#ifndef LEASE_H
#define LEASE_H

#include <hiredis/hiredis.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "dhcp.h"

#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379
#define POOL_START "192.168.50.50"
#define POOL_END "192.168.50.200"
#define OFFER_TTL 60
#define DECLINE_QUARANTINE 600

#define MAC_STR_LEN 48
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

void mac_to_str(const uint8_t *chaddr, uint8_t hlen, char *out)
{
    hlen = (hlen <= 16) ? hlen : 16; // validating client sided data

    int offset = 0;

    for (uint8_t i = 0; i < hlen; i++)
    {
        offset += snprintf(out + offset, MAC_STR_LEN - offset, (i == 0) ? "%02x" : ":%02x", chaddr[i]);
    }
}

void ip_to_str(uint32_t ip_net, char *out)
{
    struct in_addr addr;
    addr.s_addr = ip_net;

    inet_ntop(AF_INET, &addr, out, INET_ADDRSTRLEN);
}

int str_to_ip(const char *s, uint32_t *ip_net)
{
    struct in_addr addr;
    if (inet_pton(AF_INET, s, &addr) != 1)
        return 0;
    *ip_net = addr.s_addr;
    return 1;
}

int lease_find_by_mac(redisContext *c, const char *mac, uint32_t *ip_out)
{
    redisReply *r = redisCommand(c, "GET dhcp:mac:%s", mac);

    if (r == NULL)
        return 0;

    int found = 0;
    if (r->type == REDIS_REPLY_STRING)
        found = str_to_ip(r->str, ip_out);

    freeReplyObject(r);
    return found;
}

int lease_in_pool(redisContext *c, const char *ip)
{
    redisReply *r = redisCommand(c, "SISMEMBER %s %s", KEY_POOL, ip);

    if (r == NULL)
        return 0;

    int result = (r->type == REDIS_REPLY_INTEGER && r->integer == 1);

    freeReplyObject(r);
    return result;
}

int lease_get_owner(redisContext *c, const char *ip, char *mac_out, size_t len)
{
    redisReply *r = redisCommand(c, "HGET dhcp:lease:%s mac", ip);

    if (r == NULL)
        return 0;

    int found = 0;
    if (r->type == REDIS_REPLY_STRING)
    {
        snprintf(mac_out, len, "%s", r->str);
        found = 1;
    }

    freeReplyObject(r);
    return found;
}

// for DISCOVER

// claims an IP address
int lease_try_claim(redisContext *c, const char *ip, const char *mac)
{
    redisReply *r = redisCommand(c, "HSETNX dhcp:lease:%s mac %s", ip, mac);
    if (r == NULL)
        return 0;

    int claimed = (r->type == REDIS_REPLY_INTEGER && r->integer == 1);
    freeReplyObject(r);
    return claimed;
}

// reuses the old MAC's IP address if theres any
int lease_allocate(redisContext *c, const char *mac, uint32_t *ip_out)
{
    uint32_t old_ip;
    if (lease_find_by_mac(c, mac, &old_ip))
    {
        char old_str[INET_ADDRSTRLEN];
        char owner[MAC_STR_LEN];
        ip_to_str(old_ip, old_str);

        if (lease_get_owner(c, old_str, owner, sizeof(owner)) &&
            strcmp(owner, mac) == 0)
        {
            *ip_out = old_ip;
            return 1;
        }
    }

    redisReply *r = redisCommand(c, "SMEMBERS %s", KEY_POOL);
    if (r == NULL)
        return 0;

    int ok = 0;
    if (r->type == REDIS_REPLY_ARRAY)
    {
        for (size_t i = 0; i < r->elements; i++)
        {
            const char *ip = r->element[i]->str;

            if (!lease_try_claim(c, ip, mac))
                continue; // someone else holds it

            // we own it now
            freeReplyObject(redisCommand(c, "HSET dhcp:lease:%s state offered", ip));
            freeReplyObject(redisCommand(c, "EXPIRE dhcp:lease:%s %d", ip, OFFER_TTL));
            freeReplyObject(redisCommand(c, "SET dhcp:mac:%s %s EX %d", mac, ip, OFFER_TTL));

            ok = str_to_ip(ip, ip_out);
            break;
        }
    }

    freeReplyObject(r);
    return ok;
}

// for REQUEST
int lease_commit(redisContext *c, const char *mac, uint32_t ip_net, uint32_t lease_secs)
{
    char ip[INET_ADDRSTRLEN];
    ip_to_str(ip_net, ip);

    // must be an IP address we have
    if (!lease_in_pool(c, ip))
        return 0;

    char owner[MAC_STR_LEN];
    if (lease_get_owner(c, ip, owner, sizeof(owner)))
    {
        if (strcmp(owner, mac) != 0)
            return 0; // someone else holds it
    }
    else if (!lease_try_claim(c, ip, mac))
    {
        return 0;
    }

    time_t expires = time(NULL) + lease_secs;

    redisReply *r = redisCommand(c, "HSET dhcp:lease:%s state bound expires %ld", ip, (long)expires);
    if (r == NULL)
        return 0;
    freeReplyObject(r);

    freeReplyObject(redisCommand(c, "EXPIRE dhcp:lease:%s %u", ip, lease_secs));
    freeReplyObject(redisCommand(c, "SET dhcp:mac:%s %s EX %u", mac, ip, lease_secs));

    return 1;
}

int lease_release(redisContext *c, const char *mac, const char *ip)
{
    redisReply *r = redisCommand(c, "DEL dhcp:lease:%s dhcp:mac:%s", ip, mac);
    if (r == NULL)
        return 0;

    int ok = (r->type == REDIS_REPLY_INTEGER && r->integer > 0);

    freeReplyObject(r);
    return ok;
}

int lease_decline(redisContext *c, const char *mac, const char *ip)
{
    freeReplyObject(redisCommand(c, "MULTI"));
    freeReplyObject(redisCommand(c, "DEL dhcp:lease:%s dhcp:mac:%s", ip, mac));
    freeReplyObject(redisCommand(c, "HSET dhcp:lease:%s mac declined state declined", ip));
    freeReplyObject(redisCommand(c, "EXPIRE dhcp:lease:%s %d", ip, DECLINE_QUARANTINE));

    redisReply *r = redisCommand(c, "EXEC");
    if (r == NULL)
        return 0;

    int ok = (r->type == REDIS_REPLY_ARRAY);
    freeReplyObject(r);
    return ok;
}

#endif