; =============================================================================
;  Hispa Ventanas - guardar un archivo sin perder el que había
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Como hace Datos (programas/datos/base.inc, «guarda»): lo nuevo se escribe
;  entero en un temporal (la misma ruta, con la extensión .$$$). Solo si se
;  ha escrito todo, el archivo de antes pasa a llamarse .BAK, el temporal
;  toma su nombre y la .BAK se borra. Si el disco se llena, se quita la
;  memoria o falla la escritura, se borra el temporal y el archivo de antes
;  queda como estaba. Si se va la luz entre los dos cambios de nombre, queda
;  en el disco la .BAK (la versión de antes, entera) y el .$$$ (la nueva,
;  entera): CENTRO.INI, por ejemplo, prueba CENTRO.BAK si no hay CENTRO.INI.
;
;  La .BAK no se conserva (en un disquete casi lleno estorbaría), y la que
;  hubiera de antes se borra antes de empezar, para que quepa el temporal.
;  Hace falta sitio para el archivo de antes y el nuevo a la vez: si no lo
;  hay, no se guarda (y se dice), pero no se pierde nada.
;
;    gua_crear      ESI=ruta -> EAX=manejador (el del temporal); CF=1 si no
;    gua_cerrar     EBX=manejador AL=1 si algo ha fallado al escribir (0: no)
;                   -> CF=1 si no se ha guardado (el de antes, como estaba)
;    gua_escribir   ESI=ruta EDX=datos ECX=octetos: todo de una vez -> CF=1
;                   si no se puede (o no cabe entero)
;    gua_abrir_bak  ESI=ruta que no está: abre su .BAK (para leer)
;    gua_ext        ESI=ruta EDI=búfer (GUA_RUTA, 132) EAX=extensión ('BAK')
;                   EDX=la de reserva (si ya tiene esa): la ruta con EAX
;    gua_atrib      ESI=ruta -> CF=0 y CL=atributos si está; CF=1 si no
;
;  Está en VENTANAS.COM (el Bloc de notas y CENTRO.INI) y los módulos llegan
;  a gua_crear, gua_cerrar, gua_escribir, gua_ext y gua_atrib por la
;  interfaz (api.inc, 0.9.31; hvl_escribir de hvplib.inc es gua_escribir):
;  así no hay una copia en cada .HVP. Las rutas van en gua_ruta, gua_tmp y
;  gua_bak, una sola vez a la vez: entre gua_crear y gua_cerrar no se cede
;  el turno a otra tarea.
; =============================================================================

%macro GUADOS 1
        call %1
%endmacro

GUA_RUTA    equ 132                 ; 127 letras, el punto, la extensión y el 0

; ESI=ruta EDI=búfer (GUA_RUTA) EAX=extensión ('$$$') EDX=la de
; reserva, por si la ruta ya tiene esa: la misma ruta con la extensión EAX
gua_ext:
        pushad
        mov ecx, GUA_RUTA - 5
        xor ebx, ebx                ; dónde está el punto del nombre (0: no tiene)
.c:
        lodsb
        test al, al
        jz .f
        cmp al, '\'
        je .barra
        cmp al, '/'
        je .barra
        cmp al, ':'
        je .barra
        cmp al, '.'
        jne .p
        mov ebx, edi
        jmp .p
.barra:
        xor ebx, ebx                ; (el punto de una carpeta no cuenta)
.p:
        stosb
        dec ecx
        jnz .c
.f:
        mov byte [edi], 0
        test ebx, ebx
        jnz .tiene
        mov ebx, edi
        mov byte [edi], '.'
        jmp .pon
.tiene:
        mov esi, [ebx+1]            ; ¿ya tiene esa extensión? (las tres letras
        and esi, 0xDFDFDFDF         ; y el 0, sin mirar mayúsculas)
        mov ecx, [esp+28]           ; (la EAX que se dio: lodsb ha pisado AL)
        and ecx, 0xDFDFDFDF
        cmp esi, ecx
        jne .pon
        mov eax, [esp+20]           ; entonces, la de reserva (EDX)
        mov [ebx+1], eax
        popad
        ret
.pon:
        mov eax, [esp+28]
        mov [ebx+1], eax            ; las tres letras y el 0 (el cuarto octeto de EAX)
        popad
        ret

