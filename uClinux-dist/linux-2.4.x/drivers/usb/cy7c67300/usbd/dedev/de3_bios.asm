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
;*  DE3 Standard handler for the Get_Report request.
;****************************************************

.nolist
.list

; Definitions
.equ SUSB2_standard_int,    99
.equ SUSB2_standard_loc,    (SUSB2_standard_int*2)
.equ SUSB2_SEND_INT,        96
.equ bRequest,              1
.equ wValue,                3   ; LSB offset

; Entry/storage
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
    

.global code_start
code_start:
    jmp start

chain:  
.short  0

link:   
.short  0, 0, 0, 0

keyboard_report:
.byte  0x05    ;Generic Desktop
.byte  0x01
.byte  0x09    ;Keyboard
.byte  0x06
.byte  0xa1    ;Collection
.byte  0x01
.byte  0x75
.byte  0x01
.byte  0x95
.byte  0x08
.byte  0x05    ;Usage page
.byte  0x07
.byte  0x19
.byte  0xe0
.byte  0x29
.byte  0xe7
.byte  0x15
.byte  0x00
.byte  0x25
.byte  0x01
.byte  0x81    ;Input(data,val,abs)
.byte  0x02
.byte  0x95
.byte  0x01
.byte  0x75
.byte  0x08
.byte  0x81    ;Input (constant)
.byte  0x01
.byte  0x95
.byte  0x05
.byte  0x75
.byte  0x01
.byte  0x05    ;usage page (LEDS)
.byte  0x08
.byte  0x19
.byte  0x01
.byte  0x29
.byte  0x05
.byte  0x91    ;Output(dat,var,abs)
.byte  0x02
.byte  0x95
.byte  0x01
.byte  0x75
.byte  0x03
.byte  0x91    ;Output(constant)
.byte  0x01
.byte  0x95
.byte  0x06
.byte  0x75
.byte  0x08
.byte  0x15
.byte  0x00
.byte  0x25
.byte  0x65
.byte  0x05
.byte  0x07
.byte  0x19
.byte  0x00
.byte  0x29
.byte  0xff
.byte  0x81    ;Input(data, array)
.byte  0x00
.byte  0xc0
keyboard_report_end:
.byte  0x00    ; padding


; Installer
start:
    mov [chain], [SUSB2_standard_loc]
    mov r0, std_handler
    mov [SUSB2_standard_loc], r0
    ret


; Our handler
std_handler:
    mov r11, r8             ;save r8 for chain

    cmp b[r8+bRequest], 6   ; check for get descriptor
    jne chain_on

    cmp b[r8+wValue], 0x22  ; check for report descriptor
    jne chain_on

send_report: 
    xor r1,r1               ; EP0
    mov r8, link            ; link structure for send
    mov [r8], 0
    mov [r8+2], keyboard_report
    mov [r8+4], (keyboard_report_end - keyboard_report)
    mov [r8+6], 0
    int SUSB2_SEND_INT
    ret

chain_on:
    mov r8, r11     ;restore r8 for chain
    jmp [chain]


; End of handler code
code_end:
.short  0xc3b6
.short  2
.byte   4
.short  code_start
.byte   0   ; alignment
.short  0   ; end scan

