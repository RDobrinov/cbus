#include "cbus_driver.h"

typedef struct cbus_device_list {
    uint32_t id;
    cbus_bus_t bus;
    struct cbus_device_list *next;
} cbus_device_list_t;

typedef struct {
    cbus_device_list_t *devices;
    esp_event_loop_handle_t *cbus_event_loop;
} cbus_internal_config_t;

static cbus_internal_config_t *run_config = NULL;

static void cbus_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

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

void test(void) {
    //cbus_device_t cbdevce;
    //cbdevce.bus_type = BUS_1WIRE;
    
}