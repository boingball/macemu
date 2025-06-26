/*
 * asm_support.s - AmigaOS utility functions in assembly language (GAS version)
 *
 * Basilisk II (C) 1997-2001 Christian Bauer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Amiga system constants - converted from includes */
#define MEMF_PUBLIC     0x00000001
#define SIGBREAKF_CTRL_C 0x00001000
#define SIGB_DOS        8
#define TC_SIZE         92
#define TC_SIGWAIT      22
#define TC_SIGRECVD     18

        .cpu 68020

        .global _AtomicAnd
        .global _AtomicOr
        .global _MoveVBR
        .global _DisableSuperBypass
        .global _Execute68k
        .global _Execute68kTrap
        .global _TrapHandlerAsm
        .global _ExceptionHandlerAsm
        .global _AsmTriggerNMI

        .extern _OldTrapHandler
        .extern _OldExceptionHandler
        .extern _IllInstrHandler
        .extern _PrivViolHandler
        .extern _EmulatedSR
        .extern _IRQSigMask
        .extern _InterruptFlags
        .extern _MainTask
        .extern _SysBase
        .extern _quit_emulator

        .section .text

subSysName:     .byte   '+',0

/*
 * Atomic bit operations (don't trust the compiler)
 */

_AtomicAnd:     move.l  4(%sp),%a0
                move.l  8(%sp),%d0
                and.l   %d0,(%a0)
                rts

_AtomicOr:      move.l  4(%sp),%a0
                move.l  8(%sp),%d0
                or.l    %d0,(%a0)
                rts

/*
 * Move VBR away from 0 if neccessary
 */

_MoveVBR:       movem.l %d0-%d1/%a0-%a1/%a5-%a6,-(%sp)
                move.l  _SysBase,%a6

                lea     getvbr,%a5              # VBR at 0?
                jsr     -30(%a6)                # Supervisor
                tst.l   %d0
                bne.s   1f

                move.l  #0x400,%d0              # Yes, allocate memory for new table
                move.l  #MEMF_PUBLIC,%d1
                jsr     -198(%a6)               # AllocMem
                tst.l   %d0
                beq.s   1f

                jsr     -120(%a6)               # Disable

                move.l  %d0,%a5                 # Copy old table
                move.l  %d0,%a1
                sub.l   %a0,%a0
                move.l  #0x400,%d0
                jsr     -624(%a6)               # CopyMem
                jsr     -636(%a6)               # CacheClearU

                move.l  %a5,%d0                 # Set VBR
                lea     setvbr,%a5
                jsr     -30(%a6)                # Supervisor

                jsr     -126(%a6)               # Enable

1:              movem.l (%sp)+,%d0-%d1/%a0-%a1/%a5-%a6
                rts

getvbr:         movec   %vbr,%d0
                rte

setvbr:         movec   %d0,%vbr
                rte

/*
 * Disable 68060 Super Bypass mode
 */

_DisableSuperBypass:
                movem.l %d0-%d1/%a0-%a1/%a5-%a6,-(%sp)
                move.l  _SysBase,%a6

                lea     dissb,%a5
                jsr     -30(%a6)                # Supervisor

                movem.l (%sp)+,%d0-%d1/%a0-%a1/%a5-%a6
                rts

                .cpu 68060

dissb:          movec   %pcr,%d0
                bset    #5,%d0
                movec   %d0,%pcr
                rte

                .cpu 68020

/*
 * Execute 68k subroutine (must be ended with rts)
 * r->a[7] and r->sr are unused!
 */

/* void Execute68k(uint32 addr, M68kRegisters *r); */
_Execute68k:
                move.l  4(%sp),%d0              # Get arguments
                move.l  8(%sp),%a0

                movem.l %d2-%d7/%a2-%a6,-(%sp)  # Save registers

                move.l  %a0,-(%sp)              # Push pointer to M68kRegisters on stack
                pea     1f                      # Push return address on stack
                move.l  %d0,-(%sp)              # Push pointer to 68k routine on stack
                movem.l (%a0),%d0-%d7/%a0-%a6   # Load registers from M68kRegisters

                rts                             # Jump into 68k routine

1:              move.l  %a6,-(%sp)              # Save a6
                move.l  4(%sp),%a6              # Get pointer to M68kRegisters
                movem.l %d0-%d7/%a0-%a5,(%a6)   # Save d0-d7/a0-a5 to M68kRegisters
                move.l  (%sp)+,56(%a6)          # Save a6 to M68kRegisters
                addq.l  #4,%sp                  # Remove pointer from stack

                movem.l (%sp)+,%d2-%d7/%a2-%a6  # Restore registers
                rts

/*
 * Execute MacOS 68k trap
 * r->a[7] and r->sr are unused!
 */

/* void Execute68kTrap(uint16 trap, M68kRegisters *r); */
_Execute68kTrap:
                move.l  4(%sp),%d0              # Get arguments
                move.l  8(%sp),%a0

                movem.l %d2-%d7/%a2-%a6,-(%sp)  # Save registers

                move.l  %a0,-(%sp)              # Push pointer to M68kRegisters on stack
                move.w  %d0,-(%sp)              # Push trap word on stack
                subq.l  #8,%sp                  # Create fake A-Line exception frame
                movem.l (%a0),%d0-%d7/%a0-%a6   # Load registers from M68kRegisters

                move.l  %a2,-(%sp)              # Save a2 and d2
                move.l  %d2,-(%sp)
                lea     1f,%a2                  # a2 points to return address
                move.w  16(%sp),%d2             # Load trap word into d2

                move.l  0x28,%a0                # Get MacOS A-Line handler address  
                jmp     (%a0)                   # Jump to MacOS A-Line handler

