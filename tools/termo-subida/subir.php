<?php
// Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
//
// subir.php - (TERMO) el receptor de «Enviar a Carlos» del Termómetro de
// Hispa-Pentium (castel/nucleo32/termo/termo.c, TERMO /ENVIAR).
//
// Un POST multipart/form-data por archivo, con:
//   testigo  la cadena de la memoria (64 cifras hexadecimales: 32 octetos al
//            azar). Aquí solo se guarda su SHA-256, en TESTIGOS (una línea
//            «sha256 EQUIPO comentario» por memoria; se revoca borrando su línea).
//   equipo   el nombre que dice la memoria (si la línea del testigo trae
//            EQUIPO, manda la del testigo: la memoria no elige la carpeta)
//   lote     «AAAAMMDD-HHMMSS-XXXX»: lo que se manda de una vez (la fecha y
//            la hora de la memoria y 4 cifras al azar); repetir un archivo
//            del mismo lote lo sustituye (los reintentos no duplican)
//   archivo  el archivo: solo .CSV, .LOG, .TXT o .BMP, con nombre 8+3
// Contesta en texto: «OK nombre octetos» o «ERROR motivo» (y el código HTTP).
// No ejecuta nada de lo que llega: lo guarda como datos, con permisos 0600,
// en ENTRANTES/<EQUIPO>/<lote>/ (fuera de la web); un cron de root
// (termo-mover.sh) lo pasa a /root/FTP/HispaDOS/Pruebas/Pendientes/<EQUIPO>/
// Termometro/subidas/<lote>/. Cada intento queda en REGISTRO (hora, IP,
// equipo, archivo, resultado). Topes: 4 MB por archivo, 64 MB y 400 archivos
// por testigo y día.
// Para las pruebas (php -S en 127.0.0.1), las rutas se pueden cambiar con
// TERMO_TESTIGOS y TERMO_DATOS en el entorno; en el servidor no se ponen.

declare(strict_types=1);

$datos = getenv('TERMO_DATOS') ?: '/www/hispados-subidas';
define('TESTIGOS', getenv('TERMO_TESTIGOS') ?: '/etc/hispados-termo/testigos.txt');
define('ENTRANTES', $datos . '/entrantes');
define('CUOTAS', $datos . '/cuotas');
define('REGISTRO', $datos . '/registro/subidas.log');
const MAX_ARCHIVO = 4 * 1024 * 1024;
const MAX_DIA     = 64 * 1024 * 1024;
const MAX_DIA_N   = 400;

header('Content-Type: text/plain; charset=utf-8');
header('X-Content-Type-Options: nosniff');
header('Cache-Control: no-store');

function apunta(string $equipo, string $nombre, string $que): void
{
    $ip = $_SERVER['REMOTE_ADDR'] ?? '?';
    $l = sprintf("%s %s %s %s %s\n", gmdate('Y-m-d\TH:i:s\Z'), $ip,
        $equipo !== '' ? $equipo : '-', $nombre !== '' ? $nombre : '-', preg_replace('/[^\x20-\x7E]/', '?', $que));
    @file_put_contents(REGISTRO, $l, FILE_APPEND | LOCK_EX);
}

function acaba(int $http, string $texto, string $equipo = '', string $nombre = ''): never
{
    apunta($equipo, $nombre, ($http === 200 ? 'OK ' : 'ERROR ') . $texto);
    http_response_code($http);
    echo ($http === 200 ? 'OK ' : 'ERROR ') . $texto . "\n";
    exit;
}

// un nombre de carpeta: letras, cifras, «-» y «_», como mucho 40
function limpio(string $t): string
{
    $t = preg_replace('/[^A-Za-z0-9_-]+/', '-', $t);
    $t = trim((string)$t, '-');
    return substr($t, 0, 40);
}

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST')
    acaba(405, 'solo POST');

$testigo = (string)($_POST['testigo'] ?? '');
if (!preg_match('/^[0-9a-fA-F]{64}$/', $testigo))
    acaba(403, 'testigo que no vale');
$resumen = hash('sha256', strtolower($testigo));

