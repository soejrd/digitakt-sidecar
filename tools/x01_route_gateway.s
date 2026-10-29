/* SPDX-License-Identifier: GPL-2.0-or-later */
/* In-memory emulator-only gateway at I02's verified OS 1.54 pre-filter site.
 * Save all integer registers/SR; call the actual candidate C helper; replay
 * the displaced LEA. This bypasses machine selection/session gating to test
 * insertion order and native downstream processing, not the control link. */
    .text
    .global x01_gateway
x01_gateway:
    lea -64(%sp),%sp
    movem.l %d0-%d7/%a0-%a6,(%sp)
    move.w %sr,%d0
    move.l %d0,60(%sp)
    lea -20(%sp),%sp
    move.l #0x80001a18,(%sp)
    move.l #0x80001f60,%d0
    move.l %d0,4(%sp)
    move.l 0x4199f134,%d0
    move.l %d0,8(%sp)
    moveq #1,%d0
    move.l %d0,12(%sp)
    move.l #x01_map,%d0
    move.l %d0,16(%sp)
    jsr sc_return_lanes
    lea 20(%sp),%sp
    move.l 60(%sp),%d0
    move.w %d0,%sr
    movem.l (%sp),%d0-%d7/%a0-%a6
    lea 64(%sp),%sp
    lea 0x4199f444,%a4
    rts
    .section .rodata
x01_map:
    .byte 0,1
