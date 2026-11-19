; PITIDO.COM - [REINICIO-SILENCIO] (pruebas) deja el altavoz del PC sonando
; a 880 Hz (canal 2 del 8254 en el modo 3 y la puerta del puerto 61h
; abierta) y vuelve, como un programa que acaba sin callarlo.
; tests/run_reinicio_silencio_tests.sh
        cpu 8086
        org 0x100
        mov al, 0xB6
        out 0x43, al
        mov ax, 1193180 / 880
        out 0x42, al
        mov al, ah
        out 0x42, al
        in al, 0x61
        or al, 3
        out 0x61, al
        mov dx, m
        mov ah, 9
        int 0x21
        ret
m       db "PITIDO: sonando", 13, 10, "$"