// el testigo, entre los que valen (comparando sin dar pistas por el tiempo)
$equipo_testigo = null;
$lineas = @file(TESTIGOS, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
if ($lineas === false)
    acaba(500, 'el servidor no tiene la lista de testigos');
foreach ($lineas as $l) {
    $l = trim($l);
    if ($l === '' || $l[0] === '#')
        continue;
    $c = preg_split('/\s+/', $l, 3);
    if (strlen($c[0]) === 64 && hash_equals(strtolower($c[0]), $resumen)) {
        $equipo_testigo = isset($c[1]) ? limpio($c[1]) : '';
        break;
    }
}
if ($equipo_testigo === null)
    acaba(403, 'testigo desconocido o revocado');

$equipo = $equipo_testigo !== '' ? $equipo_testigo : limpio((string)($_POST['equipo'] ?? ''));
if ($equipo === '')
    $equipo = 'Sin-nombre';

$lote = (string)($_POST['lote'] ?? '');
if (!preg_match('/^20[0-9]{6}-[0-9]{6}-[0-9A-F]{4}$/', $lote))
    acaba(400, 'lote que no vale', $equipo);

$f = $_FILES['archivo'] ?? null;
if (!is_array($f) || !isset($f['error']) || is_array($f['error']))
    acaba(400, 'falta el archivo', $equipo);
if ($f['error'] === UPLOAD_ERR_INI_SIZE || $f['error'] === UPLOAD_ERR_FORM_SIZE)
    acaba(413, 'archivo demasiado grande (4 MB como mucho)', $equipo);
if ($f['error'] !== UPLOAD_ERR_OK || !is_uploaded_file($f['tmp_name']))
    acaba(400, 'el archivo no ha llegado entero', $equipo);

// el nombre: 8+3, solo .CSV .LOG .TXT .BMP (en mayúsculas)
$nombre = strtoupper(basename(str_replace('\\', '/', (string)$f['name'])));
if (!preg_match('/^[A-Z0-9_-]{1,8}\.(CSV|LOG|TXT|BMP)$/', $nombre))
    acaba(415, 'nombre o tipo que no vale (8+3; .CSV, .LOG, .TXT o .BMP)', $equipo, $nombre);
$tam = (int)$f['size'];
if ($tam <= 0 || $tam > MAX_ARCHIVO || filesize($f['tmp_name']) !== $tam)
    acaba(413, 'archivo vacío o de más de 4 MB', $equipo, $nombre);
// lo de dentro tiene que parecerse a lo que dice: texto o un BMP
$cabeza = (string)file_get_contents($f['tmp_name'], false, null, 0, 4096);
if (str_ends_with($nombre, '.BMP')) {
    if (substr($cabeza, 0, 2) !== 'BM')
        acaba(415, 'no es un BMP', $equipo, $nombre);
} elseif (str_contains($cabeza, "\0") || str_starts_with($cabeza, '<?') || stripos($cabeza, '<script') !== false) {
    acaba(415, 'no es un texto del Termómetro', $equipo, $nombre);
}

// los topes del día, por testigo
$dia = gmdate('Ymd');
@mkdir(CUOTAS, 0700, true);
$cuota = CUOTAS . '/' . substr($resumen, 0, 16) . '-' . $dia;
$h = fopen($cuota, 'c+');
if ($h === false || !flock($h, LOCK_EX))
    acaba(500, 'no se puede llevar la cuenta del día', $equipo, $nombre);
$v = explode(' ', trim((string)stream_get_contents($h)) ?: '0 0');
$usado = (int)($v[0] ?? 0);
$n = (int)($v[1] ?? 0);
if ($usado + $tam > MAX_DIA || $n + 1 > MAX_DIA_N) {
    flock($h, LOCK_UN);
    fclose($h);
    acaba(429, 'por hoy ya se ha mandado bastante (64 MB o 400 archivos); mañana más', $equipo, $nombre);
}

$dir = ENTRANTES . '/' . $equipo . '/' . $lote;
if (!is_dir($dir) && !@mkdir($dir, 0700, true)) {
    flock($h, LOCK_UN);
    fclose($h);
    acaba(500, 'no se puede guardar', $equipo, $nombre);
}
$tmp = $dir . '/.' . $nombre . '.' . bin2hex(random_bytes(4));
if (!move_uploaded_file($f['tmp_name'], $tmp) || !chmod($tmp, 0600) || !rename($tmp, $dir . '/' . $nombre)) {
    @unlink($tmp);
    flock($h, LOCK_UN);
    fclose($h);
    acaba(500, 'no se puede guardar', $equipo, $nombre);
}
ftruncate($h, 0);
rewind($h);
fwrite($h, ($usado + $tam) . ' ' . ($n + 1) . "\n");
flock($h, LOCK_UN);
fclose($h);
acaba(200, $nombre . ' ' . $tam, $equipo, $nombre);
