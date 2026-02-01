.intel_syntax noprefix

/*
 * ============================================================================
 *                    OTSOS BOOTLOADER v2.0
 * ============================================================================
 * 
 *
 *
 * BOOTLOADER CREATED FOR: OTSOS2
 * ============================================================================
 */

.set MB2_MAGIC,             0xE85250D6      
.set MB2_ARCH,              0               
.set MB2_HEADER_LEN,        multiboot2_header_end - multiboot2_header
.set MB2_CHECKSUM,          -(MB2_MAGIC + MB2_ARCH + MB2_HEADER_LEN)

.set MB1_BOOTLOADER_MAGIC,  0x2BADB002
.set MB2_BOOTLOADER_MAGIC,  0x36D76289

.set VGA_TEXT_BUFFER,       0xB8000
.set VGA_WIDTH,             80
.set VGA_HEIGHT,            25

.set COLOR_BLACK,           0x00
.set COLOR_BLUE,            0x01
.set COLOR_GREEN,           0x02
.set COLOR_CYAN,            0x03
.set COLOR_RED,             0x04
.set COLOR_MAGENTA,         0x05
.set COLOR_BROWN,           0x06
.set COLOR_LIGHT_GRAY,      0x07
.set COLOR_DARK_GRAY,       0x08
.set COLOR_LIGHT_BLUE,      0x09
.set COLOR_LIGHT_GREEN,     0x0A
.set COLOR_LIGHT_CYAN,      0x0B
.set COLOR_LIGHT_RED,       0x0C
.set COLOR_LIGHT_MAGENTA,   0x0D
.set COLOR_YELLOW,          0x0E
.set COLOR_WHITE,           0x0F

.set ATTR_TITLE,            0x1F    
.set ATTR_MENU_NORMAL,      0x07    
.set ATTR_MENU_SELECTED,    0x70    
.set ATTR_BORDER,           0x0B    
.set ATTR_ERROR,            0x4F    
.set ATTR_SUCCESS,          0x2F    
.set ATTR_INFO,             0x0E    
.set ATTR_TIMER,            0x0C    
.set ATTR_LOGO,             0x0D    

.set KEY_UP,                0x48
.set KEY_DOWN,              0x50
.set KEY_ENTER,             0x1C
.set KEY_ESC,               0x01
.set KEY_E,                 0x12    /* 'e' for edit */
.set KEY_C,                 0x2E    /* 'c' for cmd */
.set KEY_R,                 0x13    /* 'r' for reboot */

.set KB_DATA_PORT,          0x60
.set KB_STATUS_PORT,        0x64

.set PIT_CHANNEL0,          0x40
.set PIT_COMMAND,           0x43
.set PIT_FREQ,              1193182         
.set TARGET_FREQ,           100             

.set MENU_ITEMS,            4               
.set MENU_START_Y,          10              
.set AUTO_BOOT_TIMEOUT,     5               

.section .multiboot
.align 8
multiboot2_header:
    .long MB2_MAGIC
    .long MB2_ARCH
    .long MB2_HEADER_LEN
    .long MB2_CHECKSUM

    .align 8
fb_tag_start:
    .word 5                                 /* type = framebuffer */
    .word 0                                 /* flags = optional (0) */
    .long fb_tag_end - fb_tag_start         /* size */
    .long 1024                              /* width */
    .long 768                               /* height */
    .long 32                                /* depth (bpp) */
fb_tag_end:

    .align 8
end_tag:
    .word 0                                 /* type = end */
    .word 0                                 /* flags */
    .long 8                                 /* size */
multiboot2_header_end:


.section .bss
.align 4096

p4_table:
    .skip 4096
p3_table:
    .skip 4096
p2_table:
    .skip 4096
p2_table_1:
    .skip 4096
p2_table_2:
    .skip 4096
p2_table_3:
    .skip 4096

stack_bottom:
    .skip 65536
stack_top:


.section .data
.align 8

multiboot_info_ptr:
    .quad 0
multiboot_magic_val:
    .quad 0

selected_item:
    .long 0                     
timer_ticks:
    .long 0                     
boot_timeout:
    .long AUTO_BOOT_TIMEOUT     
menu_active:
    .byte 1                     
error_code:
    .long 0                     

total_memory_mb:
    .long 0                     
cpu_vendor:
    .space 16                   
cpu_features:
    .long 0                     

fb_addr:
    .long 0                     
fb_width:
    .long 0                     
fb_height:
    .long 0                     
fb_pitch:
    .long 0                     
fb_bpp:
    .long 0                     
fb_cursor_x:
    .long 0                     
fb_cursor_y:
    .long 0                     

.section .rodata