1:              move.l  %a6,-(%sp)              # Save a6
                move.l  6(%sp),%a6              # Get pointer to M68kRegisters
                movem.l %d0-%d7/%a0-%a5,(%a6)   # Save d0-d7/a0-a5 to M68kRegisters
                move.l  (%sp)+,56(%a6)          # Save a6 to M68kRegisters
                addq.l  #6,%sp                  # Remove pointer and trap word from stack

                movem.l (%sp)+,%d2-%d7/%a2-%a6  # Restore registers
                rts

/*
 * Exception handler of main task (for interrupts)
 */

_ExceptionHandlerAsm:
                move.l  %d0,-(%sp)              # Save d0

                and.l   #SIGBREAKF_CTRL_C,%d0   # CTRL-C?
                bne.s   2f

                move.w  _EmulatedSR,%d0         # Interrupts enabled in emulated SR?
                and.w   #0x0700,%d0
                bne     1f
                move.w  #0x0064,-(%sp)          # Yes, fake interrupt stack frame
                pea     1f
                move.w  _EmulatedSR,%d0
                move.w  %d0,-(%sp)
                or.w    #0x2100,%d0             # Set interrupt level in SR, enter (virtual) supervisor mode
                move.w  %d0,_EmulatedSR
                move.l  0x64,-(%sp)             # Jump to MacOS interrupt handler
                rts

1:              move.l  (%sp)+,%d0              # Restore d0
                rts

2:              jsr     -132(%a6)               # Forbid
                sub.l   %a1,%a1
                jsr     -294(%a6)               # FindTask
                move.l  %d0,%a0
                move.l  TC_SIGWAIT(%a0),%d0
                move.l  TC_SIGRECVD(%a0),%d1
                jsr     -138(%a6)               # Permit
                btst    #SIGB_DOS,%d0
                beq     3f
                btst    #SIGB_DOS,%d1
                bne     4f

3:              lea     TC_SIZE(%a0),%a0        # No, remove pending Dos packets
                jsr     -372(%a6)               # GetMsg

                move.w  _EmulatedSR,%d0
                or.w    #0x0700,%d0             # Disable all interrupts
                move.w  %d0,_EmulatedSR
                moveq   #0,%d0                  # Disable all exception signals
                moveq   #-1,%d1
                jsr     -168(%a6)               # SetExcept
                jsr     _quit_emulator          # CTRL-C, quit emulator
4:              move.l  (%sp)+,%d0
                rts

/*
 * Trap handler of main task
 */

_TrapHandlerAsm:
                cmp.l   #4,(%sp)                # Illegal instruction?
                beq.s   doillinstr
                cmp.l   #10,(%sp)               # A-Line exception?
                beq.s   doaline
                cmp.l   #8,(%sp)                # Privilege violation?
                beq.s   doprivviol

                cmp.l   #9,(%sp)                # Trace?
                beq     dotrace
                cmp.l   #3,(%sp)                # Illegal Address?
                beq.s   doilladdr
                cmp.l   #11,(%sp)               # F-Line exception
                beq.s   dofline

                cmp.l   #32,(%sp)
                blt     1f
                cmp.l   #47,(%sp)
                ble     doTrapXX                # Vector 32-47 : TRAP #0 - 15 Instruction Vectors

1:
                cmp.l   #48,(%sp)
                blt     2f
                cmp.l   #55,(%sp)
                ble     doTrapFPU
2:
                move.l  _OldTrapHandler,-(%sp)  # No, jump to old trap handler
                rts

/*
 * TRAP #0 - 15 Instruction Vectors
 */

doTrapXX:
                movem.l %a0/%d0,-(%sp)          # Save a0,d0
                move.l  (2*4,%sp),%d0           # vector number 32-47

                move.l  %usp,%a0                # Get user stack pointer
                move.l  (4*4,%sp),-(%a0)        # Copy 4-word stack frame to user stack
                move.l  (3*4,%sp),-(%a0)
                move.l  %a0,%usp                # Update USP
                or.w    #0x2000,(%a0)           # set Supervisor bit in SR

                lsl.l   #2,%d0                  # convert vector number to vector offset
                move.l  %d0,%a0
                move.l  (%a0),%d0               # get mac trap vector

                move.l  %usp,%a0                # Get user stack pointer
                move.l  %d0,-(%a0)              # store vector offset as return address
                move.l  %a0,%usp                # Update USP

                movem.l (%sp)+,%a0/%d0          # Restore a0,d0
                addq.l  #(4*2),%sp              # Remove exception frame from supervisor stack

                andi    #0xd8ff,%sr             # Switch to user mode, enable interrupts
                rts

/*
 * FPU Exception Instruction Vectors
 */

doTrapFPU:
                move.l  %d0,(%sp)
                fmove.l %fpcr,%d0
                and.w   #0x00ff,%d0             # disable FPU exceptions
                fmove.l %d0,%fpcr
                move.l  (%sp)+,%d0              # Restore d0
                rte

/*
 * trace Vector
 */

dotrace:
                move.l  %a0,(%sp)               # Save a0
                move.l  %usp,%a0                # Get user stack pointer

                move.l  (3*4,%sp),-(%a0)        # Copy 6-word stack frame to user stack
                move.l  (2*4,%sp),-(%a0)
                move.l  (1*4,%sp),-(%a0)
                move.l  %a0,%usp                # Update USP
                or.w    #0x2000,(%a0)           # set Supervisor bit in SR
                move.l  (%sp)+,%a0              # Restore a0

                lea     (6*2,%sp),%sp           # Remove exception frame from supervisor stack
                andi    #0x18ff,%sr             # Switch to user mode, enable interrupts, disable trace

                move.l  0x24,-(%sp)             # Jump to MacOS exception handler
                rts

/*
 * A-Line handler: call MacOS A-Line handler
 */

doaline:        move.l  %a0,(%sp)               # Save a0
                move.l  %usp,%a0                # Get user stack pointer
                move.l  8(%sp),-(%a0)           # Copy stack frame to user stack
                move.l  4(%sp),-(%a0)
                move.l  %a0,%usp                # Update USP

                or.w    #0x2000,(%a0)           # set Supervisor bit in SR
                move.l  (%sp)+,%a0              # Restore a0

                addq.l  #8,%sp                  # Remove exception frame from supervisor stack
                andi    #0xd8ff,%sr             # Switch to user mode, enable interrupts

                move.l  0x28,-(%sp)             # Jump to MacOS exception handler
                rts

/*
 * F-Line handler: call F-Line exception handler
 */

dofline:        move.l  %a0,(%sp)               # Save a0
                move.l  %usp,%a0                # Get user stack pointer
                move.l  8(%sp),-(%a0)           # Copy stack frame to user stack
                move.l  4(%sp),-(%a0)
                move.l  %a0,%usp                # Update USP
                or.w    #0x2000,(%a0)           # set Supervisor bit in SR
                move.l  (%sp)+,%a0              # Restore a0

                addq.l  #8,%sp                  # Remove exception frame from supervisor stack
                andi    #0xd8ff,%sr             # Switch to user mode, enable interrupts

                and.w   #0xf8ff,_EmulatedSR     # enable interrupts in EmulatedSR

                move.l  0x2c,-(%sp)             # Jump to MacOS exception handler
                rts

/*
 * Illegal address handler
 */

doilladdr:
                move.l  %a0,(%sp)               # Save a0

                move.l  %usp,%a0                # Get user stack pointer
                move.l  (3*4,%sp),-(%a0)        # Copy 6-word stack frame to user stack
                move.l  (2*4,%sp),-(%a0)
                move.l  (1*4,%sp),-(%a0)
                move.l  %a0,%usp                # Update USP
                or.w    #0x2000,(%a0)           # set Supervisor bit in SR
                move.l  (%sp)+,%a0              # Restore a0

                lea     (6*2,%sp),%sp           # Remove exception frame from supervisor stack
                andi    #0xd8ff,%sr             # Switch to user mode, enable interrupts

                move.l  0x0c,-(%sp)             # Jump to MacOS exception handler
                rts

/*
 * Illegal instruction handler: call IllInstrHandler() (which calls EmulOp())
 *   to execute extended opcodes (see emul_op.h)
 */

doillinstr:                     movem.l %a0/%d0,-(%sp)
                move.w  10(%sp),%d0             # Get instruction word from exception frame
                and.w   #0xff00,%d0
                cmp.w   #0x7100,%d0

                movem.l (%sp)+,%a0/%d0
                beq     1f

                move.l  %a0,(%sp)               # Save a0
                move.l  %usp,%a0                # Get user stack pointer
                move.l  8(%sp),-(%a0)           # Copy stack frame to user stack
                move.l  4(%sp),-(%a0)
                move.l  %a0,%usp                # Update USP
                or.w    #0x2000,(%a0)           # set Supervisor bit in SR
                move.l  (%sp)+,%a0              # Restore a0

                add.w   #(3*4),%sp              # Remove exception frame from supervisor stack
                andi    #0xd8ff,%sr             # Switch to user mode, enable interrupts

                move.l  0x10,-(%sp)             # Jump to MacOS exception handler
                rts

1:
                move.l  %a6,(%sp)               # Save a6
                move.l  %usp,%a6                # Get user stack pointer

                move.l  %a6,-10(%a6)            # Push USP (a7)
                move.l  6(%sp),-(%a6)           # Push PC
                move.w  4(%sp),-(%a6)           # Push SR
                subq.l  #4,%a6                  # Skip saved USP
                move.l  (%sp),-(%a6)            # Push old a6
                movem.l %d0-%d7/%a0-%a5,-(%a6)  # Push remaining registers
                move.l  %a6,%usp                # Update USP

                add.w   #12,%sp                 # Remove exception frame from supervisor stack
                andi    #0xd8ff,%sr             # Switch to user mode, enable interrupts

                move.l  %a6,-(%sp)              # Jump to IllInstrHandler() in main.cpp
                jsr     _IllInstrHandler
                addq.l  #4,%sp

                movem.l (%sp)+,%d0-%d7/%a0-%a6  # Restore registers
                addq.l  #4,%sp                  # Skip saved USP (!!)
                rtr                             # Return from exception

