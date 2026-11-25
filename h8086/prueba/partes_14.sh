# Las partes de la prueba que llegaron con la versión 0.14: los arreglos de la
# primera vuelta de la auditoría (25-11-2026). Cada comprobación dice el número
# del hallazgo («n.º 1»...). Las carga prueba.sh, que tiene las funciones.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

# ---- el núcleo y COMMAND (hallazgos 1, 2 y 3) ----
p_auditoria1() {
echo "== Hispa-8086: auditoría 1, el núcleo (copiar sobre sí mismo, ERRORLEVEL, «X\\.») =="
cp $IMG/hispa8086-360.img $T/a.img
printf '@eco off\r\nhola\r\nif errorlevel 10 eco MAL: 7 no es 10 o mas\r\nif errorlevel 17 eco MAL: 7 no es 17 o mas\r\nif not errorlevel 10 eco BIEN: 7 es menos de 10\r\nif errorlevel 07 eco BIEN: 07 es 7\r\n' > $T/NIVEL.BAT
mcopy -o -i $T/a.img $T/NIVEL.BAT ::
arranca $T/a.img 486
# n.º 1: crear (3Ch) un archivo abierto lo vaciaba mientras se leía
escribe "limpiar"; escribe "copiar basic.com ."; sleep 2; escribe "copiar \\hola.com hola.com"; sleep 1
pantalla u0
renglon u0 "No se puede copiar un archivo sobre sí mismo." "n.º 1: COPIAR sobre sí mismo (con textos distintos) lo dice: " 2
escribe "limpiar"; escribe "creardir g"; escribe "copiar command.com g"; sleep 3; escribe "copiar nucleo86.sys g"; sleep 3
escribe "cd g"; escribe "xcopiar *.*"; sleep 4; escribe "cd \\"
pantalla u1
renglon u1 "  no se puede copiar sobre sí mismo." "n.º 1: y XCOPIAR también: " 2
esta u1 "0 archivos copiados." "n.º 1: XCOPIAR no copia nada: "
escribe "limpiar"; escribe "dir basic.com"
pantalla u1b
esta u1b "$(printf '%-14s%10d' BASIC.COM $(stat -c %s build/BASIC.COM))" "n.º 1: BASIC.COM conserva su tamaño: "
# n.º 2: IF ERRORLEVEL de dos cifras
escribe "limpiar"; escribe "nivel"; sleep 3
pantalla u2
noesta u2 "MAL:" "n.º 2: ERRORLEVEL 10 y 17 no se cumplen con un 7: "
esta u2 "BIEN: 7 es menos de 10" "n.º 2: NOT ERRORLEVEL 10 sí: "
esta u2 "BIEN: 07 es 7" "n.º 2: y un cero delante no estorba: "
# n.º 3: «X\.» no es un nombre (RD X\. soltaba el grupo de X)
escribe "limpiar"; escribe "creardir x"; escribe "borrardir x\\."; escribe "renombrar x\\. y"; escribe "creardir x\\.."; escribe "dir x"
pantalla u3
esta u3 ".                  <DIR>" "n.º 3: tras «BORRARDIR X\\.» X sigue entera: "
noesta u3 "[Y]" ""
apaga
if cmp -s <(mtype -i $T/a.img ::BASIC.COM) build/BASIC.COM && cmp -s <(mtype -i $T/a.img ::HOLA.COM) build/HOLA.COM \
   && cmp -s <(mtype -i $T/a.img ::G/COMMAND.COM) build/COMMAND.COM && cmp -s <(mtype -i $T/a.img ::G/NUCLEO86.SYS) build/NUCLEO86.SYS; then
  ok "n.º 1: desde fuera, los cuatro archivos están intactos, octeto a octeto"; else ko "n.º 1: algún archivo ha quedado cortado"; mdir -i $T/a.img :: ::G; fi
if fsck.fat -n $T/a.img > $T/fsck.txt 2>&1 && mdir -i $T/a.img ::X > /dev/null 2>&1; then ok "n.º 1 y 3: fsck.fat no encuentra errores y X sigue ahí"; else ko "n.º 1 y 3: fsck.fat encuentra errores"; cat $T/fsck.txt; fi
}