gdt64:
    .quad 0                                             /* Null descriptor */
    .quad (1<<43) | (1<<44) | (1<<47) | (1<<53)        /* Code segment (idx 1, offset 0x08) */
    .quad (1<<44) | (1<<47) | (1<<41)                  /* Data segment (idx 2, offset 0x10) */
pointer64:
    .word . - gdt64 - 1
    .quad gdt64


logo_line1:     .asciz "  ____  _______ _____    ____   _____ "
logo_line2:     .asciz " / __ \\|__   __/ ____|  / __ \\ / ____|"
logo_line3:     .asciz "| |  | |  | | | (___   | |  | | (___  "
logo_line4:     .asciz "| |  | |  | |  \\___ \\  | |  | |\\___ \\ "
logo_line5:     .asciz "| |__| |  | |  ____) | | |__| |____) |"
logo_line6:     .asciz " \\____/   |_| |_____/   \\____/|_____/ "

title_str:      .asciz " OTSOS bootloader v2.0"
subtitle_str:   .asciz "enter '↑↓' to select, press 'enter' for enter section, 'c' for simple shell"
border_top:     .asciz "╔══════════════════════════════════════════════════════════════════════════╗"
border_mid:     .asciz "╠══════════════════════════════════════════════════════════════════════════╣"
border_bot:     .asciz "╚══════════════════════════════════════════════════════════════════════════╝"
border_side:    .asciz "║"

menu_item0:     .asciz "  > boot OTSOS default                    "
menu_item1:     .asciz "  > boot OTSOS safe mode                  "
menu_item2:     .asciz "  > system info                           "
menu_item3:     .asciz "  > reboot                                "

msg_booting:        .asciz "Boot OTSOS..."
msg_safe_mode:      .asciz "starting in safe mode..."
msg_rebooting:      .asciz "rebooting system..."
msg_timer:          .asciz "auto boot at    seconds..."
msg_loading_kernel: .asciz "[    ] loading kernel..."
msg_init_memory:    .asciz "[    ] initializing memory..."
msg_switch_mode:    .asciz "[    ] switching to long mode..."
msg_done:           .asciz "[ OK ]"
msg_fail:           .asciz "[FAIL]"

err_no_multiboot:   .asciz "ERROR: Not loaded by Multiboot-compliant bootloader!"
err_no_cpuid:       .asciz "ERROR: CPUID instruction not supported!"
err_no_long_mode:   .asciz "ERROR: CPU doesn't support 64-bit Long Mode!"
err_memory_fail:    .asciz "ERROR: Failed to initialize page tables!"
err_gdt_fail:       .asciz "ERROR: Failed to load GDT!"
err_unknown:        .asciz "ERROR: Unknown error occurred!"

sysinfo_title:      .asciz "=== System Information ==="
sysinfo_cpu:        .asciz "CPU Vendor: "
sysinfo_mem:        .asciz "Memory: "
sysinfo_mb_ver:     .asciz "Multiboot: "
sysinfo_mb1:        .asciz "Version 1"
sysinfo_mb2:        .asciz "Version 2"
sysinfo_press_key:  .asciz "Press any key to return to menu..."
sysinfo_long_mode:  .asciz "Long Mode: "
sysinfo_yes:        .asciz "Supported"
sysinfo_no:         .asciz "Not Supported"

digits:             .asciz "0123456789"

err_no_fb:          .asciz "ERROR: No framebuffer available!"
msg_fb_init:        .asciz "Initializing framebuffer..."


