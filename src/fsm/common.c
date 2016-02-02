/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Matthias P. Braendli
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

#include "common.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "timers.h"
#include <stm32f4xx.h>
#include <time.h>

static uint64_t common_timestamp = 0; // milliseconds since startup
static TimerHandle_t common_timer;

// The LFSR is used as random number generator
static const uint16_t lfsr_start_state = 0x12ABu;
static uint16_t lfsr;

static void common_increase_timestamp(TimerHandle_t t);

void common_init(void)
{
    common_timer = xTimerCreate("Timer",
            portTICK_PERIOD_MS,
            pdTRUE, // Auto-reload
            NULL,   // No unique id
            common_increase_timestamp
            );

    xTimerStart(common_timer, 0);

    lfsr = lfsr_start_state;
}

static void common_increase_timestamp(TimerHandle_t t)
{
    common_timestamp++;
}

uint64_t timestamp_now(void)
{
    return common_timestamp;
}

int dayofweek(uint8_t day, uint8_t month, uint16_t year)
{
   /* Zeller's congruence for the Gregorian calendar.
    * With 0=Monday, ... 5=Saturday, 6=Sunday
    */
   if (month < 3) {
      month += 12;
      year--;
   }
   int k = year % 100;
   int j = year / 100;
   int h = day + 13*(month+1)/5 + k + k/4 + j/4 + 5*j;
   return (h + 5) % 7 + 1;
}


// Return either 0 or 1, somewhat randomly
int random_bool(void)
{
    uint16_t bit;

    /* taps: 16 14 13 11; feedback polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
    bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
    lfsr =  (lfsr >> 1) | (bit << 15);

    return bit;
}

// For the debugger
static int faultsource = 0;
void trigger_fault(int source)
{
    usart_debug("Fatal: %d", source);

    __disable_irq();

    faultsource = source;

    while (1) {}
}

void hard_fault_handler_c(uint32_t *hardfault_args)
{
    uint32_t stacked_r0;
    uint32_t stacked_r1;
    uint32_t stacked_r2;
    uint32_t stacked_r3;
    uint32_t stacked_r12;
    uint32_t stacked_lr;
    uint32_t stacked_pc;
    uint32_t stacked_psr;

    stacked_r0 = hardfault_args[0];
    stacked_r1 = hardfault_args[1];
    stacked_r2 = hardfault_args[2];
    stacked_r3 = hardfault_args[3];

    stacked_r12 = hardfault_args[4];
    stacked_lr = hardfault_args[5];
    stacked_pc = hardfault_args[6];
    stacked_psr = hardfault_args[7];

    usart_debug_puts("\n\n[Hard fault handler - all numbers in hex]\n");
    usart_debug("R0 = %x\n", stacked_r0);
    usart_debug("R1 = %x\n", stacked_r1);
    usart_debug("R2 = %x\n", stacked_r2);
    usart_debug("R3 = %x\n", stacked_r3);
    usart_debug("R12 = %x\n", stacked_r12);
    usart_debug("LR [R14] = %x  subroutine call return address\n", stacked_lr);
    usart_debug("PC [R15] = %x  program counter\n", stacked_pc);
    usart_debug("PSR = %x\n", stacked_psr);
    usart_debug("BFAR = %x\n", (*((volatile unsigned long *)(0xE000ED38))));
    usart_debug("CFSR = %x\n", (*((volatile unsigned long *)(0xE000ED28))));
    usart_debug("HFSR = %x\n", (*((volatile unsigned long *)(0xE000ED2C))));
    usart_debug("DFSR = %x\n", (*((volatile unsigned long *)(0xE000ED30))));
    usart_debug("AFSR = %x\n", (*((volatile unsigned long *)(0xE000ED3C))));
    usart_debug("SCB_SHCSR = %x\n", SCB->SHCSR);

    while (1);
}
