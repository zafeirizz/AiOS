[BITS 32]

global isr_common_stub
global irq_common_stub

extern isr_handler   ; C function: isr_handler(int_no, err_code)
extern irq_dispatch  ; C function: irq_dispatch(irq_number)

; ─────────────────────────────────────────────────────────
; CPU exception stubs (vectors 0-31)
; Each pushes a dummy error code (if CPU doesn't) and the
; vector number, then jumps to the shared stub.
; ─────────────────────────────────────────────────────────

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    cli
    push dword 0
    push dword %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli
    push dword %1
    jmp isr_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30
ISR_NOERRCODE 31

ISR_NOERRCODE 128  ; syscall

; ─────────────────────────────────────────────────────────
; IRQ stubs (vectors 32-47, IRQ 0-15)
; Each pushes the IRQ number and jumps to the shared stub.
; ─────────────────────────────────────────────────────────

%macro IRQ_STUB 1
global irq%1
irq%1:
    cli
    push dword %1
    jmp irq_common_stub
%endmacro

IRQ_STUB 0   ; timer
IRQ_STUB 1   ; keyboard
IRQ_STUB 2
IRQ_STUB 3
IRQ_STUB 4
IRQ_STUB 5
IRQ_STUB 6
IRQ_STUB 7
IRQ_STUB 8   ; RTC
IRQ_STUB 9
IRQ_STUB 10
IRQ_STUB 11
IRQ_STUB 12
IRQ_STUB 13
IRQ_STUB 14
IRQ_STUB 15

; ─────────────────────────────────────────────────────────
; Shared CPU exception stub
;
; Stack layout on entry to this label (top = low address):
;   [esp+ 0] .. [esp+28]  pushad  (8 x 4 = 32 bytes: edi,esi,ebp,esp_dummy,ebx,edx,ecx,eax)
;   [esp+32]              ds
;   [esp+36]              es
;   [esp+40]              fs
;   [esp+44]              gs
;   [esp+48]              int_no   (pushed by ISR macro)
;   [esp+52]              err_code (pushed by ISR macro)
;   [esp+56]              eip      (pushed by CPU)
;   [esp+60]              cs       (pushed by CPU)
;   [esp+64]              eflags   (pushed by CPU)
; ─────────────────────────────────────────────────────────
isr_common_stub:
    pushad
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, [esp + 48]   ; int_no
    mov ebx, [esp + 52]   ; err_code
    push ebx
    push eax
    call isr_handler
    add esp, 8

    pop gs
    pop fs
    pop es
    pop ds
    popad

    add esp, 8            ; discard int_no + err_code
    sti
    iret

; ─────────────────────────────────────────────────────────
; Shared IRQ stub
;
; Stack layout on entry to this label (top = low address):
;   [esp+ 0] .. [esp+28]  pushad  (8 x 4 = 32 bytes)
;   [esp+32]              ds
;   [esp+36]              es
;   [esp+40]              fs
;   [esp+44]              gs
;   [esp+48]              irq_number (pushed by IRQ_STUB macro)
;   [esp+52]              eip      (pushed by CPU)
;   [esp+56]              cs       (pushed by CPU)
;   [esp+60]              eflags   (pushed by CPU)
; ─────────────────────────────────────────────────────────
irq_common_stub:
    pushad
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, [esp + 48]   ; irq_number
    mov ebx, esp          ; esp (points to register frame)
    push ebx              ; 2nd param: esp
    push eax              ; 1st param: irq_number
    call irq_dispatch
    add esp, 8

    pop gs
    pop fs
    pop es
    pop ds
    popad

    add esp, 4            ; discard irq_number
    sti
    iret

; Tell the linker this object does not need an executable stack
section .note.GNU-stack noalloc noexec nowrite progbits
