#include "mpu6050.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* MPU-6050 I2C address (AD0 pin LOW = 0x68, HIGH = 0x69) */
#define MPU6050_ADDR        0x68

/* Register map */
#define REG_GYRO_CONFIG     0x1B
#define REG_ACCEL_CONFIG    0x1C
#define REG_ACCEL_XOUT_H    0x3B    /* 14 bytes: AX AY AZ TEMP GX GY GZ */
#define REG_PWR_MGMT_1      0x6B

/* Scale factors for default ranges */
#define ACCEL_SCALE         (9.81f / 16384.0f)  /* ±2 g  → m/s² */
#define GYRO_SCALE          (1.0f  / 131.0f)    /* ±250°/s       */
#define TEMP_SCALE          (1.0f  / 340.0f)
#define TEMP_OFFSET_C       36.53f

static const char *TAG = "mpu6050";
static i2c_master_dev_handle_t s_dev;

static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};
    return i2c_master_transmit(s_dev, data, sizeof(data), 100);
}

esp_err_t mpu6050_init(i2c_master_bus_handle_t bus_handle)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = MPU6050_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &s_dev));

    /* Retry wake-up — sensor may need time after power-on */
    esp_err_t ret = ESP_FAIL;
    for (int attempt = 0; attempt < 5; attempt++) {
        ret = write_reg(REG_PWR_MGMT_1, 0x00);
        if (ret == ESP_OK) break;
        ESP_LOGW(TAG, "MPU-6050 wake attempt %d/5 failed (0x%x)", attempt + 1, ret);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MPU-6050 not responding at I2C 0x%02X – check wiring (SDA/SCL/VCC/GND)", MPU6050_ADDR);
        return ret;
    }

    /* ±2 g accelerometer range (AFS_SEL = 0) */
    ret = write_reg(REG_ACCEL_CONFIG, 0x00);
    if (ret != ESP_OK) return ret;

    /* ±250 °/s gyroscope range (FS_SEL = 0) */
    ret = write_reg(REG_GYRO_CONFIG, 0x00);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "MPU-6050 ready at I2C 0x%02X", MPU6050_ADDR);
    return ESP_OK;
}

esp_err_t mpu6050_read(mpu6050_reading_t *reading)
{
    uint8_t reg = REG_ACCEL_XOUT_H;
    uint8_t buf[14];

    esp_err_t ret = i2c_master_transmit_receive(s_dev, &reg, 1, buf, sizeof(buf), 100);
    if (ret != ESP_OK) {
        reading->valid = false;
        ESP_LOGW(TAG, "Read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    int16_t ax = (int16_t)((buf[0]  << 8) | buf[1]);
    int16_t ay = (int16_t)((buf[2]  << 8) | buf[3]);
    int16_t az = (int16_t)((buf[4]  << 8) | buf[5]);
    int16_t t  = (int16_t)((buf[6]  << 8) | buf[7]);
    int16_t gx = (int16_t)((buf[8]  << 8) | buf[9]);
    int16_t gy = (int16_t)((buf[10] << 8) | buf[11]);
    int16_t gz = (int16_t)((buf[12] << 8) | buf[13]);

    reading->accel_x       = (float)ax * ACCEL_SCALE;
    reading->accel_y       = (float)ay * ACCEL_SCALE;
    reading->accel_z       = (float)az * ACCEL_SCALE;
    reading->gyro_x        = (float)gx * GYRO_SCALE;
    reading->gyro_y        = (float)gy * GYRO_SCALE;
    reading->gyro_z        = (float)gz * GYRO_SCALE;
    reading->temperature_c = (float)t  * TEMP_SCALE + TEMP_OFFSET_C;
    reading->valid         = true;

    ESP_LOGD(TAG, "A=(%.2f,%.2f,%.2f) m/s²  G=(%.1f,%.1f,%.1f) °/s  T=%.1f°C",
             reading->accel_x, reading->accel_y, reading->accel_z,
             reading->gyro_x,  reading->gyro_y,  reading->gyro_z,
             reading->temperature_c);
    return ESP_OK;
}
