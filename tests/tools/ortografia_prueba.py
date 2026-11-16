#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""Prueba del corrector ortográfico fuera de Hispa Ventanas.

uso: ortografia_prueba.py [CUANTAS_AL_AZAR]

Toma programas/comun/ortografia.inc TAL CUAL, lo ensambla con NASM dentro de
un programa de Linux de 32 bits (las tres funciones de archivos de Hispa
Ventanas que usa, «HV dos_abrir/dos_leer/dos_cerrar», se las da el arnés con
llamadas de Linux, de modo que ort_cargar también se prueba) y le pregunta
por la entrada: una orden por línea, una respuesta por línea.

Lo compara con un modelo hecho aquí, que no comparte nada con el ensamblador:
las palabras de programas/comun/DICCIO.DAT en un conjunto de Python y las
normas de las mayúsculas y de las propuestas escritas otra vez.

  1. el diccionario: DICCIO.DAT es justo lo que sale de expandir el
     diccionario de origen (si está: ORTO_FUENTE=carpeta de es_ES.aff/.dic)
  2. todas las formas valen; con la primera en mayúscula y todas en
     mayúsculas (sin los acentos que CP437 no tiene), también
  3. lo que no debe valer: formas con una letra estropeada, palabras
     inventadas, nombres propios en minúscula, mayúsculas donde no tocan
  4. un texto de verdad: cuántas palabras da por malas, y cuáles
  5. las propuestas: una lista de faltas y miles de palabras estropeadas,
     contra el modelo
  6. las palabras del usuario
  7. archivos rotos, palabras larguísimas o vacías: nada se cuelga
  8. lo que tarda
