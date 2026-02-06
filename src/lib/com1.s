
[bits 16]


COM1_PORT equ 0x3F8
COM1_DATA equ COM1_PORT + 0
COM1_INT_ENABLE equ COM1_PORT + 1
COM1_FIFO_CTRL equ COM1_PORT + 2
COM1_LINE_CTRL equ COM1_PORT + 3
COM1_MODEM_CTRL equ COM1_PORT + 4
COM1_LINE_STATUS equ COM1_PORT + 5

com1_init:
    push ax
    push dx
    
    mov dx, COM1_INT_ENABLE
    mov al, 0x00
    out dx, al              
    
    mov dx, COM1_LINE_CTRL
    mov al, 0x80
    out dx, al              
    
    mov dx, COM1_DATA
    mov al, 0x01
    out dx, al              
    
    mov dx, COM1_INT_ENABLE
    mov al, 0x00
    out dx, al              
    
    mov dx, COM1_LINE_CTRL
    mov al, 0x03
    out dx, al              
    
    mov dx, COM1_FIFO_CTRL
    mov al, 0xC7
    out dx, al              
    
    mov dx, COM1_MODEM_CTRL
    mov al, 0x0B
    out dx, al              
    
    pop dx
    pop ax
    ret

com1_write_byte:
    push dx
    push ax
    
    mov dx, COM1_LINE_STATUS
.wait:
    in al, dx
    test al, 0x20           
    jz .wait
    
    pop ax
    mov dx, COM1_DATA
    out dx, al
    
    pop dx
    ret

com1_write_string:
    push ax
    push si
    
.loop:
    lodsb
    test al, al
    jz .done
    call com1_write_byte
    jmp .loop
    
.done:
    pop si
    pop ax
    ret

com1_write_hex_byte:
    push ax
    push bx
    
    mov bx, ax
    shr al, 4
    call .print_nibble
    
    mov ax, bx
    and al, 0x0F
    call .print_nibble
    
    pop bx
    pop ax
    ret
    
.print_nibble:
    cmp al, 9
    jle .digit
    add al, 'A' - 10
    jmp .print
.digit:
    add al, '0'
.print:
    call com1_write_byte
    ret

com1_write_hex_word:
    push ax
    
    mov al, ah
    call com1_write_hex_byte
    
    pop ax
    call com1_write_hex_byte
    ret

com1_newline:
    push ax
    mov al, 0x0D
    call com1_write_byte
    mov al, 0x0A
    call com1_write_byte
    pop ax
    ret

com1_read_byte:
    push dx
    
    mov dx, COM1_LINE_STATUS
.wait:
    in al, dx
    test al, 0x01           
    jz .wait
    
    mov dx, COM1_DATA
    in al, dx
    
    pop dx
    ret

com1_has_data:
    push dx
    
    mov dx, COM1_LINE_STATUS
    in al, dx
    and al, 0x01
    
    pop dx
    ret
