; LEETEST.COM - lee DATOS.BIN (de la carpeta actual) como lo hacen los
; cargadores de los juegos (DOS/4GW y compañía): saltos con AH=42h a sitios
; que no caen en frontera de sector y lecturas de tamaños variados, seguidas
; y salteadas. El fichero lleva en cada posición un valor que depende solo de
; la posición (la palabra doble número N vale N), así que se sabe qué tiene
; que salir en cada octeto leído. Lo usa tests/run_memprog_tests.sh, sobre un
; CD (HSPCD + ISO 9660) y sobre un disquete.
; De paso comprueba que INT 21h (AH=42h y 3Fh) conserva la mitad alta de los
; registros de 32 bits.
; Escribe «LEETEST OK» o «LEETEST FALLO en XXXXXXXX (lectura n)».
        cpu 386
        bits 16
        org 0x100

        mov sp, 0xE000              ; (el final del segmento es para el búfer)
        mov ax, 0x5A5A              ; FS y GS también tienen que volver igual
        mov fs, ax
        mov ax, 0xA5A5
        mov gs, ax
        mov dx, nombre
        mov ax, 0x3D00
        int 0x21
        jc near noabre
        mov [h], ax
        mov word [dest], buf
        mov si, tabla
        call pasada
        ; lo mismo con el búfer pegado al final del segmento (la lectura acaba
        ; justo en el octeto FFFFh: los extensores de DOS lo hacen)
        mov word [dest], 0xF000
        mov si, tabla2
        call pasada
        ; y con el búfer en medio, comprobando que no se escribe ni un octeto
        ; más allá de lo pedido (16 octetos de guarda detrás)
        mov word [dest], 0x9000
        mov byte [guarda], 1
        mov si, tabla3
        call pasada
        ; abrir para leer y escribir (AL=2) tiene que poderse también en un
        ; CD, aunque sea de solo lectura: muchos juegos abren así sus datos
        mov dx, nombre
        mov ax, 0x3D02
        int 0x21
        mov dx, m_rw
        jc fin
        mov bx, ax
        mov ah, 0x3E
        int 0x21
        mov dx, m_ok
        cmp byte [altas], 0
        je fin
        mov dx, m_altas
        cmp byte [altas], 1
        je fin
        mov dx, m_fsgs
        jmp fin

; INT 21h comprobando que vuelve con la mitad alta de EAX, EBX, ECX, EDX, ESI,
; EDI y EBP como estaba (MS-DOS no las toca; los extensores de DOS de 32 bits
; cuentan con ello)
dos21:
        and eax, 0xFFFF
        or eax, 0x11110000
        and ebx, 0xFFFF
        or ebx, 0x22220000
        and ecx, 0xFFFF
        or ecx, 0x33330000
        and edx, 0xFFFF
        or edx, 0x44440000
        and esi, 0xFFFF
        or esi, 0x55550000
        and edi, 0xFFFF
        or edi, 0x66660000
        and ebp, 0xFFFF
        or ebp, 0x77770000
        int 0x21
        pushf
        pushad
        mov bp, sp
        cmp word [bp+2], 0x6666     ; EDI
        jne .no
        cmp word [bp+6], 0x5555     ; ESI
        jne .no
        cmp word [bp+10], 0x7777    ; EBP
        jne .no
        cmp word [bp+18], 0x2222    ; EBX
        jne .no
        cmp word [bp+22], 0x4444    ; EDX
        jne .no
        cmp word [bp+26], 0x3333    ; ECX
        jne .no
        cmp word [bp+30], 0x1111    ; EAX
        jne .no
        mov ax, fs
        cmp ax, 0x5A5A
        jne .nofs
        mov ax, gs
        cmp ax, 0xA5A5
        je .si
.nofs:
        mov byte [altas], 2
        jmp .si
.no:
        mov byte [altas], 1
.si:
        popad
        popf
        ret

; recorre la tabla de SI leyendo en [dest]
pasada:
.sig:
        mov eax, [si]               ; posición (FFFFFFFFh = seguir donde está)
        mov cx, [si+4]              ; octetos (0 = fin de la tabla)
        add si, 6
        test cx, cx
        jnz .hay
        ret
.hay:
        inc word [nlect]
        cmp eax, 0xFFFFFFFF
        je .lee
        mov [pos], eax
        mov dx, ax
        shr eax, 16
        push cx
        mov cx, ax
        mov bx, [h]
        mov ax, 0x4200
        call dos21
        pop cx
        jc near mal
.lee:
        mov bx, [h]
        mov dx, [dest]
        cmp cx, 0x1000
        jbe .cabe
        mov dx, buf                 ; (las grandes, siempre en el búfer normal)
.cabe:
        cmp dx, 0xF000
        jne .dok
        mov dx, cx                  ; pegada al final: acaba en FFFFh
        neg dx
