/*
 * Copyright (c) 2018 Ken Bannister. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @brief       nanocoap block CLI client
 *
 * @author      Ken Bannister <kb2ma@runbox.com>
 *
 * @}
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net/coap.h"
#include "net/nanocoap.h"
#include "net/nanocoap_sock.h"
#include "net/sock/udp.h"
#include "od.h"

static ssize_t _send(coap_pkt_t *pkt, size_t len, char *addr_str, char *port_str)
{
    ipv6_addr_t addr;
    sock_udp_ep_t remote;

    remote.family = AF_INET6;

    /* parse for interface */
    int iface = ipv6_addr_split_iface(addr_str);
    if (iface == -1) {
        if (gnrc_netif_numof() == 1) {
            /* assign the single interface found in gnrc_netif_numof() */
            remote.netif = (uint16_t)gnrc_netif_iter(NULL)->pid;
        }
        else {
            remote.netif = SOCK_ADDR_ANY_NETIF;
        }
    }
    else {
        if (gnrc_netif_get_by_pid(iface) == NULL) {
            puts("client: interface not valid");
            return 0;
        }
        remote.netif = iface;
    }

    /* parse destination address */
    if (ipv6_addr_from_str(&addr, addr_str) == NULL) {
        puts("client: unable to parse destination address");
        return 0;
    }
    if ((remote.netif == SOCK_ADDR_ANY_NETIF) && ipv6_addr_is_link_local(&addr)) {
        puts("client: must specify interface for link local target");
        return 0;
    }
    memcpy(&remote.addr.ipv6[0], &addr.u8[0], sizeof(addr.u8));

    /* parse port */
    remote.port = atoi(port_str);
    if (remote.port == 0) {
        puts("client: unable to parse destination port");
        return 0;
    }

    return nanocoap_request(pkt, NULL, &remote, len);
}

void _print_response(coap_pkt_t *pdu)
{
    char *class_str = (coap_get_code_class(&pkt) == COAP_CLASS_SUCCESS)
                            ? "Success" : "Error";
    printf("nanocli: response %s, code %1u.%02u", class_str,
           coap_get_code_class(&pkt), coap_get_code_detail(&pkt));
    if (pdu.payload_len) {
        unsigned format = coap_get_content_type(&pdu);
        if (format == COAP_FORMAT_TEXT
                || format == COAP_FORMAT_LINK
                || coap_get_code_class(&pkt) == COAP_CLASS_CLIENT_FAILURE
                || coap_get_code_class(&pkt) == COAP_CLASS_SERVER_FAILURE) {
            /* Expecting diagnostic payload in failure cases */
            printf(", %u bytes\n%.*s\n", pdu.payload_len, pdu.payload_len,
                                                          (char *)pdu.payload);
        }
        else {
            printf(", %u bytes\n", pdu.payload_len);
            od_hex_dump(pdu.payload, pdu.payload_len, OD_WIDTH_DEFAULT);
        }
    }
    else {
        printf(", empty payload\n");
    }
}

int control_client_cmd(int argc, char **argv)
{
    unsigned buflen = 128;
    uint8_t buf[buflen];
    uint8_t token[2] = {0xDA, 0xEC};
    coap_block1_t block;
    unsigned msgid = 1;
    coap_pkt_t pdu;

    if (argc == 1) {
        /* show help for commands */
        printf("usage: %s <addr>[%%iface] <port>\n",
               argv[0]);
        return 1;
    }

    /* initialize blockwise */
    memset(&block, 0, sizeof(block));
    block.szx = coap_size2szx(32);

    _init_remote(&remote, argv[2], argv[3]);

    do {
        unsigned pktpos = coap_build_hdr(buf, COAP_TYPE_CON, token, 2,
                                         COAP_METHOD_GET, msgid++);
        pktpos += coap_opt_put_uri_path(&buf[pktpos], 0, "/riot/ver");

        coap_opt_put_block2_control(&buf[pktpos], &block, COAP_OPT_URI_PATH);

        printf("client: sending msg ID %u, %u bytes\n", coap_get_id(msgid),
               pktpos);

        ssize_t res = nanocoap_request(&pdu, NULL, &remote, buflen);
        if (res < 0) {
            printf("nanocli: msg send failed: %d\n", (int)res);
            return 1;
        }
        else {
            _print_response(pdu);
        }

        /* reuse block size provided by server and request next block */
        coap_get_block2(&pdu, &block);
        block.blknum++;

    } while (block.more);
    
    return 0;
}
