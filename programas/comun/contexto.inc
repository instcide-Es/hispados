; =============================================================================
;  contexto.inc - el menú del botón secundario (el derecho) de los programas
;  de «Productividad»
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Pedido de Carlos: «Botón secundario en toda la suite Office». Todos los
;  programas de la familia abren su menú contextual con esto, para que salga
;  y se maneje igual en todos: en el sitio del puntero, con las órdenes que
;  tocan para lo que hay debajo y en gris las que ahora no se pueden hacer.
;  Cómo llega el botón derecho a un programa y qué es «ctx_menu» está en
;  docs/VENTANAS.md («Botón derecho»); las reglas de la familia, en
;  docs/OFICINA.md.
;
;  Un menú contextual es una lista de órdenes con el formato de un
;  desplegable de la barra (dw número, db "&Texto", 0 ...; dw 0 + db 0 es
;  una raya; dw 0xFFFF acaba) y con LOS MISMOS NÚMEROS que las órdenes de la
;  barra de menús: la elegida llega a la ventana como WM_COMMAND, igual que
;  si se hubiera elegido arriba, así que aquí no hay nada que hacer dos veces.
;
;  Lo que va en gris lo dice una tabla del programa, la misma para todos sus
;  menús: de cada orden que no vale siempre,
;        dw número
;        dd rutina       ; -> CF=1 si ahora NO se puede (puede cambiar los
;                        ;    registros)
;  y un «dw 0» al final. La orden que no está en la tabla vale siempre.
;
;    ctx_abre     ESI=el menú EDI=la tabla de lo que va en gris
;                 EAX=x EBX=y, en la zona de la ventana, o EAX=-1: donde se
;                 pidió (el puntero; con el teclado, la esquina de la zona).
;                 Abre el menú; la dueña es la ventana [ven]
;    ctx_porta    -> CF=1 si en el portapapeles de Hispa Ventanas no hay
;                 texto que pegar. Usa el cuadro de texto oculto del programa
;                 («puente»: una variable con ese nombre), como su «Pegar».
;                 Solo si el programa define CTX_PUENTE antes de incluir esto
;    ctx_punto    EDX=x + y*65536 -> EAX=x EBX=y (con signo): el punto de un
;                 mensaje del ratón
;
;  El programa define «ven» (su ventana).
; =============================================================================

; ESI=menú EDI=tabla de gris EAX, EBX=dónde (EAX=-1: donde se pidió)
ctx_abre:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        cmp eax, -1
        je .gris
        push eax                    ; de la zona de la ventana a la pantalla
        push ebx
        HV ven_cli                  ; EAX, EBX=dónde empieza la zona
        pop edx
        add ebx, edx
        pop edx
        add eax, edx
.gris:
        mov [esp+28], eax           ; (se guardan en el EAX y el EBX apilados)
        mov [esp+16], ebx
        xor ecx, ecx                ; las que van en gris: un bit por orden,
        xor ebx, ebx                ; contando las rayas
        mov edx, esi
.o:
        movzx eax, word [edx]
        cmp eax, 0xFFFF
        je .abre
        test eax, eax
        jz .sig
        push edi
.t:
        cmp word [edi], 0
        je .tf
        cmp ax, [edi]
        je .es
        add edi, 6
        jmp .t
.es:
        pushad
        call [edi+2]
        popad                       ; (no toca CF)
        jnc .tf
        bts ecx, ebx
.tf:
        pop edi
.sig:
        add edx, 2                  ; al final del texto
.s1:
        inc edx
        cmp byte [edx-1], 0
        jne .s1
        inc ebx
        cmp ebx, 32
        jb .o
.abre:
        mov eax, [esp+28]
        mov ebx, [esp+16]
        HV ctx_menu
.x:
        popad
        ret

; EDX=x + y*65536 -> EAX=x EBX=y
ctx_punto:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        ret

%ifdef CTX_PUENTE
; -> CF=1 si no hay texto que pegar en el portapapeles
ctx_porta:
        pushad
        mov ebp, [puente]
        test ebp, ebp
        jz .no
        mov eax, WM_SETTEXT
        mov edx, .nada
        HV msg_enviar
        mov eax, WM_PASTE
        HV msg_enviar
        mov eax, EM_LARGO
        HV msg_enviar
        push eax
        mov eax, WM_SETTEXT
        mov edx, .nada
        HV msg_enviar
        pop eax
        test eax, eax
        jz .no
        popad
        clc
        ret
.no:
        popad
        stc
        ret
.nada   db 0
%endif
