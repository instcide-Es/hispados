#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Cliente de prueba de CONSOLA (la consola remota de Hispa-DOS).

Arranca Hispa-DOS en QEMU con HSPRED y la red de usuario de QEMU, con
  -netdev user,hostfwd=tcp:127.0.0.1:<puerto>-:2323
para poder conectarse al puerto 2323 del invitado. Teclea por el puerto serie
HSPRED, CONFRED y CONSOLA, y después, por TCP, comprueba:

  1. El código sale en la pantalla del equipo ANTES de que llegue nadie, con
     lo que le queda («caduca en 10:00»). Al conectarse, CONSOLA lo pide.
  2. Ese código deja entrar: ejecuta VER y devuelve su salida; SALIR cierra.
  3. Un solo uso: al acabar la sesión hay OTRO código, y el usado ya no vale.
  4. 3 intentos fallidos sumando dos conexiones: el código se cambia por otro
     en la pantalla («Durante 30 s»), se corta y, durante la pausa, no se
     aceptan conexiones;
     pasada la pausa, el nuevo deja entrar.
  5. Esc en el equipo cierra CONSOLA.
  6. Caducidad (CONSOLA /CADUCA:15): sin que llegue nadie, el código se renueva
     solo y lo dice; el caducado no vale y el nuevo sí.

(El código mostrado se lee del puerto serie, por donde el invitado saca toda
su consola en las pruebas.)

