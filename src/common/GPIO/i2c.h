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

#ifndef __I2C_H_
#define __I2C_H_

#include <stdint.h>

/* Initialise I2C on the board for both the audio codec and the GPS receiver */
void i2c_init(void);

/* Do an I2C write, return 1 on success, 0 on failure */
int i2c_write(uint8_t device, const uint8_t *txbuf, int len);

/* Do an I2C read into rxbuf.
 * Returns number of bytes received, or 0 in case of failure
 */
int i2c_read(uint8_t device, uint8_t *rxbuf, int len);

/* Do an I2C write for the address, and then a read into rxbuf.
 * Returns number of bytes received, or 0 in case of failure
 */
int i2c_read_from(uint8_t device, uint8_t address, uint8_t *rxbuf, int len);

/* Start an I2C transaction, keeping exclusive access to I2C */
void i2c_transaction_start(void);

/* End an I2C transaction, unlocking the exclusive access to I2C */
void i2c_transaction_end(void);

#endif // __I2C_H_

