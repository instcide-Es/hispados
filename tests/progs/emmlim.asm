; EMMLIM.COM - HSPEMM rechaza las zonas que se salen de FFFF:FFFF (o del
; primer mega en 57h) en vez de escribir sobre el monitor: 57h desde y hacia
; FFFF:0010 (A2h); 4Dh, 4Eh, 4Fh, 54h, DE01h y DE08h con ES:DI al final
; (A3h). Con una llamada buena de cada para comparar.
; HSPEMM bounds checks.
        cpu 386
        bits 16
        org 0x100

        mov ah, 0x43                ; una página EMS
        mov bx, 1
        int 0x67
        test ah, ah
        jnz fail
        mov [hnd], dx
        ; 57h: convencional (5000:0) -> EMS, 16 octetos: bien
        mov ax, [hnd]
        mov [mv_dh], ax
        mov byte [mv_st], 0
        mov word [mv_so], 0
        mov word [mv_ss], 0x5000
        mov dword [mv_len], 16
        mov si, mv
        mov ax, 0x5700
        int 0x67
        mov al, '0'
        call res
        ; 57h desde FFFF:0010, 64 KB: A2h
        mov word [mv_so], 0x0010
        mov word [mv_ss], 0xFFFF
        mov dword [mv_len], 0x10000
        mov si, mv
        mov ax, 0x5700
        int 0x67
        mov al, 'A'
        call res
        ; 57h de EMS a FFFF:0010: A2h (origen y destino al revés)
        mov byte [mv_st], 1
        mov ax, [hnd]
        mov [mv_sh], ax
        mov word [mv_so], 0
        mov word [mv_ss], 0
        mov byte [mv_dt], 0
        mov word [mv_dh], 0
        mov word [mv_do], 0x0010
        mov word [mv_ds], 0xFFFF
        mov si, mv
        mov ax, 0x5700
        int 0x67
        mov al, 'B'
        call res
        ; 4Dh bien y con ES:DI = FFFF:FFF0
        mov di, buf
        mov ah, 0x4D
        int 0x67
        mov al, '1'
        call res
        call alfinal
        mov ah, 0x4D
        int 0x67
        call ds_es
        mov al, 'C'
        call res
        ; 4Eh AL=0 (16 octetos) con ES:DI = FFFF:FFF8
        push es
        mov ax, 0xFFFF
        mov es, ax
        mov di, 0xFFF8
        mov ax, 0x4E00
        int 0x67
        pop es
        mov al, 'D'
        call res
        ; 4Fh AL=0: lista de 1 segmento (el marco), ES:DI = FFFF:FFF0
        mov ah, 0x41
        int 0x67
        mov [lista+2], bx
        mov si, lista
        call alfinal
        mov ax, 0x4F00
        int 0x67
        call ds_es
        mov al, 'E'
        call res
        ; 54h AL=0 con ES:DI = FFFF:FFF0
        call alfinal
        mov ax, 0x5400
        int 0x67
        call ds_es
        mov al, 'F'
        call res
        ; DE08h con ES:DI = FFFF:FFF0
        call alfinal
        mov ax, 0xDE08
        int 0x67
        call ds_es
        mov al, 'G'
        call res
        ; DE01h con ES:DI = FFFF:FFC0 (tabla de páginas) y DS:SI bueno
        push es
        mov ax, 0xFFFF
        mov es, ax
        mov di, 0xFFC0
        mov si, buf
        mov ax, 0xDE01
        int 0x67
        pop es
        mov al, 'H'
        call res
        ; fin
        mov dx, [hnd]
        mov ah, 0x45
        int 0x67
        mov dx, m_vivo
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
fail:
        mov dx, m_fail
        mov ah, 9
        int 0x21
        mov ax, 0x4C01
        int 0x21

alfinal:
        mov ax, 0xFFFF
        mov es, ax
        mov di, 0xFFF0
        ret
ds_es:
        push ds
        pop es
        ret
; AL = letra de la prueba, AH = lo que dio -> «EMMLIM x=hh»
res:
        mov [m_r+7], al
        mov al, ah
        shr al, 4
        call hex
        mov [m_r+9], al
        mov al, ah
        and al, 15
        call hex
        mov [m_r+10], al
        push dx
        mov dx, m_r
        push ax
        mov ah, 9
        int 0x21
        pop ax
        pop dx
        ret
hex:
        add al, '0'
        cmp al, '9'
        jbe .r
        add al, 7
.r:
        ret

hnd     dw 0
mv:
mv_len  dd 0
mv_st   db 0
mv_sh   dw 0
mv_so   dw 0
mv_ss   dw 0
mv_dt   db 1
mv_dh   dw 0
mv_do   dw 0
mv_ds   dw 0
lista   dw 1, 0
m_r     db "EMMLIM x=hh", 13, 10, "$"
m_vivo  db "EMMLIM: sigue vivo", 13, 10, "$"
m_fail  db "EMMLIM: sin EMS", 13, 10, "$"
buf     times 1024 db 0
