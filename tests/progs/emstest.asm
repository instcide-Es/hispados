; EMSTEST.COM - prueba del gestor EMS (HSPEMM): detección por el dispositivo
; EMMXXXX0 y por el vector de INT 67h, páginas, mapa, contexto, nombres,
; reasignar, mover y liberar. Cada paso escribe «EMS <algo> OK».
        cpu 386
        bits 16
        org 0x100

%macro EMS 0
        int 0x67
        test ah, ah
        jnz fail
%endmacro
%macro SAY 1
        mov si, %1
        call puts
%endmacro

        ; --- detección 1: abrir el dispositivo EMMXXXX0 ---
        mov dx, devname
        mov ax, 0x3D00
        int 0x21
        jc nodev
        mov bx, ax
        mov ax, 0x4400
        int 0x21
        jc nodev
        test dl, 0x80               ; tiene que ser un dispositivo
        jz nodev
        mov ax, 0x4407
        int 0x21
        jc nodev
        cmp al, 0xFF
        jne nodev
        mov ah, 0x3E
        int 0x21
        ; --- detección 2: el nombre en el segmento del vector 67h ---
        mov ax, 0x3567
        int 0x21
        mov di, 10
        mov si, devname
        mov cx, 8
        repe cmpsb
        jne nodev
        push cs
        pop es
        SAY m_det
        ; --- estado, versión, marco y páginas ---
        mov ah, 0x40
        EMS
        mov ah, 0x46
        EMS
        cmp al, 0x40
        jne fail
        mov ah, 0x41
        EMS
        mov [frame], bx
        SAY m_frame
        mov ax, [frame]
        call hex4
        SAY m_crlf
        mov ah, 0x42
        EMS
        mov [free0], bx
        cmp bx, 8
        jb fail
        cmp dx, bx
        jb fail
        ; --- pedir 6 páginas y marcarlas ---
        mov ah, 0x43
        mov bx, 6
        EMS
        mov [h1], dx
        mov ah, 0x42
        EMS
        mov ax, [free0]
        sub ax, 6
        cmp ax, bx
        jne fail
        xor bx, bx                  ; página lógica
.mark:
        mov ax, 0x4400              ; en la página física 0
        mov dx, [h1]
        EMS
        mov es, [frame]
        xor di, di
        mov ax, bx
        add ax, 0x1111
        mov cx, 0x2000
        rep stosw
        inc bx
        cmp bx, 6
        jb .mark
        ; ver cada una en las cuatro páginas físicas
        xor bx, bx
.chk:
        mov al, bl
        and al, 3
        mov ah, 0x44
        mov dx, [h1]
        EMS
        mov ax, bx
        and ax, 3
        shl ax, 10
        add ax, [frame]
        mov es, ax
        mov ax, bx
        add ax, 0x1111
        cmp [es:0], ax
        jne fail
        cmp [es:0x3FFE], ax
        jne fail
        inc bx
        cmp bx, 6
        jb .chk
        SAY m_map
        ; errores: página lógica y física fuera de rango, handle malo
        mov ax, 0x4400
        mov bx, 6
        mov dx, [h1]
        int 0x67
        cmp ah, 0x8A
        jne fail
        mov ax, 0x4404
        xor bx, bx
        mov dx, [h1]
        int 0x67
        cmp ah, 0x8B
        jne fail
        mov ax, 0x4400
        xor bx, bx
        mov dx, 77
        int 0x67
        cmp ah, 0x83
        jne fail
        mov ah, 0x43
        xor bx, bx
        int 0x67
        cmp ah, 0x89
        jne fail
        mov ah, 0x43
        mov bx, 0xFFF0
        int 0x67
        cmp ah, 0x87
        jne fail
        SAY m_err
        ; --- guardar y recuperar el contexto (47h/48h) ---
        mov ax, 0x4400
        mov bx, 2
        mov dx, [h1]
        EMS                         ; página 2 en la física 0
        mov ah, 0x47
        mov dx, [h1]
        EMS
        mov ah, 0x47
        mov dx, [h1]
        int 0x67
        cmp ah, 0x8D                ; ya guardado
        jne fail
        mov ax, 0x4400
        mov bx, 5
        mov dx, [h1]
        EMS
        mov es, [frame]
        cmp word [es:0], 0x1116
        jne fail
        mov ah, 0x48
        mov dx, [h1]
        EMS
        cmp word [es:0], 0x1113
        jne fail
        mov ah, 0x48
        mov dx, [h1]
        int 0x67
        cmp ah, 0x8E
        jne fail
        SAY m_ctx
        ; --- mapa completo (4Eh) ---
        mov ax, 0x4E03
        EMS
        cmp al, 64
        ja fail
        push cs
        pop es
        mov di, mapbuf
        mov ax, 0x4E00
        EMS
        mov ax, 0x4400
        mov bx, 0xFFFF              ; quitar la página física 0
        mov dx, [h1]
        EMS
        mov si, mapbuf
        mov ax, 0x4E01
        EMS
        mov es, [frame]
        cmp word [es:0], 0x1113
        jne fail
        ; --- varias a la vez (50h), por número y por segmento ---
        mov word [multi], 4
        mov word [multi+2], 0
        mov word [multi+4], 3
        mov word [multi+6], 1
        mov si, multi
        mov cx, 2
        mov dx, [h1]
        mov ax, 0x5000
        EMS
        cmp word [es:0], 0x1115
        jne fail
        cmp word [es:0x4000], 0x1114
        jne fail
        mov word [multi], 0
        mov ax, [frame]
        add ax, 0x0C00
        mov [multi+2], ax
        mov si, multi
        mov cx, 1
        mov dx, [h1]
        mov ax, 0x5001
        EMS
        cmp word [es:0xC000], 0x1111
        jne fail
        SAY m_multi
        ; --- nombre del handle (53h/54h) ---
        mov si, hname
        mov dx, [h1]
        mov ax, 0x5301
        EMS
        push cs
        pop es
        mov di, mapbuf
        mov dx, [h1]
        mov ax, 0x5300
        EMS
        mov si, hname
        mov di, mapbuf
        mov cx, 8
        repe cmpsb
        jne fail
        mov si, hname
        mov ax, 0x5401
        EMS
        cmp dx, [h1]
        jne fail
        mov ax, 0x5402
        EMS
        cmp bx, 16
        jb fail
        SAY m_name
        ; --- segundo handle, reasignar (51h) y contar (4Bh, 4Ch) ---
        mov ah, 0x43
        mov bx, 3
        EMS
        mov [h2], dx
        mov ax, 0x4401
        mov bx, 2
        EMS
        mov es, [frame]
        mov word [es:0x4000], 0x2222
        mov ah, 0x51
        mov dx, [h1]
        mov bx, 10                  ; crece: el tramo de h1 se muda
        EMS
        cmp bx, 10
        jne fail
        mov ax, 0x4400
        mov bx, 4
        mov dx, [h1]
        EMS
        cmp word [es:0], 0x1115     ; lo viejo sigue
        jne fail
        mov ax, 0x4400
        mov bx, 9
        mov dx, [h1]
        EMS
        mov word [es:0], 0x9999
        mov ax, 0x4402
        mov bx, 2
        mov dx, [h2]
        EMS
        cmp word [es:0x8000], 0x2222
        jne fail
        mov ah, 0x51
        mov dx, [h1]
        mov bx, 2                   ; encoge
        EMS
        mov ah, 0x4C
        mov dx, [h1]
        EMS
        cmp bx, 2
        jne fail
        mov ah, 0x4B
        EMS
        cmp bx, 3                   ; el 0, h1 y h2
        jne fail
        mov ax, 0x4400
        mov bx, 1
        mov dx, [h1]
        EMS
        cmp word [es:0], 0x1112
        jne fail
        SAY m_realloc
        ; --- mover memoria (57h): convencional -> EMS -> convencional ---
        mov di, src
        push cs
        pop es
        mov cx, 20000
        xor al, al
