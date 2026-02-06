
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
    mov ecx, edx    
    mov dx, di     
    mov rdi, rsi    
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
    mov ecx, edx    
    mov dx, di     
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



section .text

global cinfo
cinfo:
    push rbx
    push rdi 

    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000004
    jl .vendor

    ; Get Brand
    mov eax, 0x80000002
    cpuid
    mov [rdi], eax
    mov [rdi+4], ebx
    mov [rdi+8], ecx
    mov [rdi+12], edx

    mov eax, 0x80000003
    cpuid
    mov [rdi+16], eax
    mov [rdi+20], ebx
    mov [rdi+24], ecx
    mov [rdi+28], edx

    mov eax, 0x80000004
    cpuid
    mov [rdi+32], eax
    mov [rdi+36], ebx
    mov [rdi+40], ecx
    mov [rdi+44], edx
    mov byte [rdi+48], 0
    
    jmp .done

.vendor:
    xor eax, eax
    cpuid
    mov [rdi], ebx
    mov [rdi+4], edx
    mov [rdi+8], ecx
    mov byte [rdi+12], 0

.done:
    pop rdi
    pop rbx
    ret

global rinfo
rinfo:
    
    
    mov rax, 0
    
    test rdi, rdi 
    jz .end
    
    mov ecx, [rdi] 
    test ecx, 1
    jz .end

    mov ecx, [rdi+4] 
    mov edx, [rdi+8] 
    
    mov eax, ecx
    add eax, edx
    add eax, 1024
    
.end:
    ret