; (interna) ESI=nombre de antes EDI=el nuevo -> CF=1 si no se puede
; (HVPRUN de Hispa-Pentium, que no pasa por el DOS de modo real, pone su
; gua_renombra y su gua_atrib y define GUA_PROPIOS: castel/nucleo32/hvprun)
%ifndef GUA_PROPIOS
gua_renombra:
        pushad                      ; los dos nombres, en el búfer de paso
        push edi
        mov edi, dos_buf
        mov ecx, GUA_RUTA
        rep movsb
        pop esi
        mov edi, dos_buf + GUA_RUTA
        mov ecx, GUA_RUTA
        rep movsb
        mov dword [rmcs+0x00], dos_buf + GUA_RUTA   ; ES:DI
        mov eax, 0x5600
        mov edx, dos_buf
        xor ebx, ebx
        xor ecx, ecx
        call dos_int
        mov dword [rmcs+0x00], 0    ; (que no se quede para los demás)
        popad
        ret
%endif

gua_crear:
        push ecx
        push edx
        push esi
        push edi
        push eax
        mov edi, gua_ruta
        mov ecx, GUA_RUTA - 5
.c:
        lodsb
        stosb
        test al, al
        jz .c1
        dec ecx
        jnz .c
        mov byte [edi], 0
.c1:
        mov esi, gua_ruta
        mov edi, gua_tmp
        mov eax, '$$$'
        mov edx, '$$1'
        call gua_ext
        mov edi, gua_bak
        mov eax, 'BAK'
        mov edx, 'BK1'
        call gua_ext
        mov esi, gua_ruta           ; ¿está? De solo lectura (o una carpeta),
        call gua_atrib              ; no se toca: como al crearlo encima
        jc .sin
        test cl, 0x11
        jz .bak
        mov dword [esp], 5          ; (EAX: «acceso denegado»)
        jmp .no
.bak:
        mov esi, gua_bak            ; la .BAK de otra vez: fuera, para que
        GUADOS dos_borrar           ; quepa el temporal. Pero solo si está el
.sin:                               ; archivo: si no, la .BAK es lo único que
                                    ; queda (se fue la luz al guardar)
        mov esi, gua_tmp
        GUADOS dos_borrar
        mov esi, gua_tmp
        GUADOS dos_crear
        jnc .si
        mov [esp], eax              ; (EAX: el error de DOS)
        jmp .no
.si:
        add esp, 4                  ; (EAX: el manejador)
        jmp .x
.no:
        pop eax
        stc
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        ret

; ESI=ruta -> CF=0 y CL=sus atributos (INT 21h, 4300h) si está; CF=1 si no
%ifndef GUA_PROPIOS
gua_atrib:
        push eax
        mov eax, 0x4300
        call dos_ruta
        pop eax
        jc .x
        mov cl, [rmcs+0x18]
        clc
.x:
        ret
%endif

gua_cerrar:
        pushad
        mov dl, al
        GUADOS dos_cerrar           ; (al cerrar se escribe lo último: también
        jnc .c                      ; puede fallar)
        mov dl, 1
.c:
        test dl, dl
        jnz .mal
        mov esi, gua_ruta           ; el de antes, a .BAK (si no lo hay, es la
        mov edi, gua_bak            ; primera vez: nada)
        call gua_renombra
        setnc dh                    ; DH=1: había uno de antes
        mov esi, gua_tmp
        mov edi, gua_ruta
        call gua_renombra
        jc .deshaz
        test dh, dh                 ; (una .BAK que ya estaba, sin el archivo:
        jz .bien                    ; no es nuestra, se deja)
        mov esi, gua_bak
        GUADOS dos_borrar
.bien:
        popad
        clc
        ret
.deshaz:
        test dh, dh
        jz .mal
        mov esi, gua_bak            ; el de antes vuelve a su nombre
        mov edi, gua_ruta
        call gua_renombra
.mal:
        mov esi, gua_tmp
        GUADOS dos_borrar
        popad
        stc
        ret

gua_escribir:
        push eax
        push ebx
        push esi
        call gua_crear
        jc .x
        mov ebx, eax
        mov esi, edx
        GUADOS dos_escribir
        jc .mal
        cmp eax, ecx                ; disco lleno: DOS escribe menos sin CF
        setne al
        jmp .e
.mal:
        mov al, 1
.e:
        call gua_cerrar
.x:
        pop esi
        pop ebx
        pop eax
        ret

; ESI=ruta (que no está): abre para leer su .BAK, lo que queda si se fue la
; luz entre los dos cambios de nombre -> EAX=manejador; CF=1 si tampoco está
gua_abrir_bak:
        push esi
        push edi
        push edx
        push eax
        mov edi, gua_bak
        mov eax, 'BAK'
        mov edx, 'BK1'
        call gua_ext
        mov esi, gua_bak
        xor al, al
        call dos_abrir
        jc .no
        add esp, 4
        jmp .x
.no:
        pop eax
        stc
.x:
        pop edx
        pop edi
        pop esi
        ret