.align 4
font_8x8:
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    /* 33 = ! */
    .byte 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00
    /* 34 = " */
    .byte 0x6C, 0x6C, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00
    /* 35 = # */
    .byte 0x6C, 0xFE, 0x6C, 0x6C, 0xFE, 0x6C, 0x00, 0x00
    /* 36 = $ */
    .byte 0x18, 0x7E, 0xC0, 0x7C, 0x06, 0xFC, 0x18, 0x00
    /* 37 = % */
    .byte 0xC6, 0xCC, 0x18, 0x30, 0x66, 0xC6, 0x00, 0x00
    /* 38 = & */
    .byte 0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00
    /* 39 = ' */
    .byte 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00
    /* 40 = ( */
    .byte 0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00
    /* 41 = ) */
    .byte 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00
    /* 42 = * */
    .byte 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00
    /* 43 = + */
    .byte 0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00
    /* 44 = , */
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30
    /* 45 = - */
    .byte 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00
    /* 46 = . */
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00
    /* 47 = / */
    .byte 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x00, 0x00
    /* 48 = 0 */
    .byte 0x7C, 0xC6, 0xCE, 0xD6, 0xE6, 0xC6, 0x7C, 0x00
    /* 49 = 1 */
    .byte 0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00
    /* 50 = 2 */
    .byte 0x7C, 0xC6, 0x06, 0x1C, 0x70, 0xC6, 0xFE, 0x00
    /* 51 = 3 */
    .byte 0x7C, 0xC6, 0x06, 0x3C, 0x06, 0xC6, 0x7C, 0x00
    /* 52 = 4 */
    .byte 0x1C, 0x3C, 0x6C, 0xCC, 0xFE, 0x0C, 0x0C, 0x00
    /* 53 = 5 */
    .byte 0xFE, 0xC0, 0xFC, 0x06, 0x06, 0xC6, 0x7C, 0x00
    /* 54 = 6 */
    .byte 0x38, 0x60, 0xC0, 0xFC, 0xC6, 0xC6, 0x7C, 0x00
    /* 55 = 7 */
    .byte 0xFE, 0xC6, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00
    /* 56 = 8 */
    .byte 0x7C, 0xC6, 0xC6, 0x7C, 0xC6, 0xC6, 0x7C, 0x00
    /* 57 = 9 */
    .byte 0x7C, 0xC6, 0xC6, 0x7E, 0x06, 0x0C, 0x78, 0x00
    /* 58 = : */
    .byte 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00
    /* 59 = ; */
    .byte 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30
    /* 60 = < */
    .byte 0x0C, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0C, 0x00
    /* 61 = = */
    .byte 0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00
    /* 62 = > */
    .byte 0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00
    /* 63 = ? */
    .byte 0x7C, 0xC6, 0x0C, 0x18, 0x18, 0x00, 0x18, 0x00
    /* 64 = @ */
    .byte 0x7C, 0xC6, 0xDE, 0xDE, 0xDE, 0xC0, 0x7C, 0x00
    /* 65 = A */
    .byte 0x38, 0x6C, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0x00
    /* 66 = B */
    .byte 0xFC, 0xC6, 0xC6, 0xFC, 0xC6, 0xC6, 0xFC, 0x00
    /* 67 = C */
    .byte 0x7C, 0xC6, 0xC0, 0xC0, 0xC0, 0xC6, 0x7C, 0x00
    /* 68 = D */
    .byte 0xF8, 0xCC, 0xC6, 0xC6, 0xC6, 0xCC, 0xF8, 0x00
    /* 69 = E */
    .byte 0xFE, 0xC0, 0xC0, 0xF8, 0xC0, 0xC0, 0xFE, 0x00
    /* 70 = F */
    .byte 0xFE, 0xC0, 0xC0, 0xF8, 0xC0, 0xC0, 0xC0, 0x00
    /* 71 = G */
    .byte 0x7C, 0xC6, 0xC0, 0xCE, 0xC6, 0xC6, 0x7E, 0x00
    /* 72 = H */
    .byte 0xC6, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0x00
    /* 73 = I */
    .byte 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00
    /* 74 = J */
    .byte 0x1E, 0x06, 0x06, 0x06, 0xC6, 0xC6, 0x7C, 0x00
    /* 75 = K */
    .byte 0xC6, 0xCC, 0xD8, 0xF0, 0xD8, 0xCC, 0xC6, 0x00
    /* 76 = L */
    .byte 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xFE, 0x00
    /* 77 = M */
    .byte 0xC6, 0xEE, 0xFE, 0xD6, 0xC6, 0xC6, 0xC6, 0x00
    /* 78 = N */
    .byte 0xC6, 0xE6, 0xF6, 0xDE, 0xCE, 0xC6, 0xC6, 0x00
    /* 79 = O */
    .byte 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00
    /* 80 = P */
    .byte 0xFC, 0xC6, 0xC6, 0xFC, 0xC0, 0xC0, 0xC0, 0x00
    /* 81 = Q */
    .byte 0x7C, 0xC6, 0xC6, 0xC6, 0xD6, 0xDE, 0x7C, 0x06
    /* 82 = R */
    .byte 0xFC, 0xC6, 0xC6, 0xFC, 0xD8, 0xCC, 0xC6, 0x00
    /* 83 = S */
    .byte 0x7C, 0xC6, 0xC0, 0x7C, 0x06, 0xC6, 0x7C, 0x00
    /* 84 = T */
    .byte 0xFE, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00
    /* 85 = U */
    .byte 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00
    /* 86 = V */
    .byte 0xC6, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x10, 0x00
    /* 87 = W */
    .byte 0xC6, 0xC6, 0xC6, 0xD6, 0xFE, 0xEE, 0xC6, 0x00
    /* 88 = X */
    .byte 0xC6, 0xC6, 0x6C, 0x38, 0x6C, 0xC6, 0xC6, 0x00
    /* 89 = Y */
    .byte 0xC6, 0xC6, 0x6C, 0x38, 0x18, 0x18, 0x18, 0x00
    /* 90 = Z */
    .byte 0xFE, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xFE, 0x00
    /* 91 = [ */
    .byte 0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00
    /* 92 = \ */
    .byte 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00
    /* 93 = ] */
    .byte 0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00
    /* 94 = ^ */
    .byte 0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00
    /* 95 = _ */
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF
    /* 96 = ` */
    .byte 0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00
    /* 97 = a */
    .byte 0x00, 0x00, 0x78, 0x0C, 0x7C, 0xCC, 0x76, 0x00
    /* 98 = b */
    .byte 0xC0, 0xC0, 0xF8, 0xCC, 0xCC, 0xCC, 0xF8, 0x00
    /* 99 = c */
    .byte 0x00, 0x00, 0x78, 0xCC, 0xC0, 0xCC, 0x78, 0x00
    /* 100 = d */
    .byte 0x0C, 0x0C, 0x7C, 0xCC, 0xCC, 0xCC, 0x7C, 0x00
    /* 101 = e */
    .byte 0x00, 0x00, 0x78, 0xCC, 0xFC, 0xC0, 0x78, 0x00
    /* 102 = f */
    .byte 0x38, 0x6C, 0x60, 0xF0, 0x60, 0x60, 0xF0, 0x00
    /* 103 = g */
    .byte 0x00, 0x00, 0x76, 0xCC, 0xCC, 0x7C, 0x0C, 0xF8
    /* 104 = h */
    .byte 0xC0, 0xC0, 0xF8, 0xCC, 0xCC, 0xCC, 0xCC, 0x00
    /* 105 = i */
    .byte 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00
    /* 106 = j */
    .byte 0x06, 0x00, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C
    /* 107 = k */
    .byte 0xC0, 0xC0, 0xCC, 0xD8, 0xF0, 0xD8, 0xCC, 0x00
    /* 108 = l */
    .byte 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00
    /* 109 = m */
    .byte 0x00, 0x00, 0xFE, 0xD6, 0xD6, 0xD6, 0xD6, 0x00
    /* 110 = n */
    .byte 0x00, 0x00, 0xF8, 0xCC, 0xCC, 0xCC, 0xCC, 0x00
    /* 111 = o */
    .byte 0x00, 0x00, 0x78, 0xCC, 0xCC, 0xCC, 0x78, 0x00
    /* 112 = p */
    .byte 0x00, 0x00, 0xF8, 0xCC, 0xCC, 0xF8, 0xC0, 0xC0
    /* 113 = q */
    .byte 0x00, 0x00, 0x7C, 0xCC, 0xCC, 0x7C, 0x0C, 0x0C
    /* 114 = r */
    .byte 0x00, 0x00, 0xDC, 0x66, 0x60, 0x60, 0xF0, 0x00
    /* 115 = s */
    .byte 0x00, 0x00, 0x7C, 0xC0, 0x78, 0x0C, 0xF8, 0x00
    /* 116 = t */
    .byte 0x60, 0x60, 0xF0, 0x60, 0x60, 0x6C, 0x38, 0x00
    /* 117 = u */
    .byte 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0x76, 0x00
    /* 118 = v */
    .byte 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0x78, 0x30, 0x00
    /* 119 = w */
    .byte 0x00, 0x00, 0xC6, 0xD6, 0xD6, 0xFE, 0x6C, 0x00
    /* 120 = x */
    .byte 0x00, 0x00, 0xC6, 0x6C, 0x38, 0x6C, 0xC6, 0x00
    /* 121 = y */
    .byte 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0x7C, 0x0C, 0xF8
    /* 122 = z */
    .byte 0x00, 0x00, 0xFE, 0x0C, 0x18, 0x30, 0xFE, 0x00
    /* 123 = { */
    .byte 0x0E, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0E, 0x00
    /* 124 = | */
    .byte 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00
    /* 125 = } */
    .byte 0x70, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x70, 0x00
    /* 126 = ~ */
    .byte 0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    /* 127 = DEL */
    .byte 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF


.section .text
.code32
.global start


start:
    mov esp, offset stack_top
    
    mov [multiboot_magic_val], eax
    mov [multiboot_info_ptr], ebx
    
    cmp eax, MB2_BOOTLOADER_MAGIC
    je .Lvalid_magic
    cmp eax, MB1_BOOTLOADER_MAGIC
    je .Lvalid_magic
    
    mov dword ptr [error_code], 1
    jmp error_no_multiboot

.Lvalid_magic:
    mov ebx, [multiboot_info_ptr]
    call parse_multiboot2_fb
    
    call clear_screen
    
    call check_cpuid
    test eax, eax
    jz error_no_cpuid
    
    call check_long_mode
    test eax, eax
    jz error_no_long_mode
    
    call get_cpu_info
    
    call show_boot_menu
    
    call menu_loop
    
    jmp start_boot_sequence

clear_screen:
    push eax
    push ecx
    push edi
    
    mov edi, [fb_addr]
    test edi, edi
    jz .Lclear_done
    
    mov ecx, [fb_height]
    imul ecx, [fb_width]
    
    mov eax, 0x001428           
    
.Lclear_loop:
    mov [edi], eax              
    add edi, 4
    dec ecx
    jnz .Lclear_loop
    
    mov dword ptr [fb_cursor_x], 0
    mov dword ptr [fb_cursor_y], 0
    