/*
 * Privilege violation handler: MacOS runs in supervisor mode,
 *   so we have to emulate certain privileged instructions
 */

doprivviol:     move.l  %d0,(%sp)               # Save d0
                move.w  6(%sp),%d0              # Get instruction word from exception frame

                cmp.w   #0x40e7,%d0             # move sr,-(sp)?
                beq     pushsr
                cmp.w   #0x46df,%d0             # move (sp)+,sr?
                beq     popsr

                cmp.w   #0x007c,%d0             # ori #xxxx,sr?
                beq     orisr
                cmp.w   #0x027c,%d0             # andi #xxxx,sr?
                beq     andisr

                cmp.w   #0x46fc,%d0             # move #xxxx,sr?
                beq     movetosrimm

                cmp.w   #0x46ef,%d0             # move (xxxx,sp),sr?
                beq     movetosrsprel
                cmp.w   #0x46d8,%d0             # move (a0)+,sr?
                beq     movetosra0p
                cmp.w   #0x46d9,%d0             # move (a1)+,sr?
                beq     movetosra1p

                cmp.w   #0x40f8,%d0             # move sr,xxxx.w?
                beq     movefromsrabs
                cmp.w   #0x40d0,%d0             # move sr,(a0)?
                beq     movefromsra0
                cmp.w   #0x40d7,%d0             # move sr,(sp)?
                beq     movefromsrsp

                cmp.w   #0xf327,%d0             # fsave -(sp)?
                beq     fsavepush
                cmp.w   #0xf35f,%d0             # frestore (sp)+?
                beq     frestorepop
                cmp.w   #0xf32d,%d0             # fsave xxx(a5) ?
                beq     fsavea5
                cmp.w   #0xf36d,%d0             # frestore xxx(a5) ?
                beq     frestorea5

                cmp.w   #0x4e73,%d0             # rte?
                beq     pvrte

                cmp.w   #0x40c0,%d0             # move sr,d0?
                beq     movefromsrd0
                cmp.w   #0x40c1,%d0             # move sr,d1?
                beq     movefromsrd1
                cmp.w   #0x40c2,%d0             # move sr,d2?
                beq     movefromsrd2
                cmp.w   #0x40c3,%d0             # move sr,d3?
                beq     movefromsrd3
                cmp.w   #0x40c4,%d0             # move sr,d4?
                beq     movefromsrd4
                cmp.w   #0x40c5,%d0             # move sr,d5?
                beq     movefromsrd5
                cmp.w   #0x40c6,%d0             # move sr,d6?
                beq     movefromsrd6
                cmp.w   #0x40c7,%d0             # move sr,d7?
                beq     movefromsrd7

                cmp.w   #0x46c0,%d0             # move d0,sr?
                beq     movetosrd0
                cmp.w   #0x46c1,%d0             # move d1,sr?
                beq     movetosrd1
                cmp.w   #0x46c2,%d0             # move d2,sr?
                beq     movetosrd2
                cmp.w   #0x46c3,%d0             # move d3,sr?
                beq     movetosrd3
                cmp.w   #0x46c4,%d0             # move d4,sr?
                beq     movetosrd4
                cmp.w   #0x46c5,%d0             # move d5,sr?
                beq     movetosrd5
                cmp.w   #0x46c6,%d0             # move d6,sr?
                beq     movetosrd6
                cmp.w   #0x46c7,%d0             # move d7,sr?
                beq     movetosrd7

                cmp.w   #0x4e7a,%d0             # movec cr,x?
                beq     movecfromcr
                cmp.w   #0x4e7b,%d0             # movec x,cr?
                beq     movectocr

                cmp.w   #0xf478,%d0             # cpusha dc?
                beq     cpushadc
                cmp.w   #0xf4f8,%d0             # cpusha dc/ic?
                beq     cpushadcic

                cmp.w   #0x4e69,%d0             # move usp,a1
                beq     moveuspa1
                cmp.w   #0x4e68,%d0             # move usp,a0
                beq     moveuspa0

                cmp.w   #0x4e61,%d0             # move a1,usp
                beq     moved1usp

pv_unhandled:   move.l  (%sp),%d0               # Unhandled instruction, jump to handler in main.cpp
                move.l  %a6,(%sp)               # Save a6
                move.l  %usp,%a6                # Get user stack pointer

                move.l  %a6,-10(%a6)            # Push USP (a7)
                move.l  6(%sp),-(%a6)           # Push PC
                move.w  4(%sp),-(%a6)           # Push SR
                subq.l  #4,%a6                  # Skip saved USP
                move.l  (%sp),-(%a6)            # Push old a6
                movem.l %d0-%d7/%a0-%a5,-(%a6)  # Push remaining registers
                move.l  %a6,%usp                # Update USP

                add.w   #12,%sp                 # Remove exception frame from supervisor stack
                andi    #0xd8ff,%sr             # Switch to user mode, enable interrupts

                move.l  %a6,-(%sp)              # Jump to PrivViolHandler() in main.cpp
                jsr     _PrivViolHandler
                addq.l  #4,%sp

                movem.l (%sp)+,%d0-%d7/%a0-%a6  # Restore registers
                addq.l  #4,%sp                  # Skip saved USP
                rtr                             # Return from exception

