/*
 * Copyright (c) 2019 Ken Bannister. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @brief       nanocoap client block requests
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

#define _BUFLEN (128)

static const uint8_t block1_text[] = "If one advances confidently in the direction of his dreams...";

/* Return 1 on success, 0 on failure */
static ssize_t _init_remote(sock_udp_ep_t *remote, char *addr_str, char *port_str)
{
    ipv6_addr_t addr;
    remote->family = AF_INET6;

    /* parse for interface */
    char *iface = ipv6_addr_split_iface(addr_str);
    if (!iface) {
        if (gnrc_netif_numof() == 1) {
            /* assign the single interface found in gnrc_netif_numof() */
            remote->netif = (uint16_t)gnrc_netif_iter(NULL)->pid;
        }
        else {
            remote->netif = SOCK_ADDR_ANY_NETIF;
        }
    }
    else {
        int pid = atoi(iface);
        if (gnrc_netif_get_by_pid(pid) == NULL) {
            puts("gcoap_cli: interface not valid");
            return 0;
        }
        remote->netif = pid;
    }

    /* parse destination address */
    if (ipv6_addr_from_str(&addr, addr_str) == NULL) {
        puts("client: unable to parse destination address");
        return 0;
    }
    if ((remote->netif == SOCK_ADDR_ANY_NETIF) && ipv6_addr_is_link_local(&addr)) {
        puts("client: must specify interface for link local target");
        return 0;
    }
    memcpy(&remote->addr.ipv6[0], &addr.u8[0], sizeof(addr.u8));

    /* parse port */
    remote->port = atoi(port_str);
    if (remote->port == 0) {
        puts("client: unable to parse destination port");
        return 0;
    }

    return 1;
}

void _print_response(coap_pkt_t *pdu)
{
    if (coap_get_code_class(pdu) == COAP_CLASS_SUCCESS) {
        if (pdu->payload_len) {
            printf("%.*s\n", pdu->payload_len, (char *)pdu->payload);
        }
        else {
            printf("<no payload>\n");
        }
    }
    else {
        printf("nanocli: response Error, code %1u.%02u",
               coap_get_code_class(pdu), coap_get_code_detail(pdu));
        if (pdu->payload_len) {
            unsigned format = coap_get_content_type(pdu);
            if (format == COAP_FORMAT_TEXT
                    || format == COAP_FORMAT_LINK
                    || coap_get_code_class(pdu) == COAP_CLASS_CLIENT_FAILURE
                    || coap_get_code_class(pdu) == COAP_CLASS_SERVER_FAILURE) {
                /* Expecting diagnostic payload in failure cases */
                printf(", %u bytes\n%.*s\n", pdu->payload_len, pdu->payload_len,
                                             (char *)pdu->payload);
            }
            else {
                printf(", %u bytes\n", pdu->payload_len);
                od_hex_dump(pdu->payload, pdu->payload_len, OD_WIDTH_DEFAULT);
            }
        }
        else {
            printf(", empty payload\n");
        }
    }
}

/* Sends a request for /riot/ver resource from nanocoap_server. */
int block_get_cmd(int argc, char **argv)
{
    uint8_t buf[_BUFLEN];
    uint8_t token[2] = {0xDA, 0xEC};
    unsigned msgid = 1;
    coap_block1_t block;
    coap_pkt_t pdu;
    sock_udp_ep_t remote;

    if (argc == 1) {
        /* show help for commands */
        goto error;
    }

    coap_block_object_init(&block, 0, 32, 0);

    if (!_init_remote(&remote, argv[1], argv[2])) {
        goto error;
    }

    do {
        uint8_t *bufpos = buf;
        bufpos += coap_build_hdr((coap_hdr_t *)buf, COAP_TYPE_CON, token, 2,
                                 COAP_METHOD_GET, msgid++);

        bufpos += coap_opt_put_uri_path(bufpos, 0, "/riot/ver");
        bufpos += coap_opt_put_block2_control(bufpos, COAP_OPT_URI_PATH, &block);

        pdu.hdr = (coap_hdr_t*)buf;
        pdu.payload = bufpos;
        pdu.payload_len = 0;
        if (block.blknum == 0) {
            printf("client: sending msg ID %u, %u bytes\n\n", coap_get_id(&pdu),
                   bufpos - buf);
        }

        ssize_t res = nanocoap_request(&pdu, NULL, &remote, sizeof(buf));
        if (res < 0) {
            printf("client: msg send failed: %d\n", (int)res);
            return 1;
        }
        else {
            _print_response(&pdu);
        }

        /* reuse block size provided by server and request next block */
        coap_get_block2(&pdu, &block);
        block.blknum++;

    } while (block.more);

    return 0;

    error:
    printf("usage: %s <addr>[%%iface] <port>\n", argv[0]);
    return 1;
}

/* Posts a request for /sha256 resource to nanocoap_server. */
int block_post_cmd(int argc, char **argv)
{
    uint8_t buf[_BUFLEN];
    uint8_t token[2] = {0xDA, 0xEC};
    unsigned msgid = 1;
    coap_block_slicer_t slicer;
    unsigned blknum = 0;
    coap_pkt_t pdu;
    sock_udp_ep_t remote;

    if (argc == 1) {
        /* show help for commands */
        goto error;
    }

    if (!_init_remote(&remote, argv[1], argv[2])) {
        goto error;
    }

    do {
        coap_block_slicer_init(&slicer, blknum++, 32);

        uint8_t *bufpos = buf;
        bufpos += coap_build_hdr((coap_hdr_t *)buf, COAP_TYPE_CON, token, 2,
                                 COAP_METHOD_POST, msgid++);

        bufpos += coap_opt_put_uri_path(bufpos, 0, "/sha256");
        bufpos += coap_put_option_ct(bufpos, COAP_OPT_URI_PATH, COAP_FORMAT_TEXT);
        bufpos += coap_opt_put_block1(bufpos, COAP_OPT_CONTENT_FORMAT, &slicer, 1);
       *bufpos++ = 0xFF;

        pdu.hdr = (coap_hdr_t*)buf;
        pdu.payload = bufpos;

        bufpos += coap_blockwise_put_bytes(&slicer, bufpos, block1_text,
                                        sizeof(block1_text)-1);
        pdu.payload_len = bufpos - pdu.payload;

        coap_block1_finish(&slicer);

        if (slicer.start == 0) {
            printf("client: sending msg ID %u, %u bytes\n\n", coap_get_id(&pdu),
                   bufpos - buf);
        }

        ssize_t res = nanocoap_request(&pdu, NULL, &remote, sizeof(buf));
        if (res < 0) {
            printf("client: msg send failed: %d\n", (int)res);
            return 1;
        }
        else {
            _print_response(&pdu);
        }

    } while ((coap_get_code_class(&pdu) == COAP_CLASS_SUCCESS)
             && (slicer.cur > slicer.end));

    return 0;

    error:
    printf("usage: %s <addr>[%%iface] <port>\n", argv[0]);
    return 1;
}
