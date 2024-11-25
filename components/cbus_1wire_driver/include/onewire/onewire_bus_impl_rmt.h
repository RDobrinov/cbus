/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"

#include "onewire_bus.h"
#include "onewire_types.h"
#include "onewire/onewire_bus_interface.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    onewire_bus_t base; /*!< base class */
    rmt_channel_handle_t tx_channel; /*!< rmt tx channel handler */
    rmt_channel_handle_t rx_channel; /*!< rmt rx channel handler */

    rmt_encoder_handle_t tx_bytes_encoder; /*!< used to encode commands and data */
    rmt_encoder_handle_t tx_copy_encoder; /*!< used to encode reset pulse and bits */

    rmt_symbol_word_t *rx_symbols_buf; /*!< hold rmt raw symbols */

    size_t max_rx_bytes; /*!< buffer size in byte for single receive transaction */

    QueueHandle_t receive_queue;
    SemaphoreHandle_t bus_mutex;
} onewire_bus_rmt_obj_t;

/**
 * @brief 1-Wire bus RMT specific configuration
 */
typedef struct {
    uint32_t max_rx_bytes; /*!< Set the largest possible single receive size,
                                which determins the size of the internal buffer that used to save the receiving RMT symbols */
} onewire_bus_rmt_config_t;

/**
 * @brief Create 1-Wire bus with RMT backend
 *
 * @note One 1-Wire bus utilizes a pair of RMT TX and RX channels
 *
 * @param[in] bus_config 1-Wire bus configuration
 * @param[in] rmt_config RMT specific configuration
 * @param[out] ret_bus Returned 1-Wire bus handle
 * @return
 *      - ESP_OK: create 1-Wire bus handle successfully
 *      - ESP_ERR_INVALID_ARG: create 1-Wire bus handle failed because of invalid argument
 *      - ESP_ERR_NO_MEM: create 1-Wire bus handle failed because of out of memory
 *      - ESP_FAIL: create 1-Wire bus handle failed because some other error
 */
esp_err_t onewire_new_bus_rmt(const onewire_bus_config_t *bus_config, const onewire_bus_rmt_config_t *rmt_config, onewire_bus_handle_t *ret_bus);

#ifdef __cplusplus
}
#endif