.pat:
        stosb
        add al, 3
        loop .pat
        mov dword [mvs], 20000
        mov byte [mvs+4], 0         ; origen convencional
        mov word [mvs+5], 0
        mov word [mvs+7], src
        mov [mvs+9], cs
        mov byte [mvs+11], 1        ; destino EMS: h2, página 0, desp. 3000h
        mov ax, [h2]
        mov [mvs+12], ax
        mov word [mvs+14], 0x3000   ; (cruza a la página 1)
        mov word [mvs+16], 0
        mov si, mvs
        mov ax, 0x5700
        EMS
        mov di, src
        mov cx, 20000
        xor al, al
        rep stosb
        mov byte [mvs+4], 1
        mov ax, [h2]
        mov [mvs+5], ax
        mov word [mvs+7], 0x3000
        mov word [mvs+9], 0
        mov byte [mvs+11], 0
        mov word [mvs+12], 0
        mov word [mvs+14], src
        mov [mvs+16], cs
        mov si, mvs
        mov ax, 0x5700
        EMS
        mov si, src
        mov cx, 20000
        xor ah, ah
.cmp:
        lodsb
        cmp al, ah
        jne fail
        add ah, 3
        loop .cmp
        SAY m_move
        ; --- páginas mapeables (58h) ---
        mov ax, 0x5801
        EMS
        cmp cx, 4
        jne fail
        mov di, mapbuf
        mov ax, 0x5800
        EMS
        mov ax, [frame]
        cmp [mapbuf], ax
        jne fail
        cmp word [mapbuf+14], 3
        jne fail
        ; --- liberar: vuelve toda la memoria ---
        mov ah, 0x45
        mov dx, [h1]
        EMS
        mov ah, 0x45
        mov dx, [h2]
        EMS
        mov ah, 0x45
        mov dx, [h2]
        int 0x67
        cmp ah, 0x83
        jne fail
        mov ah, 0x42
        EMS
        cmp bx, [free0]
        jne fail
        SAY m_ok
        mov ax, 0x4C00
        int 0x21
nodev:
        SAY m_nodev
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

devname     db "EMMXXXX0", 0
hname       db "PRUEBA01"
m_det       db "EMS detectado OK", 13, 10, 0
m_frame     db "EMS marco en ", 0
m_map       db "EMS mapear OK", 13, 10, 0
m_err       db "EMS errores OK", 13, 10, 0
m_ctx       db "EMS contexto OK", 13, 10, 0
m_multi     db "EMS mapa multiple OK", 13, 10, 0
m_name      db "EMS nombres OK", 13, 10, 0
m_realloc   db "EMS reasignar OK", 13, 10, 0
m_move      db "EMS mover OK", 13, 10, 0
m_ok        db "EMS test OK", 13, 10, 0
m_nodev     db "EMS: no hay gestor (EMMXXXX0)", 13, 10, 0
m_fail      db "EMS FALLO AX=", 0
m_crlf      db 13, 10, 0
frame       dw 0
free0       dw 0
h1          dw 0
h2          dw 0
multi       times 8 dw 0
mvs         times 18 db 0
mapbuf      times 80 db 0
src:
