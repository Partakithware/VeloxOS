; interrupts.asm - Rock Solid Version
BITS 64
section .text

extern exception_handler
extern irq_handler

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push 0
    push %1
    jmp isr_common
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push %1
    jmp isr_common
%endmacro

%macro IRQ 2
global isr%1
isr%1:
    push 0
    push %2
    jmp irq_common
%endmacro


ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 6
ISR_ERRCODE   8
ISR_ERRCODE   13
ISR_ERRCODE   14

; Change the IRQ 32 line to use the common logic
IRQ 32, 0   ; This maps IRQ 0 (timer) to IDT vector 32
IRQ 33, 1
IRQ 44, 12

isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    mov rdi, rsp
    add rdi, 136         ; Point to RIP
    mov rsi, [rsp + 128] ; Error Code
    mov rdx, [rsp + 120] ; Vector
    
    call exception_handler
    jmp interrupt_exit

irq_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    mov rdi, rsp
    add rdi, 136         ; Point to RIP
    mov rsi, [rsp + 120] ; IRQ number
    
    call irq_handler
    jmp interrupt_exit

interrupt_exit:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    iretq


section .note.GNU-stack noalloc noexec nowrite progbits