#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Cliente de prueba de CONSOLA (la consola remota de Hispa-DOS).

Arranca Hispa-DOS en QEMU con HSPRED y la red de usuario de QEMU, con
  -netdev user,hostfwd=tcp:127.0.0.1:<puerto>-:2323
para poder conectarse al puerto 2323 del invitado. Teclea por el puerto serie
HSPRED, CONFRED y CONSOLA, y después, por TCP, comprueba:

  1. Al conectarse, CONSOLA pide un código (no deja entrar sin él).
  2. El código mostrado en la pantalla del equipo (que el invitado saca también
     por el puerto serie, como toda su consola) deja entrar: ejecuta VER y
     devuelve su salida.
  3. SALIR cierra la sesión.
  4. Una segunda conexión genera OTRO código distinto.
  5. Un código erróneo (sin leer el que se muestra) NO deja entrar y, tras los
     intentos, corta.

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
    send_serial("CONSOLA\r")
    if wait_serial("Esperando conexión", 20):
        log_ok("CONSOLA activada y esperando conexión")
    else:
        log_ko("CONSOLA no llegó a esperar conexión")
        print(ser_text()[-2000:])
        raise SystemExit(1)

    # ---- 1ª conexión: con el código correcto ----
    serial.clear()
    s1 = tcp_connect()
    banner = tcp_wait(s1, "código de acceso", 20)
    if "Introduzca el código de acceso" in banner:
        log_ok("pide el código de acceso al conectar")
    else:
        log_ko("no pidió el código: %r" % banner[-200:])
    # el código sale por el puerto serie (la consola del equipo)
    wait_serial("Código de acceso:", 20)
    m = re.search(r"Código de acceso:\s*(\d{6})", ser_text())
    if not m:
        log_ko("no se vio el código en la pantalla del equipo")
        print(ser_text()[-800:])
        raise SystemExit(1)
    codigo1 = m.group(1)
    log_ok("código mostrado en el equipo: %s" % codigo1)
    # mandar el código correcto
    s1.sendall((codigo1 + "\n").encode())
    out = tcp_wait(s1, "Acceso concedido", 20)
    if "Acceso concedido" in out:
        log_ok("el código correcto da acceso")
    else:
        log_ko("el código correcto NO dio acceso: %r" % out[-200:])
    # el indicador y una orden
    tcp_wait(s1, ">", 10)
    s1.sendall(b"VER\n")
    out = tcp_wait(s1, "Hispa-DOS", 20)
    if "Hispa-DOS versión" in out:
        log_ok("ejecuta VER y devuelve su salida")
    else:
        log_ko("VER no devolvió su salida: %r" % out[-200:])
    # SALIR cierra la sesión
    s1.sendall(b"SALIR\n")
    out = tcp_wait(s1, "Hasta luego", 10)
    if "Hasta luego" in out:
        log_ok("SALIR cierra la sesión")
    else:
        log_ko("SALIR no se despidió: %r" % out[-200:])
    s1.close()
    wait_serial("Sesión cerrada", 10)

    # ---- 2ª conexión: otro código, y uno erróneo no entra ----
    time.sleep(1)
    serial.clear()
    s2 = tcp_connect()
    tcp_wait(s2, "código de acceso", 20)
    wait_serial("Código de acceso:", 20)
    m = re.search(r"Código de acceso:\s*(\d{6})", ser_text())
    codigo2 = m.group(1) if m else None
    if codigo2 and codigo2 != codigo1:
        log_ok("la 2ª conexión genera OTRO código: %s" % codigo2)
    elif codigo2:
        log_ko("la 2ª conexión repitió el código (%s)" % codigo2)
    else:
        log_ko("no se vio el código de la 2ª conexión")
    # un código erróneo (sin usar el mostrado): 3 intentos y corta
    rejected = False
    for i in range(3):
        s2.sendall(b"000000\n")
        out = tcp_wait(s2, "válido", 10)
        if "Código no válido" in out:
            rejected = True
    # la conexión se cierra tras agotar los intentos
    time.sleep(1)
    pump()
    try:
        s2.settimeout(3)
        closed = (s2.recv(16) == b"")
    except socket.timeout:
        closed = False
    except OSError:
        closed = True
    if rejected:
        log_ok("un código erróneo se rechaza (Código no válido)")
    else:
        log_ko("el código erróneo no se rechazó")
    # no debe haber entrado nunca sin el código bueno
    if "Acceso concedido" not in out:
        log_ok("sin el código válido NO se entra")
    else:
        log_ko("entró sin el código válido")
    if closed:
        log_ok("tras los intentos fallidos, corta la conexión")
    else:
        log_ko("no cortó tras los intentos fallidos")
    s2.close()
    wait_serial("Sesión cerrada", 10)

    # ---- cerrar CONSOLA con Esc (por el puerto serie) y volver al indicador ----
    time.sleep(1)
    serial.clear()
    send_serial("\x1b")
    if wait_serial("CONSOLA cerrada", 10):
        log_ok("Esc en el equipo cierra CONSOLA y suelta el puerto")
    else:
        log_ko("Esc no cerró CONSOLA")
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
