// Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
// nucleo64/programas/rust/mrust/src/main.rs - Hispa-64, etapa 10 f
// (docs/64/HISPA-64.md, sección 23): MRUST, la prueba de Rust (la 1.98.0 que
// fija Ladybird, para x86_64-unknown-linux-musl, enlazado con el compilador
// cruzado de Hispa-64 y NUESTRA musl, no la que trae Rust). Solo con la
// biblioteca estándar: hola, los argumentos y el entorno, los hilos (Mutex,
// RwLock, Condvar, Barrier, canales, thread::scope, thread_local), los
// archivos (crear, escribir, añadir, leer, buscar, renombrar, carpetas,
// borrar), la red (TCP y UDP por 127.0.0.1), el tiempo (Instant, SystemTime,
// dormir), HashMap con su azar, un pánico recogido (desenrollar la pila) y un
// proceso hijo (std::process::Command, con /proc/self/exe).
// Lo que dice con dos espacios delante es igual en Linux y en Hispa-64 (lo
// compara pruebas.sh); al final, «MRUST: todo bien (N pruebas)» y código 0.
// Uso: MRUST uno dos   (los dos argumentos se comprueban)

use std::collections::HashMap;
use std::fs;
use std::io::{Read, Seek, SeekFrom, Write};
use std::net::{TcpListener, TcpStream, UdpSocket};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::mpsc;
use std::sync::{Arc, Barrier, Condvar, Mutex, RwLock};
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

static BIEN: AtomicUsize = AtomicUsize::new(0);
static MAL: AtomicUsize = AtomicUsize::new(0);

fn prueba(qué: &str, ok: bool, detalle: String) {
    if ok {
        BIEN.fetch_add(1, Ordering::SeqCst);
        println!("  {qué}: {detalle}");
    } else {
        MAL.fetch_add(1, Ordering::SeqCst);
        println!("  MAL: {qué}: {detalle}");
    }
}

/// FNV-1a de 32 bits (para dar una cifra de lo que pasa por la red y los archivos)
fn fnv(d: &[u8]) -> u32 {
    let mut h: u32 = 0x811c_9dc5;
    for &b in d {
        h ^= u32::from(b);
        h = h.wrapping_mul(0x0100_0193);
    }
    h
}

/// datos de prueba: n octetos de un generador fijo (xorshift)
fn datos(n: usize, semilla: u32) -> Vec<u8> {
    let mut x = semilla;
    (0..n)
        .map(|_| {
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            (x >> 24) as u8
        })
        .collect()
}

thread_local! {
    static PROPIO: std::cell::Cell<u64> = const { std::cell::Cell::new(0) };
}

