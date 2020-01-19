/*
 * Copyright (c) 2015-2017 Ken Bannister. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @brief       gcoap block client example
 *
 * @author      Ken Bannister <kb2ma@runbox.com>
 *
 * @}
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fmt.h"
#include "od.h"
#include "hashes/sha256.h"
#include "net/gcoap.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static void _resp_handler(const gcoap_request_memo_t *memo, coap_pkt_t* pdu,
                          const sock_udp_ep_t *remote);

static const uint8_t block1_text[] = "If one advances confidently in the direction of his dreams...";

/* last block number sent by /sha256 request */
static uint16_t last_blknum = 0;

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

/* Writes and sends next block for /sha256 request. */
static int _do_block_post(coap_pkt_t *pdu, uint16_t blknum, const sock_udp_ep_t *remote)
{
    coap_block_slicer_t slicer;
    coap_block_slicer_init(&slicer, blknum, 32);

    gcoap_req_init(pdu, (uint8_t *)pdu->hdr, GCOAP_PDU_BUF_SIZE, COAP_METHOD_POST,
                   "/sha256");
    coap_opt_add_format(pdu, COAP_FORMAT_TEXT);
    coap_opt_add_block1(pdu, &slicer, 1);
    int len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);

    len += coap_blockwise_put_bytes(&slicer, pdu->payload, block1_text,
                                    sizeof(block1_text)-1);

    coap_block1_finish(&slicer);

    if (slicer.start == 0) {
        printf("client: sending msg ID %u, %u bytes\n\n", coap_get_id(pdu), len);
    }

    ssize_t res = gcoap_req_send((uint8_t *)pdu->hdr, len, remote, _resp_handler, NULL);
    if (res < 0) {
        printf("client: msg send failed: %d\n", (int)res);
        return 1;
    }
    return 0;
}

/* Response handler for client request to /sha256. */
static void _resp_handler(const gcoap_request_memo_t *memo, coap_pkt_t* pdu,
                          const sock_udp_ep_t *remote)
{
    if (memo->state == GCOAP_MEMO_TIMEOUT) {
        printf("gcoap: timeout for msg ID %02u\n", coap_get_id(pdu));
        return;
    }
    else if (memo->state == GCOAP_MEMO_ERR) {
        printf("gcoap: error in response\n");
        return;
    }

    char *class_str = (coap_get_code_class(pdu) == COAP_CLASS_SUCCESS)
                            ? "Success" : "Error";
    printf("gcoap: response %s, code %1u.%02u", class_str,
                                                coap_get_code_class(pdu),
                                                coap_get_code_detail(pdu));
    if (pdu->payload_len) {
        unsigned content_type = coap_get_content_type(pdu);
        if (content_type == COAP_FORMAT_TEXT
                || content_type == COAP_FORMAT_LINK
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

    /* send next block if present */
    if (coap_get_code_raw(pdu) == COAP_CODE_CONTINUE) {
        _do_block_post(pdu, ++last_blknum, remote);
    }
}

/* Initial POST request for block based /sha256 resource. */
static int _block_post_cmd(int argc, char **argv)
{
    uint8_t buf[GCOAP_PDU_BUF_SIZE];
    coap_pkt_t pdu;
    sock_udp_ep_t remote;

    if (argc == 2) {
        /* show help for commands */
        goto error;
    }

    if (!_init_remote(&remote, argv[2], argv[3])) {
        goto error;
    }

    last_blknum = 0;
    pdu.hdr = (coap_hdr_t *)buf;
    _do_block_post(&pdu, 0, &remote);

    return 0;

    error:
    printf("usage: %s post <addr>[%%iface] <port>\n", argv[0]);
    return 1;
}

int gcoap_cli_cmd(int argc, char **argv)
{
    if (argc == 1) {
        /* show help for main commands */
        goto end;
    }

    if (strcmp(argv[1], "post") == 0) {
        _block_post_cmd(argc, argv);
        return 0;
    }
    else if (strcmp(argv[1], "info") == 0) {
        uint8_t open_reqs = gcoap_op_state();

        printf("CoAP server is listening on port %u\n", GCOAP_PORT);
        printf("CoAP open requests: %u\n", open_reqs);
        return 0;
    }

    end:
    printf("usage: %s <post|info>\n", argv[0]);
    return 1;
}

void gcoap_cli_init(void)
{
}
