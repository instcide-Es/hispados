#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""Hace programas/comun/DICCIO.DAT, el diccionario del corrector ortográfico
(programas/comun/ortografia.inc), a partir de un diccionario de Hunspell
(es_ES.dic, las raíces con sus marcas, y es_ES.aff, las reglas de prefijos y
sufijos). De dónde sale, su licencia y los límites: docs/ORTOGRAFIA.md.

uso: hazdiccio.py CARPETA_O_BASE [-o DICCIO.DAT] [--formas LISTA.txt]
                  [--mas PALABRAS.txt] [--sin-suplemento] [--medir]

  CARPETA_O_BASE  la carpeta donde están es_ES.aff y es_ES.dic (o esa ruta
                  sin la extensión, para otro nombre)
  -o              dónde dejar el diccionario (si no se da, no se escribe)
  --formas        deja además todas las formas, una por línea, en UTF-8
  --mas           palabras de más (una por línea, UTF-8), tal cual
  --sin-suplemento  sin los nombres propios que añade esta herramienta (la
                  lista SUPLEMENTO, más abajo): solo el diccionario de origen
  --medir         dice también lo que ocuparía el otro camino (guardar raíces
                  y reglas y aplicarlas al consultar)

Lo que hace:

 1. EXPANDE: de cada raíz saca todas sus formas aplicando las reglas como lo
    hace Hunspell: un sufijo de sus marcas; un segundo sufijo si el primero
    trae marcas («ción/S»: «abreviar» -> «abreviación» -> «abreviaciones»);
    y un prefijo de sus marcas delante de la raíz o de cualquiera de esas
    formas (todas las reglas del español son de las que se combinan, «Y»).
    Una regla se aplica si la palabra cumple su condición y acaba (o
    empieza) por lo que la regla quita.
 2. Pasa cada forma a la página de códigos 437, que es la de Hispa-386. Ahí
    no hay Á, Í, Ó ni Ú: en los nombres propios se guardan sin acento
    («Ávila» -> «Avila»), que es como se pueden teclear.
 3. Hace el AUTÓMATA MÍNIMO que reconoce justo esas palabras (un «DAWG»:
    comparte los principios, como un árbol de letras, y también los finales,
    que en español son casi todos iguales: «-ación», «-ándoselos»...) y lo
    guarda.

El archivo (los números, en little-endian):

     0  "DICCIO1", 26
     8  dd  cuántos arcos
    12  dd  cuántas palabras reconoce (solo se enseña)
    16  dd  suma de todos los arcos (módulo 2^32)
    20  db  largo de la palabra más larga
    21  (11 octetos a cero)
    32  los arcos, de 4 octetos:
          bits 0-7    la letra (CP437)
          bit 8       con esta letra se acaba una palabra
          bit 9       es el último arco de su nodo
          bits 10-31  el primer arco del nodo al que lleva (0: a ninguno)

