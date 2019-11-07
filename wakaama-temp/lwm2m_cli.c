#include "saul_reg.h"
#include "lwm2m_client.h"
#include "lwm2m_client_objects.h"
#include "temp_sensor.h"

#define OBJ_COUNT (5)

uint8_t connected = 0;
lwm2m_object_t *obj_list[OBJ_COUNT];
lwm2m_client_data_t client_data;

static saul_reg_t* _saul_dev;

void lwm2m_cli_init(void)
{
    /* this call is needed before creating any objects */
    lwm2m_client_init(&client_data);

    /* add objects that will be registered */
    obj_list[0] = lwm2m_client_get_security_object(&client_data);
    obj_list[1] = lwm2m_client_get_server_object(&client_data);
    obj_list[2] = lwm2m_client_get_device_object(&client_data);
    obj_list[3] = lwm2m_client_get_acc_ctrl_object(&client_data);
    obj_list[4] = object_temperature_get(1);

    if (!obj_list[0] || !obj_list[1] || !obj_list[2]) {
        puts("Could not create mandatory objects");
    }

    _saul_dev = saul_reg_find_name("jc42");
    if (_saul_dev == NULL) {
#ifdef BOARD_NATIVE
        puts("Ignoring sensor init on native");
#else
        puts("Can't find jc42 sensor");
#endif
    }
}

int _read_temp(void)
{
    /* take a temperature reading */
    phydat_t phy;

#ifdef BOARD_NATIVE        
    phy.val[0] = 110;
    int res = 1;
#else
    int res = saul_reg_read(_saul_dev, &phy);
#endif
    if (res) {
        printf("temperature: %d.%02d C\n", phy.val[0] / 100, phy.val[0] % 100);
    }
    else {
        printf("Sensor read failure: %d\n", res);
    }

    temperature_instance_t *instance;
    /* try to find the requested instance */
    instance = (temperature_instance_t *)lwm2m_list_find(obj_list[4]->instanceList, 0);

    if (!instance) {
        puts("Could not find lwm2m instance\n");
        res = 0;
    }
    instance->sensor_value = (double)phy.val[0] / 100;

    /* mark changed */
    lwm2m_uri_t uri;

    uri.flag = LWM2M_URI_FLAG_OBJECT_ID | LWM2M_URI_FLAG_INSTANCE_ID |
               LWM2M_URI_FLAG_RESOURCE_ID;
    uri.objectId = LWM2M_TEMPERATURE_OBJECT_ID;
    uri.instanceId = instance->shortID;
    uri.resourceId = LWM2M_TEMPERATURE_RES_SENSOR_VALUE;

    lwm2m_resource_value_changed(client_data.lwm2m_ctx, &uri);

    return res;
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
        }
        return 0;
    }

    if (!strcmp(argv[1], "temp")) {
        _read_temp();
    }

help_error:
    printf("usage: %s <start|temp>\n", argv[0]);
    return 1;
}
