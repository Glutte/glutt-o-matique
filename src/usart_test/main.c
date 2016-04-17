#include "stm32f4xx.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stm32f4xx.h>
#include <stm32f4xx_usart.h>
#include <stm32f4xx_conf.h>
#include "tm_stm32f4_ds18b20.h"
#include "tm_stm32f4_onewire.h"

const uint16_t GPIOA_PIN_USART2_RX = GPIO_Pin_3;
const uint16_t GPIOA_PIN_USART2_TX = GPIO_Pin_2;

#define ONEWIRE_PIN       GPIO_Pin_1 // PA1

volatile uint64_t timer;
void SysTick_Handler(void)
{
    if (timer) timer--;
}

void delay(const uint64_t us)
{
    timer = us / 10;
    while (timer);
}



char msg[32];
static int cnt = 0;
void printcnt(void)
{
    snprintf(msg, 31, "%d ", cnt++);

    char* c = msg;
    while (*c) {
        USART_SendData(USART2, *c);
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) ;
        c++;
    }
}

static void usart_puts(USART_TypeDef* USART, const char* str)
{
    while(*str) {
        // wait until data register is empty
        USART_SendData(USART, *str);
        while(USART_GetFlagStatus(USART, USART_FLAG_TXE) == RESET) ;
        str++;
    }
}
#define MAX_MSG_LEN 80
static char usart_debug_message[MAX_MSG_LEN];

void usart_debug(const char *format, ...)
{
    va_list list;
    va_start(list, format);
    vsnprintf(usart_debug_message, MAX_MSG_LEN-1, format, list);
    usart_puts(USART2, usart_debug_message);
    va_end(list);
}

void usart_debug_puts(const char* str)
{
    usart_puts(USART2, str);
}


static TM_OneWire_t tm_onewire;

void ds18b20_init()
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef  GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   = ONEWIRE_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    TM_OneWire_Init(&tm_onewire, GPIOA, ONEWIRE_PIN);
}

int ds18b20_gettemp_one(float *temperature)
{
    uint8_t rom_addr[8];

    TM_DS18B20_StartAll(&tm_onewire);
    int status = TM_OneWire_First(&tm_onewire);
    if (status) {
        //Save ROM number from device
        TM_OneWire_GetFullROM(&tm_onewire, rom_addr);
        TM_DS18B20_Start(&tm_onewire, rom_addr);

        // The sensor needs time to do the conversion
        delay(100ul * 1000ul);
        status = TM_DS18B20_Read(&tm_onewire, rom_addr, temperature);
    }

    return status;
}

int ds18b20_gettemp(float *temperature)
{
    int status;
    for (int i = 0; i < 10; i++) {
        status = ds18b20_gettemp_one(temperature);

        if (status) {
            break;
        }
        delay(5000ul);
    }
    return status;
}


int main(void)
{
    SysTick_Config(SystemCoreClock / 100000); // 10us tick

    // ============== PC DEBUG USART ===========
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = GPIOA_PIN_USART2_RX | GPIOA_PIN_USART2_TX;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

    // Setup USART2 for 9600,8,N,1
    USART_InitTypeDef USART_InitStruct;
    USART_InitStruct.USART_BaudRate = 9600;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART2, &USART_InitStruct);

#if 0
    // enable the USART2 receive interrupt
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_SetPriority(USART2_IRQn, 6);
#endif

    // finally this enables the complete USART2 peripheral
    USART_Cmd(USART2, ENABLE);

    usart_debug_puts("DS18B20 init\r\n");
    ds18b20_init();

    while(1) {
        float temp = 0.0f;
        if (ds18b20_gettemp(&temp)) {
            usart_debug("Temperature %f\r\n", temp);
        }
        else {
            usart_debug_puts("No temp\r\n");
        }

        delay(5 * 1000000ul);
    }
}

void USART2_IRQHandler(void)
{
    /* RXNE handler */
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        /* If received 't', toggle LED and transmit 'T' */
        if((char)USART_ReceiveData(USART2) == 't')
        {
            USART_SendData(USART2, 'T');

            //while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) ;
        }
        else {
            USART_SendData(USART2, 'N');
            USART_SendData(USART2, 'o');
            USART_SendData(USART2, '!');
            USART_SendData(USART2, '\n');
        }
    }
}

