#ifndef _I2C_PRIVATE_DEF_H_
#define _I2C_PRIVATE_DEF_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Type of unique ID for attached i2c device
*/
typedef union {
    struct {
        uint32_t i2caddr:10;    /*!< Device I2C Address */
        uint32_t i2cbus:2;      /*!< Controller bus number */
        uint32_t gpiosda:7;     /*!< Bus SDA GPIO */
        uint32_t gpioscl:7;     /*!< Bus SDA GPIO */
        uint32_t reserved:6;    /*!< Not used */
    };
    uint32_t id;                /*!< Single i2c device ID */
} i2cbus_device_id_t;

#ifdef __cplusplus
}
#endif 
#endif /* _I2C_PRIVATE_DEF_H_ */