"""
import os, random, re, struct, subprocess, sys, tempfile, time

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.join(AQUI, '..', '..')
sys.path.insert(0, os.path.join(RAIZ, 'tools'))
import hazdiccio                                            # noqa: E402

DAT = os.path.join(RAIZ, 'programas', 'comun', 'DICCIO.DAT')
SIN = str.maketrans('ÁÍÓÚ', 'AIOU')

ARNES = r'''
        bits 32
        cpu 386
ENT_MAX     equ 48 * 1024 * 1024
SAL_MAX     equ 1024 * 1024
%macro HV 1
        call arn_%1
%endmacro
        section .text
        global _start
%include "ortografia.inc"

; ---- lo que hace de Hispa Ventanas: los archivos ----
arn_dos_abrir:                      ; ESI=ruta -> EAX=manejador, CF
        push ebx
        push ecx
        push edx
        mov eax, 5
        mov ebx, esi
        xor ecx, ecx
        xor edx, edx
        int 0x80
        pop edx
        pop ecx
        pop ebx
        cmp eax, 0
        jl .no
        clc
        ret
.no:
        stc
        ret
arn_dos_leer:                       ; EBX=manejador EDI=dónde ECX=cuánto -> EAX=leído, CF
        push ebx
        push ecx
        push edx
        mov edx, ecx
        mov ecx, edi
        mov eax, 3
        int 0x80
        pop edx
        pop ecx
        pop ebx
        cmp eax, 0
        jl .no
        clc
        ret
.no:
        stc
        ret
arn_dos_cerrar:
        push eax
        push ebx
        mov eax, 6
        int 0x80
        pop ebx
        pop eax
        ret

emite:                              ; AL -> a la salida
        push edi
        mov edi, [sal_p]
        stosb
        mov [sal_p], edi
        cmp edi, salida + SAL_MAX - 4096
        pop edi
        jb .x
        call vuelca
.x:
        ret
vuelca:
        pushad
        mov eax, 4
        mov ebx, 1
        mov ecx, salida
        mov edx, [sal_p]
        sub edx, ecx
        int 0x80
        mov dword [sal_p], salida
        popad
        ret
numero:                             ; ESI=cifras ECX=cuántas -> EAX
        push ebx
        xor eax, eax
        jecxz .x
.l:
        movzx ebx, byte [esi]
        sub ebx, '0'
        imul eax, 10
        add eax, ebx
        inc esi
        dec ecx
        jnz .l
.x:
        pop ebx
        ret
decimal:                            ; EAX -> a la salida, en cifras
        pushad
        xor ecx, ecx
        mov ebx, 10
.l:
        xor edx, edx
        div ebx
        push edx
        inc ecx
        test eax, eax
        jnz .l
.p:
        pop eax
        add al, '0'
        call emite
        dec ecx
        jnz .p
        popad
        ret

_start:
        cld
        mov dword [sal_p], salida
        mov dword [rep_n], 1
        mov dword [sug_max], 8
        mov edi, entrada
.lee:
        mov eax, 3
        xor ebx, ebx
        mov ecx, edi
        mov edx, entrada + ENT_MAX
        sub edx, edi
        jz .leido
        int 0x80
        cmp eax, 0
        jle .leido
        add edi, eax
        jmp .lee
.leido:
        mov [ent_fin], edi
        mov esi, entrada
.linea:
        cmp esi, [ent_fin]
        jae .fin
        mov ebx, esi
.e:
        cmp byte [ebx], 10
        je .e1
        inc ebx
        cmp ebx, [ent_fin]
        jb .e
.e1:
        lea eax, [ebx+1]
        mov [sig], eax
        mov al, [esi]
        mov ecx, ebx
        sub ecx, esi
        sub ecx, 2
        jns .e2
        xor ecx, ecx
.e2:
        add esi, 2
        mov [arg], esi
        mov [largo], ecx
        cmp al, 'P'
        je .cmd_p
        cmp al, 'S'
        je .cmd_s
        cmp al, 'A'
        je .cmd_a
        cmp al, 'L'
        je .cmd_l
        cmp al, 'C'
        je .cmd_c
        cmp al, 'D'
        je .cmd_d
        cmp al, 'T'
        je .cmd_t
        cmp al, 'M'
        je .cmd_m
        cmp al, 'Q'
        je .cmd_q
        mov al, '?'
        call emite
.nl:
        mov al, 10
        call emite
        mov esi, [sig]
        jmp .linea
.fin:
        call vuelca
        mov eax, 1
        xor ebx, ebx
        int 0x80

.cmd_t:                             ; «T 20»: cada «P» se hace tantas veces
        call numero
        mov [rep_n], eax
        jmp .nl
.cmd_m:                             ; «M 3»: cuántas propuestas se piden
        call numero
        mov [sug_max], eax
        jmp .nl
.cmd_q:                             ; «Q»: lo más que ha gastado una búsqueda, y a cero
        mov eax, [gasto]
        call decimal
        mov dword [gasto], 0
        jmp .nl
.cmd_d:                             ; «D ruta»: ort_cargar -> H0 o H1
        mov byte [esi+ecx], 0
        mov eax, 0x11111111
        mov ebx, 0x22222222
        mov edx, 0x33333333
        mov edi, 0x44444444
        mov ebp, 0x55555555
        call ort_cargar
        call registros
        mov al, 'H'
        call emite
        mov al, [ort_hay]
        add al, '0'
        call emite
        jmp .nl
.cmd_p:                             ; «P palabra»: 1 si está bien, 0 si no
        mov eax, [rep_n]
        mov [veces], eax
.p1:
        mov esi, [arg]
        mov ecx, [largo]
        mov eax, 0x11111111
        mov ebx, 0x22222222
        mov edx, 0x33333333
        mov edi, 0x44444444
        mov ebp, 0x55555555
        call ort_palabra
        mov byte [resp], '1'
        jnc .p2
        mov byte [resp], '0'
.p2:
        dec dword [veces]
        jnz .p1
        call registros
        mov al, [resp]
        call emite
        jmp .nl
.cmd_a:                             ; «A palabra»: ort_propia -> 0 (apuntada) o 1 (no)
        mov eax, 0x11111111
        mov ebx, 0x22222222
        mov edx, 0x33333333
        mov edi, 0x44444444
        mov ebp, 0x55555555
        call ort_propia
        mov byte [resp], '0'
        jnc .a2
        mov byte [resp], '1'
.a2:
        call registros
        mov al, [resp]
        call emite
        jmp .nl
.cmd_l:                             ; «L»: la lista del usuario, con «|» por cada salto
        call ort_propias_ptr
        test ecx, ecx
        jz .nl
.l1:
        lodsb
        cmp al, 13
        je .l2
        cmp al, 10
        jne .l3
        mov al, '|'
.l3:
        call emite
.l2:
        dec ecx
        jnz .l1
        jmp .nl
.cmd_c:                             ; «C texto»: ort_propias_poner («|» = salto de línea)
        jecxz .c2
        mov edi, esi
        push ecx
.c1:
        cmp byte [edi], '|'
        jne .c3
        mov byte [edi], 10
.c3:
        inc edi
        dec ecx
        jnz .c1
        pop ecx
.c2:
        call ort_propias_poner
        jmp .nl
.cmd_s:                             ; «S palabra»: las propuestas, con comas
        mov edi, sug
        mov ecx, 10 * 64
        mov al, 0xA5
        rep stosb
        mov esi, [arg]
        mov ecx, [largo]
        mov ebx, 0x22222222
        mov ebp, 0x55555555
        mov edi, sug
        mov edx, [sug_max]
        call ort_sugerir
        mov [cuantas], eax
        cmp ebx, 0x22222222
        jne .s_roto
        cmp ebp, 0x55555555
        jne .s_roto
        cmp edi, sug
        jne .s_roto
        cmp edx, [sug_max]
        jne .s_roto
        cmp esi, [arg]
        jne .s_roto
        cmp ecx, [largo]
        jne .s_roto
        cmp eax, 8
        ja .s_roto
        cmp eax, [sug_max]
        ja .s_roto
        mov eax, ORT_TOPE           ; lo que ha gastado la búsqueda
        sub eax, [ort_tope]
        cmp dword [cuantas], 0
        je .s0
        cmp eax, [gasto]
        jbe .s0
        mov [gasto], eax
.s0:
        ; detrás del sitio que se le dio (64 por cada una de las pedidas, 8 a
        ; lo más) no puede haber tocado nada
        mov edi, [sug_max]
        cmp edi, 8
        jbe .s00
        mov edi, 8
.s00:
        shl edi, 6
        add edi, sug
.s1:
        cmp edi, sug + 10 * 64
        jae .s2
        cmp byte [edi], 0xA5
        jne .s_roto
        inc edi
        jmp .s1
.s2:
        xor ebx, ebx
.s3:
        cmp ebx, [cuantas]
        jae .nl
        mov esi, ebx
        shl esi, 6
        add esi, sug
        test ebx, ebx
        jz .s4
        mov al, ','
        call emite
.s4:
        mov ecx, 64
.s5:
        lodsb
        test al, al
        jz .s6
        call emite
        dec ecx
        jnz .s5
        jmp .s_roto                 ; (sin su 0 dentro de los 64)
.s6:
        inc ebx
        jmp .s3
.s_roto:
        mov al, '!'
        call emite
        jmp .nl

registros:                          ; ¿siguen como estaban? Si no, «!» en la salida
        cmp eax, 0x11111111
        jne .roto
        cmp ebx, 0x22222222
        jne .roto
        cmp edx, 0x33333333
        jne .roto
        cmp edi, 0x44444444
        jne .roto
        cmp ebp, 0x55555555
        jne .roto
        cmp esi, [arg]
        jne .roto
        cmp ecx, [largo]
        jne .roto
        ret
.roto:
        mov al, '!'
        jmp emite

        section .bss
        ORTO_BSS
sal_p       resd 1
ent_fin     resd 1
sig         resd 1
arg         resd 1
largo       resd 1
rep_n       resd 1
veces       resd 1
sug_max     resd 1
cuantas     resd 1
gasto       resd 1
resp        resb 4
sug         resb 10 * 64
salida      resb SAL_MAX
entrada     resb ENT_MAX
'''

# ---- las letras de CP437, escritas otra vez (no se copian del ensamblador) ----
MAYUS = set(range(65, 91)) | {165, 144, 154, 128, 142, 143, 146, 153}
A_MIN = {c: c + 32 for c in range(65, 91)}
A_MIN.update({165: 164, 144: 130, 154: 129, 128: 135, 142: 132, 143: 134, 146: 145, 153: 148})
A_MAY = {c: c - 32 for c in range(97, 123)}
A_MAY.update({164: 165, 130: 144, 129: 154, 160: 65, 161: 73, 162: 79, 163: 85, 135: 128, 132: 142,
              134: 143, 145: 146, 148: 153})
MINUS = set(A_MAY)
ACENTO = {65: 160, 73: 161, 79: 162, 85: 163}
BASE = {160: 97, 130: 101, 161: 105, 162: 111, 163: 117, 129: 117}
GRUPOS = ((b'll', b'y'), (b'y', b'll'), (b'k', b'qu'), (b'qu', b'k'), (b'x', b'cc'), (b'cc', b'x'))


def mayus(p):
    return bytes(A_MAY.get(c, c) for c in p)


def minus(p):
    return bytes(A_MIN.get(c, c) for c in p)


def sin_acentos(p):
    return bytes(BASE.get(c, c) for c in p)


class Modelo:
    def __init__(self, palabras):
        self.dic = set(palabras)
        self.largo = max(len(p) for p in palabras)
        self.todo_mayus = {mayus(p) for p in palabras}
        self.clave = {}                     # sin acentos y con la primera en minúscula
        for p in palabras:
            self.clave.setdefault(self.pliega(p), []).append(p)
        self.letras = sorted({c for k in self.clave for c in k})

    @staticmethod
    def pliega(p):
        return sin_acentos(minus(p[:1]) + p[1:])

    def vale(self, p, propias=()):
        if len(p) < 2 or len(p) > 63 or any(48 <= c <= 57 for c in p):
            return True
        if all(c in MAYUS for c in p):
            return p in self.todo_mayus
        if p in self.dic:
            return True
        if p[0] in MAYUS and all(c in MINUS for c in p[1:]):
            if minus(p[:1]) + p[1:] in self.dic:
                return True
            return p[0] in ACENTO and bytes([ACENTO[p[0]]]) + p[1:] in self.dic
        return False

    def cerca(self, p):
        """las palabras del diccionario a una corrección de p (en minúsculas), sin contar acentos"""
        s = sin_acentos(p)
        if len(p) < 2 or len(p) > self.largo + 1:
            return set()
        v = {s}
        for i in range(len(s) + 1):
            for c in self.letras:
                v.add(s[:i] + bytes([c]) + s[i:])
            if i < len(s):
                v.add(s[:i] + s[i + 1:])
                for c in self.letras:
                    v.add(s[:i] + bytes([c]) + s[i + 1:])
                if i + 1 < len(s):
                    v.add(s[:i] + s[i + 1:i + 2] + s[i:i + 1] + s[i + 2:])
                for de, a in GRUPOS:
                    if s[i:i + len(de)] == de:
                        v.add(s[:i] + a + s[i + len(de):])
        salen = set()
        for k in v:
            salen.update(self.clave.get(k, ()))
        salen.discard(p)
        return {w for w in salen if len(w) <= 63}


def hacer(tmp):
    """ensambla ortografia.inc tal cual con el arnés; -> el programa"""
    inc = open(os.path.join(RAIZ, 'programas', 'comun', 'ortografia.inc'), encoding='utf-8').read()
    open(os.path.join(tmp, 'ortografia.inc'), 'wb').write(inc.encode('cp437', errors='replace'))
    asm = os.path.join(tmp, 'arnes.asm')
    open(asm, 'w', encoding='cp437').write(ARNES)
    subprocess.run([os.environ.get('NASM', 'nasm'), '-f', 'elf32', '-w+error=number-overflow', '-i', tmp + os.sep,
                    '-o', asm + '.o', asm], check=True)
    subprocess.run(['ld', '-m', 'elf_i386', '-o', os.path.join(tmp, 'orto'), asm + '.o'], check=True)
    return os.path.join(tmp, 'orto')


class Cuenta:
    def __init__(self):
        self.ok = self.mal = 0

    def mira(self, bien, texto, detalle=''):
        if bien:
            self.ok += 1
            print('  OK   ' + texto)
        else:
            self.mal += 1
            print('  FAIL ' + texto + (': ' + detalle if detalle else ''))
        return bien


def txt(p):
    return p.decode('cp437')


def cp(t):
    return t.translate(SIN).encode('cp437')


TEXTO = """
Aquella mañana de otoño, Carmen se levantó temprano porque había quedado con
sus amigas para ir al mercadillo del pueblo. Desayunó deprisa un café con
leche y unas tostadas con mantequilla, se puso el abrigo azul y salió a la
calle. Hacía frío, pero el cielo estaba despejado y los árboles de la plaza
empezaban a perder las hojas. En la esquina se encontró con Antonio, el
vecino del tercero, que paseaba a su perrito como todos los días.

