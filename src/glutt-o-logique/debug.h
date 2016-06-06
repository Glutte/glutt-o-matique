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

/* Debugging utilities using the ARM semihosting facilities. Warning, it's quite
 * slow.
 *
 * when used with OpenOCD's gdb server, requires the gdb command
 *
 * monitor arm semihosting enable
 */
#ifndef __DEBUG_H_
#define __DEBUG_H_

/* Example usage for the send_command function
const char *s = "Hello world\n";
uint32_t m[] = { 2, (uint32_t)s, sizeof(s)/sizeof(char) };
send_command(0x05, m);
// some interrupt ID

*/

/* Print a string to the OpenOCD console */
void debug_print(const char* str);


void debug_send_command(int, void*);
void put_char(char);

#endif // __DEBUG_H_

