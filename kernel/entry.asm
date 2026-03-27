BITS 32

; ── Multiboot2 header ─────────────────────────────────────────────────────
section .multiboot
align 8
header_start:
    dd 0xE85250D6                                       ; magic
    dd 0                                                ; arch: i386
    dd header_end - header_start                        ; length
    dd -(0xE85250D6 + 0 + (header_end - header_start)) ; checksum
    ; Framebuffer request tag (type 5) — ask GRUB for 1024x768x32
    dw 5                    ; type = framebuffer request
    dw 0                    ; flags
    dd 24                   ; size (must be 8-byte aligned: 4+4+4+4+4+4)
    dd 1024                 ; width
    dd 768                  ; height
    dd 32                   ; depth (bpp)
    dd 0                    ; reserved/padding for 8-byte alignment
    ; End tag
    dw 0
    dw 0
    dd 8
header_end:

; ── GDT (flat 4 GB, ring 0) ───────────────────────────────────────────────
section .data
align 8
gdt_start:
    dq 0                        ; null descriptor
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xCF, 0x00  ; code: base=0, limit=4GB, ring0, 32-bit
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00  ; data: base=0, limit=4GB, ring0
    dw 0xFFFF, 0x0000
    db 0x00, 0xFA, 0xCF, 0x00  ; user code: base=0, limit=4GB, ring3, 32-bit
    dw 0xFFFF, 0x0000
    db 0x00, 0xF2, 0xCF, 0x00  ; user data: base=0, limit=4GB, ring3
gdt_end:

gdt_ptr:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ── Framebuffer info filled from multiboot2 tag ───────────────────────────
section .bss
align 4
global fb_phys_addr
global fb_pitch
global fb_width
global fb_height
global fb_bpp
fb_phys_addr:   resd 1
fb_pitch:       resd 1
fb_width:       resd 1
fb_height:      resd 1
fb_bpp:         resd 1

; Temporary save slot for EBX (multiboot2 info ptr) across stack switch
mb2_ptr: resd 1

; ── Entry ─────────────────────────────────────────────────────────────────
section .text
global _start
extern kernel_main
extern stack_top

_start:
    cli

    ; Verify multiboot2 magic (EAX must be 0x36d76289)
    cmp eax, 0x36d76289
    jne .hang               ; not loaded by a multiboot2 compliant loader

    ; Save EBX (multiboot2 info ptr) to a .bss slot BEFORE changing ESP
    mov [mb2_ptr], ebx

    ; Load our GDT
    lgdt [gdt_ptr]

    ; Reload data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to flush CS pipeline with our code selector
    jmp 0x08:.flush_cs
.flush_cs:

    ; Switch to our kernel stack
    mov esp, stack_top

    ; Restore multiboot2 info ptr
    mov ebx, [mb2_ptr]

    ; ── Parse multiboot2 tags for framebuffer info (type 8) ──────────────
    ; Structure: uint32 total_size, uint32 reserved, then tags
    ; Each tag: uint32 type, uint32 size, ... data ...
    ; Tags are 8-byte aligned.
    add ebx, 8              ; skip fixed header (total_size + reserved)

.tag_loop:
    mov eax, [ebx]          ; tag type
    test eax, eax
    jz  .tags_done          ; type 0 = end tag

    cmp eax, 8
    je  .fb_tag             ; type 8 = framebuffer info

    ; Advance to next tag (size field at +4, round up to 8-byte boundary)
    mov ecx, [ebx + 4]     ; tag size
    add ecx, 7
    and ecx, 0xFFFFFFF8
    add ebx, ecx
    jmp .tag_loop

.fb_tag:
    ; Multiboot2 framebuffer tag layout (type 8):
    ;  +0  uint32  type  (=8)
    ;  +4  uint32  size
    ;  +8  uint64  framebuffer_addr   (we read low 32 bits only)
    ;  +16 uint32  framebuffer_pitch
    ;  +20 uint32  framebuffer_width
    ;  +24 uint32  framebuffer_height
    ;  +28 uint8   framebuffer_bpp
    ;  +29 uint8   framebuffer_type   (0=indexed, 1=RGB, 2=EGA text)
    mov eax, [ebx + 8]      ; framebuffer addr low 32 bits
    mov [fb_phys_addr], eax
    mov eax, [ebx + 16]     ; pitch (bytes per row)
    mov [fb_pitch], eax
    mov eax, [ebx + 20]     ; width in pixels
    mov [fb_width], eax
    mov eax, [ebx + 24]     ; height in pixels
    mov [fb_height], eax
    movzx eax, byte [ebx + 28]  ; bits per pixel
    mov [fb_bpp], eax
    ; After finding fb tag, continue to tags_done
    jmp .tags_done

.tags_done:
    mov ebx, [mb2_ptr]      ; Restore original Multiboot info pointer
    push ebx                ; Arg 2: addr (pointer to Multiboot info)
    push 0x36d76289         ; Arg 1: magic number
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
