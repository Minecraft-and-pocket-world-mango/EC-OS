[BITS 16]

global kernel_entry
extern kernel_main
extern __bss_start
extern __bss_end

; TEMP diagnostic: write byte %1 to COM1
%macro SER 1
    mov dx, 0x3FD
%%w: in al, dx
    test al, 0x20
    jz %%w
    mov dx, 0x3F8
    mov al, %1
    out dx, al
%endmacro

kernel_entry:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; TEMP: init COM1 then mark entry
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8
    mov al, 0x01
    out dx, al
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x03
    out dx, al
    mov dx, 0x3FA
    mov al, 0xC7
    out dx, al
    mov dx, 0x3FC
    mov al, 0x0B
    out dx, al
    SER '1'

    ; Enable A20 before protected mode.  Without this, accesses above
    ; 1 MiB (e.g. the BSS clear) wrap around into low memory and destroy
    ; the running kernel.  Fast method (port 0x92).
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Build the GDTR on the stack and load it with a 32-bit address.
    ; (lgdt [gdt_desc] uses a 16-bit absolute reference in 16-bit mode,
    ;  which breaks once .data crosses the 64 KiB boundary.)
    sub sp, 6
    mov word [esp], gdt_end - gdt_start - 1
    mov dword [esp+2], gdt_start
    lgdt [esp]

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp dword 0x08:pm_entry

[BITS 32]
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x04000000        ; 64 MiB stack, above the ~6 MiB BSS image
    cld
    SER '2'

    ; Zero the BSS.  linker.ld places .bss above 1 MiB so this never
    ; touches the legacy VGA/BIOS hole (0xA0000-0xFFFFF).
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
    SER '3'

    call kernel_main

.halt:
    jmp .halt

; ---------------------------------------------------------------------------
;  Minimal CPU-exception stubs.  Each pushes a uniform frame -- (vector,
;  error code) -- and enters fault_handler(), which reports the faulting
;  EIP/CR2 and halts.  Without this any fault triple-faults invisibly.
; ---------------------------------------------------------------------------
extern fault_handler

%macro ISR_NOERR 1
global isr_stub_%1
isr_stub_%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr_stub_%1
isr_stub_%1:
    push dword %1
    jmp isr_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

isr_common:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call fault_handler
    add esp, 4
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret

global isr_stub_table
isr_stub_table:
    dd isr_stub_0, isr_stub_1, isr_stub_2, isr_stub_3
    dd isr_stub_4, isr_stub_5, isr_stub_6, isr_stub_7
    dd isr_stub_8, isr_stub_9, isr_stub_10, isr_stub_11
    dd isr_stub_12, isr_stub_13, isr_stub_14, isr_stub_15
    dd isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19
    dd isr_stub_20, isr_stub_21, isr_stub_22, isr_stub_23
    dd isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27
    dd isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31

section .data
align 8
gdt_start:
    dq 0x0000000000000000
gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00
gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00
gdt_end:
gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start
