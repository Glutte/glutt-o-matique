/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Matthias P. Braendli, Maximilien Cuony
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

#include <stm32f4xx.h>
#include "GPIO/usart.h"

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

