/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cbus_driver.h"

/**
 * Event post macros
 */
#define cbus_event_err(event_data, rcode) ( {((cbus_event_data_t *)event_data)->status = rcode; \
            esp_event_post_to(*(run_config->cbus_event_loop), CBUS_EVENT, CBUS_EVENT_ERROR, event_data, sizeof(cbus_event_data_t), 1);});

#define cbus_event_post(event_data) ( { \
            esp_event_post_to(*(run_config->cbus_event_loop), CBUS_EVENT, CBUS_EVENT_DATA, event_data, sizeof(cbus_event_data_t), 1);})


typedef struct cbus_device_list {
    uint32_t id;
    cbus_driver_t *handle;
    struct cbus_device_list *next;
} cbus_device_list_t;

typedef struct {
    cbus_device_list_t *devices;
    esp_event_loop_handle_t *cbus_event_loop;
} cbus_internal_config_t;

static cbus_internal_config_t *run_config = NULL;

void *(*get_bus[3])(void) = {i2cbus_get_bus, ow_get_bus, NULL};

static void cbus_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static cbus_device_list_t *cbus_find_device(uint32_t id);

esp_event_loop_handle_t *cbus_initialize(void) {
    if(!run_config) {
        run_config = (cbus_internal_config_t *)calloc(1, sizeof(cbus_internal_config_t));
        if(run_config) {
            run_config->cbus_event_loop = (esp_event_loop_handle_t *)calloc(1, sizeof(esp_event_loop_handle_t));
            esp_event_loop_args_t uevent_args = {
                .queue_size = 10,
                .task_name = "cbusloop",
                .task_priority = 15,
                .task_stack_size = 3072,
                .task_core_id = tskNO_AFFINITY
            };
            esp_event_loop_create(&uevent_args, run_config->cbus_event_loop);
            if(run_config->cbus_event_loop) {
                esp_event_handler_instance_register_with(*(run_config->cbus_event_loop), CBUS_EVENT, CBUS_EVENT_EXEC, cbus_event_handler, NULL, NULL );
            }
        }
    }
    return run_config->cbus_event_loop;
}

static void cbus_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if(CBUS_EVENT != event_base | event_id != CBUS_EVENT_EXEC) return;
    switch (((cbus_event_data_t*)event_data)->command) {
        case CBUSCMD_ATTACH:
            cbus_device_list_t *new_device = (cbus_device_list_t *)calloc(1, sizeof(cbus_device_list_t));
            if(!new_device) {
                cbus_event_err(event_data, CBUS_ERR_NO_MEM);
                return;
            }
            new_device->handle = (cbus_driver_t *)(*get_bus[((cbus_device_config_t *)((cbus_event_data_t*)event_data)->payload)->bus_type])();
            if(!new_device->handle) {
                cbus_event_err(event_data, CBUS_ERR_BAD_ARGS);
                free(new_device);
                return;
            }
            cbus_common_id_t new_id = new_device->handle->attach((cbus_device_config_t *)((cbus_device_config_t *)((cbus_event_data_t*)event_data)->payload));
            if(CBUS_OK != new_id.error) {
                cbus_event_err(event_data, new_id.error);
                free(new_device);
                return;
            }
            new_device->id = new_id.id;
            new_device->next = run_config->devices;
            run_config->devices = new_device;
            ((cbus_event_data_t*)event_data)->device_id = new_id.id;
            cbus_event_post(event_data);
            return;
            break;
        case CBUSCMD_DEATTACH:
            cbus_device_list_t *device = cbus_find_device(((cbus_event_data_t*)event_data)->device_id);
            if(!device) {
                cbus_event_err(event_data, CBUS_ERR_DEVICE_NOT_FOUND);
                return;
            }
            cbus_common_id_t result = device->handle->deattach(device->id);
            if(CBUS_OK != result.error) {
                cbus_event_err(event_data, result.error);
                return;
            }
            
        default:
            return;
    }
}

static cbus_device_list_t *cbus_find_device(uint32_t id) {
    for(cbus_device_list_t *device = run_config->devices; device != NULL; device = device->next) {
        if(device->id == id) return device;
    }
    return NULL;
}