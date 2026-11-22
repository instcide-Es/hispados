<?php
// Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
//
// informe.php - (COMPAT) el receptor de «Enviar informe a Hispa-DOS» del
// Asistente de compatibilidad de Hispa-Pentium (castel/nucleo32/compat/
// enviar.c). El mismo diseño que el de «Enviar a Carlos» del Termómetro
// (tools/termo-subida/subir.php): un POST multipart, con testigo, que
// contesta «OK ...» o «ERROR motivo»; guarda los datos fuera de la web, con
// 0600, y un cron de root los pasa al FTP. Lo distinto: aquí los equipos no
// se dan de alta a mano (es para cualquiera que use el Asistente), así que
// el testigo se aprende la primera vez que llega («primer uso»), con topes.
//
// El POST:
//   testigo    64 cifras hexadecimales (32 octetos al azar del equipo). Aquí
//              solo se guarda su SHA-256. Se bloquea uno poniendo su SHA-256
//              en BLOQUEADOS (una línea cada uno).
//   protocolo  1
//   informe    el informe (INFORME.TXT): texto UTF-8, 240 KB como mucho, con
//              la forma exacta que hace enviar.c (se mira renglón a renglón;
//              lo que no casa, fuera)
// Topes: por testigo, 20 informes y 2 MB al día; testigos nuevos, 5 al día por
// IP y 300 al día en total; entre todos, 3000 informes y 100 MB al día.
// De la IP no se guarda nada (solo un resumen con sal del día, para el tope
// de testigos nuevos, que se borra con los topes del día).
// Lo guarda en ENTRANTES/<AAAAMMDD>/<HHMMSS>-<testigo8>-<azar>.txt; el cron
// (compat-mover.sh) lo pasa a /root/FTP/HispaDOS/Compatibilidad/<AAAAMMDD>/ y
// rehace el resumen (compat-resumen.py).
// Para las pruebas (php -S en 127.0.0.1): COMPAT_DATOS y COMPAT_BLOQUEADOS
// en el entorno; en el servidor no se ponen.

declare(strict_types=1);

$datos = getenv('COMPAT_DATOS') ?: '/www/hispados-compat-datos';
define('BLOQUEADOS', getenv('COMPAT_BLOQUEADOS') ?: '/etc/hispados-compat/bloqueados.txt');
define('ENTRANTES', $datos . '/entrantes');
define('TESTIGOS', $datos . '/testigos');
define('CUOTAS', $datos . '/cuotas');
define('REGISTRO', $datos . '/registro/informes.log');
const MAX_INFORME = 240 * 1024;
const MAX_LINEAS = 20000;
const POR_TESTIGO_N = 20;
const POR_TESTIGO_MB = 2 * 1024 * 1024;
const NUEVOS_POR_IP = 5;
const NUEVOS_DIA = 300;
const TODOS_N = 3000;
const TODOS_MB = 100 * 1024 * 1024;

header('Content-Type: text/plain; charset=utf-8');
header('X-Content-Type-Options: nosniff');
header('Cache-Control: no-store');

function apunta(string $t8, string $que): void
{
    $l = sprintf("%s %s %s\n", gmdate('Y-m-d\TH:i:s\Z'), $t8 !== '' ? $t8 : '-', preg_replace('/[\x00-\x1F\x7F]/', '?', $que));
    @file_put_contents(REGISTRO, $l, FILE_APPEND | LOCK_EX);
}

function acaba(int $http, string $texto, string $t8 = ''): never
{
    apunta($t8, ($http === 200 ? 'OK ' : 'ERROR ') . $texto);
    http_response_code($http);
    echo ($http === 200 ? 'OK ' : 'ERROR ') . $texto . "\n";
    exit;
}

// suma $n y $tam a la cuenta $archivo («n tam»), si no se pasa de los topes
function cuenta(string $archivo, int $tam, int $max_n, int $max_tam): ?bool
{
    @mkdir(dirname($archivo), 0700, true);
    $h = @fopen($archivo, 'c+');
    if ($h === false || !flock($h, LOCK_EX))
        return null;
    $v = explode(' ', trim((string)stream_get_contents($h)) ?: '0 0');
    $n = (int)($v[0] ?? 0);
    $usado = (int)($v[1] ?? 0);
    if ($n + 1 > $max_n || $usado + $tam > $max_tam) {
        flock($h, LOCK_UN);
        fclose($h);
        return false;
    }
    ftruncate($h, 0);
    rewind($h);
    fwrite($h, ($n + 1) . ' ' . ($usado + $tam) . "\n");
    flock($h, LOCK_UN);
    fclose($h);
    return true;
}

// la forma de un informe (la de enviar.c): true si casa entero
function forma_buena(string $t): bool
{
    if (!mb_check_encoding($t, 'UTF-8') || str_contains($t, "\0") || str_contains($t, "\r"))
        return false;
    $l = explode("\n", rtrim($t, "\n"));
    if (count($l) < 6 || count($l) > MAX_LINEAS || $l[0] !== 'HISPA-COMPAT 1' || end($l) !== 'fin')
        return false;
    $N = '[A-Za-z0-9._-]{1,64}';
    $V = '[0-9A-Za-z._-]{1,20}';
    if (!preg_match("/^hispa-pentium=$V\$/", $l[1]) || !preg_match("/^win32=$V\$/", $l[2]) || !preg_match("/^asistente=$V\$/", $l[3])
        || !preg_match('/^archivos=([0-9]{1,5})$/', $l[4], $m))
        return false;
    $n_arch = (int)$m[1];
    $vistos = 0;
    for ($i = 5; $i < count($l) - 1; $i++) {
        $x = $l[$i];
        if (preg_match("/^archivo=$N\\t[0-9]{1,10}\\t([0-9a-f]{64}|-)\\t(ventanas|consola|programa|dll|no-pe)\\t[0-9]{1,6}\\t(si|instalador|servidor|glide|actualizador|fallos|steam|editor)\$/", $x)) {
            if (++$vistos > $n_arch)
                return false;
        } elseif (!preg_match("/^falta=$N\\t$N\\t[A-Za-z0-9_@#?\$]{1,80}\$/", $x) && !preg_match("/^faltadll=$N\\t$N\\t[0-9]{1,6}\$/", $x)) {
            return false;
        }
    }
    return $vistos === $n_arch;
}

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST')
    acaba(405, 'solo POST');
