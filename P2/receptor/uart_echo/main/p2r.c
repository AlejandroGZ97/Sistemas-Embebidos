/* Receptor
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "myUart.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "crc32.h"

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

struct uartSendPacket{

    uint8_t init;
    uint8_t command;
    uint8_t longitudDato;
    uint32_t dato;
    uint8_t end;
    uint32_t checkCRC32;

}packageUart;

static uint8_t estadoLed = 0;
static uint16_t temperature = 0;
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
bool checkPackage(){

    uint8_t data[]={0x5a,packageUart.command,packageUart.longitudDato,packageUart.dato,0xb2}; 
    uint32_t calculateCRC32;
    calculateCRC32=calc_crc32(data, sizeof(data));
    //calculateCRC32=crc32_le(0,data,sizeof(data));
    //printf("\n%d",calculateCRC32);
    if(calculateCRC32 == packageUart.checkCRC32)
        return true;
    else 
        return false;
}

void sendPacket(uart_port_t uart_num, char *str,uint8_t longitudDato,uint32_t dato){

    packageUart.init=0x5a;
    packageUart.command=myAtoi(str);
    packageUart.longitudDato=longitudDato;
    packageUart.dato=dato;
    packageUart.end=0xb2;

    uint8_t dataSend[]={packageUart.init,packageUart.command,packageUart.longitudDato,packageUart.dato,packageUart.end};  
    //packageUart.checkCRC32=crc32_le(0,dataSend,sizeof(dataSend));
    packageUart.checkCRC32=calc_crc32(dataSend, sizeof(dataSend));
    uartPutchar(uart_num,packageUart.init);
    uartPutchar(uart_num,packageUart.command);
    uartPutchar(uart_num,packageUart.longitudDato);
    uartPutchar(uart_num,packageUart.dato);
    uartPutchar(uart_num,packageUart.end);
    //Send CRC32
    uartPutchar(uart_num,packageUart.checkCRC32);
    uartPutchar(uart_num,packageUart.checkCRC32>>8);
    uartPutchar(uart_num,packageUart.checkCRC32>>16);
    uartPutchar(uart_num,packageUart.checkCRC32>>24);

}

void receivePacket(uart_port_t uart_num){

    packageUart.init=uartGetchar(uart_num);
    packageUart.command=uartGetchar(uart_num);
    packageUart.longitudDato=uartGetchar(uart_num);
    packageUart.dato=uartGetchar(uart_num);
    packageUart.end=uartGetchar(uart_num);

    packageUart.checkCRC32=uartGetchar(uart_num);
    packageUart.checkCRC32|=uartGetchar(uart_num) << 8;
    packageUart.checkCRC32|=uartGetchar(uart_num) << 16;
    packageUart.checkCRC32|=uartGetchar(uart_num) << 24;
}

void Timestamp(uint32_t start){
    uint32_t stop=0;
    //char cad[]={"10"};
    char  timeCad[50]={0};

    stop=xTaskGetTickCount()-start; 
    sprintf(timeCad, "Tiempo transcurrido en : %d ms", stop);
    uartPuts(0,timeCad);
}

void TemperatureCmd(){

    char cad[10]={0};

    temperature++;
    myItoa(temperature,cad,10);
    uartPuts(0,"Temperatura actual: ");
    uartPuts(0,cad);
    //sendPacket(2,cad,sizeof(temperature),temperature);
}

void invertLedState(){

   // char cad[10]={"13"};
    estadoLed= !estadoLed;
    gpio_set_level(2,estadoLed);
    uartPuts(0,"Invertido");
}

void LedStateCmd(){
    if(estadoLed)
        uartPuts(0,"Led state : 1");
    else
        uartPuts(0,"Led state : 0");
}


void app_main(void)
{
   // char cad[20]={0};

    uint32_t start;
    
    start=xTaskGetTickCount();

    gpio_reset_pin(2);
    gpio_set_direction(2,GPIO_MODE_OUTPUT);

    uartInit(PC_UART_PORT,115200,8,0,1, PC_UART_TX_PIN, PC_UART_RX_PIN);
    uartInit(UART2_PORT,115200,8,0,1,UART2_TX_PIN,UART2_RX_PIN);
    
    delayMs(500);

    while(1)
    {

        uartGoto11(0);
        uartClrScr(0);

        receivePacket(2);

        if (checkPackage()){
            switch (packageUart.command)
            {
            case 10:
                Timestamp(start);
                //mandar paquete 
                //sendPacket(2,"10",sizeof(Timestamp(start)),Timestamp(start));
                
                break;
            case 11:
                LedStateCmd();
                break;
            case 12:
                TemperatureCmd();
                break;
            case 13:
                invertLedState();
                break;
            default:
                uartPuts(0,"Esperando comando valido");
                break;
            }
        }
        else
            uartPuts(0,"Esperando comando valido");
        
         delayMs(1000);

    }
}
