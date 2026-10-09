/* Minimal Cortex-M4F startup for QEMU mps2-an386 (from motor-temp-edge). */
#include <stdint.h>
#include "bms_io.h"

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
void Reset_Handler(void);

void Reset_Handler(void)
{
	uint32_t *s = &_sidata;
	uint32_t *d = &_sdata;

	while (d < &_edata) {
		*d++ = *s++;
	}
	for (d = &_sbss; d < &_ebss; d++) {
		*d = 0u;
	}
	/* enable CP10/CP11 (FPU) full access, then sync */
	*(volatile uint32_t *)0xE000ED88u |= (0xFu << 20);
	__asm volatile("dsb\n isb");
	io_exit(pil_main());
}

static void Fault_Handler(void)
{
	io_puts("FAULT\n");
	io_exit(1);
}

__attribute__((section(".isr_vector"), used)) static void (*const vectors[16])(void) = {
	(void (*)(void))(&_estack), Reset_Handler, Fault_Handler, Fault_Handler,
	Fault_Handler, Fault_Handler, Fault_Handler, 0, 0, 0, 0,
	Fault_Handler, Fault_Handler, 0, Fault_Handler, Fault_Handler,
};