if ((string)($_POST['protocolo'] ?? '') !== '1')
    acaba(400, 'protocolo que no se conoce');
$testigo = strtolower((string)($_POST['testigo'] ?? ''));
if (!preg_match('/^[0-9a-f]{64}$/', $testigo))
    acaba(403, 'testigo que no vale');
$resumen = hash('sha256', $testigo);
$t8 = substr($resumen, 0, 8);

// ¿bloqueado?
$bl = @file(BLOQUEADOS, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
if (is_array($bl))
    foreach ($bl as $b) {
        $b = strtolower(trim(explode(' ', trim($b))[0] ?? ''));
        if (strlen($b) === 64 && hash_equals($b, $resumen))
            acaba(403, 'este equipo no puede mandar informes', $t8);
    }

$f = $_FILES['informe'] ?? null;
if (!is_array($f) || !isset($f['error']) || is_array($f['error']))
    acaba(400, 'falta el informe', $t8);
if ($f['error'] === UPLOAD_ERR_INI_SIZE || $f['error'] === UPLOAD_ERR_FORM_SIZE)
    acaba(413, 'el informe es demasiado grande', $t8);
if ($f['error'] !== UPLOAD_ERR_OK || !is_uploaded_file($f['tmp_name']))
    acaba(400, 'el informe no ha llegado entero', $t8);
$tam = (int)$f['size'];
if ($tam <= 0 || $tam > MAX_INFORME || filesize($f['tmp_name']) !== $tam)
    acaba(413, 'el informe está vacío o pasa de 240 KB', $t8);
$texto = (string)file_get_contents($f['tmp_name']);
if (!forma_buena($texto))
    acaba(415, 'el informe no tiene la forma de los del Asistente', $t8);

$dia = gmdate('Ymd');
// el testigo: conocido, o nuevo (con sus topes)
$rt = TESTIGOS . '/' . substr($resumen, 0, 2) . '/' . $resumen;
if (!is_file($rt)) {
    $sal = @file_get_contents(CUOTAS . '/sal-' . $dia);
    if ($sal === false || strlen($sal) < 32) {
        @mkdir(CUOTAS, 0700, true);
        $sal = bin2hex(random_bytes(16));
        $h = @fopen(CUOTAS . '/sal-' . $dia, 'x');
        if ($h !== false) {
            fwrite($h, $sal);
            fclose($h);
        } else
            $sal = (string)@file_get_contents(CUOTAS . '/sal-' . $dia);
    }
    $ip = hash('sha256', $sal . ($_SERVER['REMOTE_ADDR'] ?? '?'));
    $r1 = cuenta(CUOTAS . '/nuevos-ip-' . substr($ip, 0, 20) . '-' . $dia, 0, NUEVOS_POR_IP, PHP_INT_MAX);
    if ($r1 === null)
        acaba(500, 'no se puede llevar la cuenta del día', $t8);
    if ($r1 === false)
        acaba(429, 'hoy ya han llegado muchos equipos nuevos desde aquí; mañana más', $t8);
    $r2 = cuenta(CUOTAS . '/nuevos-' . $dia, 0, NUEVOS_DIA, PHP_INT_MAX);
    if ($r2 !== true)
        acaba($r2 === null ? 500 : 429, 'hoy ya han llegado muchos equipos nuevos; mañana más', $t8);
    @mkdir(dirname($rt), 0700, true);
    @file_put_contents($rt, gmdate('Y-m-d') . "\n", LOCK_EX);
    apunta($t8, 'equipo nuevo');
}
$r = cuenta(CUOTAS . '/t-' . substr($resumen, 0, 16) . '-' . $dia, $tam, POR_TESTIGO_N, POR_TESTIGO_MB);
if ($r !== true)
    acaba($r === null ? 500 : 429, 'por hoy ya se han mandado bastantes informes desde este equipo; mañana más', $t8);
$r = cuenta(CUOTAS . '/todos-' . $dia, $tam, TODOS_N, TODOS_MB);
if ($r !== true)
    acaba($r === null ? 500 : 429, 'hoy ya han llegado muchos informes; mañana más', $t8);

$dir = ENTRANTES . '/' . $dia;
if (!is_dir($dir) && !@mkdir($dir, 0700, true))
    acaba(500, 'no se puede guardar', $t8);
$nombre = gmdate('His') . '-' . $t8 . '-' . bin2hex(random_bytes(3)) . '.txt';
$tmp = $dir . '/.' . $nombre;
if (!move_uploaded_file($f['tmp_name'], $tmp) || !chmod($tmp, 0600) || !rename($tmp, $dir . '/' . $nombre)) {
    @unlink($tmp);
    acaba(500, 'no se puede guardar', $t8);
}
acaba(200, $dia . '/' . $nombre, $t8);
