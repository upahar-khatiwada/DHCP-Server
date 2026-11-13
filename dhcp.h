// https://datatracker.ietf.org/doc/html/rfc2131

#include <stdint.h>
#include <stddef.h>

// 99, 130, 83 and 99 in hexa
#define DHCP_MAGIC 0x63825363
#define DHCP_HDR_LEN 240 // 236 + 4 bytes for magic

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

// the options parameter contains various stuffs
// code 53 tells the message type
//    Value   Message Type
//            -----   ------------
//              1     DHCPDISCOVER
//              2     DHCPOFFER
//              3     DHCPREQUEST
//              4     DHCPDECLINE
//              5     DHCPACK
//              6     DHCPNAK
//              7     DHCPRELEASE

// https://datatracker.ietf.org/doc/html/rfc1533
static int get_message_type(uint8_t *option, size_t len)
{
    size_t i = 0;
    while (i < len)
    {
        uint8_t code = option[i];
        if (code == 0)
        {
            i++; // code 0 is for padding
            continue;
        }

        if (code == 255)
            // code 255 is for end of options
            break;

        if (i + 1 >= len)
            break;

        uint8_t option_len = option[i + 1];

        if (i + 2 + option_len > len) // we are only looking for the code 53
            break;

        if (code == 53 && option_len == 1)
            return option[i + 2];

        i += 2 + (size_t)option_len; // 2 because 1 for code and 1 for code length
    }

    return -1; // error
}