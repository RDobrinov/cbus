/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cbus_internal.h"

typedef struct cbus_device_list {
    uint32_t id;
    cbus_driver_t *handle;
    struct cbus_device_list *next;
} cbus_device_list_t;

typedef struct {
    cbus_device_list_t *devices;
    esp_event_loop_handle_t *cbus_event_loop;
} cbus_internal_config_t;

ESP_EVENT_DEFINE_BASE(CBUS_EVENT);

static cbus_internal_config_t *run_config = NULL;

void *(*get_bus[3])(void) = {i2cbus_get_bus, spi_get_bus, ow_get_bus};      //Dynamic definition

static void cbus_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static void cbus_event_post(cbus_event_data_t *event_data, cbus_id_t *result);
static cbus_device_list_t *cbus_find_device(uint32_t id);

esp_event_loop_handle_t cbus_initialize(void) {
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
    return *(run_config->cbus_event_loop);
}

static void cbus_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    cbus_event_data_t *event = (cbus_event_data_t*)event_data;
    cbus_device_list_t *device = NULL;
    cbus_id_t result = {.id = 0x00000000UL};
    cbus_cmd_t cmd;
    if((CBUS_EVENT != event_base) || (event_id != CBUS_EVENT_EXEC)) return;
    if(event->cmd.command != CBUSCMD_ATTACH && event->cmd.command != CBUSCMD_DEATTACH) {
        device = cbus_find_device(event->transaction.device_id);
        if(!device) {
            result.error = CBUS_ERR_DEVICE_NOT_FOUND;
            cbus_event_post(event_data, &result);
            return;
        }
    }
    switch (event->cmd.command) {
        case CBUSCMD_ATTACH:
            cbus_device_list_t *new_device = (cbus_device_list_t *)calloc(1, sizeof(cbus_device_list_t));
            if(!new_device) {
                result.error = CBUS_ERR_NO_MEM;
                cbus_event_post(event_data, &result);
                return;
            }
            new_device->handle = (cbus_driver_t *)(*get_bus[((cbus_device_config_t *)(event->payload))->bus_type])();
            if(!new_device->handle) {
                result.error = CBUS_ERR_BAD_ARGS;
                cbus_event_post(event_data, &result);
                free(new_device);
                return;
            }
            result = new_device->handle->attach((cbus_device_config_t *)(event->payload));
            if(CBUS_OK == result.error) {
                new_device->id = result.id;
                new_device->next = run_config->devices;
                run_config->devices = new_device;
                event->transaction.device_id = result.id;
            } else free(new_device);
            cbus_event_post(event_data, &result);
            break;
        case CBUSCMD_DEATTACH:
            cbus_device_list_t *target = NULL;
            device = run_config->devices;
            while ( device && device->id != event->transaction.device_id ) {
                target = device;
                device = device->next;
            }
            if(!device) {
                result.error = CBUS_ERR_DEVICE_NOT_FOUND;
                cbus_event_post(event_data, &result);
                return;
            }
            result = device->handle->deattach(device->id);
            if(CBUS_OK != result.error) {
                cbus_event_post(event_data, &result);
                return;
            }
            if(target) target->next = device->next;
            else run_config->devices = device->next;

            free(device);
            //((cbus_event_data_t*)event_data)->status = CBUS_OK;
            cbus_event_post(event_data, &result);
            break;
        case CBUSCMD_INFO:
            result = device->handle->info(event->transaction.device_id, event->payload, 40);
            cbus_event_post(event_data, &result);
            break;
        case CBUSCMD_SCAN:
        case CBUSCMD_PROBE:
        case CBUSCMD_RESET:
            result = device->handle->info(event->transaction.device_id, event->payload, 40); //Probe, scan, reset
            cbus_event_post(event_data, &result);
            break;
        case CBUSCMD_READ:
        case CBUSCMD_WRITE:
        case CBUSCMD_RW:
            bool read_trans = (event->cmd.command == CBUSCMD_READ || event->cmd.command == CBUSCMD_RW );
            if(((event->cmd.inDataLen == 0) && (event->cmd.command == CBUSCMD_WRITE || event->cmd.command == CBUSCMD_RW)) || 
                ((event->cmd.data_type == CBUSDATA_BLOB || event->cmd.data_type == CBUSDATA_MAX) && (event->cmd.outDataLen == 0) && read_trans)) {
                result.error = CBUS_ERR_BAD_ARGS;
                cbus_event_post(event_data, &result);
                return;
            }
            if((event->cmd.outDataLen == 0) && read_trans) {
                event->cmd.outDataLen = 1 << event->cmd.data_type;
            }
            printf("Checked\n");
            cmd = (cbus_cmd_t){
                .device_transaction = event->transaction,
                .device_command.uCommand = event->cmd.event_command,
                .data = event->payload
            };
            result = device->handle->execute(event_data);
            cbus_event_post(event_data, &result);
            return;
        default:
            break;
        result.error = CBUS_ERR_NOT_USED;
        cbus_event_post(event_data, &result);
        return;
    }
}

static void cbus_event_post(cbus_event_data_t *event_data, cbus_id_t *result) {
    event_data->cmd.status = result->error;
    esp_event_post_to(*(run_config->cbus_event_loop), CBUS_EVENT, (CBUS_OK == result->error) ? CBUS_EVENT_DATA : CBUS_EVENT_ERROR, event_data, sizeof(cbus_event_data_t), 1);
}

static cbus_device_list_t *cbus_find_device(uint32_t id) {
    for(cbus_device_list_t *device = run_config->devices; device != NULL; device = device->next) {
        if(device->id == id) return device;
    }
    return NULL;
}