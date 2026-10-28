/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CORE_CANDIDATE_RTOS_H
#define CORE_CANDIDATE_RTOS_H
#include <stdint.h>
/* OS1.54 audited RTOS ABI, isolated for a future core export proposal.
 * Native create writes through offset0x50; scheduler register save ends0x48.
 * Signal is bounded and IRQ-safe (the native ISR primitive); wait is TASK ONLY.
 * No hardware timer, interrupt vector, boot site or OS scheduling rate changes. */
struct core_rtos_task { uint32_t words[21]; };
struct core_rtos_sem { int32_t count; void *waiter; };
int fw_rtos_task_create(struct core_rtos_task *,void (*)(void),uint32_t,void *,uint32_t);
void fw_rtos_task_start(struct core_rtos_task *);
void fw_rtos_sem_init(struct core_rtos_sem *,uint32_t);
void fw_rtos_sem_wait(struct core_rtos_sem *);
void fw_rtos_sem_signal(struct core_rtos_sem *);
#endif
