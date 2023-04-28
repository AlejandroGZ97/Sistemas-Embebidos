/* UART Echo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "myUart.h"
#include <stdio.h>
#include "driver/gpio.h"

// UART 0
#define PC_UART_PORT    (0)
#define PC_UART_RX_PIN  (3)
#define PC_UART_TX_PIN  (1)
// UART 1
#define UART1_PORT      (1)
#define UART1_RX_PIN    (9)
#define UART1_TX_PIN    (10)
// UART 2
#define UART2_PORT      (2)
#define UART2_RX_PIN    (16)
#define UART2_TX_PIN    (17)

#define UARTS_BAUD_RATE         (115200)
#define TASK_STACK_SIZE         (1048)
#define READ_BUF_SIZE           (1024)

/* Message printed by the "consoletest" command.
 * It will also be used by the default UART to check the reply of the second
 * UART. As end of line characters are not standard here (\n, \r\n, \r...),
 * let's not include it in this string. */
const char test_message[] = "This is an example string, if you can read this, the example is a success!";

/**
 * @brief Configure and install the default UART, then, connect it to the
 * console UART.
 */
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

void app_main(void)
{    
    uartInit(PC_UART_PORT,115200,8,0,1, PC_UART_TX_PIN, PC_UART_RX_PIN);
    uartInit(UART2_PORT,115200,8,0,1,UART2_TX_PIN,UART2_RX_PIN);
    
    delayMs(1000);

    while(1)
    {
        char cad[30] ={0};

        uartGoto11(0);
        uartClrScr(0);
        uartPuts(0,"0x");
        uartGets(0,cad);  
        uartPuts(2,cad);
        delayMs(1000);
        int len = uart_read_bytes(UART2_PORT,cad,READ_BUF_SIZE,20/portTICK_RATE_MS);
        uartClrScr(0);
        uartPuts(0,cad);
        delayMs(1000);
    }
}