—Buenos días, Carmen. ¿Adónde vas tan pronto? —le preguntó él, sonriendo.
—Al mercadillo. Quiero comprar unas manzanas y, si las encuentro, aquellas
galletas caseras que tanto le gustaban a mi abuela. ¿Te apetece venir?
—No puedo: tengo que llevar el coche al taller. Me dijeron que estaría
arreglado ayer, pero todavía no me han llamado. Si ves a Pilar, dile que la
llamaré esta tarde; cuéntaselo todo, por favor.
—Se lo diré. ¡Hasta luego!

Cuando llegó, sus amigas ya la estaban esperando junto a la fuente. Habían
traído bolsas grandes y muchas ganas de hablar. Recorrieron los puestos uno
por uno, mirándolo todo con calma: frutas, verduras, quesos, zapatos, libros
viejos y juguetes de madera. Isabel encontró una lámpara antigua que le
encantó y estuvo un buen rato regateando con el vendedor, diciéndole que era
demasiado cara. Al final se la llevó por la mitad de lo que le pedían.
—Dámelo envuelto, que es para un regalo —le dijo, y el hombre, riéndose, se
la envolvió en papel de periódico.

Después comieron juntas en un restaurante pequeñito cerca del río. Pidieron
sopa de verduras, pescado a la plancha y, de postre, arroz con leche. Mientras
esperaban, María les contó que su hijo mayor había aprobado los exámenes y
que el próximo año empezaría a trabajar en un hospital de Barcelona. Todas la
felicitaron. «Si hubiera sabido que ibais a alegraros tanto, os lo habría
dicho antes», bromeó. Brindaron con vino blanco y prometieron volver a verse
pronto, quizá en Navidad, cuando vinieran también los nietos.

