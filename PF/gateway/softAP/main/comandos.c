#include "comandos.h"

char cad[30] = " ";
uint8_t temp=0; //Simulated temperature
static uint8_t ledState = 0;

void timeStamp(uint32_t start){
    uint32_t stop=0;
    //char cad[]={"10"};
    char  timeCad[30]={0};

    stop=xTaskGetTickCount()-start; 
    sprintf(timeCad, "Tiempo transcurrido: %d ms", stop);
    strcpy(cad,timeCad);
}

void showTemperature(){
    char tempCad[30]={0};

    temp++;
    myItoa(temp,cad,10);
    sprintf(tempCad,"Temperatura actual: %d",temp);
    strcpy(cad,tempCad);
}

void invertLedState(){
    ledState = !ledState;
    gpio_set_level(2,ledState);
    strcpy(cad,"Estado del led invertido");
}

void showLedState(){
    if(ledState)
        strcpy(cad,"Estado del led : 1");
    else
        strcpy(cad,"Estado del led : 0");
}

void myItoa(uint16_t number, char* str, uint8_t base)
{
    char nums[15]={0};
	uint16_t count = 0;
	
	if(number < 1 || base < 2) {
		*str++ = '0';
		*str = 0;
		return;
	}

	while(number) {
		if((number % base) > 9) nums[count++] = (number % base) + '0' + 7;
		else nums[count++] = (number % base) + '0';
		number -= (number % base);
		number /= base;
	}

	while(count > 0) {*str++ = nums[--count];}
	*str = 0;
}