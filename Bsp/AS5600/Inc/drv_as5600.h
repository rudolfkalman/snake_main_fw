#ifndef __DRV_AS5600_H__
#define __DRV_AS5600_H__

#include "stm32g4xx_hal.h"
#include <stdbool.h>

#define AS5600_ADDR 0x36
#define AS5600_CONF_ADDR 0x07
#define RAW_ANGLE_ADDR 0x0C

bool init_as5600(I2C_HandleTypeDef *hi2cx);

bool get_raw_angle(I2C_HandleTypeDef *hi2cx, float *angle);

#endif // !__DRV_AS5600_H__
