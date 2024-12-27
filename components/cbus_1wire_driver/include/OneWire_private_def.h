#ifndef _ONEWIRE_PRIVATE_DEF_H_
#define _ONEWIRE_PRIVATE_DEF_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Type for 1-Wire device id generator */
typedef union {
    struct {
        uint32_t gpio:7;        /*!< 1-Wire bus pin */
        uint32_t tx_channel:3;  /*!< RMT TX Channel */
        uint32_t rx_channel:3;  /*!< RMT RX Channel */
        uint32_t reserved:3;    /*!< Not used */
        uint32_t intr_id:16;    /*!< CRC-16/MCRF4XX Seed is part of MAC */
    };
    uint32_t id;                /*!< Device ID wrapper */
} ow_device_id_t;

#ifdef __cplusplus
}
#endif 
#endif /* _ONEWIRE_PRIVATE_DEF_H_ */