.Lclear_done:
    pop edi
    pop ecx
    pop eax
    ret


print_string:
    push eax
    push ebx
    push ecx
    push esi
    
.Lprint_loop:
    lodsb                       
    test al, al                 
    jz .Lprint_done
    
    cmp al, 0xE2
    je .Lskip_utf8_char
    cmp al, 0xC2
    jae .Lcheck_utf8_2byte
    
    push ebx
    push ecx
    push edx
    call fb_put_char
    pop edx
    pop ecx
    pop ebx
    
    add ebx, 8                  
    jmp .Lprint_loop

.Lcheck_utf8_2byte:
    cmp al, 0xDF
    ja .Lskip_utf8_char
    lodsb                       
    mov al, '?'                 
    push ebx
    push ecx
    push edx
    call fb_put_char
    pop edx
    pop ecx
    pop ebx
    add ebx, 8
    jmp .Lprint_loop
    
.Lskip_utf8_char:
    lodsb                       
    lodsb                       
    mov al, '-'                 
    push ebx
    push ecx
    push edx
    call fb_put_char
    pop edx
    pop ecx
    pop ebx
    add ebx, 8
    jmp .Lprint_loop
    
.Lprint_done:
    pop esi
    pop ecx
    pop ebx
    pop eax
    ret


fb_put_char:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp
    
    mov edi, [fb_addr]
    test edi, edi
    jz .Lchar_done
    
    movzx eax, al
    sub eax, 32                 
    jl .Lchar_done              
    cmp eax, 95
    jg .Lchar_done              
    
    shl eax, 3                  
    lea esi, [font_8x8 + eax]   
    
    mov ebp, 8                  
    
.Lchar_row_loop:
    lodsb                       
    push ecx                    
    
    mov ah, 8                   
    push ebx                    
    
.Lchar_pixel_loop:
    test al, 0x80               
    jz .Lskip_pixel
    
    push eax
    
    mov eax, ecx                
    imul eax, [fb_pitch]        
    push ebx
    shl ebx, 2                  
    add eax, ebx
    pop ebx
    add eax, [fb_addr]
    
    mov [eax], edx              
    pop eax
    jmp .Lpixel_done

.Lskip_pixel:
    push eax
    mov eax, ecx                
    imul eax, [fb_pitch]
    push ebx
    shl ebx, 2
    add eax, ebx
    pop ebx
    add eax, [fb_addr]
    
    mov dword ptr [eax], 0x001428 
    pop eax

.Lpixel_done:
    shl al, 1                   
    inc ebx                     
    dec ah
    jnz .Lchar_pixel_loop
    
    pop ebx                     
    pop ecx                     
    inc ecx                     
    
    dec ebp
    jnz .Lchar_row_loop
    
.Lchar_done:
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret


calc_vga_pos:
    push eax
    
    movzx ebx, ah               
    shl ebx, 3                  
    
    movzx ecx, al               
    shl ecx, 3                  
    pop eax
    ret


parse_multiboot2_fb:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    
    /* Multiboot2 info
     * [0-3] = total_size
     * [4-7] = reserved
     * [8+]  = tags...
     */
    
    add ebx, 8
    
.Lparse_tag_loop:
    mov eax, [ebx]              /* tag type */
    mov ecx, [ebx + 4]          /* tag size */
    
    test eax, eax
    jz .Lparse_done
    
    cmp eax, 8
    je .Lfound_fb_tag
    
    add ecx, 7
    and ecx, ~7
    add ebx, ecx
    jmp .Lparse_tag_loop
    
.Lfound_fb_tag:
    /* 
     * [0-3]   = type (8)
     * [4-7]   = size
     * [8-15]  = framebuffer_addr (64-bit)
     * [16-19] = framebuffer_pitch
     * [20-23] = framebuffer_width
     * [24-27] = framebuffer_height
     * [28]    = framebuffer_bpp
     * [29]    = framebuffer_type
     */
    
    mov eax, [ebx + 8]
    mov [fb_addr], eax
    
    mov eax, [ebx + 16]
    mov [fb_pitch], eax
    
    mov eax, [ebx + 20]
    mov [fb_width], eax
    
    mov eax, [ebx + 24]
    mov [fb_height], eax
    
    movzx eax, byte ptr [ebx + 28]
    mov [fb_bpp], eax
    