/* move sr,-(sp) */
pushsr:         move.l  %a0,-(%sp)              # Save a0
                move.l  %usp,%a0                # Get user stack pointer
                move.w  8(%sp),%d0              # Get CCR from exception stack frame
                or.w    _EmulatedSR,%d0         # Add emulated supervisor bits
                move.w  %d0,-(%a0)              # Store SR on user stack
                move.l  %a0,%usp                # Update USP
                move.l  (%sp)+,%a0              # Restore a0
                move.l  (%sp)+,%d0              # Restore d0
                addq.l  #2,%sp                  # Skip instruction
                rte

/* move (sp)+,sr */
popsr:          move.l  %a0,-(%sp)              # Save a0
                move.l  %usp,%a0                # Get user stack pointer
                move.w  (%a0)+,%d0              # Get SR from user stack
                move.w  %d0,8(%sp)              # Store into CCR on exception stack frame
                and.w   #0x00ff,8(%sp)
                and.w   #0xe700,%d0             # Extract supervisor bits
                move.w  %d0,_EmulatedSR         # And save them

                and.w   #0x0700,%d0             # Rethrow exception if interrupts are pending and reenabled
                bne     1f
                tst.l   _InterruptFlags
                beq     1f
                movem.l %d0-%d1/%a0-%a1/%a6,-(%sp)
                move.l  _SysBase,%a6
                move.l  _MainTask,%a1
                move.l  _IRQSigMask,%d0
                jsr     -324(%a6)               # Signal
                movem.l (%sp)+,%d0-%d1/%a0-%a1/%a6
1:              move.l  (%sp)+,%d0              # Restore d0
                addq.l  #4,2(%sp)               # Skip instruction by modifying return PC
                rte

/* move #xxxx,sr */
movetosrimm:    move.w  8(%sp),%d0              # Get immediate value
                bra.s   storesr4

/* move (xxxx,sp),sr */
movetosrsprel:  move.l  %a0,-(%sp)              # Save a0
                move.l  %usp,%a0                # Get user stack pointer
                move.w  8(%sp),%d0              # Get offset
                move.w  (%a0,%d0.w),%d0         # Read word
                move.l  (%sp)+,%a0              # Restore a0
                bra.s   storesr4

/* move (a0)+,sr */
movetosra0p:    move.w  (%a0)+,%d0              # Read word
                bra     storesr2

/* move (a1)+,sr */
movetosra1p:    move.w  (%a1)+,%d0              # Read word
                bra     storesr2

/* move sr,xxxx.w */
movefromsrabs:  move.l  %a0,-(%sp)              # Save a0
                move.w  8(%sp),%a0              # Get address
                move.w  8(%sp),%d0              # Get CCR
                or.w    _EmulatedSR,%d0         # Add emulated supervisor bits
                move.w  %d0,(%a0)               # Store SR
                move.l  (%sp)+,%a0              # Restore a0
                move.l  (%sp)+,%d0              # Restore d0
                addq.l  #4,%sp                  # Skip instruction
                rte

/* move sr,(a0) */
movefromsra0:   move.w  4(%sp),%d0              # Get CCR
                or.w    _EmulatedSR,%d0         # Add emulated supervisor bits
                move.w  %d0,(%a0)               # Store SR
                move.l  (%sp)+,%d0              # Restore d0
                addq.l  #2,%sp                  # Skip instruction
                rte

/* move sr,(sp) */
movefromsrsp:   move.l  %a0,-(%sp)              # Save a0
                move.l  %usp,%a0                # Get user stack pointer
                move.w  8(%sp),%d0              # Get CCR
                or.w    _EmulatedSR,%d0         # Add emulated supervisor bits
                move.w  %d0,(%a0)               # Store SR
                move.l  (%sp)+,%a0              # Restore a0
                move.l  (%sp)+,%d0              # Restore d0
                addq.l  #2,2(%sp)               # Skip instruction
                rte

/* fsave -(sp) */
fsavepush:      move.l  (%sp),%d0               # Restore d0
                move.l  %a0,(%sp)               # Save a0
                move.l  %usp,%a0                # Get user stack pointer
                move.l  #0x41000000,-(%a0)      # Push idle frame
                move.l  %a0,%usp                # Update USP
                move.l  (%sp)+,%a0              # Restore a0
                addq.l  #2,2(%sp)               # Skip instruction
                rte

/* fsave xxx(a5) */
fsavea5:        move.l  (%sp),%d0               # Restore d0
                move.l  %a0,(%sp)               # Save a0
                move.l  %a5,%a0                 # Get base register
                add.w   8(%sp),%a0              # Add offset to base register
                move.l  #0x41000000,(%a0)       # Push idle frame
                move.l  (%sp)+,%a0              # Restore a0
                addq.l  #4,2(%sp)               # Skip instruction
                rte

/* frestore (sp)+ */
frestorepop:    move.l  (%sp),%d0               # Restore d0
                move.l  %a0,(%sp)               # Save a0
                move.l  %usp,%a0                # Get user stack pointer
                addq.l  #4,%a0                  # Nothing to do...
                move.l  %a0,%usp                # Update USP
                move.l  (%sp)+,%a0              # Restore a0
                addq.l  #2,2(%sp)               # Skip instruction
                rte

