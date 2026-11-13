// https://datatracker.ietf.org/doc/html/rfc2131

#include <stddef.h>

// 99, 130, 83 and 99 in hexa
#define DHCP_MAGIC 0x63825363
#define DHCP_HDR_LEN 240 // 236 + 4 bytes for magic
#define BOOTREQUEST 1 // packet sent by client to server
#define BOOTREPLY   2 // packet sent by server to client

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

struct dhcp_header
{
    uint8_t op, htype, hlen, hops;
    uint32_t xid;
    uint16_t secs, flags;
    uint32_t ciaddr, yiaddr, siaddr, giaddr;
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t magic;
} __attribute__((packed));
// attribute packed disables padding

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