.Lparse_done:
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

show_boot_menu:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    
    call clear_screen
    
    mov ebx, 160                
    mov ecx, 16                 
    mov edx, 0xFF00FF           
    mov esi, offset logo_line1
    call print_string
    
    mov ebx, 160
    mov ecx, 24
    mov edx, 0xFF00FF
    mov esi, offset logo_line2
    call print_string
    
    mov ebx, 160
    mov ecx, 32
    mov edx, 0xFF00FF
    mov esi, offset logo_line3
    call print_string
    
    mov ebx, 160
    mov ecx, 40
    mov edx, 0xFF00FF
    mov esi, offset logo_line4
    call print_string
    
    mov ebx, 160
    mov ecx, 48
    mov edx, 0xFF00FF
    mov esi, offset logo_line5
    call print_string
    
    mov ebx, 160
    mov ecx, 56
    mov edx, 0xFF00FF
    mov esi, offset logo_line6
    call print_string
    
    mov ebx, 176                
    mov ecx, 80                 
    mov edx, 0x00FFFF           
    mov esi, offset title_str
    call print_string
    
    call update_menu_display
    
    mov ebx, 120                
    mov ecx, 200                
    mov edx, 0xFFFF00           
    mov esi, offset subtitle_str
    call print_string
    
    call update_timer_display
    
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

update_menu_display:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    
    mov edi, 0                  
    
.Lmenu_item_loop:
    mov eax, edi
    shl eax, 4                  
    add eax, 100                
    mov ecx, eax                
    mov ebx, 144                
    
    cmp edi, [selected_item]
    je .Lselected
    mov edx, 0xAAAAAA           
    jmp .Ldraw_item
.Lselected:
    mov edx, 0x00FF00           
    
.Ldraw_item:
    cmp edi, 0
    je .Litem0
    cmp edi, 1
    je .Litem1
    cmp edi, 2
    je .Litem2
    cmp edi, 3
    je .Litem3
    jmp .Lnext_item
    
.Litem0:
    mov esi, offset menu_item0
    jmp .Lprint_item
.Litem1:
    mov esi, offset menu_item1
    jmp .Lprint_item
.Litem2:
    mov esi, offset menu_item2
    jmp .Lprint_item
.Litem3:
    mov esi, offset menu_item3
    
.Lprint_item:
    call print_string
    
.Lnext_item:
    inc edi
    cmp edi, MENU_ITEMS
    jl .Lmenu_item_loop
    
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

update_timer_display:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    
    mov ebx, 200                
    mov ecx, 220                
    mov edx, 0xFF4444           
    mov esi, offset msg_timer
    call print_string
    
    mov ebx, 304                
    mov ecx, 220                
    mov edx, 0xFF4444           
    mov eax, [boot_timeout]
    add eax, '0'                
    call fb_put_char
    
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret


menu_loop:
    push eax
    push ebx
    push ecx
    
    call init_pit_timer
    
.Lmenu_main_loop:
    in al, KB_STATUS_PORT
    test al, 1                  
    jz .Lcheck_timer
    
    in al, KB_DATA_PORT
    
    test al, 0x80
    jnz .Lmenu_main_loop

    mov dword ptr [boot_timeout], 0xFFFFFFFF
    
    push eax
    mov ebx, 200
    mov ecx, 220
    mov edx, 0xAAAAAA
    mov esi, offset .Lmsg_manual
    call print_string
    pop eax

    
    cmp al, KEY_UP
    je .Lmove_up
    cmp al, KEY_DOWN
    je .Lmove_down
    cmp al, KEY_ENTER
    je .Lselect_item
    cmp al, KEY_ESC
    je .Lmenu_main_loop         
    cmp al, KEY_R
    je .Lreboot
    
    jmp .Lmenu_main_loop

.Lmsg_manual: .asciz "Manual mode active.       "

.Lstop_timer:                   
    jmp .Lmenu_main_loop
    
.Lmove_up:
    mov eax, [selected_item]
    test eax, eax
    jz .Lwrap_to_bottom
    dec dword ptr [selected_item]
    jmp .Lrefresh_menu
    
.Lwrap_to_bottom:
    mov dword ptr [selected_item], MENU_ITEMS - 1
    jmp .Lrefresh_menu
    
.Lmove_down:
    mov eax, [selected_item]
    inc eax
    cmp eax, MENU_ITEMS
    jl .Ldown_ok
    xor eax, eax               
.Ldown_ok:
    mov [selected_item], eax
    
.Lrefresh_menu:
    call update_menu_display
    jmp .Lmenu_main_loop
    