/* frestore xxx(a5) */
frestorea5:     move.l  (%sp),%d0               # Restore d0
                move.l  %a0,(%sp)               # Save a0
                move.l  (%sp)+,%a0              # Restore a0
                addq.l  #4,2(%sp)               # Skip instruction
                rte

/* rte */
pvrte:          movem.l %a0/%a1,-(%sp)          # Save a0 and a1
                move.l  %usp,%a0                # Get user stack pointer

                move.w  (%a0)+,%d0              # Get SR from user stack
                move.w  %d0,12(%sp)             # Store into CCR on exception stack frame  
                and.w   #0xc0ff,12(%sp)
                and.w   #0xe700,%d0             # Extract supervisor bits
                move.w  %d0,_EmulatedSR         # And save them
                move.l  (%a0)+,14(%sp)          # Store return address in exception stack frame

                move.w  (%a0)+,%d0              # get format word
                lsr.w   #7,%d0                  # get stack frame Id 
                lsr.w   #4,%d0
                and.w   #0x001e,%d0
                move.w  (StackFormatTable,%pc,%d0.w),%d0 # get total stack frame length
                subq.w  #4,%d0                  # count only extra words
                lea     20(%sp),%a1             # destination address (in supervisor stack)
                bra     1f

2:              move.w  (%a0)+,(%a1)+           # copy additional stack words back to supervisor stack
1:              dbf     %d0,2b

                move.l  %a0,%usp                # Update USP
                movem.l (%sp)+,%a0/%a1          # Restore a0 and a1
                move.l  (%sp)+,%d0              # Restore d0
                rte

/* sizes of exceptions stack frames */
StackFormatTable:
                .word   4                       # Four-word stack frame, format $0
                .word   4                       # Throwaway four-word stack frame, format $1
                .word   6                       # Six-word stack frame, format $2
                .word   6                       # MC68040 floating-point post-instruction stack frame, format $3
                .word   8                       # MC68EC040 and MC68LC040 floating-point unimplemented stack frame, format $4
                .word   4                       # Format $5
                .word   4                       # Format $6
                .word   30                      # MC68040 access error stack frame, Format $7
                .word   29                      # MC68010 bus and address error stack frame, format $8
                .word   10                      # MC68020 and MC68030 coprocessor mid-instruction stack frame, format $9
                .word   16                      # MC68020 and MC68030 short bus cycle stack frame, format $a
                .word   46                      # MC68020 and MC68030 long bus cycle stack frame, format $b
                .word   12                      # CPU32 bus error for prefetches and operands stack frame, format $c
                .word   4                       # Format $d
                .word   4                       # Format $e
                .word   4                       # Format $f

/* move sr,dx */
movefromsrd0:   addq.l  #4,%sp                  # Skip saved d0
                moveq   #0,%d0
                move.w  (%sp),%d0               # Get CCR
                or.w    _EmulatedSR,%d0         # Add emulated supervisor bits
                addq.l  #2,2(%sp)               # Skip instruction
                rte

movefromsrd1:   move.l  (%sp)+,%d0
                moveq   #0,%d1
                move.w  (%sp),%d1
                or.w    _EmulatedSR,%d1
                addq.l  #2,2(%sp)
                rte

movefromsrd2:   move.l  (%sp)+,%d0
                moveq   #0,%d2
                move.w  (%sp),%d2
                or.w    _EmulatedSR,%d2
                addq.l  #2,2(%sp)
                rte

movefromsrd3:   move.l  (%sp)+,%d0
                moveq   #0,%d3
                move.w  (%sp),%d3
                or.w    _EmulatedSR,%d3
                addq.l  #2,2(%sp)
                rte

movefromsrd4:   move.l  (%sp)+,%d0
                moveq   #0,%d4
                move.w  (%sp),%d4
                or.w    _EmulatedSR,%d4
                addq.l  #2,2(%sp)
                rte

movefromsrd5:   move.l  (%sp)+,%d0
                moveq   #0,%d5
                move.w  (%sp),%d5
                or.w    _EmulatedSR,%d5
                addq.l  #2,2(%sp)
                rte

movefromsrd6:   move.l  (%sp)+,%d0
                moveq   #0,%d6
                move.w  (%sp),%d6
                or.w    _EmulatedSR,%d6
                addq.l  #2,2(%sp)
                rte

movefromsrd7:   move.l  (%sp)+,%d0
                moveq   #0,%d7
                move.w  (%sp),%d7
                or.w    _EmulatedSR,%d7
                addq.l  #2,2(%sp)
                rte

/* move dx,sr */
movetosrd0:     move.l  (%sp),%d0
storesr2:       move.w  %d0,4(%sp)
                and.w   #0x00ff,4(%sp)
                and.w   #0xe700,%d0
                move.w  %d0,_EmulatedSR

                and.w   #0x0700,%d0             # Rethrow exception if interrupts are pending and reenabled
                bne.s   1f
                tst.l   _InterruptFlags
                beq.s   1f
                movem.l %d0-%d1/%a0-%a1/%a6,-(%sp)
                move.l  _SysBase,%a6
                move.l  _MainTask,%a1
                move.l  _IRQSigMask,%d0
                jsr     -324(%a6)               # Signal
                movem.l (%sp)+,%d0-%d1/%a0-%a1/%a6
1:              move.l  (%sp)+,%d0
                addq.l  #2,2(%sp)
                rte