.dok:
        mov [dact], dx
        cmp byte [guarda], 0
        je .sing
        push di
        push cx
        mov di, dx
        add di, cx
        mov cx, 16
        mov al, 0xEE
        rep stosb
        pop cx
        pop di
.sing:
        mov ah, 0x3F
        call dos21
        jc near mal
        cmp ax, cx
        jne near mal
        ; comprobar cada octeto
        mov di, [dact]
.cmp:
        mov eax, [pos]
        mov bl, al
        and bl, 3                   ; octeto dentro de la palabra doble
        shr eax, 2                  ; su número
        push cx
        mov cl, bl
        shl cl, 3
        shr eax, cl
        pop cx
        cmp al, [di]
        jne mal
        inc di
        inc dword [pos]
        loop .cmp
        cmp byte [guarda], 0
        je .sig
        mov cx, 16
.g:
        cmp byte [di], 0xEE
        jne mal
        inc di
        loop .g
        jmp .sig
noabre:
        mov dx, m_noabre
        jmp fin
mal:
        mov dx, m_mal
        mov ah, 9
        int 0x21
        mov eax, [pos]
        call hex8
        mov dx, m_lect
        mov ah, 9
        int 0x21
        mov ax, [nlect]
        call hex4
        mov dx, m_fin
fin:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21

hex8:
        push eax
        shr eax, 16
        call hex4
        pop eax
hex4:
        mov cx, 4
.h:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        mov dl, al
        mov ah, 2
        int 0x21
        pop ax
        loop .h
        ret

nombre      db "DATOS.BIN", 0
m_ok        db "LEETEST OK", 13, 10, "$"
m_altas     db "LEETEST FALLO: INT 21h no conserva la mitad alta de los registros de 32 bits", 13, 10, "$"
m_fsgs      db "LEETEST FALLO: INT 21h no conserva FS o GS", 13, 10, "$"
altas       db 0
m_rw        db "LEETEST FALLO: no se puede abrir DATOS.BIN para leer y escribir (AL=2)", 13, 10, "$"
m_noabre    db "LEETEST: no se puede abrir DATOS.BIN", 13, 10, "$"
m_mal       db "LEETEST FALLO en $"
m_lect      db " (lectura $"
m_fin       db ")", 13, 10, "$"
h           dw 0
dest        dw 0
guarda      db 0
dact        dw 0
nlect       dw 0
pos         dd 0

%macro LEE 2
        dd %1
        dw %2
%endmacro
SIGUE       equ 0xFFFFFFFF
tabla:
        ; la cabecera y unos trozos pequeños, a saltos (ida y vuelta)
        LEE 0, 0x1C
        LEE 0x1F264, 6
        LEE 0, 0xB0
        LEE 0x1F264, 0xB0
        LEE 0x1B624, 0xB0
        LEE 0x10D74, 0xB0
        LEE 0x32C2C, 0x40
        LEE 0x10D74, 0x40
        LEE 0x32E6C, 0x0B
        LEE 0x32E77, 1
        LEE 0x3653D, 0x37
        LEE 0x32CF0, 0x30
        LEE 0x32D20, 0x14C
        LEE 0x32E78, 0x150
        ; 19 lecturas seguidas de 4 KB desde un sitio que no es frontera de
        ; sector (ni de 512 ni de 2048) y el resto
        LEE 0x32FC8, 0x1000
%rep 18
        LEE SIGUE, 0x1000
%endrep
        LEE SIGUE, 0x575
        ; de 4 KB en 4 KB con un salto delante de cada una, cruzando 64 KB
%assign p 0x46574
%rep 82
        LEE p, 0x1000
%assign p p + 0x1000
%endrep
        ; lecturas grandes y pequeñas mezcladas, y el final del fichero
        LEE 0x7FFF, 0x2000
        LEE SIGUE, 0x8000
        LEE SIGUE, 1
        LEE SIGUE, 0x7FF
        LEE SIGUE, 0x801
        LEE 0xFFFF, 2
        LEE 0x1FFFE, 4
        LEE 0x9FFF0, 0x4000
        LEE 0xAAC00 - 0x123, 0x123
        LEE 1, 0x7FFF
        LEE 0, 0
tabla2:
        LEE 0x32FC8, 0x1000
        LEE SIGUE, 0x1000
        LEE SIGUE, 0x575
        LEE 0x46574, 0x1000
        LEE 0x47574, 0x1000
        LEE 0x12345, 0x800
        LEE SIGUE, 0xFFF
        LEE SIGUE, 1
        LEE 0x7FFFF, 0x801
        LEE 0, 0
tabla3:
        LEE 0x32FC8, 0x1000
        LEE SIGUE, 0x1000
        LEE SIGUE, 0x575
%assign p 0x46574
%rep 40
        LEE p, 0x1000
%assign p p + 0x1000
%endrep
        LEE 0x10D74, 0xB0
        LEE 0x3653D, 0x37
        LEE 0x32E77, 1
        LEE 0, 0
buf:
