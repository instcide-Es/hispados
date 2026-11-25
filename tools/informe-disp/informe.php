<?php
// Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
//
// informe.php - (INFORME-DISP) el receptor de «Enviar informe de los
// dispositivos que fallan» del Administrador de dispositivos de Hispa-Pentium
// (castel/nucleo32/udisposi.cas, DISPOSIT /ENVIAR y el botón de la ventana).
//
// SIN testigo: lo manda cualquier equipo instalado, sin decir quién es. Por
// eso, todo con topes:
//   - solo POST, con «Content-Type: text/plain; charset=utf-8» y el informe
//     tal cual en el cuerpo (nada de multipart: no hay nombres de archivo);
//   - 256 KB como mucho (y nginx corta antes, en 300 KB);
//   - solo texto UTF-8 bien hecho: sin el 0 ni otros caracteres de control
//     (salvo \t, \r y \n) y sin «<» (ni HTML ni PHP; DISPOSIT los cambia por
//     «(» antes de mandar), y que empiece por la primera línea del informe;
//   - 5 informes por IP y día (con IPv6, por su /64) y 200 en total al día;
//   - y si lo que espera a que el cron lo pase al FTP pasa de 2000 informes o
//     de 100 MB, no se acepta más (por si el cron se para: el disco no se llena).
// Contesta en texto: «OK número» (el número del informe: AAAAMMDD-HHMMSS-XXXX)
// o «ERROR motivo» (y el código HTTP: 405, 411, 413, 415, 429, 503, 500).
// No ejecuta nada de lo que llega: lo guarda como datos, con permisos 0600, en
// INFORMES/<número>_<fabricante-modelo>.TXT (fuera de la web; el nombre lo
// hace este guion, nunca lo dice el que manda); un cron de root
// (informe-mover.sh) lo pasa a /root/FTP/HispaDOS/Pruebas/Pendientes/
// Informes-dispositivos/<AAAAMMDD-HHMMSS>-<fabricante-modelo>/INFORME.TXT.
// Cada intento queda en REGISTRO (hora, IP, resultado), sin el informe (hasta
// 4 MB: el cron lo rota).
// Para las pruebas (php -S en 127.0.0.1), la carpeta y los topes se pueden
// cambiar con INFORME_DATOS, INFORME_POR_IP, INFORME_POR_DIA e INFORME_MAX en el
// entorno;
// en el servidor no se ponen.

declare(strict_types=1);
umask(0077);

$datos = getenv('INFORME_DATOS') ?: '/www/hispados-subidas';
define('INFORMES', $datos . '/informes');
define('CUOTAS', $datos . '/informes-cuotas');
define('REGISTRO', $datos . '/registro/informes.log');
define('POR_IP', (int)(getenv('INFORME_POR_IP') ?: 5));
define('POR_DIA', (int)(getenv('INFORME_POR_DIA') ?: 200));
// (con INFORME_MAX, las pruebas lo bajan; nunca de más de 256 KB)
define('MAX_INFORME', min(256 * 1024, (int)(getenv('INFORME_MAX') ?: 256 * 1024)));
const MAX_ESPERANDO_N = 2000;
const MAX_ESPERANDO_OCTETOS = 100 * 1024 * 1024;
const PRIMERA = 'Hispa-Pentium: informe de los dispositivos que fallan';

header('Content-Type: text/plain; charset=utf-8');
header('X-Content-Type-Options: nosniff');
header('Cache-Control: no-store');

function apunta(string $que): void
{
    $ip = $_SERVER['REMOTE_ADDR'] ?? '?';
    $l = sprintf("%s %s %s\n", gmdate('Y-m-d\TH:i:s\Z'), preg_replace('/[^0-9A-Fa-f:.]/', '?', $ip),
        preg_replace('/[\x00-\x1F\x7F]/', '?', $que));
    // (el registro no crece sin fin con peticiones malas: a partir de 4 MB,
    // no se apunta más hasta que el cron lo rote)
    if (@filesize(REGISTRO) > 4 * 1024 * 1024)
        return;
    @file_put_contents(REGISTRO, $l, FILE_APPEND | LOCK_EX);
}

function acaba(int $http, string $texto): never
{
    apunta(($http === 200 ? 'OK ' : 'ERROR ' . $http . ' ') . $texto);
    http_response_code($http);
    echo ($http === 200 ? 'OK ' : 'ERROR ') . $texto . "\n";
    exit;
}

// un trozo de nombre de carpeta: letras, cifras, «-» y «_», como mucho 40
function limpio(string $t): string
{
    $t = preg_replace('/[^A-Za-z0-9_-]+/', '-', $t);
    $t = trim((string)$t, '-');
    return substr($t, 0, 40);
}

// la cuenta de la IP: IPv4 entera; IPv6, su /64 (cambiar de dirección dentro
// de la misma red no da más informes)
function clave_ip(string $ip): string
{
    $b = @inet_pton($ip);
    if ($b === false)
        return 'x';
    if (strlen($b) === 16)
        $b = substr($b, 0, 8);
    return bin2hex($b);
}

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST')
    acaba(405, 'solo POST');
