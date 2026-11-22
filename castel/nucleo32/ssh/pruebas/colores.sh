#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# colores.sh - (TERMO) lo que pinta una interfaz de hoy (como la de Claude
# Code, hecha con Ink): colores de 24 bits y de 256, cajas redondas y
# gruesas, bloques, Braille, los símbolos de siempre, letras anchas, la
# actualización sincronizada (2026), el pegado entre corchetes (2004), el
# cursor oculto, borrar líneas y subir el cursor para repintar. Lo usan las
# pruebas del terminal de SSH.BIN (vt_prueba y QEMU). Acaba con «LISTO».
e=$'\e'
printf '%s[?2004h%s[?25l' "$e" "$e"
printf '%s[?2026h' "$e"
# una franja de 24 bits (de rojo a azul) y la paleta de 256
for i in $(seq 0 63); do r=$((255 - i * 4)); b=$((i * 4)); printf '%s[48;2;%d;80;%dm ' "$e" $r $b; done; printf '%s[0m\n' "$e"
for i in $(seq 16 79); do printf '%s[48;5;%dm ' "$e" $i; done; printf '%s[0m\n' "$e"
printf '%s[38;2;215;119;87m╭──────────────────────────────────────────╮%s[0m\n' "$e" "$e"
printf '%s[38;2;215;119;87m│%s[0m %s[1m✻ Bienvenido a la prueba del terminal%s[0m     %s[38;2;215;119;87m│%s[0m\n' "$e" "$e" "$e" "$e" "$e" "$e"
printf '%s[38;2;215;119;87m│%s[0m   %s[2m/help para la ayuda · ñ á é í ó ú ü%s[0m   %s[38;2;215;119;87m│%s[0m\n' "$e" "$e" "$e" "$e" "$e" "$e"
printf '%s[38;2;215;119;87m╰──────────────────────────────────────────╯%s[0m\n' "$e" "$e"
printf '%s[38;5;114m⏺%s[0m Leyendo %s[1mtermo.c%s[0m\n' "$e" "$e" "$e" "$e"
printf '  %s[2m⎿  1.696 líneas%s[0m\n' "$e" "$e"
printf '%s[32m✓%s[0m bien  %s[31m✗%s[0m mal  %s[33m⚠%s[0m ojo  ● ○ ◆ ■ ▲ ▶ ← ↑ → ↓ ↵ … ✢ ✳ ✶ ✽\n' "$e" "$e" "$e" "$e" "$e" "$e"
printf '┏━━┳━━┓ ╔══╦══╗ ▐▛███▜▌ ▘▝▖▗ ░▒▓ ⣿⡇⠿ 日本語 🙂 x\n'
printf '%s[3mcursiva%s[0m %s[4msubrayado%s[0m %s[9mtachado%s[0m %s[7minverso%s[0m %s[38:2::30:144:255mcolor con dos puntos%s[0m\n' "$e" "$e" "$e" "$e" "$e" "$e" "$e" "$e" "$e" "$e"
# la línea del «pensando», repintada tres veces (sube y borra, como Ink)
for s in '·' '✢' '✻'; do printf '%s[2K%s[38;2;215;119;87m%s Pensando…%s[0m (esc para parar)\n%s[1A' "$e" "$e" "$s" "$e" "$e"; done
printf '\n'
printf '%s[38;2;136;136;136m╭──────────────────────────────────────────╮%s[0m\n' "$e" "$e"
printf '%s[38;2;136;136;136m│%s[0m ❯ %s[7m %s[0m                                      %s[38;2;136;136;136m│%s[0m\n' "$e" "$e" "$e" "$e" "$e" "$e"
printf '%s[38;2;136;136;136m╰──────────────────────────────────────────╯%s[0m\n' "$e" "$e"
printf '  %s[2m⏵⏵ aceptar cambios (mayús+tab para cambiar)%s[0m\n' "$e" "$e"
printf '%s[?2026l' "$e"
# las preguntas: el tamaño, la posición del cursor, el fondo y el 2026
printf '%s[18t%s[6n%s]11;?%s\\%s[?2026$p%s[c' "$e" "$e" "$e" "$e" "$e" "$e"
printf 'LISTO\n'
printf '%s[?25h' "$e"
sleep 3