movetosrd1:     move.l  %d1,%d0
                bra.s   storesr2

movetosrd2:     move.l  %d2,%d0
                bra.s   storesr2

movetosrd3:     move.l  %d3,%d0
                bra.s   storesr2

movetosrd4:     move.l  %d4,%d0
                bra.s   storesr2

movetosrd5:     move.l  %d5,%d0
                bra.s   storesr2

movetosrd6:     move.l  %d6,%d0
                bra.s   storesr2

movetosrd7:     move.l  %d7,%d0
                bra.s   storesr2

/* movec cr,x */
movecfromcr:                    move.w  8(%sp),%d0              # Get next instruction word

                cmp.w   #0x8801,%d0             # movec vbr,a0?
                beq.s   movecvbra0
                cmp.w   #0x9801,%d0             # movec vbr,a1?
                beq.s   movecvbra1
                cmp.w   #0xA801,%d0             # movec vbr,a2?
                beq.s   movecvbra2
                cmp.w   #0x1801,%d0             # movec vbr,d1?
                beq     movecvbrd1
                cmp.w   #0x0002,%d0             # movec cacr,d0?
                beq.s   moveccacrd0
                cmp.w   #0x1002,%d0             # movec cacr,d1?
                beq.s   moveccacrd1
                cmp.w   #0x0003,%d0             # movec tc,d0?
                beq.s   movectcd0
                cmp.w   #0x1003,%d0             # movec tc,d1?
                beq.s   movectcd1
                cmp.w   #0x1000,%d0             # movec sfc,d1?
                beq     movecsfcd1
                cmp.w   #0x1001,%d0             # movec dfc,d1?
                beq     movecdfcd1
                cmp.w   #0x0806,%d0             # movec urp,d0?
                beq     movecurpd0
                cmp.w   #0x0807,%d0             # movec srp,d0?
                beq.s   movecsrpd0
                cmp.w   #0x0004,%d0             # movec itt0,d0
                beq.s   movecitt0d0
                cmp.w   #0x0005,%d0             # movec itt1,d0
                beq.s   movecitt1d0
                cmp.w   #0x0006,%d0             # movec dtt0,d0
                beq.s   movecdtt0d0
                cmp.w   #0x0007,%d0             # movec dtt1,d0
                beq.s   movecdtt1d0

                bra     pv_unhandled

/* movec cacr,d0 */
moveccacrd0:    move.l  (%sp)+,%d0
                move.l  #0x3111,%d0             # All caches and bursts on
                addq.l  #4,2(%sp)
                rte

/* movec cacr,d1 */
moveccacrd1:    move.l  (%sp)+,%d0
                move.l  #0x3111,%d1             # All caches and bursts on
                addq.l  #4,2(%sp)
                rte

/* movec vbr,a0 */
movecvbra0:     move.l  (%sp)+,%d0
                sub.l   %a0,%a0                 # VBR always appears to be at 0
                addq.l  #4,2(%sp)
                rte

/* movec vbr,a1 */
movecvbra1:     move.l  (%sp)+,%d0
                sub.l   %a1,%a1                 # VBR always appears to be at 0
                addq.l  #4,2(%sp)
                rte

/* movec vbr,a2 */
movecvbra2:     move.l  (%sp)+,%d0
                sub.l   %a2,%a2                 # VBR always appears to be at 0
                addq.l  #4,2(%sp)
                rte

/* movec vbr,d1 */
movecvbrd1:     move.l  (%sp)+,%d0
                moveq.l #0,%d1                  # VBR always appears to be at 0
                addq.l  #4,2(%sp)
                rte

/* movec tc,d0 */
movectcd0:      addq.l  #4,%sp
                moveq   #0,%d0                  # MMU is always off
                addq.l  #4,2(%sp)
                rte

/* movec tc,d1 */
movectcd1:      move.l  (%sp)+,%d0              # Restore d0
                moveq   #0,%d1                  # MMU is always off
                addq.l  #4,2(%sp)
                rte

/* movec sfc,d1 */
movecsfcd1:     move.l  (%sp)+,%d0              # Restore d0
                moveq   #0,%d1
                addq.l  #4,2(%sp)
                rte

/* movec dfc,d1 */
movecdfcd1:     move.l  (%sp)+,%d0              # Restore d0
                moveq   #0,%d1
                addq.l  #4,2(%sp)
                rte

movecurpd0:     /* movec urp,d0 */
movecsrpd0:     /* movec srp,d0 */
movecitt0d0:    /* movec itt0,d0 */
movecitt1d0:    /* movec itt1,d0 */
movecdtt0d0:    /* movec dtt0,d0 */
movecdtt1d0:    /* movec dtt1,d0 */
                addq.l  #4,%sp
                moveq.l #0,%d0                  # MMU is always off
                addq.l  #4,2(%sp)               # skip instruction
                rte

/* movec x,cr */
movectocr:                      move.w  8(%sp),%d0              # Get next instruction word

                cmp.w   #0x0801,%d0             # movec d0,vbr?
                beq.s   movectovbr
                cmp.w   #0x1801,%d0             # movec d1,vbr?
                beq.s   movectovbr
                cmp.w   #0xA801,%d0             # movec a2,vbr?
                beq.s   movectovbr
                cmp.w   #0x0002,%d0             # movec d0,cacr?
                beq.s   movectocacr
                cmp.w   #0x1002,%d0             # movec d1,cacr?
                beq.s   movectocacr
                cmp.w   #0x1000,%d0             # movec d1,sfc?
                beq.s   movectoxfc
                cmp.w   #0x1001,%d0             # movec d1,dfc?
                beq.s   movectoxfc

                bra     pv_unhandled

