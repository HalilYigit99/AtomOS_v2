section .text
BITS 32

global arch_task_context_switch

arch_task_context_switch:
    pushfd
    pushad
    mov eax, [esp + 40]
    test eax, eax
    jz .skip_save
    mov [eax], esp
.skip_save:
    mov eax, [esp + 44]
    mov esp, [eax]
    popad
    popfd
    ret
