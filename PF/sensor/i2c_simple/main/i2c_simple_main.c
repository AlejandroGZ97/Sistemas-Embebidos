#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/i2c.h"

#include "esp_http_client.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define I2C_MASTER_SCL_IO 22         // Pin GPIO para el reloj SCL del bus I2C
#define I2C_MASTER_SDA_IO 21         // Pin GPIO para el dato SDA del bus I2C
#define I2C_MASTER_NUM 0             // I2C port número
#define I2C_MASTER_FREQ_HZ 100000    // Frecuencia de operación del bus I2C
#define AHT10_I2C_ADDR 0x38          // Dirección I2C del sensor AHT10

#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0

#define AHT10_CMD_CALIBRATE 0xE1     // Comando de calibración del sensor
#define AHT10_CMD_MEASURE 0xAC       // Comando de medición del sensor
#define AHT10_STATUS_BUSY 0x80       // Estado de ocupado del sensor

static const char *TAG = "AHT10";

///////////////////////////////////////////////////////////////////////////////
static const char *TAG2 = "ESP32_Client";
// Configuración de la red WiFi

#define IP_DEL_ESP32_SERVIDOR  "192.168.4.1"
#define NOMBRE_RED_WIFI        "P7AKS"
#define CLAVE_RED_WIFI    "SE_12345678"
///////////////////////////////////////////////////////////////////////////////

void send_command(const char* command)
{
    esp_http_client_config_t config = {
        .url = "http://" IP_DEL_ESP32_SERVIDOR "/",
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, command, strlen(command));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            ESP_LOGI(TAG2, "Comando enviado correctamente");
        } else {
            ESP_LOGE(TAG2, "Error en la respuesta del servidor: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG2, "Error al enviar el comando: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

static esp_err_t wifi_event_handler(void* ctx, system_event_t* event)
{
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG2, "Conectado a la red WiFi");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG2, "Desconectado de la red WiFi. Intentando reconectar...");
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG2, "Obtenida dirección IP: %s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            break;
        default:
            break;
    }
    return ESP_OK;
}

static esp_err_t i2c_master_init()
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

static void aht10_calibrate()
{
    uint8_t cmd[3] = {AHT10_CMD_CALIBRATE, 0x08, 0x00};
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, AHT10_I2C_ADDR << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd_handle, cmd, sizeof(cmd), true);
    i2c_master_stop(cmd_handle);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd_handle);
}

static void aht10_read_data(uint8_t* data)
{
    uint8_t cmd[3] = {AHT10_CMD_MEASURE, 0x33, 0x00};
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, AHT10_I2C_ADDR << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd_handle, cmd, sizeof(cmd), true);
    i2c_master_stop(cmd_handle);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd_handle);

    vTaskDelay(75 / portTICK_PERIOD_MS); // Esperar 75 ms

    cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, AHT10_I2C_ADDR << 1 | I2C_MASTER_READ, true);
    i2c_master_read(cmd_handle, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd_handle);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd_handle);
}

static float aht10_calculate_temperature(const uint8_t *data)
{
    int32_t rawTemperature = ((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5];
    return (rawTemperature * 200.0 / 1048576.0) - 50.0;
}

static float aht10_calculate_humidity(const uint8_t *data)
{
    int32_t rawHumidity = (data[1] << 12) | (data[2] << 4) | ((data[3] >> 4) & 0x0F);
    return (rawHumidity * 100.0) / 1048576.0;
}

void app_main()
{
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "Inicializado I2C master");

    aht10_calibrate();

    uint8_t data[6];

    //wifi
     ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = NOMBRE_RED_WIFI,
            .password = CLAVE_RED_WIFI,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Esperar a que se conecte a la red WiFi
    ESP_LOGI(TAG, "Conectando a la red WiFi...");
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        wifi_ap_record_t ap_info;
        ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));
        if (ap_info.rssi != 31) {  // Verificar si se ha establecido la conexión
            ESP_LOGI(TAG, "Conexión establecida con la red WiFi");
            break;
        }
    }

    // Enviar comandos al servidor
    send_command("0x10");  // Ejemplo de enviar el comando 0x10
    vTaskDelay(pdMS_TO_TICKS(5000));  // Retardo de 5 segundos

    // Detener la conexión WiFi
    ESP_LOGI(TAG, "Deteniendo la conexión WiFi...");
    ESP_ERROR_CHECK(esp_wifi_stop());

    while (1)
    {
        aht10_read_data(data);

        float temperature = aht10_calculate_temperature(data);
        ESP_LOGI(TAG, "Temperatura: %.2f °C", temperature);

        float humidity = aht10_calculate_humidity(data);
        ESP_LOGI(TAG, "Humedad: %.2f %%", humidity);

        vTaskDelay(2000 / portTICK_PERIOD_MS); // Esperar 2 segundos

        send_command("0x13");  // Ejemplo de enviar el comando 0x10
        vTaskDelay(pdMS_TO_TICKS(1000));  // Esperar 1 segundo
        send_command("0x13");  // Ejempl

        
    }
}