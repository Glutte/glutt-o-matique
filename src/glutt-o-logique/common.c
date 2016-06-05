#include <stm32f4xx.h>

#include "../common/src/Core/common.c"

void hard_fault_handler_extra() {
    usart_debug("SCB_SHCSR = %x\n", SCB->SHCSR);
}