Por la tarde, ya en casa, Carmen guardó la compra, regó las plantas del
balcón y se sentó a leer junto a la ventana. Pensó que los días sencillos,
los que no parecen importantes, son a menudo los que mejor se recuerdan. Sonó
el teléfono: era su hermana, que quería saber cómo le había ido. Estuvieron
hablando hasta que anocheció, contándose las novedades de la familia,
recordando los veranos en la playa cuando eran niñas y riéndose de las mismas
historias de siempre. Antes de acostarse escribió unas líneas en su cuaderno:
«Hoy ha sido un buen día. Mañana, si no llueve, iremos caminando hasta el
faro y llevaremos bocadillos; díselo a Luis y avísame cuando lo sepáis».

El señor García y su mujer, Lucía Fernández, viajaron en tren de Tarragona a
Sevilla para visitar a sus primos de Andalucía. Les enviaron un correo
electrónico desde el ordenador de la biblioteca: «Llegaremos el sábado veinte
de diciembre a las ocho y media; no hace falta que vengáis a recogernos.
Traednos, eso sí, las llaves del piso, y decidle a Javier que no se olvide de
devolvernos los libros que le prestamos». En España, como en Portugal o en
Francia, los trenes rápidos han cambiado la manera de viajar: lo que antes
exigía un día entero ahora se resuelve en pocas horas, cómodamente sentados.
"""

# la falta, la buena y entre cuántas de las primeras tiene que estar (0: no se
# espera que salga; se enseña lo que propone)
FALTAS = [
    ('haver', 'haber', 1), ('tubo', 'tuvo', 3), ('arbol', 'árbol', 1), ('canpo', 'campo', 1), ('hizieron', 'hicieron', 1),
    ('llendo', 'yendo', 1), ('recivir', 'recibir', 1), ('exelente', 'excelente', 1), ('camion', 'camión', 1),
    ('tanbien', 'también', 1), ('haiga', 'haya', 0), ('aver', 'haber', 0), ('habia', 'había', 1),
    ('desir', 'decir', 1), ('jente', 'gente', 1), ('cavallo', 'caballo', 1), ('ormiga', 'hormiga', 1),
    ('hechar', 'echar', 2), ('zapato', 'zapato', 0), ('zapatto', 'zapato', 1), ('caro', 'carro', 3),
    ('gerra', 'guerra', 2), ('kilometro', 'kilómetro', 1), ('keso', 'queso', 2), ('nesecito', 'necesito', 0),
    ('dijistes', 'dijiste', 2), ('trajieron', 'trajeron', 1), ('conduci', 'conduje', 0), ('absorver', 'absorber', 1),
    ('espontaneo', 'espontáneo', 2), ('preveer', 'prever', 1), ('acer', 'hacer', 1), ('alludar', 'ayudar', 1),
    ('enpezar', 'empezar', 1), ('Arvol', 'Arbol', 1), ('CANPO', 'CAMPO', 1), ('madrit', 'Madrid', 1),
    ('barselona', 'Barcelona', 1), ('dameló', 'dámelo', 1), ('diciendoselo', 'diciéndoselo', 1),
    ('pscina', 'piscina', 1), ('ecselente', 'excelente', 0), ('porqe', 'porque', 1), ('qeu', 'que', 1),
]


def main():
    cuantas = int(sys.argv[1]) if len(sys.argv) > 1 else 60000
    random.seed(20261116)
    c = Cuenta()
    datos = open(DAT, 'rb').read()
    palabras = hazdiccio.lee(datos)
    m = Modelo(palabras)

    print('== el diccionario ==')
    narcos = struct.unpack_from('<I', datos, 8)[0]
    c.mira(datos[:8] == b'DICCIO1\x1a' and len(datos) == 32 + 4 * narcos and len(datos) < 600 * 1024,
           'DICCIO.DAT: %d octetos, %d arcos, %d palabras, la más larga de %d letras'
           % (len(datos), narcos, len(palabras), m.largo))
    fuente = os.environ.get('ORTO_FUENTE') or '/usr/share/hunspell'
    if os.path.exists(os.path.join(fuente, 'es_ES.dic')):
        formas, lista, reglas, _ = hazdiccio.expande(fuente)
        esperadas, _ = hazdiccio.a_cp437(formas)
        c.mira(esperadas == sorted(palabras),
               'reconoce justo las %d formas que salen de las %d raíces y %d reglas de %s (y las añadidas)'
               % (len(esperadas), len(lista), reglas, fuente),
               '%d de más, %d de menos' % (len(set(palabras) - set(esperadas)), len(set(esperadas) - set(palabras))))
    else:
        print('  --   sin el diccionario de origen (ORTO_FUENTE): no se rehace para compararlo')

    # ---- lo que se pregunta ----
    ordenes, espera, rotulo = [], [], []

    def pon(orden, esperado, rot):
        ordenes.append(orden)
        espera.append(esperado)
        rotulo.append(rot)

    def pregunta(p, grupo):
        pon(b'P ' + p, b'1' if m.vale(p) else b'0', grupo)

    # 2. lo que debe valer
    for p in palabras:
        pregunta(p, 'vale')
    muestra = random.sample(palabras, min(cuantas, len(palabras)))
    for p in muestra:
        pregunta(mayus(p[:1]) + p[1:], 'vale-Primera')
        pregunta(mayus(p), 'vale-MAYUS')
    # 3. lo que no debe valer
    letras = [x for x in m.letras if x in MINUS]
    hechas = 0
    while hechas < cuantas:
        p = random.choice(palabras)
        if len(p) < 3:
            continue
        i = random.randrange(len(p))
        k = random.random()
        if k < 0.4:
            q = p[:i] + bytes([random.choice(letras)]) + p[i + 1:]
        elif k < 0.6:
            q = p[:i] + p[i + 1:]
        elif k < 0.8:
            q = p[:i] + bytes([random.choice(letras)]) + p[i:]
        else:
            q = p[:i] + p[i + 1:i + 2] + p[i:i + 1] + p[i + 2:]
        if not m.vale(q):
            pregunta(q, 'no-estropeada')
            hechas += 1
    for _ in range(cuantas // 3):
        q = bytes(random.choice(letras) for _ in range(random.randint(2, 14)))
        pregunta(q, 'inventada')
    for p in palabras:
        if p[0] in MAYUS:
            pregunta(minus(p), 'propio-en-minúscula')
    for p in random.sample(palabras, min(cuantas // 3, len(palabras))):
        if len(p) > 2:
            i = random.randrange(1, len(p))
            pregunta(p[:i] + mayus(p[i:i + 1]) + p[i + 1:], 'mayúscula-en-medio')
            pregunta(minus(p[:1]) + mayus(p[1:]), 'mayúsculas-menos-la-primera')
    # casos sueltos
    for t, v in (('tubo', 1), ('tuvo', 1), ('haver', 0), ('árbol', 1), ('Arbol', 1), ('ARBOL', 1), ('arbol', 0), ('Árbol', 1), ('camión', 1), ('CAMION', 1),
                 ('CAMIÓN', 1), ('Camion', 0), ('CAFÉ', 1), ('CAFE', 0), ('Madrid', 1), ('MADRID', 1), ('madrid', 0),
                 ('MALAGA', 1), ('Malaga', 0), ('Málaga', 1), ('África', 1), ('Africa', 1), ('AFRICA', 1), ('africa', 0),
                 ('ADSL', 1), ('adsl', 0), ('Adsl', 0), ('CIGÜEÑA', 1), ('CIGUEÑA', 0), ('Cigüeña', 1), ('ESPAÑA', 1), ('España', 1),
                 ('españa', 0), ('ESPANA', 0), ('Ñoño', 1), ('ÑOÑO', 1), ('a', 1), ('X', 1), ('', 1), ('x9', 1), ('3D', 1),
                 ('1984', 1), ('abc123', 1), ('qwrtp', 0), ('casa-', 0), ("d'un", 0), ('dámelo', 1), ('diciéndoselo', 1),
                 ('DICIENDOSELO', 0), ('DICIÉNDOSELO', 1), ('DANDOSELO', 1), ('Diciéndoselo', 1), ('McDonald', 1), ('MCDONALD', 1), ('mcdonald', 0),
                 ('pH', 1), ('PH', 1), ('ph', 0), ('Écija', 1), ('ÉCIJA', 1), ('ECIJA', 0), ('Ecija', 0),
                 ('z' * 63, 0), ('z' * 64, 1), ('z' * 200, 1), ('a' * 63, 0), ('A' * 63, 0), ('Ü' * 40, 0)):
        p = cp(t)
        if m.vale(p) != bool(v):
            print('  FAIL el modelo no da %d para «%s»' % (v, t))
            c.mal += 1
        pregunta(p, 'suelto')

    # 5. las propuestas
    for falta, _, _ in FALTAS:
        pon(b'S ' + cp(falta), None, 'falta')
    estropeadas = []
    while len(estropeadas) < cuantas // 10:
        p = random.choice(palabras)
        if len(p) < 4 or p[0] in MAYUS:
            continue
        i = random.randrange(len(p))
        k = random.random()
        if k < 0.35:
            q = p[:i] + bytes([random.choice(letras)]) + p[i + 1:]
        elif k < 0.55:
            q = p[:i] + p[i + 1:]
        elif k < 0.75:
            q = p[:i] + bytes([random.choice(letras)]) + p[i:]
        elif k < 0.9:
            q = p[:i] + p[i + 1:i + 2] + p[i:i + 1] + p[i + 2:]
        else:
            q = bytes(random.choice(letras) for _ in range(random.randint(2, 9)))
        if not m.vale(q):
            estropeadas.append((q, p if k < 0.9 else None))
    pon(b'Q', None, 'gasto0')
    for q, _ in estropeadas:
        pon(b'S ' + q, None, 'propuestas')
    pon(b'Q', None, 'gasto')
    pon(b'M 3', b'', 'm')
    for q, _ in estropeadas[:300]:
        pon(b'S ' + q, None, 'propuestas3')
    pon(b'M 0', b'', 'm')
    pon(b'S ' + cp('canpo'), b'', 'ninguna si se piden 0')
    pon(b'M 200', b'', 'm')
    pon(b'S ' + cp('canpo'), None, 'pedir 200')
    pon(b'M 8', b'', 'm')
    for t in ('', 'a', 'z' * 24, 'z' * 25, 'z' * 63, 'z' * 64, 'z' * 300, 'x9', 'aeiou' * 5, 'a' * 24, 'e' * 63,
              'lllllllllllllllllllllll', 'kkkkkkkkkkkkkkkkkkkkkkkk', 'yxyxyxyxyxyxyxyxyxyxyxyx', '-', '--', 'ñ' * 12,
              'aaaaaaaaaaaaaaaaaaaaaaaa', 'desoxirribonucleico', 'esternocleidomastoideo'):
        pon(b'S ' + cp(t), None, 'propuestas-raras')

    # 6. las palabras del usuario
    usuario = [
        (b'P clicgest', b'0'), (b'A clicgest', b'0'), (b'P clicgest', b'1'), (b'P Clicgest', b'1'),
        (b'P CLICGEST', b'1'), (b'P clicGest', b'0'), (b'P clicgestx', b'0'), (b'P clicges', b'0'),
        (b'A Albinyana', b'0'), (b'P Albinyana', b'1'), (b'P ALBINYANA', b'1'), (b'P albinyana', b'0'),
        (b'A clicgest', b'0'), (b'A Clicgest', b'0'), (b'L', b'clicgest|Albinyana|'),
        (b'A ' + cp('ñandú2'), b'0'), (b'A dos palabras', b'1'), (b'A ', b'1'), (b'A ' + b'w' * 64, b'1'),
        (b'A ' + b'w' * 63, b'0'), (b'P ' + b'w' * 63, b'1'), (b'P ' + b'W' * 63, b'1'), (b'P ' + b'w' * 62, b'0'),
        (b'A ' + cp('zíriam'), b'0'), (b'P ZIRIAM', b'1'), (b'P ' + cp('Zíriam'), b'1'), (b'P Ziriam', b'0'),
        (b'L', b'clicgest|Albinyana|' + cp('ñandú2') + b'|' + b'w' * 63 + b'|' + cp('zíriam') + b'|'),
        (b'C   uno|dos\r|| Tres  ' + b'q' * 64 + b' cuatro\x1a', b''), (b'L', b'uno|dos|Tres|cuatro|'),
        (b'P clicgest', b'0'), (b'P TRES', b'1'), (b'P tres', b'1'), (b'P Cuatro', b'1'),
        (b'C', b''), (b'L', b''), (b'P Albinyana', b'0'),
    ]
    for o, e in usuario:
        pon(o, e, 'usuario')
    # llenarla: 4096 octetos; cada una ocupa su largo y dos más
    pon(b'C', b'', 'usuario')
    for i in range(420):
        pon(b'A zzpropia' + bytes(97 + int(x) for x in '%04d' % i), b'0' if (i + 1) * 14 <= 4096 else b'1',
            'usuario-llena')
    pon(b'P zzpropiaacjb', b'1', 'usuario')     # la 291, la última que cabe
    pon(b'P zzpropiaacjc', b'0', 'usuario')
    pon(b'A hhhhhh', b'0', 'usuario-llena')     # (292 de 14 son 4088: caben justo 8 más)
    pon(b'A hh', b'1', 'usuario-llena')
    pon(b'P hhhhhh', b'1', 'usuario')
    pon(b'C', b'', 'usuario')

    with tempfile.TemporaryDirectory() as tmp:
        prog = hacer(tmp)

        def corre(lineas, limite=300):
            t0 = time.time()
            r = subprocess.run([prog], input=b'\n'.join(lineas) + b'\n', capture_output=True, cwd=tmp, timeout=limite)
            return r.stdout.split(b'\n')[:-1], r.returncode, time.time() - t0

        open(os.path.join(tmp, 'A:\\PROGS\\DICCIO.DAT'), 'wb').write(datos)
        carga = b'D A:\\PROGS\\PLUMA.HVP'
        sal, cod, _ = corre([carga] + ordenes)
        c.mira(cod == 0 and len(sal) == len(ordenes) + 1 and sal[0] == b'H1',
               'ort_cargar lee DICCIO.DAT de la carpeta del módulo y lo da por bueno',
               'código %d, %d respuestas de %d, %r' % (cod, len(sal), len(ordenes) + 1, sal[:1]))
        sal = sal[1:] + [b''] * (len(ordenes) + 1 - len(sal))
        c.mira(not any(b'!' in s for s in sal), 'ninguna rutina cambia un registro ni escribe fuera de lo suyo',
               ', '.join(txt(o) for o, s in zip(ordenes, sal) if b'!' in s)[:300])

        print('== ¿está bien escrita?, contra el modelo ==')
        grupos = {}
        for o, e, r, s in zip(ordenes, espera, rotulo, sal):
            if o[:1] == b'P' and not r.startswith('usuario'):
                g = grupos.setdefault(r, [0, 0, []])
                g[0] += 1
                if s == e:
                    g[1] += 1
                elif len(g[2]) < 8:
                    g[2].append('%s=%s' % (txt(o[2:]), txt(s)))
        for nombre, que in (('vale', 'formas del diccionario, tal cual: todas valen'),
                            ('vale-Primera', 'con la primera en mayúscula: valen'),
                            ('vale-MAYUS', 'todas en mayúsculas (sin los acentos que no hay): valen'),
                            ('no-estropeada', 'formas con una letra cambiada, quitada, puesta o trocada: no valen'),
                            ('inventada', 'palabras inventadas: como el modelo'),
                            ('propio-en-minúscula', 'nombres propios en minúscula: como el modelo'),
                            ('mayúscula-en-medio', 'con una mayúscula en medio: como el modelo'),
                            ('mayúsculas-menos-la-primera', 'en mayúsculas menos la primera: como el modelo'),
                            ('suelto', 'casos sueltos (acentos, mayúsculas, cifras, largos)')):
            g = grupos[nombre]
            c.mira(g[0] == g[1], '%d %s (%.2f %% igual que el modelo)' % (g[0], que, 100.0 * g[1] / g[0]),
                   ' '.join(g[2]))
        inv = [(o, s) for o, r, s in zip(ordenes, rotulo, sal) if r == 'inventada']
        print('       (de las %d inventadas, %d resultan ser palabras; de los nombres propios en minúscula, %d son '
              'también nombres comunes)' % (len(inv), sum(1 for _, s in inv if s == b'1'),
                                            sum(1 for r, s in zip(rotulo, sal) if r == 'propio-en-minúscula' and s == b'1')))

        print('== un texto de verdad ==')
        pals = re.findall(r'[A-Za-zÁÉÍÓÚÜÑáéíóúüñ]+', TEXTO)
        r2, _, _ = corre([carga] + [b'P ' + cp(p) for p in pals] + [b'S ' + cp(p) for p in pals])
        malas = [p for p, s in zip(pals, r2[1:]) if s != b'1']
        distintas = sorted(set(malas), key=malas.index)
        c.mira(len(malas) <= len(pals) * 0.01, 'de %d palabras (%d distintas) da por malas %d (%.1f %%): %s'
               % (len(pals), len(set(pals)), len(malas), 100.0 * len(malas) / len(pals), ', '.join(distintas) or 'ninguna'))
        for p in distintas:
            print('       %s -> %s' % (p, txt(r2[1 + len(pals) + pals.index(p)]) or '(nada)'))
        erratas = 'Aller fuimos ha la plalla con los niñoz y comimos paeya; hizo muxo calor y nos vañamos hasta que anochecio'
        pe = re.findall(r'[A-Za-zÁÉÍÓÚÜÑáéíóúüñ]+', erratas)
        r3, _, _ = corre([carga] + [b'P ' + cp(p) for p in pe])
        malas = [p for p, s in zip(pe, r3[1:]) if s != b'1']
        c.mira(malas == ['Aller', 'plalla', 'niñoz', 'paeya', 'muxo', 'vañamos', 'anochecio'],
               'en una frase con siete faltas señala justo esas siete', ', '.join(malas))

        print('== las propuestas ==')
        bien = True
        for (falta, buena, entre), s in zip(FALTAS, [s for r, s in zip(rotulo, sal) if r == 'falta']):
            props = txt(s).split(',') if s else []
            sitio = props.index(buena) + 1 if buena in props else 0
            estado = '  ' if entre == 0 else 'ok' if 0 < sitio <= entre else 'NO'
            if estado == 'NO':
                bien = False
            print('       %s %-13s -> %s' % (estado, falta, ', '.join(props) or '(nada)'))
        c.mira(bien, 'las faltas de la lista: la buena, entre las primeras')
        mal, det, dentro, primera, tres, con = 0, [], 0, 0, 0, 0
        resp = [s for r, s in zip(rotulo, sal) if r == 'propuestas']
        for (q, origen), s in zip(estropeadas, resp):
            props = s.split(b',') if s else []
            cerca = m.cerca(q)
            # todas son del diccionario y están a una corrección; no falta ninguna si caben
            if (len(set(props)) != len(props) or not set(props) <= cerca or len(props) != min(8, len(cerca))):
                mal += 1
                if len(det) < 6:
                    det.append('%s -> %s (modelo: %s)' % (txt(q), txt(s), txt(b','.join(sorted(cerca)[:10]))))
            if origen is not None:
                con += 1
                dentro += origen in props
                primera += props[:1] == [origen]
                tres += origen in props[:3]
        c.mira(mal == 0, '%d palabras estropeadas: las propuestas son palabras a una corrección, sin repetir, y '
               'no falta ninguna cuando caben en 8' % len(estropeadas), ' / '.join(det))
        print('       (de las %d hechas estropeando una palabra: la original es la primera propuesta en el %.1f %%, '
              'está entre las 3 primeras en el %.1f %% y entre las 8 en el %.1f %%)'
              % (con, 100.0 * primera / con, 100.0 * tres / con, 100.0 * dentro / con))
        r8 = resp[:300]
        r3 = [s for r, s in zip(rotulo, sal) if r == 'propuestas3']
        c.mira(all(b == b','.join(a.split(b',')[:3]) for a, b in zip(r8, r3)),
               'si se piden 3 son las 3 primeras de las 8 (y no se escribe más allá)')
        s0 = [s for r, s in zip(rotulo, sal) if r in ('ninguna si se piden 0', 'pedir 200')]
        c.mira(s0[0] == b'' and s0[1].count(b',') <= 7 and s0[1].startswith(b'campo'),
               'si se piden 0 no pone ninguna; si se piden 200, 8 a lo más', repr(s0))
        raras = [(o, s) for o, r, s in zip(ordenes, rotulo, sal) if r == 'propuestas-raras']
        ok_r = all(set(s.split(b',') if s else []) <= m.cerca(minus(o[2:])) | {b''} for o, s in raras)
        c.mira(ok_r, 'palabras vacías, de una letra, larguísimas o de letras repetidas: propuestas sanas o ninguna',
               ' / '.join('%s -> %s' % (txt(o[2:22]), txt(s)) for o, s in raras if s)[:300])
        gasto = int([s for r, s in zip(rotulo, sal) if r == 'gasto'][0])
        c.mira(0 < gasto < 100000, 'la búsqueda más larga gasta %d pasos (el tope es 400000)' % gasto)

        print('== las palabras del usuario ==')
        mal = ['%s -> %s (esperado %s)' % (txt(o), txt(s), txt(e)) for o, e, r, s in zip(ordenes, espera, rotulo, sal)
               if r.startswith('usuario') and s != e]
        n_u = sum(1 for r in rotulo if r.startswith('usuario'))
        c.mira(not mal, '%d órdenes: apuntar, que valgan (también con mayúsculas), volcar la lista, ponerla desde un '
               'texto, vaciarla y llenarla hasta que no cabe' % n_u, ' / '.join(mal[:6]))

        print('== archivos rotos ==')
        arcos = list(struct.unpack_from('<%dI' % narcos, datos, 32))

        def archivo(nombre, contenido):
            os.makedirs(os.path.join(tmp, 'rotos'), exist_ok=True)
            open(os.path.join(tmp, 'rotos', nombre + '\\DICCIO.DAT'), 'wb').write(contenido)
            return b'D rotos/' + nombre.encode() + b'\\M.HVP'

        def rehecho(a, largo=None, n=None, suma=None):
            cab = bytearray(datos[:32])
            struct.pack_into('<II', cab, 8, len(a) if n is None else n, 0)
            struct.pack_into('<I', cab, 16, (sum(a) & 0xFFFFFFFF) if suma is None else suma)
            if largo is not None:
                cab[20] = largo
            return bytes(cab) + struct.pack('<%dI' % len(a), *a)

        prueba = [b'P casa', b'P qwrtp', b'P ' + b'z' * 63, b'P ', b'P ARBOL', b'S canpo', b'S ' + b'a' * 24,
                  b'S ' + b'z' * 63, b'S ']
        casos = [('falta', None)]
        for corte in (0, 1, 7, 8, 20, 31, 32, 35, 36, 1000, 1001, len(datos) // 2, len(datos) - 4, len(datos) - 1):
            casos.append(('cortado en %d' % corte, datos[:corte]))
        casos.append(('con un octeto de más', datos + b'\0'))
        casos.append(('con cuatro octetos de más', datos + b'\0\0\0\0'))
        casos.append(('todo ceros', bytes(len(datos))))
        casos.append(('basura', bytes(random.randrange(256) for _ in range(200000))))
        casos.append(('basura con cabecera', datos[:32] + bytes(random.randrange(256) for _ in range(len(datos) - 32))))
        casos.append(('enorme', datos[:32] + bytes(800 * 1024)))
        for i in (33, 5000, len(datos) - 2):
            d = bytearray(datos)
            d[i] ^= 0x10
            casos.append(('un bit cambiado en %d' % i, bytes(d)))
        d = bytearray(datos)
        d[0] = ord('X')
        casos.append(('otra firma', bytes(d)))
        casos.append(('cuenta de arcos 0', rehecho(arcos, n=0)))
        casos.append(('cuenta de arcos enorme', rehecho(arcos, n=0x40000000)))
        casos.append(('cuenta de arcos que desborda', rehecho(arcos, n=0x40000000 + narcos)))
        casos.append(('largo 0', rehecho(arcos, largo=0)))
        casos.append(('largo 64', rehecho(arcos, largo=64)))
        a = arcos[:]
        a[100] = (a[100] & 0x3FF) | (narcos << 10)
        casos.append(('un arco que lleva fuera (suma buena)', rehecho(a)))
        a = arcos[:]
        a[-1] &= ~0x200
        casos.append(('el último nodo sin cerrar (suma buena)', rehecho(a)))
        a = arcos[:]
        a[7] &= ~0xFF
        casos.append(('una letra 0 (suma buena)', rehecho(a)))
        lineas = []
        for i, (nombre, contenido) in enumerate(casos):
            if contenido is None:
                lineas.append(b'D rotos/nohay\\M.HVP')
            else:
                lineas.append(archivo('r%d' % i, contenido))
            lineas += prueba
        lineas += [carga, b'P casa', b'P qwrtp']
        r4, cod, _ = corre(lineas, 120)
        esperado = ([b'H0', b'1', b'1', b'1', b'1', b'1', b'', b'', b'', b''] * len(casos)) + [b'H1', b'1', b'0']
        malos = [casos[i // 10][0] for i in range(0, len(casos) * 10, 10) if r4[i:i + 10] != esperado[i:i + 10]]
        c.mira(cod == 0 and r4 == esperado, '%d archivos que faltan, cortados, con basura o con la cabecera o un arco '
               'mal: ninguno se da por bueno, todo se da por bien escrito y después se carga el bueno' % len(casos),
               ', '.join(malos) or 'código %d' % cod)
        # diccionarios que pasan la comprobación pero no tienen ni pies ni cabeza:
        # arcos al azar (con ciclos), con su suma bien. No se pide que contesten
        # nada con sentido: que acaben pronto y sin salirse
        lineas, n_az = [], 40
        for i in range(n_az):
            n = random.choice((1, 2, 5, 50, 3000))
            a = []
            for j in range(n):
                letra = random.choice(b'aeiou' if i % 2 else b'abcdefghijklmnopqrstuvwxyz\xa0\x82\xa1\xa2\xa3')
                a.append(letra | random.choice((0, 0x100)) | random.choice((0, 0, 0x200)) | (random.randrange(n) << 10))
            a[-1] |= 0x200
            lineas.append(archivo('azar%d' % i, rehecho(a, largo=63)))
            lineas += [b'P casa', b'P AEIOUAEIOUAEIOUAEIOUAEIOUAEIOUAEIOUAEIOUAEIOUAEIOUAEIOUAEIOUAEI',
                       b'P ' + b'a' * 63, b'P ' + b'A' * 63, b'S casa', b'S ' + b'a' * 63, b'S ' + b'aeiou' * 12,
                       b'S ' + b'AEIOU' * 12, b'S abcdefghijklmnopqrstuvwxyz']
        r5, cod, t5 = corre(lineas, 120)
        c.mira(cod == 0 and len(r5) == n_az * 10 and all(r5[i] == b'H1' for i in range(0, len(r5), 10))
               and not any(b'!' in s for s in r5),
               '%d diccionarios de arcos al azar (con ciclos) que pasan la comprobación: las consultas acaban '
               '(%.2f s todas) y nada se sale de lo suyo' % (n_az, t5), 'código %d, %d respuestas' % (cod, len(r5)))

        # el peor: un nodo que vuelve a sí mismo por «a» y por «á». Sin tope, una
        # palabra de 63 aes tendría 2^63 caminos
        a = [ord('a') | 0x200 | (1 << 10), ord('a') | 0x100 | (1 << 10), 0xA0 | 0x100 | 0x200 | (1 << 10)]
        lineas = [archivo('ciclo', rehecho(a, largo=63)), b'P ' + b'A' * 63, b'P ' + b'A' * 62 + b'B',
                  b'P ' + b'a' * 62 + b'b', b'S ' + b'a' * 62 + b'b', b'S ' + b'a' * 60 + b'bbb', b'S ' + b'A' * 63,
                  b'P casa', carga, b'P casa', b'S canpo', b'M 1', b'S canpo']
        r6, cod, t6 = corre(lineas, 120)
        c.mira(cod == 0 and r6[:4] == [b'H1', b'1', b'0', b'0'] and r6[7:] == [b'0', b'H1', b'1', s0[1], b'', b'campo']
               and not any(b'!' in s for s in r6),
               'un diccionario hecho para no acabar nunca (2^63 caminos): el tope de pasos lo corta (%.2f s) y '
               'después todo sigue bien' % t6, repr([x[-30:] for x in r6]))

        print('== lo que tarda (en esta máquina) ==')
        consultas = [b'P ' + p for p in random.sample(palabras, 100000)]
        consultas += [o for o, r in zip(ordenes, rotulo) if r == 'no-estropeada'][:50000]
        t1 = min(corre([carga, b'T 1'] + consultas)[2] for _ in range(3))
        t21 = min(corre([carga, b'T 21'] + consultas)[2] for _ in range(3))
        por = (t21 - t1) / 20 / len(consultas)
        sug = [b'S ' + q for q, _ in estropeadas[:3000]]
        t0 = min(corre([carga])[2] for _ in range(3))
        ts = (min(corre([carga] + sug * 5)[2] for _ in range(3)) - t0) / (len(sug) * 5)
        c.mira(por < 5e-6 and ts < 2e-3, 'ort_palabra: %.0f ns por palabra (%d consultas); ort_sugerir: %.0f µs por '
               'palabra; cargar y comprobar el diccionario: %.1f ms' % (por * 1e9, len(consultas), ts * 1e6, t0 * 1e3))
        # cuánto anda por el autómata, que es lo que cuenta en un 386
        arcos_vistos = 0
        for o in consultas[:20000]:
            p, i = o[2:], 0
            for letra in p:
                while True:
                    arcos_vistos += 1
                    x = arcos[i]
                    if x & 0xFF >= letra or x & 0x200:
                        break
                    i += 1
                if x & 0xFF != letra or not x >> 10:
                    break
                i = x >> 10
        print('       (una consulta mira de media %.0f arcos del autómata; una búsqueda de propuestas, hasta %d pasos)'
              % (arcos_vistos / 20000.0, gasto))

    print('%d OK / %d FAIL' % (c.ok, c.mal))
    return 1 if c.mal else 0


if __name__ == '__main__':
    sys.setrecursionlimit(10000)
    sys.exit(main())
