/* i2c - Simple example

   Simple I2C example that shows how to initialize I2C
   as well as reading and writing from and to registers for a sensor connected over I2C.

   The sensor used in this example is a MPU6050 inertial measurement unit.

   For other examples please check:
   https://github.com/espressif/esp-idf/tree/master/examples

   See README.md file to get detailed usage of this example.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"

#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           21
#define I2C_MASTER_NUM              0                          
#define I2C_MASTER_FREQ_HZ          400000                     
#define I2C_MASTER_TX_BUF_DISABLE   0                          
#define I2C_MASTER_RX_BUF_DISABLE   0                          
#define I2C_MASTER_TIMEOUT_MS       1000

#define MPU6050_SENSOR_ADDR                 0x68        
#define MPU6050_WHO_AM_I_REG_ADDR           0x75        

#define MPU6050_PWR_MGMT_1_REG_ADDR         0x6B        
#define MPU6050_RESET_BIT                   7

#define MOUT_X_H                     0x3B
#define MOUT_X_L                     0x3C    
#define MOUT_Y_H                     0x3D
#define MOUT_Y_L                     0x3E
#define MOUT_Z_H                     0x3F
#define MOUT_Z_L                     0x40

static const char *TAG = "i2c-simple-example";

/**
 * @brief Read a sequence of bytes from a MPU6050 sensor registers
 */
static esp_err_t mpu6050_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, DEVICE_ADDR, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);
}

/**
 * @brief Write a byte to a MPU6050 sensor register
 */
static esp_err_t mpu6050_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    int ret;
    uint8_t write_buf[2] = {reg_addr, data};

    ret = i2c_master_write_to_device(I2C_MASTER_NUM, DEVICE_ADDR, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);

    return ret;
}

/**
 * @brief i2c master initialization
 */
static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

void delayMs(uint16_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void app_main(void)
{
    uint8_t data[10];
    uint8_t accelx;
    uint8_t accely;
    uint8_t accelz;
    
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");
    ESP_ERROR_CHECK(mpu6050_register_read(MPU6050_WHO_AM_I_REG_ADDR, data, 1));
    ESP_ERROR_CHECK(mpu6050_register_write_byte(MPU6050_PWR_MGMT_1_REG_ADDR, 1));
   

    /* Read the MPU6050 WHO_AM_I register, on power up the register should have the value 0x71 */
    while(1){
        ESP_ERROR_CHECK(mpu6050_register_read(MOUT_X_H , data+1, 1));
        ESP_ERROR_CHECK(mpu6050_register_read(MOUT_X_L , data+2, 1));
        ESP_ERROR_CHECK(mpu6050_register_read(MOUT_Y_H , data+3, 1));
        ESP_ERROR_CHECK(mpu6050_register_read(MOUT_Y_L , data+4, 1));
        ESP_ERROR_CHECK(mpu6050_register_read(MOUT_Z_H , data+5, 1));
        ESP_ERROR_CHECK(mpu6050_register_read(MOUT_Z_L , data+6, 1));

        ESP_LOGI(TAG, "Who Am I = %X",data[0]);
        ESP_LOGI(TAG, "X High = %X",data[1]);
        ESP_LOGI(TAG, "X Low = %X",data[2]);
        ESP_LOGI(TAG, "Y High = %X",data[3]);
        ESP_LOGI(TAG, "Y Low = %X",data[4]);
        ESP_LOGI(TAG, "Z High = %X",data[5]);
        ESP_LOGI(TAG, "Z Low = %X",data[6]);

        accelx = (data[1] <<8 )| data[2];
        accely = (data[3] <<8 )| data[4];
        accelz = (data[5] <<8 )| data[6];
        ESP_LOGI(TAG, "X = %d  Y = %d  Z = %d\n",accelx,accely,accelz);
        delayMs(3000);
        ESP_ERROR_CHECK(mpu6050_register_read(MPU6050_WHO_AM_I_REG_ADDR, data, 1));
        ESP_ERROR_CHECK(mpu6050_register_write_byte(MPU6050_PWR_MGMT_1_REG_ADDR, 1));
    }
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");
    
}