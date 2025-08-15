section .multiboot
align 8
multiboot2_header:
    dd 0xe85250d6              ; Multiboot2 magic number
    dd 0                       ; Architecture
    dd multiboot2_header_end - multiboot2_header  ; Header length
    dd -(0xe85250d6 + 0 + (multiboot2_header_end - multiboot2_header))  ; Checksum

    align 8
    ; Bilgi Talep Etiketi (Information Request Tag)
    ; Bu etiket ile önyükleyiciden hangi bilgileri istediğimizi belirtiriz.
request_tag_start:
    dw 1      ; type = MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST
    dw 0      ; flags = 0
    dd request_tag_end - request_tag_start   ; size

    dd 17     ; MULTIBOOT_TAG_EFI_MEMORY_MAP (0x00000011)
    dd 6      ; MULTIBOOT_TAG_TYPE_MMAP       (0x00000006)
    dd 4      ; MULTIBOOT_TAG_TYPE_BASIC_MEMINFO  (0x00000004)
request_tag_end:


    align 8
    .video_tag:
        dw 5        ; Type (framebuffer tag)
        dw 0        ; Flags
        dd 20       ; Size
        dd 0      ; Width
        dd 0      ; Height
        dd 32       ; Depth

    align 8
    .efi_boot_services_tag:
        dw 7
        dw 0
        dd 8
    

    align 8
    .end_tag:
        ; End tag
        dw 0    ; Type
        dw 0    ; Flags
        dd 8    ; Size
multiboot2_header_end:

global mb2_signature
global mb2_tagptr

section .bss
mb2_signature resb 4
mb2_tagptr resb 4