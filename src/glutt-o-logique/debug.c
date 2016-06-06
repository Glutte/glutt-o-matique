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

#include <stddef.h>
#include <stdint.h>

#include "debug.h"

#if _DEBUG

void debug_send_command(int command, void *message)
{
    __asm__ volatile (
        "mov r0, %[cmd];"
            "mov r1, %[msg];"
            "bkpt #0xAB"
            :
            : [cmd] "r" (command), [msg] "r" (message)
            : "r0", "r1", "memory");
}

void put_char(char c)
{
    __asm__ volatile (
            "mov r0, #0x03\n"   /* SYS_WRITEC */
            "mov r1, %[msg]\n"
            "bkpt #0xAB\n"
            :
            : [msg] "r" (&c)
            : "r0", "r1"
        );
}

void debug_print(const char* str)
{
    const int std_err = 2;

    int strlen = 0;
    const char* s;
    s = str;
    while (*s) {
        strlen++;
        s++;
    }

    uint32_t m[] = { std_err, (uint32_t)str, strlen };
    debug_send_command(0x05, m);
}

#else
void USART3_IRQHandler(void);
void debug_send_command(int __attribute__ ((unused))command, void __attribute__ ((unused))*message) { }
void put_char(char __attribute__ ((unused))c) { }
void debug_print(const char __attribute__ ((unused))*str) { }

#endif