fn hilos() {
    // 8 hilos que suman con un Mutex
    let cuenta = Arc::new(Mutex::new(0u64));
    let hs: Vec<_> = (0..8)
        .map(|i| {
            let c = Arc::clone(&cuenta);
            thread::spawn(move || {
                for _ in 0..10_000 {
                    *c.lock().unwrap() += 1;
                }
                PROPIO.with(|p| p.set(i * 1000));
                PROPIO.with(|p| p.get()) + 10_000
            })
        })
        .collect();
    let mut propios = 0;
    for h in hs {
        propios += h.join().unwrap();
    }
    let total = *cuenta.lock().unwrap();
    prueba("hilos con un Mutex", total == 80_000 && propios == 28_000 + 80_000,
        format!("8 hilos, {total} sumas; thread_local de cada uno, {propios}"));

    // canales: 4 productores, un consumidor
    let (tx, rx) = mpsc::channel::<u64>();
    for p in 0..4u64 {
        let tx = tx.clone();
        thread::spawn(move || {
            for i in 0..1000 {
                tx.send(p * 1000 + i).unwrap();
            }
        });
    }
    drop(tx);
    let (mut n, mut suma) = (0u64, 0u64);
    for v in rx {
        n += 1;
        suma += v;
    }
    prueba("canales (mpsc)", n == 4000 && suma == 7_998_000, format!("4 productores, {n} mensajes, suma {suma}"));

    // canal con tope (sync_channel de 0: cita)
    let (tx, rx) = mpsc::sync_channel::<String>(0);
    let h = thread::spawn(move || {
        for s in ["uno", "dos", "tres"] {
            tx.send(s.to_string()).unwrap();
        }
    });
    let v: Vec<String> = rx.iter().collect();
    h.join().unwrap();
    prueba("canal de cita (sync_channel 0)", v == ["uno", "dos", "tres"], v.join(" "));

    // RwLock: lectores a la vez y un escritor
    let rw = Arc::new(RwLock::new(vec![0u32; 16]));
    let hs: Vec<_> = (0..6)
        .map(|i| {
            let rw = Arc::clone(&rw);
            thread::spawn(move || {
                let mut leído = 0u64;
                for _ in 0..500 {
                    if i == 0 {
                        let mut w = rw.write().unwrap();
                        for x in w.iter_mut() {
                            *x += 1;
                        }
                    } else {
                        let r = rw.read().unwrap();
                        let a = r[0];
                        assert!(r.iter().all(|&x| x == a));
                        leído += 1;
                    }
                }
                leído
            })
        })
        .collect();
    let leídas: u64 = hs.into_iter().map(|h| h.join().unwrap()).sum();
    let fin = rw.read().unwrap()[15];
    prueba("RwLock", leídas == 2500 && fin == 500, format!("5 lectores ({leídas} lecturas enteras) y un escritor ({fin})"));

    // Condvar y Barrier
    let par = Arc::new((Mutex::new(0u32), Condvar::new()));
    let barrera = Arc::new(Barrier::new(5));
    let hs: Vec<_> = (0..4)
        .map(|_| {
            let par = Arc::clone(&par);
            let b = Arc::clone(&barrera);
            thread::spawn(move || {
                b.wait();
                let (m, c) = &*par;
                let mut g = m.lock().unwrap();
                *g += 1;
                c.notify_all();
            })
        })
        .collect();
    barrera.wait();
    let (m, c) = &*par;
    let g = c.wait_timeout_while(m.lock().unwrap(), Duration::from_secs(20), |v| *v < 4).unwrap();
    let vistos = *g.0;
    drop(g);
    for h in hs {
        h.join().unwrap();
    }
    prueba("Condvar y Barrier", vistos == 4, format!("una barrera de 5 y {vistos} avisos"));

    // thread::scope: prestar sin Arc; con nombre y pila propia
    let v: Vec<u64> = (1..=1000).collect();
    let (a, b) = v.split_at(500);
    let (sa, sb) = thread::scope(|s| {
        let ha = s.spawn(|| a.iter().sum::<u64>());
        let hb = thread::Builder::new().name("segundo".into()).stack_size(256 * 1024)
            .spawn_scoped(s, || (thread::current().name().unwrap_or("").to_string(), b.iter().sum::<u64>())).unwrap();
        (ha.join().unwrap(), hb.join().unwrap())
    });
    prueba("thread::scope", sa + sb.1 == 500_500 && sb.0 == "segundo",
        format!("{sa} + {} = {}, el hilo «{}»", sb.1, sa + sb.1, sb.0));

    // muchos hilos
    let hs: Vec<_> = (0..100u64).map(|i| thread::spawn(move || i * i)).collect();
    let s: u64 = hs.into_iter().map(|h| h.join().unwrap()).sum();
    prueba("100 hilos", s == 328_350, format!("suma de los cuadrados {s}"));
    let n = thread::available_parallelism().map(|n| n.get()).unwrap_or(0);
    if n == 0 {
        prueba("available_parallelism", false, "no se sabe".into());
    }
}

fn archivos() -> std::io::Result<()> {
    let base = format!("/tmp/mrust-{}", std::process::id());
    let _ = fs::remove_dir_all(&base);
    fs::create_dir_all(format!("{base}/a/b/c"))?;
    let d = datos(200_000, 7);
    let r = format!("{base}/a/datos.bin");
    fs::write(&r, &d)?;
    let leído = fs::read(&r)?;
    prueba("escribir y leer", leído == d, format!("{} octetos, FNV {:08x}", leído.len(), fnv(&leído)));
    {
        let mut f = fs::OpenOptions::new().append(true).open(&r)?;
        f.write_all("¡añadido!".as_bytes())?;
        f.sync_all()?;
    }
    let mut f = fs::File::open(&r)?;
    let largo = f.metadata()?.len();
    f.seek(SeekFrom::Start(200_000))?;
    let mut cola = String::new();
    f.read_to_string(&mut cola)?;
    f.seek(SeekFrom::Start(1000))?;
    let mut trozo = [0u8; 4];
    f.read_exact(&mut trozo)?;
    prueba("añadir, metadata y buscar", largo == 200_011 && cola == "¡añadido!" && trozo[..] == d[1000..1004],
        format!("{largo} octetos, la cola «{cola}», en 1000: {:02x}{:02x}{:02x}{:02x}", trozo[0], trozo[1], trozo[2], trozo[3]));
    drop(f);
    for (i, n) in ["uno.txt", "dos.txt", "señal.txt", "árbol.txt"].iter().enumerate() {
        fs::write(format!("{base}/a/b/{n}"), format!("archivo {i}\n"))?;
    }
    fs::rename(format!("{base}/a/b/dos.txt"), format!("{base}/a/b/c/dos-movido.txt"))?;
    let mut nombres: Vec<String> = fs::read_dir(format!("{base}/a/b"))?
        .map(|e| {
            let e = e.unwrap();
            let t = if e.file_type().unwrap().is_dir() { "/" } else { "" };
            format!("{}{t}", e.file_name().to_string_lossy())
        })
        .collect();
    nombres.sort();
    let movido = fs::read_to_string(format!("{base}/a/b/c/dos-movido.txt"))?;
    prueba("carpetas, nombres con acentos y renombrar", nombres.join(" ") == "c/ señal.txt uno.txt árbol.txt" && movido == "archivo 1\n",
        nombres.join(" "));
    fs::remove_file(format!("{base}/a/b/uno.txt"))?;
    let error = fs::read(format!("{base}/a/b/uno.txt")).err().map(|e| e.kind());
    let error_carpeta = fs::remove_dir(format!("{base}/a")).err().map(|e| e.kind());
    prueba("borrar y los errores", error == Some(std::io::ErrorKind::NotFound) && error_carpeta == Some(std::io::ErrorKind::DirectoryNotEmpty),
        format!("{error:?} y {error_carpeta:?}"));
    fs::remove_dir_all(&base)?;
    prueba("remove_dir_all", fs::metadata(&base).is_err(), "no queda nada".into());
    Ok(())
}

