#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"

//GPIO pins to use for SPI data
#define GPIO_CS     14
#define GPIO_SCLK   15
#define GPIO_MISO   13
#define GPIO_MOSI   12

// Acelerometro LIS2DW12
#define DEVICE_ADDR 0x19 
#define WHO_AM_I 0X0F    
#define OUT_X_H 0x29
#define OUT_X_L 0x28
#define OUT_Y_H 0x2B
#define OUT_Y_L 0x2A
#define OUT_Z_H 0x2D
#define OUT_Z_L 0x2C

#define CTRL1 0x20   // Control register 1 (r/w)
#define ODR 0x01     // 4bit
#define MODE 0x01    // 2bit
#define LP_MODE 0x01 // 2bit  0001 01 01 0x25 ODR-MODE-LP_MODE
#define MODE_OPERATION 0X15

#define DEVICE_ADDR_L_R 0x31 
#define DEVICE_ADDR_L_W 0x30
#define DEVICE_ADDR_H_R 0x33 
#define DEVICE_ADDR_H_W 0x32

//Global SPI variables
esp_err_t ret;
spi_device_handle_t spi;

static const char *TAG = "spi-simple-example";

void spi_config() {
    //Configuration for the SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = GPIO_MOSI,
        .miso_io_num = GPIO_MISO,
        .sclk_io_num = GPIO_SCLK,
        .quadwp_io_num = -1,    //write-protect (-1 for "not used")
        .quadhd_io_num = -1     //hold signal (-1 for "not used")
    };

    //Initialize the SPI bus
    ret = spi_bus_initialize(VSPI_HOST, &buscfg, 0);
    assert(ret == ESP_OK);
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

void spi_config_accel() {
    //Configuration for the SPI device on the other side of the bus (Accelerometer)
    spi_device_interface_config_t devcfg = {
        .command_bits = 2,
        .address_bits = 6,
        .dummy_bits = 0,
        .clock_speed_hz = 5000000,   // 5 MHz
        .input_delay_ns = 10,            // t_hold on accelerometer
        .duty_cycle_pos = 128,          //50% duty cycle
        .mode = 0,
        .spics_io_num = GPIO_CS,
        .cs_ena_pretrans = 1,
        .cs_ena_posttrans = 3,
        .queue_size = 1,
        .flags = SPI_DEVICE_HALFDUPLEX
    };

    //Initialize the device we want to send stuff to.
    ret = spi_bus_add_device(VSPI_HOST, &devcfg, &spi);
    assert(ret == ESP_OK);
    vTaskDelay(100 / portTICK_PERIOD_MS);

}

esp_err_t  device_register_read(uint8_t reg_addr, uint8_t *data, size_t len) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    t.cmd          = 0b10;       //read, single byte
    t.addr         = reg_addr;    //address to read
    t.length       = 0;          //length of tx
    t.rxlength     = 8;          //length of rx
    t.tx_buffer    = NULL;       //no writing to do
    t.rx_buffer    = data;      //put input into variable data


    ret = spi_device_transmit(spi, &t);
    assert(ret == ESP_OK);
    
    return ret;
}

esp_err_t  device_register_write_byte(uint8_t address, uint8_t data) {

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    t.cmd          = 0b00;         //write, single byte
    t.addr         = address;      //address to write
    t.length       = 8;            //length of tx
    t.rxlength     = 0;            //length of rx
    t.tx_buffer    = &data;        //write from Write_Data
    t.rx_buffer    = NULL;         //no reading to do

    ret = spi_device_transmit(spi, &t);
    assert(ret == ESP_OK);
    return ret;

}

void delayMs(uint16_t ms){vTaskDelay(ms / portTICK_PERIOD_MS);}

void app_main() {
    uint8_t data=0;

    spi_config();
    spi_config_accel();

    //spi_device_acquire_bus(accelerometer, portMAX_DELAY);
    ESP_ERROR_CHECK(device_register_write_byte(CTRL1, MODE_OPERATION));

    //SPI transactions
    while(1){
        ESP_ERROR_CHECK(device_register_read(WHO_AM_I, &data, 1));
        ESP_LOGI(TAG, "Who I Am = %X", data);
        ESP_ERROR_CHECK(device_register_read(OUT_X_H, &data, 1));
        ESP_LOGI(TAG, "X High = %X", data);
        ESP_ERROR_CHECK(device_register_read(OUT_X_L, &data, 1));
        ESP_LOGI(TAG, "X Low = %X", data); 
        ESP_ERROR_CHECK(device_register_read(OUT_Y_H, &data, 1));
        ESP_LOGI(TAG, "Y High = %X",data);
        ESP_ERROR_CHECK(device_register_read(OUT_Y_L, &data, 1));
        ESP_LOGI(TAG, "Y Low = %X",data);
        ESP_ERROR_CHECK(device_register_read(OUT_Z_H, &data, 1));
        ESP_LOGI(TAG, "Z High = %X",data);
        ESP_ERROR_CHECK(device_register_read(OUT_Z_L, &data, 1));
        ESP_LOGI(TAG, "Z Low = %X",data);
    
        delayMs(3000);
    }
}