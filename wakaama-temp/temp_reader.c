/*
 * Copyright (C) 2019 Ken Bannister. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     applications
 * @{
 *
 * @file
 * @brief       wakaama-temp - Shell handler and temperature reader
 *
 * @author      Ken Bannister <kb2ma@runbox.com>
 *
 * @}
 */

#include <stdio.h>
#include "liblwm2m.h"
#include "saul_reg.h"
#include "thread.h"
#include "xtimer.h"

#include "temp_sensor.h"
#include "temp_reader.h"

static char _msg_stack[TEMP_READER_STACK_SIZE];
static kernel_pid_t _pid = KERNEL_PID_UNDEF;
static saul_reg_t* _saul_dev;
static lwm2m_uri_t _uri;
static temperature_instance_t *_temp_instance;
static lwm2m_context_t *_lwm2m_ctx;

static void *_read_loop(void *arg)
{
    (void)arg;
    /* take a temperature reading */
    phydat_t phy;

    while(1) {
        int res;
        if (IS_ACTIVE(BOARD_NATIVE)) {
            phy.val[0] = 110;
            res = 1;
        }
        else {
            res = saul_reg_read(_saul_dev, &phy);
        }

        if (res) {
            printf("temperature: %d.%02d C\n", phy.val[0] / 100, phy.val[0] % 100);
        }
        else {
            printf("Sensor read failure: %d\n", res);
        }

        _temp_instance->sensor_value = (double)phy.val[0] / 100;

        lwm2m_resource_value_changed(_lwm2m_ctx, &_uri);

        xtimer_sleep(60);
    }
    return 0;
}

int temp_reader_start(lwm2m_context_t *lwm2m_ctx, temperature_instance_t *instance)
{

    _saul_dev = saul_reg_find_name("jc42");
    if (_saul_dev == NULL) {
        if (IS_ACTIVE(BOARD_NATIVE)) {
            puts("Ignoring sensor init on native");
        }
        else {
            puts("Can't find jc42 sensor");
        }
        return -1;
    }

    _lwm2m_ctx = lwm2m_ctx;
    _temp_instance = instance;

    _uri.flag = LWM2M_URI_FLAG_OBJECT_ID | LWM2M_URI_FLAG_INSTANCE_ID |
               LWM2M_URI_FLAG_RESOURCE_ID;
    _uri.objectId = LWM2M_TEMPERATURE_OBJECT_ID;
    _uri.instanceId = instance->shortID;
    _uri.resourceId = LWM2M_TEMPERATURE_RES_SENSOR_VALUE;

    _pid = thread_create(_msg_stack, sizeof(_msg_stack),
                         THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                         _read_loop, NULL, "temp reader");
    return 0;
}
