; VCPITEST.COM - cliente VCPI mínimo: pide la interfaz (DE01h), una página
; (DE04h), monta su directorio y su tabla de páginas, pasa a modo protegido
; (DE0Ch), escribe en la página por una dirección lineal propia, llama al
; servidor desde el modo protegido (DE03h), vuelve al modo V86 y lo cuenta.
        cpu 386
        bits 16
        org 0x100

SEL_VCPI    equ 0x08                ; tres descriptores del servidor
SEL_CODE    equ 0x20
SEL_DATA    equ 0x28
SEL_TSS     equ 0x30
SEL_FLAT    equ 0x38
PAGELIN     equ 0x300000            ; donde se ve la página pedida

%macro SAY 1
        mov si, %1
        call puts
%endmacro

        mov ax, 0xDE00
        int 0x67
        test ah, ah
        jnz novcpi
        cmp bx, 0x0100
        jne fail
        SAY m_det
        ; directorio y tabla de páginas, alineados a 4 KB, tras el programa
        mov ax, cs
        add ax, 0x1100
        and ax, 0xFF00
        mov [dirseg], ax
        mov es, ax
        xor di, di
        xor ax, ax
        mov cx, 0x1000              ; 8 KB a cero
        rep stosw
        mov ax, [dirseg]
        add ax, 0x100
        mov [tabseg], ax
        ; páginas libres y una para nosotros
        mov ax, 0xDE03
        int 0x67
        test ah, ah
        jnz fail
        mov [free0], edx
        mov ax, 0xDE04
        int 0x67
        test ah, ah
        jnz fail
        mov [page], edx
        mov ax, 0xDE03
        int 0x67
        mov eax, [free0]
        dec eax
        cmp eax, edx
        jne fail
        SAY m_page
        ; la interfaz: tabla de páginas y descriptores del servidor
        mov es, [tabseg]
        xor di, di
        mov si, gdt + SEL_VCPI
        mov ax, 0xDE01
        int 0x67
        test ah, ah
        jnz fail
        mov [ventry], ebx
        cmp di, 0x400               ; como poco, el primer mega
        jb fail
        cmp di, 0xC00
        ja fail
        ; nuestra página, en PAGELIN
        mov eax, [page]
        or al, 7
        mov [es:(PAGELIN >> 12) * 4], eax
        ; directorio: la entrada 0 es la tabla (dirección física: DE06h)
        mov cx, [tabseg]
        shr cx, 8
        mov ax, 0xDE06
        int 0x67
        test ah, ah
        jnz fail
        or dl, 7
        mov es, [dirseg]
        mov [es:0], edx
        mov cx, [dirseg]
        shr cx, 8
        mov ax, 0xDE06
        int 0x67
        mov [sw_cr3], edx
        ; descriptores propios
        xor eax, eax
        mov ax, cs
        mov [v86seg], ax
        shl eax, 4
        mov [base], eax
        mov [gdt+SEL_CODE+2], ax
        mov [gdt+SEL_DATA+2], ax
        shr eax, 16
        mov [gdt+SEL_CODE+4], al
        mov [gdt+SEL_DATA+4], al
        mov eax, [base]
        add eax, tss
        mov [gdt+SEL_TSS+2], ax
        shr eax, 16
        mov [gdt+SEL_TSS+4], al
        mov eax, [base]
        add eax, gdt
        mov [gdtr+2], eax
        mov eax, [base]
        add eax, gdtr
        mov [sw_gdtr], eax
        mov eax, [base]
        add eax, idtr
        mov [sw_idtr], eax
        mov [v86sp], sp
        ; ¡allá vamos!
        cli
        mov esi, [base]
        add esi, sw_cr3
        mov ax, 0xDE0C
        int 0x67
        jmp fail                    ; no debe volver por aquí

