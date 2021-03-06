/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_cortexm_common
 * @{
 *
 * @file
 * @brief       Cortex-M specific configuration and initialization options
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include "cpu.h"
#include "periph/pm.h"

/**
 * @name   Pattern to write into the co-processor Access Control Register to
 *         allow full FPU access
 */
#define FULL_FPU_ACCESS         (0x00f00000)

/**
 * Interrupt vector base address, defined by the linker
 */
extern const void *_isr_vectors;

void cortexm_init(void)
{
    /* initialize the FPU on Cortex-M4F CPUs */
#if defined(CPU_ARCH_CORTEX_M4F) || defined(CPU_ARCH_CORTEX_M7)
    /* give full access to the FPU */
    SCB->CPACR |= (uint32_t)FULL_FPU_ACCESS;
#endif

    /* configure the vector table location to internal flash */
#if defined(CPU_ARCH_CORTEX_M3) || defined(CPU_ARCH_CORTEX_M4) || \
    defined(CPU_ARCH_CORTEX_M4F) || defined(CPU_ARCH_CORTEX_M7) || \
    (defined(CPU_ARCH_CORTEX_M0PLUS) && (__VTOR_PRESENT == 1))
    SCB->VTOR = (uint32_t)&_isr_vectors;
#endif

    /* initialize the interrupt priorities */
    /* set pendSV interrupt to same priority as the rest */
    NVIC_SetPriority(PendSV_IRQn, CPU_DEFAULT_IRQ_PRIO);
    /* set SVC interrupt to same priority as the rest */
    NVIC_SetPriority(SVCall_IRQn, CPU_DEFAULT_IRQ_PRIO);
    /* initialize all vendor specific interrupts with the same value */
    for (unsigned i = 0; i < CPU_IRQ_NUMOF; i++) {
        NVIC_SetPriority((IRQn_Type) i, CPU_DEFAULT_IRQ_PRIO);
    }

    /* enable wake up on events for __WFE CPU sleep */
    SCB->SCR |= SCB_SCR_SEVONPEND_Msk;

    /* for Cortex-M3 r1p0 and up the STKALIGN option was added, but not automatically
     * enabled until revision r2p0. For 64bit function arguments to work properly this
     * needs to be enabled.
     */
#ifdef SCB_CCR_STKALIGN_Msk
    SCB->CCR |= SCB_CCR_STKALIGN_Msk;
#endif
}

size_t cpu_find_memory_size(char *base, uint32_t block, uint32_t maxsize) {
    char *address = base;
    do {
        address += block;
        if (!cpu_check_address(address)) {
            break;
        }
    } while ((size_t)(address - base) < maxsize);

    return (size_t)(address - base);
}

bool cpu_check_address(volatile const char *address)
{
#if defined(CPU_ARCH_CORTEX_M3) || defined(CPU_ARCH_CORTEX_M4) || \
    defined(CPU_ARCH_CORTEX_M4F) || defined(CPU_ARCH_CORTEX_M7)
    static const uint32_t BFARVALID_MASK = (0x80 << SCB_CFSR_BUSFAULTSR_Pos);
    
    bool is_valid = true;

    /* Clear BFARVALID flag */
    SCB->CFSR |= BFARVALID_MASK;

    /* Ignore BusFault by enabling BFHFNMIGN and disabling interrupts */
    uint32_t mask = __get_FAULTMASK();
    __disable_fault_irq();
    SCB->CCR |= SCB_CCR_BFHFNMIGN_Msk;

    *address;
    /* Check BFARVALID flag */
    if ((SCB->CFSR & BFARVALID_MASK) != 0)
    {
        /* Bus Fault occured reading the address */
        is_valid = false;
    }

    /* Reenable BusFault by clearing  BFHFNMIGN */
    SCB->CCR &= ~SCB_CCR_BFHFNMIGN_Msk;
    __set_FAULTMASK(mask);

    return is_valid;
#else
    /* Cortex-M0 doesn't have BusFault so we need to catch HardFault */
    (void)address;
    
    /* R5 will be set to 0 by HardFault handler */
    /* to indicate HardFault has occured */
    register uint32_t result __asm("r5") = 1;

    __asm__ volatile (
        "ldr  r1, =0xDEADF00D   \n" /* set magic number     */
        "ldr  r2, =0xCAFEBABE   \n" /* 2nd magic to be sure */
        "ldrb r3, [r0]          \n" /* probe address        */
    );

    return result;
#endif
}