Uso: consola_cliente.py <imagen> <nic> <puerto_local>
La variable CONSOLA_QEMU añade opciones a QEMU delante de la tarjeta (para
tests/run_red_usb_tests.sh: la controladora USB del adaptador usb-net).
No usa Internet: la red de usuario de QEMU va dentro del espacio de nombres de
red que crea run_consola_tests.sh (unshare -rn).
"""
import os, re, shlex, socket, subprocess, sys, time

IMG, NIC, PORT = sys.argv[1], sys.argv[2], int(sys.argv[3])

serial = bytearray()
fails = 0


def log_ok(m):
    print("  OK   " + m)


def log_ko(m):
    global fails
    fails += 1
    print("  FAIL " + m)


def pump():
    """Pasar lo que haya en la salida de QEMU al acumulador (no bloquea)."""
    try:
        data = os.read(qemu.stdout.fileno(), 65536)
        if data:
            serial.extend(data)
    except (BlockingIOError, OSError):
        pass


def ser_text():
    return serial.decode("cp437", "replace")


def send_serial(s):
    qemu.stdin.write(s.encode("cp437"))
    qemu.stdin.flush()


def wait_serial(substr, timeout=25):
    t = time.time() + timeout
    while time.time() < t:
        pump()
        if substr in ser_text():
            return True
        time.sleep(0.1)
    pump()
    return substr in ser_text()


def tcp_connect():
    s = socket.create_connection(("127.0.0.1", PORT), timeout=20)
    s.settimeout(1.0)
    return s


def tcp_wait(sock, substr, timeout=25):
    """Leer del socket hasta ver substr (o agotar el tiempo). Devuelve lo leído."""
    buf = b""
    t = time.time() + timeout
    while time.time() < t:
        pump()
        try:
            d = sock.recv(4096)
            if d == b"":
                break
            buf += d
            if substr.encode("cp437") in buf:
                return buf.decode("cp437", "replace")
        except socket.timeout:
            pass
    return buf.decode("cp437", "replace")


CODRE = re.compile(r"Código de acceso:\s*(\d{6})")


def codigo_desde(pos, marca=None, timeout=20):
    """El código que se ve en la pantalla del equipo desde la posición pos del
    acumulador (y, si se da, después del texto marca)."""
    t = time.time() + timeout
    while time.time() < t:
        pump()
        txt = serial[pos:].decode("cp437", "replace")
        if marca is not None:
            i = txt.find(marca)
            if i < 0:
                time.sleep(0.1)
                continue
            txt = txt[i:]
        m = CODRE.search(txt)
        if m:
            return m.group(1)
        time.sleep(0.1)
    return None


def cerrada(sock, timeout=5):
    """¿Ha cortado el otro lado la conexión?"""
    t = time.time() + timeout
    sock.settimeout(1.0)
    while time.time() < t:
        pump()
        try:
            if sock.recv(4096) == b"":
                return True
        except socket.timeout:
            pass
        except OSError:
            return True
    return False


def entra(codigo, que):
    """Conectar, dar el código, VER y SALIR. -> True si todo fue bien."""
    s = tcp_connect()
    banner = tcp_wait(s, "código de acceso", 20)
    if "Introduzca el código de acceso" not in banner:
        log_ko("%s: no pidió el código: %r" % (que, banner[-200:]))
        s.close()
        return False
    s.sendall((codigo + "\n").encode())
    out = tcp_wait(s, "Acceso concedido", 20)
    if "Acceso concedido" not in out:
        log_ko("%s: el código %s NO dio acceso: %r" % (que, codigo, out[-200:]))
        s.close()
        return False
    tcp_wait(s, ">", 10)
    s.sendall(b"VER\n")
    out = tcp_wait(s, "Hispa-DOS", 20)
    ver = "Hispa-DOS versión" in out
    s.sendall(b"SALIR\n")
    out = tcp_wait(s, "Hasta luego", 10)
    s.close()
    if ver and "Hasta luego" in out:
        log_ok("%s: el código %s da acceso, VER va y SALIR cierra" % (que, codigo))
        return True
    log_ko("%s: VER o SALIR no fueron bien: %r" % (que, out[-200:]))
    return False


def malos(sock, codigos):
    """Mandar códigos erróneos -> lo que contestó al último."""
    out = ""
    for c in codigos:
        sock.sendall((c + "\n").encode())
        out = tcp_wait(sock, "válido", 15)
    return out


# ---- arrancar QEMU ----
cmd = [
    "qemu-system-i386", "-display", "none", "-serial", "stdio",
    "-monitor", "none", "-parallel", "none", "-m", "32",
    "-drive", "file=%s,format=raw,if=floppy" % IMG, "-boot", "a", "-no-reboot",
    "-netdev", "user,id=n0,hostfwd=tcp:127.0.0.1:%d-:2323" % PORT,
] + shlex.split(os.environ.get("CONSOLA_QEMU", "")) + [
    "-device", "%s,netdev=n0" % NIC,
]
qemu = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT)
os.set_blocking(qemu.stdout.fileno(), False)

rc = 1
try:
    # esperar al indicador tras arrancar
    if not wait_serial("Escriba AYUDA", 40):
        print("  (no arrancó a tiempo)")
        print(ser_text()[-2000:])
        raise SystemExit(1)
    time.sleep(1)
    # cargar la red y lanzar CONSOLA
    send_serial("HSPRED\r")
    wait_serial("MAC", 20)
    time.sleep(0.5)
    send_serial("CONFRED\r")
    wait_serial("Dirección IP", 20)
    time.sleep(0.5)
    pos = len(serial)
    send_serial("CONSOLA\r")
    if wait_serial("Esperando conexión", 20):
        log_ok("CONSOLA activada y esperando conexión")
    else:
        log_ko("CONSOLA no llegó a esperar conexión")
        print(ser_text()[-2000:])
        raise SystemExit(1)

    # ---- 1. el código, en la pantalla antes de conectar nadie ----
    codigo1 = codigo_desde(pos)
    if not codigo1:
        log_ko("no se vio el código en la pantalla del equipo")
        print(ser_text()[-800:])
        raise SystemExit(1)
    time.sleep(1.5)
    pump()
    txt = serial[pos:].decode("cp437", "replace")
    if re.search(r"caduca en (10:00|9:5\d)", txt) and "Conexión entrante" not in txt:
        log_ok("código en la pantalla antes de que llegue nadie: %s, con «caduca en»" % codigo1)
    else:
        log_ko("el código previo no dice lo que le queda: %r" % txt[-300:])

    # ---- 2. entra con él ----
    pos = len(serial)
    entra(codigo1, "1ª conexión")

    # ---- 3. un solo uso ----
    codigo2 = codigo_desde(pos, "ya se ha usado")
    if codigo2 and codigo2 != codigo1:
        log_ok("un solo uso: al acabar, otro código en la pantalla: %s" % codigo2)
    else:
        log_ko("tras usarlo no salió otro código (%s)" % codigo2)
        codigo2 = codigo2 or "000000"
    s2 = tcp_connect()
    tcp_wait(s2, "código de acceso", 20)
    out = malos(s2, [codigo1])
    if "Código no válido" in out and "Acceso concedido" not in out:
        log_ok("el código ya usado no vale")
    else:
        log_ko("el código usado dio otra respuesta: %r" % out[-200:])

    # ---- 4. 3 fallos sumando conexiones: código nuevo y pausa ----
    out = malos(s2, ["000000"])         # 2º fallo con codigo2
    s2.close()
    wait_serial("Sesión cerrada", 10)
    pos = len(serial)
    s3 = tcp_connect()
    tcp_wait(s3, "código de acceso", 20)
    out = malos(s3, ["000001"])         # 3º: el código cambia y corta
    t_quemado = time.time()
    if "Demasiados intentos" in out and cerrada(s3):
        log_ok("al 3er fallo (sumando dos conexiones) corta y lo dice")
    else:
        log_ko("al 3er fallo no cortó: %r" % out[-200:])
    s3.close()
    codigo3 = codigo_desde(pos, "3 intentos fallidos")
    if "Durante 30 s no se aceptan" not in serial[pos:].decode("cp437", "replace"):
        log_ko("la primera pausa no es de 30 s")
    if codigo3 and codigo3 != codigo2:
        log_ok("tras 3 fallos, otro código en la pantalla: %s" % codigo3)
    else:
        log_ko("tras 3 fallos no cambió el código (%s)" % codigo3)
        codigo3 = codigo3 or "000000"
    wait_serial("Sesión cerrada", 10)
    s4 = tcp_connect()
    out = tcp_wait(s4, "Espere", 15)
    if "Espere y vuelva a probar" in out and "Introduzca" not in out and cerrada(s4):
        log_ok("durante la pausa no se aceptan conexiones")
    else:
        log_ko("en la pausa dejó intentar: %r" % out[-200:])
    s4.close()
    wait_serial("Sesión cerrada", 10)
    time.sleep(max(0, t_quemado + 32 - time.time()))
    entra(codigo3, "tras la pausa, el código nuevo")

    # ---- 5. cerrar CONSOLA con Esc (por el puerto serie) ----
    time.sleep(1)
    wait_serial("Esperando conexión", 10)
    serial.clear()
    send_serial("\x1b")
    if wait_serial("CONSOLA cerrada", 10):
        log_ok("Esc en el equipo cierra CONSOLA y suelta el puerto")
    else:
        log_ko("Esc no cerró CONSOLA")

    # ---- 6. caducidad: CONSOLA /CADUCA:15 ----
    time.sleep(1)
    pos = len(serial)
    send_serial("CONSOLA /CADUCA:15\r")
    wait_serial("Esperando conexión", 20)
    ca = codigo_desde(pos)
    cb = codigo_desde(pos, "ha caducado", 30)
    if ca and cb and ca != cb:
        log_ok("caduca solo y enseña otro sin que llegue nadie: %s -> %s" % (ca, cb))
    else:
        log_ko("no caducó (%s, %s)" % (ca, cb))
    if cb:
        s6 = tcp_connect()
        tcp_wait(s6, "código de acceso", 20)
        out = malos(s6, [ca or "000000"])
        if "Código no válido" in out:
            log_ok("el código caducado no vale")
        else:
            log_ko("el caducado dio otra respuesta: %r" % out[-200:])
        s6.close()
        wait_serial("Sesión cerrada", 10)
        # el vigente (puede haber caducado otra vez mientras tanto: el último)
        txt = serial[pos:].decode("cp437", "replace")
        vig = CODRE.findall(txt)[-1]
        entra(vig, "/CADUCA:15, el código vigente")
    wait_serial("Esperando conexión", 10)
    serial.clear()
    send_serial("\x1b")
    wait_serial("CONSOLA cerrada", 10)
    send_serial("ECHO fin-de-la-prueba\r")
    wait_serial("fin-de-la-prueba", 10)

    rc = 1 if fails else 0
finally:
    try:
        qemu.stdin.close()
    except Exception:
        pass
    try:
        qemu.terminate()
        qemu.wait(timeout=5)
    except Exception:
        try:
            qemu.kill()
        except Exception:
            pass

if fails:
    print("  --- últimos 1500 caracteres de la consola serie del invitado ---")
    print(ser_text()[-1500:])
sys.exit(rc)