/* movec x,vbr */
movectovbr:     move.l  (%sp)+,%d0              # Ignore moves to VBR
                addq.l  #4,2(%sp)
                rte

/* movec dx,cacr */
movectocacr:    movem.l %d1/%a0-%a1/%a6,-(%sp)  # Move to CACR, clear caches
                move.l  _SysBase,%a6
                jsr     -636(%a6)               # CacheClearU
                movem.l (%sp)+,%d1/%a0-%a1/%a6
                move.l  (%sp)+,%d0
                addq.l  #4,2(%sp)
                rte

/* movec x,sfc */
/* movec x,dfc */
movectoxfc:     move.l  (%sp)+,%d0              # Ignore moves to SFC, DFC
                addq.l  #4,2(%sp)
                rte

/* cpusha */
cpushadc:
cpushadcic:
                movem.l %d1/%a0-%a1/%a6,-(%sp)  # Clear caches
                move.l  _SysBase,%a6
                jsr     -636(%a6)               # CacheClearU
                movem.l (%sp)+,%d1/%a0-%a1/%a6
                move.l  (%sp)+,%d0
                addq.l  #2,2(%sp)
                rte

/* move usp,a1 */
moveuspa1:      move.l  (%sp)+,%d0
                move    %usp,%a1
                addq.l  #2,2(%sp)
                rte

/* move usp,a0 */
moveuspa0:      move.l  (%sp)+,%d0
                move    %usp,%a0
                addq.l  #2,2(%sp)
                rte

/* move a1,usp */
moved1usp:      move.l  (%sp)+,%d0
                move    %a1,%usp
                addq.l  #2,2(%sp)
                rte

/*
 * Trigger NMI (Pop up debugger)
 */

_AsmTriggerNMI: move.l  %d0,-(%sp)              # Save d0
                move.w  #0x007c,-(%sp)          # Yes, fake NMI stack frame
                pea     1f
                move.w  _EmulatedSR,%d0
                and.w   #0xf8ff,%d0             # Set interrupt level in SR
                move.w  %d0,-(%sp)
                move.w  %d0,_EmulatedSR

                move.l  0x7c,-(%sp)             # Jump to MacOS NMI handler
                rts

1:              move.l  (%sp)+,%d0              # Restore d0
                rts

CopyTrapStack:
                movem.l %d0/%a0/%a1,-(%sp)

                move.w  26(%sp),%d0             # get format word (5*4+6 = 26)
                lsr.w   #7,%d0                  # get stack frame Id 
                lsr.w   #4,%d0
                and.w   #0x001e,%d0
                move.w  (StackFormatTable,%pc,%d0.w),%d0 # get total stack frame length

                lea     20(%sp),%a0             # get start of exception stack frame (5*4 = 20)
                move.l  %usp,%a1                # Get user stack pointer
                bra     1f

2:              move.w  (%a0)+,(%a1)+           # copy additional stack words back to supervisor stack
1:              dbf     %d0,2b

                move.l  12(%sp),-(%a0)          # copy return address to new top of stack (3*4 = 12)
                move.l  %a0,%sp
                rts_MainTask,%a1
                move.l  _IRQSigMask,%d0
                jsr     -324(%a6)               # Signal
                movem.l (%sp)+,%d0-%d1/%a0-%a1/%a6
1:
                move.l  %a0,%usp                # Update USP
                move.l  (%sp)+,%a0              # Restore a0
                move.l  (%sp)+,%d0              # Restore d0
                addq.l  #2,2(%sp)               # Skip instruction
                rte

/* ori #xxxx,sr */
orisr:          move.w  4(%sp),%d0              # Get CCR from stack
                or.w    _EmulatedSR,%d0         # Add emulated supervisor bits
                or.w    8(%sp),%d0              # Or with immediate value
                move.w  %d0,4(%sp)              # Store into CCR on stack
                and.w   #0x00ff,4(%sp)
                and.w   #0xe700,%d0             # Extract supervisor bits
                move.w  %d0,_EmulatedSR         # And save them
                move.l  (%sp)+,%d0              # Restore d0
                addq.l  #4,2(%sp)               # Skip instruction
                rte

/* andi #xxxx,sr */
andisr:         move.w  4(%sp),%d0              # Get CCR from stack
                or.w    _EmulatedSR,%d0         # Add emulated supervisor bits
                and.w   8(%sp),%d0              # And with immediate value
storesr4:       move.w  %d0,4(%sp)              # Store into CCR on stack
                and.w   #0x00ff,4(%sp)
                and.w   #0xe700,%d0             # Extract supervisor bits
                move.w  %d0,_EmulatedSR         # And save them

                and.w   #0x0700,%d0             # Rethrow exception if interrupts are pending and reenabled
                bne.s   1f
                tst.l   _InterruptFlags
                beq.s   1f
                movem.l %d0-%d1/%a0-%a1/%a6,-(%sp)
                move.l  _SysBase,%a6
                move.l