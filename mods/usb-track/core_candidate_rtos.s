| SPDX-License-Identifier: GPL-2.0-or-later
| Generic firmware exports; OS addresses supplied by the release manifest.
| Separated from USB1 so these can move into core without feature code.
        .globl fw_rtos_task_create, fw_rtos_task_start
        .globl fw_rtos_sem_init, fw_rtos_sem_wait, fw_rtos_sem_signal
        .set fw_rtos_task_create, CORE_RTOS_TASK_CREATE
        .set fw_rtos_task_start, CORE_RTOS_TASK_START
        .set fw_rtos_sem_init, CORE_RTOS_SEM_INIT
        .set fw_rtos_sem_wait, CORE_RTOS_SEM_WAIT
        .set fw_rtos_sem_signal, CORE_RTOS_SEM_SIGNAL