fn red() -> std::io::Result<()> {
    let l = TcpListener::bind("127.0.0.1:0")?;
    let dir = l.local_addr()?;
    let d = datos(1 << 20, 11);
    let h = thread::spawn(move || -> std::io::Result<(usize, u32)> {
        let (mut s, _) = l.accept()?;
        let mut r = Vec::new();
        s.read_to_end(&mut r)?;
        let h = fnv(&r);
        s.write_all(format!("{} {:08x}", r.len(), h).as_bytes())?;
        Ok((r.len(), h))
    });
    let mut c = TcpStream::connect(dir)?;
    c.set_nodelay(true)?;
    c.set_read_timeout(Some(Duration::from_secs(30)))?;
    c.write_all(&d)?;
    c.shutdown(std::net::Shutdown::Write)?;
    let mut resp = String::new();
    c.read_to_string(&mut resp)?;
    let (n, hh) = h.join().unwrap()?;
    prueba("TCP por 127.0.0.1", n == d.len() && hh == fnv(&d) && resp == format!("{} {:08x}", d.len(), fnv(&d)),
        format!("1 MB de ida (FNV {hh:08x}) y la respuesta «{resp}»"));

    // varias conexiones a la vez
    let l = TcpListener::bind("127.0.0.1:0")?;
    let dir = l.local_addr()?;
    let srv = thread::spawn(move || {
        let mut suma = 0u64;
        for _ in 0..8 {
            let (mut s, _) = l.accept().unwrap();
            let mut b = [0u8; 8];
            s.read_exact(&mut b).unwrap();
            let v = u64::from_le_bytes(b);
            suma += v;
            s.write_all(&(v * 2).to_le_bytes()).unwrap();
        }
        suma
    });
    let cs: Vec<_> = (1..=8u64)
        .map(|i| {
            thread::spawn(move || {
                let mut s = TcpStream::connect(dir).unwrap();
                s.write_all(&i.to_le_bytes()).unwrap();
                let mut b = [0u8; 8];
                s.read_exact(&mut b).unwrap();
                u64::from_le_bytes(b)
            })
        })
        .collect();
    let dobles: u64 = cs.into_iter().map(|h| h.join().unwrap()).sum();
    let suma = srv.join().unwrap();
    prueba("8 conexiones TCP a la vez", suma == 36 && dobles == 72, format!("suma {suma}, dobles {dobles}"));

    // UDP
    let a = UdpSocket::bind("127.0.0.1:0")?;
    let b = UdpSocket::bind("127.0.0.1:0")?;
    b.set_read_timeout(Some(Duration::from_secs(10)))?;
    a.set_read_timeout(Some(Duration::from_secs(10)))?;
    let mut n = 0;
    for i in 0..10u8 {
        a.send_to(&[i; 100], b.local_addr()?)?;
        let mut buf = [0u8; 200];
        let (m, de) = b.recv_from(&mut buf)?;
        if m == 100 && buf[..m].iter().all(|&x| x == i) && de == a.local_addr()? {
            n += 1;
        }
    }
    b.connect(a.local_addr()?)?;
    b.send("respuesta".as_bytes())?;
    let mut buf = [0u8; 64];
    let m = a.recv(&mut buf)?;
    prueba("UDP por 127.0.0.1", n == 10 && &buf[..m] == b"respuesta", format!("{n} datagramas de 100 octetos y la respuesta"));
    let malo = TcpStream::connect("127.0.0.1:1").err().map(|e| e.kind());
    prueba("conectar a un puerto cerrado", malo == Some(std::io::ErrorKind::ConnectionRefused), format!("{malo:?}"));
    Ok(())
}

