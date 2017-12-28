;*******************************************************************************
;*
;* Copyright (c) 2003 Cypress Semiconductor
;*
;*
;* This program is free software; you can redistribute it and/or modify
;* it under the terms of the GNU General Public License as published by
;* the Free Software Foundation; either version 2 of the License, or
;* (at your option) any later version.
;*
;* This program is distributed in the hope that it will be useful,
;* but WITHOUT ANY WARRANTY; without even the implied warranty of
;* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;* GNU General Public License for more details.
;*
;* You should have received a copy of the GNU General Public License along 
;* with this program; if not, write to the Free Software Foundation, Inc., 
;* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
;*
;****************************************************
;*  DE1 SOF and standard (setfeature filter) handler for OTG
;****************************************************

.nolist
.list

; Definitions
.equ USB1_SOF_ISR_INT,  21
.equ USB2_SOF_ISR_INT,  29
.equ SUSB1_STD_INT,     0x0053
.equ SUSB1_FINISH_INT,  0x0059
.equ HW_SWAP_REG,       77
.equ HW_REST_REG,       78

.equ USB1_SOF_ISR_LOC,  (USB1_SOF_ISR_INT*2)
.equ USB2_SOF_ISR_LOC,  (USB2_SOF_ISR_INT*2)
.equ SUSB1_STD_INT_LOC,	(SUSB1_STD_INT*2)

.equ DEV1_STAT_REG,     0xc090
.equ DEV2_STAT_REG,     0xc0b0

.equ bmISR_SOF,         0x200
.equ SUSB_SOF_MSG,      0x200
.equ SUSB_SUS_MSG,      0x800
.equ SUSB_ID_MSG,       0x4000

; The two following lines are XROM specific 
;   locations, they are different for IROM.
.equ mbx_msg1,          0x404
.equ mbx_msg2,          0x454





.text

; Dummy header for alignment
.short  0xc3b6
.short  4
.byte   0
.short  0xe000
.short  0

; Code header with entry
.short  0xc3b6
.short  (code_end - code_start + 2)
.byte   0
.short  code_start
    

; Entry/storage


.global code_start
code_start:
    jmp start


; Local variables

Orig_SUSB1_Std_Isr: .short 0


; Installer
start:
    cli
    mov  [USB1_SOF_ISR_LOC], USB1_SOF_Isr
    mov  [USB2_SOF_ISR_LOC], USB2_SOF_Isr
; store original standard handler
	mov  [Orig_SUSB1_Std_Isr], [SUSB1_STD_INT_LOC]
    mov  [SUSB1_STD_INT_LOC], SUSB1_Std_Isr
    sti
    ret

; Our standard handler
; The BIOS sends a message when any HNP setfeature command is received.  However the BIOS
; does not provide any mechanism to determine which HNP setfeature command occurred.  This
; information is required to correctly maintain the OTG state information.
; This handler filters out the setfeature(a_hnp_support) and setfeature(a_alt_hnp_support)
; and passes all other commands to the the original handler.
SUSB1_Std_Isr:                      ; process standard command pointed to by r8
    push r8
    mov  r1, b[r8++]                ; bmRequest
	cmp  r1, 0
	jne  orig_handler
    mov  r1, b[r8++]                ; bRequest, r8->wValue
    cmp  r1, 3                      ; 3 = set_feature
    jne  orig_handler
    cmp  [r8], 4                    ; 4 = a_hnp_support
    je   0f
    cmp  [r8], 5                    ; 5 = a_alt_hnp_support
    je   0f
    jne  orig_handler
0:  pop  r8
    int  SUSB1_FINISH_INT           ; do SIE1 FINISH INT
    ret

orig_handler:
    pop  r8
	jmp  [Orig_SUSB1_Std_Isr];

; Our SOF handler
USB2_SOF_Isr:
    int  HW_SWAP_REG
    mov  r9, DEV2_STAT_REG   ; SIE2 registers
    mov  r10, mbx_msg2
    jmp  msfc

USB1_SOF_Isr:
    int  HW_SWAP_REG
    mov  r9, DEV1_STAT_REG   ; SIE1 registers
    mov  r10, mbx_msg1

msfc: 
    mov  [r9++], bmISR_SOF   ; clr int
    mov  r0, [r9]
    subi r9, 6               ; 0xc08c or 0xc0ac
    and  r0, 0xf000
    test r0, 0x8000          ; check SOF time-out
    je   sof

se0:
    subi r9, 2               ; 0xc08a or 0xc0aa
    mov  r1, [r9++]          ; 0xc08c or 0xc0ac  
    mov  r2, r1              ; check for SE1
    shr  r2, 1
    xor  r1, r2
    test r1, 0x1000
    jne  0f

    and  [r9], ~0x800        ; !SE1, reset timeout
    or   [r9], 0x800
    jmp  exit

0:
    cmp  r0, 0xf000
    jne  exit

    and  [r9], ~0x800        ; disable time-out
    mov  [r10], SUSB_SUS_MSG ; send message to co-proc
    jmp  exit

sof: 
    test [r9], 0x800         ; check time-out enable
    jnz  exit
    mov  [r10], SUSB_SOF_MSG ; set active flag
    or   [r9], 0x800         ; enable time-out

exit:
    int  HW_REST_REG         ; restore regs+sti+ret




.text
; End of handler code
code_end:
.short  0xc3b6
.short  2
.byte   4
.short  code_start
.byte   0   ; alignment
.short  0   ; end scan

