/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Matthias P. Braendli, Maximilien Cuony
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

#include "stm32f4xx_conf.h"

// This is a set of guards to make sure the FPU compile configuration
// is correct
#ifndef __FPU_USED
#  error "no __FPU_USED"
#endif

#ifndef __FPU_PRESENT
#  error "No __FPU_PRESENT"
#endif

#ifndef __GNUC__
#  error "No __GNUC__"
#endif

#ifndef __VFP_FP__
#  error "No VFP_FP"
#endif

#if defined(__SOFTFP__)
#  error  "SOFTFP"
#endif

void init(void);
void init()
{
    /* Setup Watchdog
     * The IWDG runs at 32kHz. With a prescaler of 32 -> 1kHz.
     * Counting to 2000 / 1000 = 2 seconds
     */
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_32);
    IWDG_SetReload(2000);
    IWDG_Enable();
}
