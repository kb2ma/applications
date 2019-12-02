/*
 * Copyright (c) 2019 Ken Bannister. All rights reserved.
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
 * @brief       wakaama-temp - temperature reader interface
 *
 * @author      Ken Bannister <kb2ma@runbox.com>
 */

#ifndef TEMP_READER_H
#define TEMP_READER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "liblwm2m.h"
#include "temp_sensor.h"

/**
 * @brief Stack size for module thread
 */
#ifndef TEMP_READER_STACK_SIZE
#define TEMP_READER_STACK_SIZE (THREAD_STACKSIZE_DEFAULT)
#endif

/**
 * @brief Start thread to read temperatures, once per minute
 *
 * @param lwm2m_ctx LwM2M context so can notify of temperature
 * @param instance LwM2M temperature object instance to hold temperature
 *
 * @return 0 if start OK
 * @return 1 if start failed
 */
int temp_reader_start(lwm2m_context_t *lwm2m_ctx, temperature_instance_t *instance);

#ifdef __cplusplus
}
#endif

#endif /* TEMP_READER_H */
/** @} */
