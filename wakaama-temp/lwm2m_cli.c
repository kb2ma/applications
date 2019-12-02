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

#include "lwm2m_client.h"
#include "lwm2m_client_objects.h"
#include "lwm2m_platform.h"
#include "temp_reader.h"
#include "temp_sensor.h"

#define OBJ_COUNT (4)

uint8_t connected = 0;
lwm2m_object_t *obj_list[OBJ_COUNT];
lwm2m_client_data_t client_data;

void lwm2m_cli_init(void)
{
    /* this call is needed before creating any objects */
    lwm2m_client_init(&client_data);

    /* add objects that will be registered */
    obj_list[0] = lwm2m_client_get_security_object(&client_data);
    obj_list[1] = lwm2m_client_get_server_object(&client_data);
    obj_list[2] = lwm2m_client_get_device_object(&client_data);
    obj_list[3] = lwm2m_get_temperature_object(1);

    if (!obj_list[0] || !obj_list[1] || !obj_list[2]) {
        puts("Could not create mandatory objects");
    }
}

int lwm2m_cli_cmd(int argc, char **argv)
{
    if (argc == 1) {
        goto help_error;
    }

    if (!strcmp(argv[1], "start")) {
        /* run the LwM2M client */
        if (!connected && lwm2m_client_run(&client_data, obj_list, OBJ_COUNT)) {
            connected = 1;

            temperature_instance_t *instance;
            /* try to find the requested instance */
            instance = (temperature_instance_t *)lwm2m_list_find(obj_list[3]->instanceList, 0);

            if (!instance) {
                puts("Could not find lwm2m instance\n");
                return 1;
            }
            temp_reader_start(client_data.lwm2m_ctx, instance);
        }
        return 0;
    }

    if (IS_ACTIVE(DEVELHELP) && !strcmp(argv[1],"mem")) {
        lwm2m_tlsf_status();
        return 0;
    }

help_error:
    printf("usage: %s <start|mem>\n", argv[0]);
    return 1;
}
