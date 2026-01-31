section .text
global inb
inb:
%ifdef __x86_64__
    mov dx, di          
    xor eax, eax        
    in al, dx           
    ret
%else
    push ebp
    mov ebp, esp
    mov dx, [ebp+8]     
    xor eax, eax        
    in al, dx           
    pop ebp
    ret
%endif
global outb
outb:
%ifdef __x86_64__
    mov dx, di          
    mov al, sil         
    out dx, al          
    ret
%else
    push ebp
    mov ebp, esp
    mov dx, [ebp+8]     
    mov al, [ebp+12]    
    out dx, al          
    pop ebp
    ret
%endif

global inw
inw:
%ifdef __x86_64__
    mov dx, di
    xor eax, eax
    in ax, dx
    ret
%else
    push ebp
    mov ebp, esp
    mov dx, [ebp+8]
    xor eax, eax
    in ax, dx
    pop ebp
    ret
%endif

global outw
outw:
%ifdef __x86_64__
    mov dx, di
    mov ax, si
    out dx, ax
    ret
%else
    push ebp
    mov ebp, esp
    mov dx, [ebp+8]
    mov ax, [ebp+12]
    out dx, ax
    pop ebp
    ret
%endif

global insw
insw:
%ifdef __x86_64__
    push rdi
    mov dx, di     
    mov rdi, rsi    
    mov rcx, rdx    
    cld
    rep insw
    pop rdi
    ret
%else
    push ebp
    mov ebp, esp
    push edi
    mov dx, [ebp+8]
    mov edi, [ebp+12]
    mov ecx, [ebp+16]
    cld
    rep insw
    pop edi
    pop ebp
    ret
%endif

global outsw
outsw:
%ifdef __x86_64__
    mov dx, di     
    mov rcx, rdx    
    cld
    rep outsw
    ret
%else
    push ebp
    mov ebp, esp
    push esi
    mov dx, [ebp+8]
    mov esi, [ebp+12]
    mov ecx, [ebp+16]
    cld
    rep outsw
    pop esi
    pop ebp
    ret
%endif
