/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * MultiSense OS base sensor definitions
 */

#ifndef _MSOS_SENSOR_BASE_H_
#define _MSOS_SENSOR_BASE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>
#include "esp_err.h"

#include "cbus_driver.h"

/**
 * @brief Create new 1-Wire bus handle
 *
 * @return
 *      - Created handle
 */

typedef enum {
    MAGNITUDE_NONE,
    MAGNITUDE_TEMPERATURE,
    MAGNITUDE_HUMIDITY,
    MAGNITUDE_PRESSURE,
    MAGNITUDE_LAST
} msos_magnitudes_t;

typedef struct {
    uint32_t magnitude_index:4;
    uint32_t magnitude:8;
    uint32_t decimals:4;
    uint32_t reserved:16;
} msos_sensor_magnitude_t;

typedef struct {
    esp_err_t (*_init)(void *handle);
    esp_err_t (*_prepare)(void *handle);
    esp_err_t (*_measure)(void *handle);
    esp_err_t (*_magnitudes)(void *handle, msos_sensor_magnitude_t *magnitudes, size_t *len);
    esp_err_t (*_get)(void *handle, msos_sensor_magnitude_t *magnitude, float *value);
    bool (*_ready)(void *handle);
} msos_sensor_api;

typedef msos_sensor_api *msos_sensor_api_t;

typedef struct {
    esp_err_t (*_add)(void *data, msos_sensor_api_t **handle);
    esp_err_t (*_remove)(void *handle);
} msos_sensor_interface;

typedef msos_sensor_interface *msos_sensor_interface_t;

#ifdef __cplusplus
}
#endif 
#endif /* _MSOS_SENSOR_BASE_H_ */