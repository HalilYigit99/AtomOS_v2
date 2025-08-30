section .multiboot
align 8
multiboot2_header:
    dd 0xe85250d6              ; Multiboot2 magic number
    dd 0                       ; Architecture
    dd multiboot2_header_end - multiboot2_header  ; Header length
    dd -(0xe85250d6 + 0 + (multiboot2_header_end - multiboot2_header))  ; Checksum

    align 8
    ; Information Request Tag - BURASI ÖNEMLİ!
    ; Bu tag ile bootloader'dan hangi bilgileri istediğimizi belirtiyoruz
request_tag_start:
    dw 1      ; type = MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST
    dw 0      ; flags = 0
    dd request_tag_end - request_tag_start   ; size

    ; Talep edilen tag'lar (ÖNEMLİ: Sıra önemli değil)
    dd 1      ; MULTIBOOT_TAG_TYPE_CMDLINE
    dd 2      ; MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME  
    dd 4      ; MULTIBOOT_TAG_TYPE_BASIC_MEMINFO
    dd 6      ; MULTIBOOT_TAG_TYPE_MMAP
    dd 8      ; MULTIBOOT_TAG_TYPE_FRAMEBUFFER
    dd 9      ; MULTIBOOT_TAG_TYPE_ELF_SECTIONS
    dd 11     ; MULTIBOOT_TAG_TYPE_EFI32
    dd 12     ; MULTIBOOT_TAG_TYPE_EFI64
    dd 17     ; MULTIBOOT_TAG_TYPE_EFI_MMAP
    dd 19     ; MULTIBOOT_TAG_TYPE_EFI32_IH
    dd 20     ; MULTIBOOT_TAG_TYPE_EFI64_IH
request_tag_end:


    align 8
    .video_tag:
        dw 5        ; Type (framebuffer tag)
        dw 0        ; Flags
        dd 20       ; Size
        dd 1366     ; Width
        dd 720      ; Height
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