$tipo = strtolower((string)($_SERVER['CONTENT_TYPE'] ?? ''));
if (!preg_match('#^text/plain\s*(;\s*charset=utf-8\s*)?$#', $tipo))
    acaba(415, 'tiene que ser texto (text/plain; charset=utf-8)');
$largo = $_SERVER['CONTENT_LENGTH'] ?? '';
if ($largo === '' || !ctype_digit((string)$largo))
    acaba(411, 'falta el largo (Content-Length)');
if ((int)$largo > MAX_INFORME)
    acaba(413, 'el informe es demasiado grande (' . intdiv(MAX_INFORME, 1024) . ' KB como mucho)');

// el cuerpo, sin pasar de 256 KB aunque el largo mienta
$h = fopen('php://input', 'rb');
$texto = $h === false ? false : stream_get_contents($h, MAX_INFORME + 1);
if ($texto === false)
    acaba(400, 'el informe no ha llegado');
if (strlen($texto) > MAX_INFORME)
    acaba(413, 'el informe es demasiado grande (' . intdiv(MAX_INFORME, 1024) . ' KB como mucho)');
if (strlen($texto) < 64)
    acaba(400, 'el informe está vacío o cortado');
if (!mb_check_encoding($texto, 'UTF-8'))
    acaba(415, 'no es texto UTF-8');
if (preg_match('/[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/', $texto))
    acaba(415, 'lleva caracteres que no son de texto');
if (str_contains($texto, '<'))
    acaba(415, 'lleva «<» (ni HTML ni PHP)');
if (!str_starts_with($texto, PRIMERA))
    acaba(415, 'no es un informe de los dispositivos de Hispa-Pentium');

// el fabricante y el modelo (para el nombre de la carpeta), del propio informe
$fab = $mod = '';
if (preg_match('/^  Fabricante: (.{1,64})$/mu', $texto, $m))
    $fab = trim($m[1]);
if (preg_match('/^  Modelo: (.{1,64})$/mu', $texto, $m))
    $mod = trim($m[1]);
$equipo = limpio(iconv('UTF-8', 'ASCII//TRANSLIT//IGNORE', $fab . '-' . $mod) ?: '');
if ($equipo === '')
    $equipo = 'Sin-nombre';

// lo que espera al cron: si es mucho, no se acepta más
@mkdir(INFORMES, 0700, true);
@mkdir(CUOTAS, 0700, true);
$n = 0;
$octetos = 0;
foreach (new DirectoryIterator(INFORMES) as $f) {
    if ($f->isFile()) {
        $n++;
        $octetos += $f->getSize();
    }
}
if ($n >= MAX_ESPERANDO_N || $octetos + strlen($texto) > MAX_ESPERANDO_OCTETOS)
    acaba(503, 'el servidor no admite más informes ahora; pruebe mañana');

// los topes del día: el total y el de la IP (con un cerrojo para los dos)
$dia = gmdate('Ymd');
$ht = fopen(CUOTAS . '/total-' . $dia, 'c+');
if ($ht === false || !flock($ht, LOCK_EX))
    acaba(500, 'no se puede llevar la cuenta del día');
$total = (int)trim((string)stream_get_contents($ht));
if ($total >= POR_DIA) {
    flock($ht, LOCK_UN);
    acaba(429, 'hoy ya han llegado muchos informes; pruebe mañana');
}
$fi = CUOTAS . '/ip-' . substr(hash('sha256', clave_ip((string)($_SERVER['REMOTE_ADDR'] ?? ''))), 0, 16) . '-' . $dia;
$de_ip = is_file($fi) ? (int)trim((string)file_get_contents($fi)) : 0;
if ($de_ip >= POR_IP) {
    flock($ht, LOCK_UN);
    acaba(429, 'desde esta conexión ya se han mandado ' . POR_IP . ' informes hoy; pruebe mañana');
}

// el número del informe y el archivo (el nombre, nuestro: nada de lo que llega)
$numero = gmdate('Ymd-His') . '-' . strtoupper(bin2hex(random_bytes(2)));
$destino = INFORMES . '/' . $numero . '_' . $equipo . '.TXT';
$tmp = INFORMES . '/.' . $numero . '.' . bin2hex(random_bytes(4));
$hf = @fopen($tmp, 'xb');
if ($hf === false) {
    flock($ht, LOCK_UN);
    acaba(500, 'no se puede guardar');
}
$bien = fwrite($hf, $texto) === strlen($texto);
$bien = fclose($hf) && $bien;
if (!$bien || !chmod($tmp, 0600) || !rename($tmp, $destino)) {
    @unlink($tmp);
    flock($ht, LOCK_UN);
    acaba(500, 'no se puede guardar');
}
@file_put_contents($fi, (string)($de_ip + 1), LOCK_EX);
@chmod($fi, 0600);
ftruncate($ht, 0);
rewind($ht);
fwrite($ht, (string)($total + 1));
flock($ht, LOCK_UN);
fclose($ht);
acaba(200, $numero);
