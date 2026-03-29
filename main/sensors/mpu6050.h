#pragma once

#include "esp_err.h"
#include "driver/i2c.h"

typedef struct {
    float accel_x, accel_y, accel_z;   /* m/s²  */
    float gyro_x,  gyro_y,  gyro_z;    /* °/s   */
    float temperature_c;                /* MPU-6050 internal die temp */
    bool  valid;
} mpu6050_reading_t;

/**
 * @brief Initialise MPU-6050 on the shared I2C bus.
 *        Default I2C address: 0x68 (AD0 pin LOW).
 *        Configured for ±2 g accelerometer, ±250 °/s gyroscope.
 */
esp_err_t mpu6050_init(i2c_port_t i2c_port);

/** @brief Read one sample from all MPU-6050 axes. */
esp_err_t mpu6050_read(mpu6050_reading_t *reading);
