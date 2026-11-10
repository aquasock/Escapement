/*
 * include/asm-xtensa/irq.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_IRQ_H
#define _XTENSA_IRQ_H

#include <linux/init.h>
#include <asm/core.h>

#ifdef CONFIG_PLATFORM_NR_IRQS
# define PLATFORM_NR_IRQS CONFIG_PLATFORM_NR_IRQS
#else
# define PLATFORM_NR_IRQS 0
#endif
#define XTENSA_NR_IRQS XCHAL_NUM_INTERRUPTS
/*
 * EXTRA_NR_IRQS: room for IRQs that the platform's own controllers map.
 *
 * Upstream sizes the descriptor table at the number of lines the CPU's
 * interrupt controller has -- 32 on this core -- plus one. That is correct for
 * a machine whose peripherals hang directly off those lines. It is not correct
 * for a board like this one, where the SoC GPIO controller is itself an
 * interrupt controller and maps one Linux IRQ per GPIO line a driver asks for,
 * and where the PMU puts a whole virtual domain on top of one of those lines.
 *
 * With the upstream arithmetic the table is 33 entries. Measured on this
 * board, that is not enough: by the time the fourth driver asks for an
 * interrupt, irq_create_mapping() fails with ENOSPC, gpiod_to_irq() turns that
 * into -ENXIO, and the driver quietly falls back to polling. The symptom is
 * indistinguishable from "this board's GPIO interrupts do not work", and it
 * hid for a whole session -- the RTC and PMU lines mapped only because they
 * probed while slots were still free.
 *
 * 128 is headroom rather than a measured figure. CONFIG_SPARSE_IRQ would size
 * the table dynamically, but it is not selectable on xtensa (it needs
 * MAY_HAVE_SPARSE_IRQ), which leaves this. The cost is a static array of
 * irq_desc: about 24 KiB of .bss on this board, against 7.8 MiB of RAM.
 *
 * See KRN-008.
 */
#define EXTRA_NR_IRQS 128
#define NR_IRQS (XTENSA_NR_IRQS + PLATFORM_NR_IRQS + EXTRA_NR_IRQS + 1)
#define XTENSA_PIC_LINUX_IRQ(hwirq) ((hwirq) + 1)

static __inline__ int irq_canonicalize(int irq)
{
	return (irq);
}

struct irqaction;
struct irq_domain;

void migrate_irqs(void);
int xtensa_irq_domain_xlate(const u32 *intspec, unsigned int intsize,
		unsigned long int_irq, unsigned long ext_irq,
		unsigned long *out_hwirq, unsigned int *out_type);
int xtensa_irq_map(struct irq_domain *d, unsigned int irq, irq_hw_number_t hw);
unsigned xtensa_map_ext_irq(unsigned ext_irq);
unsigned xtensa_get_ext_irq_no(unsigned irq);

#endif	/* _XTENSA_IRQ_H */
