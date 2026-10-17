; ============================================================================
;  EC-OS boot sector
;   - sets VBE WxHx32 linear framebuffer    (int 0x10, AX=0x4F00/01/02)
;   - detects RAM                            (int 0x12 / int 0x15 E801)
;   - loads the kernel from LBA 1 to 0x8000  (int 0x13, AH=0x42)
;   - jumps to kernel_entry (which enters protected mode)
;
;  Resolution defaults to 1280x1024 (native VMware VBE).  Build with
;  '-DKRES_W=1920 -DKRES_H=1080' for the old 1920x1080 build; the value
;  must match SCREEN_W/SCREEN_H in system/gfx.h.
;
;  assemble:  nasm -f bin boot/boot.asm -o build/boot.bin     (<= 512 bytes)
; ============================================================================
[BITS 16]
[ORG 0x7C00]

; ---- resolution (overridable at build time) ----
%ifndef KRES_W
%define KRES_W  1280
%define KRES_H  1024
%endif

; ---- hand-off block read by system/vbe.h (keep in sync!) ----
%define VBE_PARAMS   0x5000      ; 32-byte struct, magic "ECV1"
%define VBE_INFO     0x5600      ; VbeControllerInfoBlock (512 B)
%define VBE_MODE     0x5800      ; VbeModeInfoBlock       (256 B)

; ---- kernel image parameters ----
%define KERNEL_SEG   0x0000      ; load segment
%define KERNEL_OFF   0x8000      ; load offset  -> physical 0x8000
%define KERNEL_LBA   1           ; first kernel sector on disk
%define KERNEL_SEC   512         ; max sectors to load (256 KiB)

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [boot_drive], dl

    call detect_memory
    call vbe_setup
    test al, al
    jz   vbe_error
    call load_kernel
    jc   kernel_error

    jmp KERNEL_SEG:KERNEL_OFF

; --------------------------------------------------------------------------
;  memory detection
; --------------------------------------------------------------------------
detect_memory:
    int 0x12
    mov [VBE_PARAMS + 24], ax          ; conventional memory, KB
    xor ax, ax
    xor bx, bx
    mov ax, 0xE801
    int 0x15
    jc  .done
    test ax, ax
    jnz .store
    mov ax, bx
.store:
    mov [VBE_PARAMS + 26], ax          ; KB between 1M and 16M
    mov [VBE_PARAMS + 28], bx          ; 64K blocks above 16M
.done:
    ret

; --------------------------------------------------------------------------
;  VBE : find KRES_WxKRES_Hx32 with linear framebuffer and switch to it
; --------------------------------------------------------------------------
vbe_setup:
    mov di, VBE_INFO
    mov dword [VBE_INFO], 0x32454256   ; request "VBE2" fields
    mov ax, 0x4F00
    int 0x10
    cmp ax, 0x004F
    jne .fail
    cmp dword [VBE_INFO], 0x41534556   ; signature "VESA"
    jne .fail

    mov si, [VBE_INFO + 0x0E]          ; VideoModePtr offset
    mov ax, [VBE_INFO + 0x10]          ; VideoModePtr segment
    mov fs, ax
.next:
    mov cx, [fs:si]
    add si, 2
    cmp cx, 0xFFFF
    je  .fail
    push si
    push fs
    mov di, VBE_MODE
    mov ax, 0x4F01                     ; get mode info
    int 0x10
    pop fs
    pop si
    cmp ax, 0x004F
    jne .next
    mov ax, [VBE_MODE + 0x00]          ; ModeAttributes
    and ax, 0x0081                     ; bit0 supported + bit7 LFB
    cmp ax, 0x0081
    jne .next
    cmp byte [VBE_MODE + 0x19], 32     ; BitsPerPixel
    jne .next
    cmp word [VBE_MODE + 0x12], KRES_W    ; XResolution
    jne .next
    cmp word [VBE_MODE + 0x14], KRES_H    ; YResolution
    jne .next

    ; ---- record the hand-off block ----
    mov eax, [VBE_MODE + 0x28]         ; PhysBasePtr
    mov [VBE_PARAMS + 4], eax
    movzx eax, word [VBE_MODE + 0x10]  ; BytesPerScanLine
    mov [VBE_PARAMS + 8], eax
    mov dword [VBE_PARAMS + 12], KRES_W
    mov dword [VBE_PARAMS + 16], KRES_H
    mov dword [VBE_PARAMS + 20], 32
    mov dword [VBE_PARAMS], 0x31564345 ; "ECV1"

    ; ---- switch to it, linear framebuffer ----
    mov bx, cx
    or  bx, 0x4000
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .fail
    mov al, 1
    ret
.fail:
    xor al, al
    ret

; --------------------------------------------------------------------------
;  load kernel with int 0x13 AH=0x42 (no BIOS geometry issues)
; --------------------------------------------------------------------------
load_kernel:
    mov word [dap.seg], 0x0800
    mov word [dap.off], 0x0000
    mov dword [dap.lba], KERNEL_LBA
    mov bx, KERNEL_SEC
.loop:
    mov ax, bx
    cmp ax, 127
    jbe .sz
    mov ax, 127
.sz:
    mov [dap.count], ax
    mov [chunk], ax
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc  .err
    mov ax, [chunk]
    add dword [dap.lba], eax
    shl ax, 5                          ; sectors*512/16 -> segment delta
    add word [dap.seg], ax
    mov ax, [chunk]
    sub bx, ax
    jnz .loop
    clc
    ret
.err:
    stc
    ret

; --------------------------------------------------------------------------
%defstr _KW KRES_W
%defstr _KH KRES_H
%strcat _BV 'EC-OS: VBE ', _KW, 'x', _KH, 'x32 unavailable'

vbe_error:
    mov si, msg_vbe
    jmp print_halt
kernel_error:
    mov si, msg_kern

print_halt:
    lodsb
    test al, al
    jz .h
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp print_halt
.h:
    cli
.halt:
    hlt
    jmp .halt

boot_drive: db 0
chunk:      dw 0
msg_vbe:    db _BV, 0
msg_kern:   db 'EC-OS: cannot read kernel (boot media)', 0

dap:
    db 0x10
    db 0
.count: dw 0
.off:   dw 0
.seg:   dw 0
.lba:   dq 0

    times 510-($-$$) db 0
    dw 0xAA55
