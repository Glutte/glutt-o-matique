.syntax unified
.cpu cortex-m3
.thumb

.global HardFault_Handler
.extern hard_fault_handler_c

HardFault_Handler:
  tst lr, #4
  ite eq
  mrseq r0, msp
  mrsne r0, psp
  b hard_fault_handler_c