fn tiempo() {
    let t0 = Instant::now();
    thread::sleep(Duration::from_millis(50));
    let e = t0.elapsed();
    prueba("dormir 50 ms (Instant)", e >= Duration::from_millis(50) && e < Duration::from_secs(5), "ni antes de tiempo".into());
    let mut atrás = 0;
    let mut a = Instant::now();
    for _ in 0..10_000 {
        let b = Instant::now();
        if b < a {
            atrás += 1;
        }
        a = b;
    }
    prueba("Instant no va hacia atrás", atrás == 0, "10000 veces".into());
    let ahora = SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0);
    // 2026-01-01 = 1767225600
    prueba("SystemTime", ahora > 1_767_225_600, "después del 1 de enero de 2026".into());
}

fn varios(args: &[String]) {
    prueba("los argumentos", args.len() == 3 && args[1] == "uno" && args[2] == "dos", format!("{:?}", &args[1..]));
    std::env::set_var("MRUST_PRUEBA", "ñandú");
    let v = std::env::var("MRUST_PRUEBA").unwrap_or_default();
    std::env::remove_var("MRUST_PRUEBA");
    prueba("el entorno", v == "ñandú" && std::env::var("MRUST_PRUEBA").is_err(), format!("MRUST_PRUEBA={v}"));
    let mut m: HashMap<String, usize> = HashMap::new();
    for (i, p) in "en un lugar de la mancha de cuyo nombre no quiero acordarme".split(' ').enumerate() {
        *m.entry(p.to_string()).or_default() += i;
    }
    let mut c: Vec<_> = m.iter().filter(|(_, &v)| v > 5).map(|(k, v)| format!("{k}={v}")).collect();
    c.sort();
    prueba("HashMap (con el azar de RandomState)", m.len() == 11, c.join(" "));
    let s = "¡Señor Muñoz, qué pingüino tan añejo!";
    let mayús: String = s.to_uppercase();
    prueba("UTF-8", s.chars().count() == 37 && s.len() == 43, format!("{mayús} ({} letras, {} octetos)", s.chars().count(), s.len()));
    prueba("coma flotante", format!("{:.6}", 2f64.sqrt() * std::f64::consts::PI) == "4.442883",
        format!("{:.6} {:e} {}", 2f64.sqrt() * std::f64::consts::PI, 1.5e-7f64, 0.1f32 + 0.2f32));
    // un pánico recogido: la pila se desenrolla (con el libunwind de Rust)
    let ganchos = std::panic::take_hook();
    std::panic::set_hook(Box::new(|_| {}));
    let r = std::panic::catch_unwind(|| {
        let v: Vec<u8> = Vec::new();
        v[3]
    });
    std::panic::set_hook(ganchos);
    prueba("un pánico recogido (catch_unwind)", r.is_err(), "la pila, desenrollada".into());
    // un proceso hijo: este mismo programa, por /proc/self/exe
    match std::env::current_exe() {
        Ok(yo) => {
            let s = std::process::Command::new(&yo).arg("hijo").env("MRUST_PADRE", "sí").output();
            match s {
                Ok(o) => prueba("un proceso hijo (Command)", o.status.code() == Some(7) && o.stdout == "hola del hijo, sí\n".as_bytes(),
                    format!("código {:?}, «{}»", o.status.code(), String::from_utf8_lossy(&o.stdout).trim_end())),
                Err(e) => prueba("un proceso hijo (Command)", false, format!("{e}")),
            }
        }
        Err(e) => prueba("current_exe", false, format!("{e}")),
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() == 2 && args[1] == "hijo" {
        println!("hola del hijo, {}", std::env::var("MRUST_PADRE").unwrap_or_default());
        std::process::exit(7);
    }
    println!("MRUST: Rust {} para x86_64-unknown-linux-musl, con la musl de Hispa-64", option_env!("MRUST_RUSTC").unwrap_or("?"));
    println!("  ¡hola desde Rust!");
    varios(&args);
    hilos();
    if let Err(e) = archivos() {
        prueba("los archivos", false, format!("{e}"));
    }
    if let Err(e) = red() {
        prueba("la red", false, format!("{e}"));
    }
    tiempo();
    let (b, m) = (BIEN.load(Ordering::SeqCst), MAL.load(Ordering::SeqCst));
    if m == 0 {
        println!("MRUST: todo bien ({b} pruebas)");
    } else {
        println!("MRUST: {m} MAL de {}", b + m);
        std::process::exit(1);
    }
}