.Lcheck_timer:
    
    cmp dword ptr [boot_timeout], 0xFFFFFFFF
    je .Lmenu_main_loop

    mov ecx, 0x200000           
.Ldelay_loop:
    nop
    dec ecx
    jnz .Ldelay_loop
    
    inc dword ptr [timer_ticks]
    cmp dword ptr [timer_ticks], 100 
    jl .Lmenu_main_loop         
    
    mov dword ptr [timer_ticks], 0
    
    mov eax, [boot_timeout]
    test eax, eax
    jz .Lauto_boot
    
    dec eax
    mov [boot_timeout], eax
    call update_timer_display
    
    jmp .Lmenu_main_loop
    
.Lauto_boot:
    mov dword ptr [selected_item], 0
    jmp .Lselect_item
    
.Lselect_item:
    mov eax, [selected_item]
    
    cmp eax, 0
    je .Lboot_normal
    cmp eax, 1
    je .Lboot_safe
    cmp eax, 2
    je .Lshow_sysinfo
    cmp eax, 3
    je .Lreboot
    
    jmp .Lmenu_main_loop
    
.Lboot_normal:
    pop ecx
    pop ebx
    pop eax
    ret                         
    
.Lboot_safe:
    
    pop ecx
    pop ebx
    pop eax
    ret
    
.Lshow_sysinfo:
    call show_system_info
    call show_boot_menu
    jmp .Lmenu_main_loop
    
.Lreboot:
    call do_reboot

init_pit_timer:
    push eax
    
    mov al, 0x36                
    out PIT_COMMAND, al
    
    mov ax, (PIT_FREQ / TARGET_FREQ)
    out PIT_CHANNEL0, al
    mov al, ah
    out PIT_CHANNEL0, al
    
    pop eax
    ret

show_system_info:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    
    call clear_screen
    
    mov ebx, 216                
    mov ecx, 16                 
    mov edx, 0x00FFFF           
    mov esi, offset sysinfo_title
    call print_string
    
    mov ebx, 40                 
    mov ecx, 48                 
    mov edx, 0xFFFF00           
    mov esi, offset sysinfo_cpu
    call print_string
    
    mov ebx, 136                
    mov edx, 0x00FF00           
    mov esi, offset cpu_vendor
    call print_string
    
    mov ebx, 40                 
    mov ecx, 72                 
    mov edx, 0xFFFF00           
    mov esi, offset sysinfo_mb_ver
    call print_string
    
    mov eax, [multiboot_magic_val]
    cmp eax, MB2_BOOTLOADER_MAGIC
    je .Lmb2_info
    mov esi, offset sysinfo_mb1
    jmp .Lprint_mb_ver
.Lmb2_info:
    mov esi, offset sysinfo_mb2
.Lprint_mb_ver:
    mov ebx, 128                
    mov edx, 0x00FF00           
    call print_string
    
    mov ebx, 40                 
    mov ecx, 96                 
    mov edx, 0xFFFF00           
    mov esi, offset sysinfo_long_mode
    call print_string
    
    mov ebx, 128                
    mov edx, 0x00FF00          
    mov esi, offset sysinfo_yes
    call print_string
    
    mov ebx, 160                
    mov ecx, 200                
    mov edx, 0xAAAAAA           
    mov esi, offset sysinfo_press_key
    call print_string
    
.Lwait_key_sysinfo:
    in al, KB_STATUS_PORT
    test al, 1
    jz .Lwait_key_sysinfo
    in al, KB_DATA_PORT         
    test al, 0x80               
    jnz .Lwait_key_sysinfo
    
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret


do_reboot:
    push ebx
    push ecx
    push edx
    push esi
    
    mov ebx, 240                
    mov ecx, 100                
    mov edx, 0xFFFF00           
    mov esi, offset msg_rebooting
    call print_string
    
    mov ecx, 0x1000000
.Lreboot_delay:
    dec ecx
    jnz .Lreboot_delay
    
    mov al, 0xFE
    out 0x64, al
    
    lidt [.Lnull_idt]
    int3
    
.Lnull_idt:
    .word 0
    .long 0
    
    cli
    hlt
    jmp .


check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 0x200000           
    push eax
    popfd
    
    pushfd
    pop eax
    push ecx
    popfd                       
    
    xor eax, ecx
    jz .Lno_cpuid_support
    mov eax, 1
    ret
    
.Lno_cpuid_support:
    xor eax, eax
    ret

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .Lno_long_mode
    
    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)        
    jz .Lno_long_mode
    
    mov eax, 1
    ret
    
.Lno_long_mode:
    xor eax, eax
    ret


get_cpu_info:
    push eax
    push ebx
    push ecx
    push edx
    push edi
    
    xor eax, eax
    cpuid
    
    mov edi, offset cpu_vendor
    mov [edi], ebx              
    mov [edi + 4], edx          
    mov [edi + 8], ecx          
    mov byte ptr [edi + 12], 0  
    
    pop edi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

