#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/igmp.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "soc/rtc_periph.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_spi_flash.h"

#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "esp_timer.h"
#include "myUart.h"

// Pins in use
#define GPIO_MOSI 12
#define GPIO_MISO 13
#define GPIO_SCLK 15
#define GPIO_CS 14

// UART 0
#define PC_UART_PORT    (0)
#define PC_UART_RX_PIN  (3)
#define PC_UART_TX_PIN  (1)
#define READ_BUF_SIZE           (1024)


void uartInit(uart_port_t uart_num, uint32_t baudrate, uint8_t size, uint8_t parity, uint8_t stop, uint8_t txPin, uint8_t rxPin)
{
    uart_config_t uart_config = {
        .baud_rate = (int) baudrate,
        .data_bits = (uart_word_length_t)(size-5),
        .parity    = (parity==eParityEven)?UART_PARITY_EVEN:(parity==eParityOdd)?UART_PARITY_ODD:UART_PARITY_DISABLE,
        .stop_bits = (stop==eStop1)?UART_STOP_BITS_1:UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_REF_TICK,
    };

    ESP_ERROR_CHECK(uart_driver_install(uart_num, READ_BUF_SIZE, READ_BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, txPin, rxPin,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

}

void delayMs(uint16_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void uartClrScr(uart_port_t uart_num)
{
    // Uso "const" para sugerir que el contenido del arreglo lo coloque en Flash y no en RAM
    const char caClearScr[] = "\e[2J";
    uart_write_bytes(uart_num, caClearScr, sizeof(caClearScr));
}
void uartGoto11(uart_port_t uart_num)
{
    // Limpie un poco el arreglo de caracteres, los siguientes tres son equivalentes:
     // "\e[1;1H" == "\x1B[1;1H" == {27,'[','1',';','1','H'}
    const char caGoto11[] = "\e[1;1H";
    uart_write_bytes(uart_num, caGoto11, sizeof(caGoto11));
}

bool uartKbhit(uart_port_t uart_num)
{
    uint8_t length;
    uart_get_buffered_data_len(uart_num, (size_t*)&length);
    return (length > 0);
}

void uartPutchar(uart_port_t uart_num, char c)
{
    uart_write_bytes(uart_num, &c, sizeof(c));
}

char uartGetchar(uart_port_t uart_num)
{
    unsigned char c;
    // Wait for a received byte
    while(!uartKbhit(uart_num))
    {
        delayMs(10);
    }
    // read byte, no wait
    uart_read_bytes(uart_num,&c, sizeof(c), 0);

    return c;
}

void uartPuts(uart_port_t uart_num, char *str)
{
    while(*str!='\0')
    {   
        uartPutchar(uart_num,*str);
        (str++);
    }
}

void uartGets(uart_port_t uart_num, char *str)
{
    char c;
    int cont=0;

    while(1)
    {
        c=uartGetchar(uart_num);

        if(c==13)
        {
            str[cont]='\0';
            break;
        }
        else
        {
            if(c==8)
            {
                cont--;
                if(cont<0)
                {
                     cont=0;
                     uartPutchar(uart_num,32);
                }
                else
                {
                    uartPutchar(uart_num,8);
                    uartPutchar(uart_num,32);
                    uartPutchar(uart_num,8);
                    uartPutchar(uart_num,32);
                }
            }
            else
            {
                str[cont]=c;
                cont++;
            }
        }
        uartPutchar(uart_num,c);
    }
}


int contar(char *str)
{
    int i,cont=0;

    for(i=0;str[i]!='\0';i++)
    {
        cont++;
    }
    return cont;
}

uint16_t myAtoi(char *str)
{
    int x=0,i,pos=1,tam=0;
    
    tam=contar(str); 

    for(i=0;i<tam-1;i++)
    {
        pos=pos*10; 
    }

    for(i=0;i<tam;i++)
    {
        if(str[i]>='0' && str[i]<='9')
        {
            x=x+(str[i]-48)*pos; 
            pos=pos/10;
        }
        else
        {
            x=x/(pos*10);
            break;
        }
    }

    return x;
}

void myItoa(uint16_t number, char* str, uint8_t base)
{
    uint16_t cociente=0,residuo=0;
    int tam,i=0,j=0;
    char c,c2;

    cociente=number;

    while(cociente!=0)
    {
        residuo=cociente%base; 
        cociente=(cociente/base);
        if(residuo==15 || residuo==14 || residuo==13 || residuo==12 || residuo==11 || residuo==10)
            str[i]=residuo+55; 
        else
            str[i]=residuo+48; 
        i++;
    }
    str[i]='\0';

    tam=contar(str);

    for(i=0;i<tam-1;i++)
    {
        for(j=i+1;j<tam;j++)
        {
            c=str[i]; 
            c2=str[j];
            str[i]=c2;
            str[j]=c;
        }
    }
}

void uartSetColor(uart_port_t uart_num, uint8_t color)
{
    char setColor[10];

    myItoa(color,setColor,10);
    uartPuts(uart_num,"\033[0;");
    uartPuts(uart_num,setColor);
    uartPuts(uart_num,"m");

}

void uartGotoxy(uart_port_t uart_num, uint8_t x, uint8_t y)
{
    char setX[10] = {0} ,setY[10] = {0};

    myItoa(x,setX,10);
    myItoa(y,setY,10);

    uartPuts(uart_num,"\e[");
    uartPuts(uart_num,setX);
    uartPuts(uart_num,";");
    uartPuts(uart_num,setY);
    uartPuts(uart_num,"H");
}

//Main application
void app_main(void)
{
    spi_device_handle_t handle;

    // Configuration for the SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = GPIO_MOSI,
        .miso_io_num = GPIO_MISO,
        .sclk_io_num = GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1};

    // Configuration for the SPI device on the other side of the bus
    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .clock_speed_hz = 5000000,
        .duty_cycle_pos = 128, // 50% duty cycle
        .mode = 0,
        .spics_io_num = GPIO_CS,
        .cs_ena_posttrans = 3, // Keep the CS low 3 cycles after transaction
        .queue_size = 3};

    char recvbuf[129] = "";
    memset(recvbuf, 0, 33);
    spi_transaction_t r;
    memset(&r, 0, sizeof(r));

    char sendbuf[128] = {0};

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(HSPI_HOST, &devcfg, &handle);

    printf("Master output:\n");

    uartInit(PC_UART_PORT,115200,8,0,1, PC_UART_TX_PIN, PC_UART_RX_PIN);

    while(1) {
        char cad[30] ={0};

        uartGoto11(0);
        uartClrScr(0);
        uartPuts(0,"0x");
        uartGets(0,cad);

        snprintf(sendbuf, sizeof(sendbuf), cad);
        t.length = sizeof(sendbuf) * 8;
        t.tx_buffer = sendbuf;
        spi_device_transmit(handle, &t);
        printf("Transmitted: %s\n", sendbuf);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        
        uartClrScr(0);
        t.length = 128 * 8;
        t.rx_buffer = recvbuf;
        spi_device_transmit(handle, &r);
        printf("Received: %s\n", recvbuf);
        delayMs(1000);

        /*
        t.length = 128 * 8;
        t.rx_buffer = recvbuf;
        spi_device_transmit(handle, &t);
        printf("Received: %s\n", recvbuf);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        uartClrScr(0);
        uartPuts(0,cad);
        delayMs(1000);
        */

         
        
    }   
}
