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
 * @brief       gcoap block server handler example
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
#include "hashes/sha256.h"
#include "net/gcoap.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

#define COAP_CODE_CONTINUE     ((2 << 5) | 31)

static ssize_t _stats_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len, void *ctx);
static ssize_t _riot_board_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len, void *ctx);
static ssize_t _sha256_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len, void *ctx);
static ssize_t _riot_block2_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, void *ctx);

/* CoAP resources */
static const coap_resource_t _resources[] = {
    { "/cli/stats", COAP_GET | COAP_PUT, _stats_handler, NULL },
    { "/riot/board", COAP_GET, _riot_board_handler, NULL },
    { "/riot/ver", COAP_GET, _riot_block2_handler, NULL },
    { "/sha256", COAP_POST, _sha256_handler, NULL },
};

static gcoap_listener_t _listener = {
    &_resources[0],
    sizeof(_resources) / sizeof(_resources[0]),
    NULL
};

/* Counts requests sent by CLI. */
static uint16_t req_count = 0;

static const uint8_t block2_intro[] = "This is RIOT (Version: ";
static const uint8_t block2_board[] = " running on a ";
static const uint8_t block2_mcu[] = " board with a ";

static ssize_t _riot_block2_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;
    coap_block_slicer_t slicer;
    coap_block2_init(pdu, &slicer);

    gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
    coap_opt_add_format(pdu, COAP_FORMAT_TEXT);
    coap_opt_add_block2(pdu, &slicer, 1);
    ssize_t plen = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);

    /* Add actual content */
    plen += coap_blockwise_put_bytes(&slicer, buf+plen, block2_intro, sizeof(block2_intro)-1);
    plen += coap_blockwise_put_bytes(&slicer, buf+plen, (uint8_t*)RIOT_VERSION, strlen(RIOT_VERSION));
    plen += coap_blockwise_put_char(&slicer, buf+plen, ')');
    plen += coap_blockwise_put_bytes(&slicer, buf+plen, block2_board, sizeof(block2_board)-1);
    plen += coap_blockwise_put_bytes(&slicer, buf+plen, (uint8_t*)RIOT_BOARD, strlen(RIOT_BOARD));
    plen += coap_blockwise_put_bytes(&slicer, buf+plen, block2_mcu, sizeof(block2_mcu)-1);
    plen += coap_blockwise_put_bytes(&slicer, buf+plen, (uint8_t*)RIOT_MCU, strlen(RIOT_MCU));
    /* To demonstrate individual chars */
    plen += coap_blockwise_put_char(&slicer, buf+plen, ' ');
    plen += coap_blockwise_put_char(&slicer, buf+plen, 'M');
    plen += coap_blockwise_put_char(&slicer, buf+plen, 'C');
    plen += coap_blockwise_put_char(&slicer, buf+plen, 'U');
    plen += coap_blockwise_put_char(&slicer, buf+plen, '.');

    coap_block2_finish(&slicer);

    return plen;
}

/*
 * Server callback for /cli/stats. Accepts either a GET or a PUT.
 *
 * GET: Returns the count of packets sent by the CLI.
 * PUT: Updates the count of packets. Rejects an obviously bad request, but
 *      allows any two byte value for example purposes. Semantically, the only
 *      valid action is to set the value to 0.
 */
static ssize_t _stats_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;

    /* read coap method type in packet */
    unsigned method_flag = coap_method2flag(coap_get_code_detail(pdu));

    switch(method_flag) {
        case COAP_GET:
            gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);

            /* write the response buffer with the request count value */
            size_t payload_len = fmt_u16_dec((char *)pdu->payload, req_count);

            return gcoap_finish(pdu, payload_len, COAP_FORMAT_TEXT);

        case COAP_PUT:
            /* convert the payload to an integer and update the internal
               value */
            if (pdu->payload_len <= 5) {
                char payload[6] = { 0 };
                memcpy(payload, (char *)pdu->payload, pdu->payload_len);
                req_count = (uint16_t)strtoul(payload, NULL, 10);
                return gcoap_response(pdu, buf, len, COAP_CODE_CHANGED);
            }
            else {
                return gcoap_response(pdu, buf, len, COAP_CODE_BAD_REQUEST);
            }
    }

    return 0;
}

static ssize_t _riot_board_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;
    gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
    /* write the RIOT board name in the response buffer */
    memcpy(pdu->payload, RIOT_BOARD, strlen(RIOT_BOARD));
    return gcoap_finish(pdu, strlen(RIOT_BOARD), COAP_FORMAT_TEXT);
}

/*
 * Uses block1 POSTs to generate an sha256 digest. */
static ssize_t _sha256_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;

    /* using a shared sha256 context *will* break if two requests are handled
     * at the same time.  doing it anyways, as this is meant to showcase block1
     * support, not proper synchronisation. */
    static sha256_context_t sha256;
    uint8_t digest[SHA256_DIGEST_LENGTH];
    coap_block1_t block1;

    int blockwise = coap_get_block1(pdu, &block1);

    printf("_sha256_handler: received data: offset=%u len=%u blockwise=%i more=%i\n",
            (unsigned)block1.offset, pdu->payload_len, blockwise, block1.more);

    /* initialize sha256 calculation and add payload bytes */
    if (block1.offset == 0) {
        puts("_sha256_handler: init");
        sha256_init(&sha256);
    }
    sha256_update(&sha256, pdu->payload, pdu->payload_len);

    unsigned resp_code = COAP_CODE_CHANGED;
    if (block1.more) {
        resp_code = COAP_CODE_CONTINUE;
    }

    /* start response */
    gcoap_resp_init(pdu, buf, len, resp_code);

    if (blockwise) {
        coap_opt_add_block1_control(pdu, &block1);
    }

    /* include digest if done, otherwise response code above asks for next block */
    size_t pdu_len = 0;
    if (!blockwise || !block1.more) {
        puts("_sha256_handler: finish");
        sha256_final(&sha256, digest);
        pdu_len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);

        pdu_len += fmt_bytes_hex((char *)pdu->payload, digest, sizeof(digest));
    }
    else {
        pdu_len = coap_opt_finish(pdu, COAP_OPT_FINISH_NONE);
    }

    return pdu_len;
}

int gcoap_cli_cmd(int argc, char **argv)
{
    if (argc == 1) {
        /* show help for main commands */
        goto end;
    }

    if (strcmp(argv[1], "info") == 0) {
        uint8_t open_reqs = gcoap_op_state();

        printf("CoAP server is listening on port %u\n", GCOAP_PORT);
        printf("CoAP open requests: %u\n", open_reqs);
        return 0;
    }

    end:
    printf("usage: %s <info>\n", argv[0]);
    return 1;
}

void gcoap_cli_init(void)
{
    gcoap_register_listener(&_listener);
}