error_no_multiboot:
    call clear_screen
    call draw_error_box
    mov ebx, 80                 
    mov ecx, 100                
    mov edx, 0xFF0000           
    mov esi, offset err_no_multiboot
    call print_string
    jmp halt_system

error_no_cpuid:
    call clear_screen
    call draw_error_box
    mov ebx, 120                
    mov ecx, 100                
    mov edx, 0xFF0000           
    mov esi, offset err_no_cpuid
    call print_string
    jmp halt_system

error_no_long_mode:
    call clear_screen
    call draw_error_box
    mov ebx, 80                 
    mov ecx, 100                
    mov edx, 0xFF0000           
    mov esi, offset err_no_long_mode
    call print_string
    jmp halt_system

draw_error_box:
    push ebx
    push ecx
    push edx
    push esi
    
   
    mov ebx, 240                
    mov ecx, 64                 
    mov edx, 0xFF4444           
    mov esi, offset .Lerr_title
    call print_string
    
    pop esi
    pop edx
    pop ecx
    pop ebx
    ret

.Lerr_title:
    .asciz "!!! BOOT ERROR !!!"

halt_system:
    cli
    hlt
    jmp halt_system

start_boot_sequence:
.Lflush_kb:
    in al, KB_STATUS_PORT
    test al, 1
    jz .Lkb_flushed
    in al, KB_DATA_PORT
    jmp .Lflush_kb
.Lkb_flushed:

    call clear_screen
    
    mov al, 5
    mov ah, 5
    call calc_vga_pos
    mov esi, offset msg_loading_kernel
    mov ah, ATTR_INFO
    call print_string
    
    call setup_page_tables
    
    mov al, 5
    mov ah, 5
    call calc_vga_pos
    mov esi, offset msg_done
    mov ah, ATTR_SUCCESS
    call print_string
    
    mov al, 7
    mov ah, 5
    call calc_vga_pos
    mov esi, offset msg_init_memory
    mov ah, ATTR_INFO
    call print_string
    
    mov ecx, 0x400000
.Ldelay1:
    dec ecx
    jnz .Ldelay1
    
    mov al, 7
    mov ah, 5
    call calc_vga_pos
    mov esi, offset msg_done
    mov ah, ATTR_SUCCESS
    call print_string
    
    mov al, 9
    mov ah, 5
    call calc_vga_pos
    mov esi, offset msg_switch_mode
    mov ah, ATTR_INFO
    call print_string
    
    mov ecx, 0x400000
.Ldelay2:
    dec ecx
    jnz .Ldelay2
    
    mov al, 9
    mov ah, 5
    call calc_vga_pos
    mov esi, offset msg_done
    mov ah, ATTR_SUCCESS
    call print_string
    
    mov al, 12
    mov ah, 5
    call calc_vga_pos
    mov esi, offset msg_booting
    mov ah, ATTR_SUCCESS
    call print_string
    
    mov ecx, 0x800000
.Ldelay3:
    dec ecx
    jnz .Ldelay3
    
    jmp enable_long_mode

setup_page_tables:
    
    mov eax, offset p3_table
    or eax, 0b11                
    mov [p4_table], eax

    mov eax, offset p2_table
    or eax, 0b11
    mov [p3_table + 0], eax

    mov eax, offset p2_table_1
    or eax, 0b11
    mov [p3_table + 8], eax

    mov eax, offset p2_table_2
    or eax, 0b11
    mov [p3_table + 16], eax

    mov eax, offset p2_table_3
    or eax, 0b11
    mov [p3_table + 24], eax

    mov ecx, 0
.Lmap_p2:
    mov eax, 0x200000           
    mul ecx
    or eax, 0b10000011          
    mov [p2_table + ecx * 8], eax
    
    inc ecx
    cmp ecx, 2048               
    jne .Lmap_p2
    
    ret


enable_long_mode:
    
    mov eax, cr4
    or eax, 1 << 5              /* CR4.PAE */
    mov cr4, eax

    mov ecx, 0xC0000080         /* EFER MSR */
    rdmsr
    or eax, 1 << 8              /* EFER.LME */
    wrmsr

    
    mov eax, offset p4_table
    mov cr3, eax

    
    mov eax, cr0
    or eax, 1 << 31            
    mov cr0, eax

    lgdt [pointer64]
    push 0x08
    push offset start64
    retf

.code64
start64:
    mov ax, 0x10
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rdi, [multiboot_magic_val]
    mov rsi, [multiboot_info_ptr]
    mov edx, [selected_item]

    .extern kmain
    call kmain

    cli
    hlt
    jmp .