Un nodo son varios arcos seguidos, ordenados por su letra; el del principio
de las palabras empieza en el arco 0. Para ver si una palabra está se sigue
letra a letra; está si con la última letra se llega a un arco con el bit 8.
"""
import argparse, os, re, struct, sys

MAGIA = b'DICCIO1\x1a'
CAB = 32
# las mayúsculas acentuadas que no hay en CP437
SIN_ACENTO = str.maketrans('ÁÍÓÚ', 'AIOU')

# El diccionario de origen trae pocos nombres propios (unos 940). Estos son
# los que más se echan en falta al escribir una carta: nombres de pila,
# apellidos y lugares corrientes, y unas pocas palabras sueltas. Se añaden tal
# cual (sin plurales ni nada) los que no estén ya. Esta lista es de este
# proyecto (GPL-2.0 o posterior); «--sin-suplemento» la deja fuera.
SUPLEMENTO = """
Antonio Manuel José Francisco David Juan Javier Daniel Carlos Jesús Alejandro Miguel Rafael Pedro Pablo Ángel
Sergio Fernando Jorge Luis Alberto Álvaro Adrián Diego Raúl Iván Enrique Ramón Vicente Óscar Andrés Joaquín
Santiago Eduardo Víctor Mario Roberto Jaime Ignacio Marcos Hugo Jordi Ricardo Salvador Guillermo Emilio Gabriel
Julio Julián Tomás Martín Agustín Nicolás Gonzalo Félix Joan Josep Ismael Cristian Samuel Héctor Mariano
Domingo Alfonso Felipe Lucas Mateo Rodrigo Esteban César Lorenzo Gregorio Rubén Marc Pau Xavier Ernesto Arturo
Alfredo Sebastián Bernardo Isidro Benito Eugenio Federico Germán Gerardo Leonardo Marcelino Matías Pascual
Rodolfo Rogelio Teodoro Valentín Leandro Aurelio Anselmo Fermín Gaspar Melchor Baltasar Cristóbal Bartolomé
Jacinto Narciso Simón Abel Adolfo Aitor Iker Unai Asier Mikel Íñigo Borja Gorka Ander Oriol Ferran Albert
Lluís Carles Francesc Miquel Enric Pepe Paco Manolo Nacho Toni Quique Juanjo Josemari
María Carmen Ana Isabel Dolores Pilar Teresa Rosa Cristina Laura Marta Antonia Francisca Lucía Mercedes Elena
Josefa Concepción Sara Paula Raquel Manuela Rosario Juana Beatriz Encarnación Nuria Silvia Julia Patricia Irene
Andrea Rocío Mónica Alba Ángela Sonia Sandra Marina Susana Yolanda Natalia Margarita Eva Inmaculada Claudia
Alicia Esther Verónica Noelia Carla Sofía Carolina Amparo Nerea Lorena Inés Miriam Victoria Lidia Catalina
Consuelo Gloria Ángeles Daniela Martina Emilia Aurora Esperanza Olga Milagros Josefina Purificación Montserrat
Virginia Fátima Celia Adriana Clara Blanca Soledad Asunción Araceli Begoña Remedios Trinidad Elisa Emma Valeria
Aitana Vanesa Tamara Gema Estefanía Lourdes Luisa Magdalena Matilde Paloma Rebeca Regina Rita Elvira Eugenia
Felisa Guadalupe Leonor Leticia Macarena Maite Nieves Noemí Olivia Ramona Violeta Jimena Agustina Amalia Amelia
Bárbara Cecilia Estrella Fabiola Gabriela Graciela Lola Maribel Paquita Pepa Concha Charo Maruja Merche Loli
Mari Montse Conchita Lupe Núria Mercè
García Rodríguez González Fernández López Martínez Sánchez Pérez Gómez Jiménez Ruiz Hernández Díaz Moreno Muñoz
Álvarez Romero Alonso Gutiérrez Navarro Torres Domínguez Vázquez Ramos Gil Ramírez Serrano Blanco Molina Morales
Suárez Ortega Delgado Castro Ortiz Rubio Marín Sanz Núñez Iglesias Medina Garrido Cortés Castillo Santos Lozano
Guerrero Cano Prieto Méndez Cruz Calvo Gallego Vidal León Márquez Herrera Peña Flores Cabrera Campos Vega Fuentes
Carrasco Díez Caballero Reyes Nieto Aguilar Santana Herrero Montero Hidalgo Giménez Ibáñez Ferrer Durán Benítez
Mora Vargas Arias Carmona Crespo Román Pastor Soto Sáez Velasco Moya Soler Parra Bravo Gallardo Rojas Pardo
Merino Franco Espinosa Izquierdo Lara Rivas Silva Rivera Casado Arroyo Redondo Camacho Rey Vera Otero Luque
Galán Montes Ríos Sierra Segura Carrillo Martí Soriano Mendoza Bernal Robles Vila Valero Palacios Expósito
Varela Macías Pereira Guerra Heras Bueno Roldán Villar Contreras Miranda Guillén Mateos Escudero Aguilera
Menéndez Casas Aparicio Rivero Estévez Beltrán Padilla Rico Calderón Abad Gracia Salas Conde Jurado Plaza
Quintana Acosta Aranda Blázquez Roca Bermúdez Costa Santamaría Salazar Guzmán Serra Cuesta Villanueva Manzano
Hurtado Trujillo Rueda Pacheco Ávila Lázaro Escobar Sancho Zamora Millán Alarcón Luna Castaño Bermejo Paredes
Ballesteros Valverde Maldonado Bautista Valle Ponce Oliva Lorente Juárez Cordero Collado Pozo Murillo Cuenca
Montoya Cuevas Martos Barroso Ros Quesada Barrera Marco Ordóñez Corral Cabello Navas Pulido Rojo Saiz Soria
Arenas Aguado Galindo Vallejo Asensio Mena Escribano Caro Polo Naranjo Aguirre Villa Mata Reina Paz Amador
Linares Leal Burgos Ojeda Carretero Bonilla Oliver Sosa Roig Aragón Carrión Castellano Clemente Córdoba
Villalba Carrera Andreu Hernando Cáceres Calero Cobo Correa Sola Velázquez Alfaro Chacón Zapata Toledo Riera
Saavedra Pineda Olmo Salinas Ayala
Andalucía Aragón Asturias Baleares Canarias Cantabria Castilla Cataluña Extremadura Galicia Rioja Madrid Murcia
Navarra Euskadi Valencia Ceuta Melilla Álava Albacete Alicante Almería Badajoz Barcelona Cádiz Castellón
Coruña Gerona Girona Granada Guadalajara Guipúzcoa Huelva Huesca Jaén Lérida Lleida Lugo Málaga Orense Ourense
Palencia Pontevedra Salamanca Segovia Sevilla Tarragona Tenerife Teruel Valladolid Vizcaya Zaragoza Bilbao Vigo
Gijón Oviedo Pamplona Santander Logroño Vitoria Mérida Elche Cartagena Jerez Marbella Reus Mallorca Menorca
Ibiza Lanzarote Fuerteventura
España Portugal Francia Italia Alemania Inglaterra Irlanda Escocia Gales Holanda Bélgica Suiza Austria Grecia
Turquía Rusia Ucrania Polonia Suecia Noruega Dinamarca Finlandia Islandia Hungría Rumanía Bulgaria Croacia
Serbia Marruecos Argelia Túnez Egipto Sudáfrica Nigeria Senegal Canadá México Méjico Guatemala Honduras
Nicaragua Panamá Cuba Venezuela Colombia Ecuador Perú Bolivia Chile Argentina Uruguay Paraguay Brasil China
Japón India Corea Israel Irán Irak Australia Europa América Asia África Oceanía Londres París Roma Berlín
Lisboa Bruselas Moscú Washington Pekín Tokio Atlántico Pacífico Mediterráneo Pirineos Ebro Duero Tajo
Guadalquivir Dios Navidad Internet
internet wifi email emails hardware software byte bytes kilobyte kilobytes megabyte megabytes gigabyte gigabytes
guión guiones vámonos
"""


# ---------------------------------------------------------------- las reglas
def lee_aff(ruta):
    """-> (prefijos, sufijos): {marca: [(quita, pone, marcas, condición)]}"""
    pre, suf = {}, {}
    for linea in open(ruta, encoding='utf-8'):
        p = linea.split()
        if len(p) < 5 or p[0] not in ('PFX', 'SFX'):
            if p and p[0] in ('NEEDAFFIX', 'CIRCUMFIX', 'COMPOUNDFLAG', 'COMPOUNDRULE', 'ONLYINCOMPOUND',
                              'FORBIDDENWORD', 'KEEPCASE', 'AF', 'AM', 'COMPLEXPREFIXES', 'NOSUGGEST'):
                sys.exit('hazdiccio: el .aff usa «%s», que este expansor no entiende' % p[0])
            if len(p) == 4 and p[0] in ('PFX', 'SFX') and p[2] != 'Y':
                sys.exit('hazdiccio: la regla «%s» no se combina (N): no está previsto' % p[1])
            continue
        tipo, marca, quita, pone, cond = p[:5]
        marcas = ''
        if '/' in pone:
            pone, marcas = pone.split('/', 1)
        quita = '' if quita == '0' else quita
        pone = '' if pone == '0' else pone
        if cond == '.':
            er = None
        elif tipo == 'SFX':
            er = re.compile('(?:%s)$' % cond)
        else:
            er = re.compile('^(?:%s)' % cond)
        (pre if tipo == 'PFX' else suf).setdefault(marca, []).append((quita, pone, marcas, er))
    return pre, suf


def con_sufijos(suf, pal, marcas, nivel=0):
    for m in marcas:
        for quita, pone, mas, er in suf.get(m, ()):
            if quita and not pal.endswith(quita):
                continue
            if er and not er.search(pal):
                continue
            nueva = pal[:len(pal) - len(quita)] + pone
            yield nueva
            if mas and nivel == 0:
                yield from con_sufijos(suf, nueva, mas, 1)


def con_prefijos(pre, pal, marcas):
    for m in marcas:
        for quita, pone, _, er in pre.get(m, ()):
            if quita and not pal.startswith(quita):
                continue
            if er and not er.search(pal):
                continue
            yield pone + pal[len(quita):]


def raices(ruta):
    """las raíces del .dic con sus marcas; -> ([(raíz, marcas)], las que se dejan)"""
    lista, fuera = [], []
    for i, linea in enumerate(open(ruta, encoding='utf-8')):
        linea = linea.strip()
        if not linea or (i == 0 and linea.isdigit()):
            continue
        pal, _, marcas = linea.partition('/')
        pal = pal.strip()
        # con espacios, puntos o cifras no son «una palabra» para quien corrige
        if re.search(r'[^A-Za-zÁÉÍÓÚÑÜáéíóúüñ]', pal):
            fuera.append(pal)
            continue
        lista.append((pal, marcas))
    return lista, fuera


def expande(base, mas=None, suplemento=True):
    """-> (el conjunto de todas las formas, en texto; raíces; reglas; las que se dejan)"""
    if os.path.isdir(base):
        base = os.path.join(base, 'es_ES')
    pre, suf = lee_aff(base + '.aff')
    lista, fuera = raices(base + '.dic')
    formas = set()
    for pal, marcas in lista:
        suyas = [pal] + list(con_sufijos(suf, pal, marcas))
        formas.update(suyas)
        if any(m in pre for m in marcas):
            for s in suyas:
                formas.update(con_prefijos(pre, s, marcas))
    if suplemento:
        formas.update(SUPLEMENTO.split())
    if mas:
        for linea in open(mas, encoding='utf-8'):
            if linea.strip() and not linea.startswith('#'):
                formas.add(linea.strip())
    reglas = sum(len(v) for v in pre.values()) + sum(len(v) for v in suf.values())
    return formas, lista, reglas, fuera


def a_cp437(formas):
    """-> (las formas en CP437, ordenadas y sin repetir; las que no se pueden poner)"""
    bien, mal = set(), []
    for f in formas:
        try:
            b = f.translate(SIN_ACENTO).encode('cp437')
        except UnicodeEncodeError:
            mal.append(f)
            continue
        if 1 <= len(b) <= 63 and min(b) > 32:
            bien.add(b)
        else:
            mal.append(f)
    return sorted(bien), mal


# -------------------------------------------------------------- el autómata
class Nodo:
    __slots__ = ('fin', 'arcos', 'num')

    def __init__(self):
        self.fin = False        # aquí se acaba una palabra
        self.arcos = {}         # letra -> Nodo
        self.num = None

    def clave(self):
        return (self.fin, tuple((c, id(n)) for c, n in sorted(self.arcos.items())))


def automata(palabras):
    """El autómata mínimo de una lista ORDENADA (el método de Daciuk: como
    llegan en orden, al pasar a la palabra siguiente lo que ya no comparte
    con ella está acabado y se puede cambiar por un nodo igual que ya haya)."""
    vistos = {}
    raiz = Nodo()
    camino = [raiz]
    antes = b''

    def cierra(hasta):
        while len(camino) - 1 > hasta:
            hijo = camino.pop()
            igual = vistos.setdefault(hijo.clave(), hijo)
            if igual is not hijo:
                camino[-1].arcos[antes[len(camino) - 1]] = igual

    for pal in palabras:
        comun = 0
        while comun < len(pal) and comun < len(antes) and pal[comun] == antes[comun]:
            comun += 1
        cierra(comun)
        for c in pal[comun:]:
            n = Nodo()
            camino[-1].arcos[c] = n
            camino.append(n)
        camino[-1].fin = True
        antes = pal
    cierra(0)
    return raiz


def guarda(raiz, cuantas, largo):
    """-> el archivo. Los nodos, a lo ancho desde la raíz (que queda en el 0)."""
    orden, cola = [], [raiz]
    raiz.num = 0
    sitio = 0
    while cola:
        sig = []
        for n in cola:
            orden.append(n)
            sitio += len(n.arcos)
            for c in sorted(n.arcos):
                h = n.arcos[c]
                if h.num is None and h.arcos:
                    h.num = -1          # (ya en la cola)
                    sig.append(h)
        cola = sig
    sitio = 0
    for n in orden:
        n.num = sitio
        sitio += len(n.arcos)
    if sitio >= 1 << 22:
        sys.exit('hazdiccio: demasiados arcos (%d)' % sitio)
    arcos = []
    for n in orden:
        letras = sorted(n.arcos)
        for i, c in enumerate(letras):
            h = n.arcos[c]
            arcos.append(c | (0x100 if h.fin else 0) | (0x200 if i == len(letras) - 1 else 0)
                         | ((h.num if h.arcos else 0) << 10))
    cab = MAGIA + struct.pack('<IIIB', len(arcos), cuantas, sum(arcos) & 0xFFFFFFFF, largo)
    return cab.ljust(CAB, b'\0') + struct.pack('<%dI' % len(arcos), *arcos), len(orden)


# ------------------------------------------------ leerlo (lo usan las pruebas)
def lee(datos):
    """-> las palabras (en CP437) que reconoce un DICCIO.DAT ya hecho."""
    if datos[:8] != MAGIA:
        raise ValueError('no es un DICCIO.DAT')
    n = struct.unpack_from('<I', datos, 8)[0]
    arcos = struct.unpack_from('<%dI' % n, datos, CAB)
    salen = []

    def baja(i, pal):
        while True:
            a = arcos[i]
            p = pal + bytes([a & 0xFF])
            if a & 0x100:
                salen.append(p)
            if a >> 10:
                baja(a >> 10, p)
            if a & 0x200:
                return
            i += 1
    baja(0, b'')
    return salen


# ------------------------------------------------------------ el otro camino
def mide_otro(lista, reglas, formas437):
    """Lo que ocuparía guardar raíces y reglas (y aplicarlas al consultar)."""
    orden = sorted(lista)
    juegos = {m for _, m in orden}
    tam = 0
    antes = ''
    for pal, marcas in orden:               # cada raíz: lo que comparte con la de
        comun = 0                           # antes (1), lo que no (n), y sus marcas (2)
        while comun < len(pal) and comun < len(antes) and pal[comun] == antes[comun]:
            comun += 1
        tam += 1 + len(pal[comun:].encode('cp437', 'replace')) + 2
        antes = pal
    tam_juegos = sum(len(j) + 1 for j in juegos)
    tam_reglas = reglas * 12                # quita, pone, condición y marcas, por lo bajo
    print('  camino (a), raíces y reglas: %d raíces (%d K con el principio compartido), %d juegos de marcas (%d K), '
          '%d reglas (unos %d K): unos %d K en total'
          % (len(orden), tam // 1024, len(juegos), tam_juegos // 1024, reglas, tam_reglas // 1024,
             (tam + tam_juegos + tam_reglas) // 1024))


def main():
    ap = argparse.ArgumentParser(description='Hace DICCIO.DAT desde un diccionario de Hunspell')
    ap.add_argument('base')
    ap.add_argument('-o', dest='salida')
    ap.add_argument('--formas')
    ap.add_argument('--mas')
    ap.add_argument('--sin-suplemento', action='store_true')
    ap.add_argument('--medir', action='store_true')
    a = ap.parse_args()
    solas = expande(a.base, None, False)[0]
    formas, lista, reglas, fuera = expande(a.base, a.mas, not a.sin_suplemento)
    pals, mal = a_cp437(formas)
    print('  %d raíces, %d reglas -> %d formas; con las %d palabras añadidas, %d'
          % (len(lista), reglas, len(a_cp437(solas)[0]), len(formas - solas), len(pals)))
    if fuera:
        print('  raíces que se dejan (no son una sola palabra): ' + ', '.join(fuera))
    if mal:
        print('  formas que no se pueden poner en CP437: ' + ', '.join(sorted(mal)[:20]))
    if a.formas:
        with open(a.formas, 'w', encoding='utf-8') as f:
            f.write(''.join(p.decode('cp437') + '\n' for p in pals))
    largo = max(len(p) for p in pals)
    datos, nodos = guarda(automata(pals), len(pals), largo)
    print('  autómata: %d nodos, %d arcos, %d octetos; la palabra más larga, %d letras'
          % (nodos, (len(datos) - CAB) // 4, len(datos), largo))
    if lee(datos) != pals:
        sys.exit('hazdiccio: el autómata no reconoce justo las formas (error de esta herramienta)')
    if a.medir:
        mide_otro(lista, reglas, pals)
    if a.salida:
        with open(a.salida, 'wb') as f:
            f.write(datos)
        print('  escrito ' + a.salida)


if __name__ == '__main__':
    sys.setrecursionlimit(10000)
    main()
