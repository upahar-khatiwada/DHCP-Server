// https://datatracker.ietf.org/doc/html/rfc2131

#ifndef DHCP_H
#define DHCP_H

#include <stdint.h>

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

// 99, 130, 83 and 99 in hexa
#define DHCP_MAGIC 0x63825363
#define DHCP_HDR_LEN 240 // 236 + 4 bytes for magic
#define BOOTREQUEST 1    // packet sent by client to server
#define BOOTREPLY 2      // packet sent by server to client

// hardcoded for now
#define SERVER_IP "192.168.50.1"
#define OFFER_IP "192.168.50.100"
#define SUBNET_MASK "255.255.255.0"
#define LEASE_SECS 3600
#define MIN_REPLY 300 // minimum size of reply packet

enum dhcp_option
{
    DHCP_OPT_PAD = 0,
    DHCP_OPT_SUBNET_MASK = 1,
    DHCP_OPT_ROUTER = 3,
    DHCP_OPT_DNS = 6,
    DHCP_OPT_REQUESTED_IP = 50,
    DHCP_OPT_LEASE_TIME = 51,
    DHCP_OPT_MSG_TYPE = 53,
    DHCP_OPT_SERVER_ID = 54,
    DHCP_OPT_PARAM_REQ_LIST = 55,
    DHCP_OPT_CLIENT_ID = 61,
    DHCP_OPT_END = 255
};

enum dhcp_msg_type
{
    DHCPDISCOVER = 1,
    DHCPOFFER = 2,
    DHCPREQUEST = 3,
    DHCPDECLINE = 4,
    DHCPACK = 5,
    DHCPNAK = 6,
    DHCPRELEASE = 7,
};

// The packed attribute disables padding.
struct __attribute__((packed)) dhcp_header
{
    uint8_t op, htype, hlen, hops;
    uint32_t xid;
    uint16_t secs, flags;
    uint32_t ciaddr, yiaddr, siaddr, giaddr;
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t magic;
};

#endif