; ---- modo protegido (16 bits, anillo 0) ----
pm_entry:
        mov ax, SEL_DATA
        mov ds, ax
        mov ss, ax
        mov esp, pmstack
        mov ax, SEL_FLAT
        mov es, ax
        mov dword [es:dword PAGELIN], 0x12345678
        mov dword [es:dword PAGELIN + 0xFFC], 0x9ABCDEF0
        mov eax, [es:dword PAGELIN]
        mov [seen], eax
        ; el primer mega se ve igual: el texto del programa, por FLAT
        mov ebx, [base]
        mov eax, [es:ebx + m_det]
        mov [seen2], eax
        ; llamar al servidor desde aquí
        mov ax, 0xDE03
        call dword far [ventry]
        mov [pmfree], edx
        mov [pmstat], ah
        ; volver al modo V86
        movzx eax, word [v86seg]
        push eax                    ; GS
        push eax                    ; FS
        push eax                    ; DS
        push eax                    ; ES
        push eax                    ; SS
        movzx ebx, word [v86sp]
        push ebx                    ; ESP
        push dword 0                ; EFLAGS (lo pone el servidor)
        push eax                    ; CS
        push dword v86_back         ; EIP
        mov ax, 0xDE0C
        call dword far [ventry]

; ---- otra vez en modo V86 ----
v86_back:
        sti
        cmp dword [seen], 0x12345678
        jne fail
        mov eax, [m_det]
        cmp [seen2], eax
        jne fail
        cmp byte [pmstat], 0
        jne fail
        mov eax, [free0]
        dec eax
        cmp [pmfree], eax
        jne fail
        SAY m_pm
        ; devolver la página
        mov edx, [page]
        mov ax, 0xDE05
        int 0x67
        test ah, ah
        jnz fail
        mov ax, 0xDE03
        int 0x67
        cmp edx, [free0]
        jne fail
        mov edx, [page]
        mov ax, 0xDE05              ; dos veces no
        int 0x67
        test ah, ah
        jz fail
        ; otros servicios
        mov ax, 0xDE0A
        int 0x67
        cmp bx, 0x08
        jne fail
        cmp cx, 0x70
        jne fail
        mov ax, 0xDE07
        int 0x67
        test bl, 1
        jz fail
        mov ax, 0xDE02
        int 0x67
        test edx, edx
        jz fail
        SAY m_ok
        mov ax, 0x4C00
        int 0x21
novcpi:
        SAY m_no
        mov ax, 0x4C02
        int 0x21
fail:
        push ax
        SAY m_fail
        pop ax
        call hex4
        SAY m_crlf
        mov ax, 0x4C01
        int 0x21

puts:
        lodsb
        test al, al
        jz .r
        mov dl, al
        mov ah, 2
        int 0x21
        jmp puts
.r:
        ret
hex4:
        mov cx, 4
.l:
        rol ax, 4
        push ax
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        mov dl, al
        mov ah, 2
        int 0x21
        pop ax
        loop .l
        ret

m_det       db "VCPI detectado OK", 13, 10, 0
m_page      db "VCPI paginas OK", 13, 10, 0
m_pm        db "VCPI modo protegido OK", 13, 10, 0
m_ok        db "VCPI test OK", 13, 10, 0
m_no        db "VCPI: no hay servidor", 13, 10, 0
m_fail      db "VCPI FALLO AX=", 0
m_crlf      db 13, 10, 0
            align 4
dirseg      dw 0
tabseg      dw 0
v86seg      dw 0
v86sp       dw 0
base        dd 0
free0       dd 0
page        dd 0
seen        dd 0
seen2       dd 0
pmfree      dd 0
pmstat      db 0
            align 4
ventry      dd 0
            dw SEL_VCPI
            align 4
; lo que pide DE0Ch
sw_cr3      dd 0
sw_gdtr     dd 0
sw_idtr     dd 0
            dw 0                    ; LDTR
            dw SEL_TSS
            dd pm_entry
            dw SEL_CODE
            align 4
gdtr        dw gdt_end - gdt - 1
            dd 0
idtr        dw 0
            dd 0
            align 8
gdt         dq 0
            dq 0, 0, 0              ; los del servidor
            dq 0x00009A000000FFFF   ; código (16 bits)
            dq 0x000092000000FFFF   ; datos y pila
            dq 0x0000890000000067   ; TSS
            dq 0x00CF92000000FFFF   ; todo el espacio lineal
gdt_end:
tss         times 104 db 0
            times 256 db 0
pmstack:
