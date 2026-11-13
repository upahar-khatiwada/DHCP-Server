# DHCP Server

This repository contains a small DHCP server prototype written in C. It listens
for DHCP client broadcasts on UDP port 67, validates the DHCP header and magic
cookie, and parses selected DHCP options from incoming requests.

The current implementation is intentionally minimal: it logs the DHCP message
type and requested IP address, but does not yet send `DHCPOFFER`, `DHCPACK`, or
other replies. The constants in [dhcp.h](dhcp.h) show the planned server,
offered address, subnet mask, and lease duration.

## Build and run

```sh
make
sudo ./dhcp_server
```

Port 67 is privileged on most Unix-like systems, so running the server usually
requires `sudo`. To remove the compiled binary:

```sh
make clean
```

Use this program only on an isolated test network. A DHCP server can disrupt
address assignment for other devices on the same network.

## DHCP packet format

DHCP is built on BOOTP and uses a fixed header followed by variable-length
options. Numeric fields are transmitted in network byte order. The server
associates requests and replies using the transaction ID (`xid`).

| Field | Size | Meaning |
|---|---:|---|
| `op` | 1 byte | Message direction: `1` (`BOOTREQUEST`) from a client or `2` (`BOOTREPLY`) from a server. |
| `htype` | 1 byte | Hardware address type. `1` represents Ethernet. |
| `hlen` | 1 byte | Length of the hardware address. Ethernet MAC addresses normally use `6`. |
| `hops` | 1 byte | Relay-agent hop count. A client starts this at zero; relay agents may update it. |
| `xid` | 4 bytes | Random transaction ID used to match a client request with its reply. |
| `secs` | 2 bytes | Seconds elapsed since the client started acquiring or renewing an address. |
| `flags` | 2 bytes | Client flags. The broadcast flag can request that a reply be broadcast. |
| `ciaddr` | 4 bytes | Client address, used when the client already has a usable address. |
| `yiaddr` | 4 bytes | “Your” client address offered or assigned by the server. |
| `siaddr` | 4 bytes | Address of the next bootstrap server, when applicable. |
| `giaddr` | 4 bytes | Relay-agent address. A nonzero value indicates relayed DHCP traffic. |
| `chaddr` | 16 bytes | Client hardware address, usually the MAC address followed by zero padding. |
| `sname` | 64 bytes | Optional server host name. |
| `file` | 128 bytes | Boot file name, used by network-boot clients. |
| `options` | Variable | DHCP options, beginning after the four-byte magic cookie. |

The fixed BOOTP portion is 236 bytes. This project adds the four-byte DHCP
magic cookie (`63 82 53 63`), making `DHCP_HDR_LEN` 240 bytes before options
are parsed.

## DHCP message exchange

The common address-allocation exchange is:

1. **`DHCPDISCOVER`** — A client broadcasts that it needs a DHCP configuration.
2. **`DHCPOFFER`** — A server proposes an address and configuration.
3. **`DHCPREQUEST`** — The client requests one offered address and identifies
   the selected server.
4. **`DHCPACK`** — The server confirms the lease and supplies its parameters.

Other message types support exceptional or later lifecycle events:

- **`DHCPDECLINE`**: the client reports that an offered address is already in use.
- **`DHCPNAK`**: the server rejects the requested configuration.
- **`DHCPRELEASE`**: the client gives up its lease.

## Options and parameters

Each option is encoded as a one-byte code, a one-byte length, and a
length-dependent value. Code `0` is padding and code `255` marks the end of the
options. The magic cookie must appear before the option list.

The option codes currently defined by this project are:

| Code | Name | Value |
|---:|---|---|
| `1` | Subnet Mask | IPv4 subnet mask, such as `255.255.255.0`. |
| `3` | Router | One or more default-gateway IPv4 addresses. |
| `6` | DNS | One or more DNS resolver IPv4 addresses. |
| `50` | Requested IP Address | The IPv4 address the client wants, commonly copied from an earlier offer. |
| `51` | IP Address Lease Time | Lease duration in seconds. |
| `53` | DHCP Message Type | One byte identifying `DHCPDISCOVER`, `DHCPREQUEST`, and so on. |
| `54` | DHCP Server Identifier | The IPv4 address identifying the DHCP server. |
| `55` | Parameter Request List | Option codes for configuration values requested by the client. |
| `61` | Client Identifier | A client identifier, often based on the hardware address. |

The parser in [server.h](server.h) safely walks option lengths, skips padding,
stops at the end marker, and rejects values that would extend beyond the
received packet. [main.c](main.c) currently uses options `53` and `50` for
logging.

## Project files

- [main.c](main.c): receives packets and logs selected values.
- [dhcp.h](dhcp.h): DHCP constants, option/message identifiers, and packet
  header definition.
- [server.h](server.h): socket setup, option parsing, and reply-building helper.
- [Makefile](Makefile): build, run, and clean targets.

## References

- [RFC 2131 — Dynamic Host Configuration Protocol](https://datatracker.ietf.org/doc/html/rfc2131)
- [RFC 2132 — DHCP Options and BOOTP Vendor Extensions](https://datatracker.ietf.org/doc/html/rfc2132)
