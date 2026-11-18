#!/bin/bash
# [DX] Descarga programas libres de Windows 95/98 que usan DirectX 2D, para
# probar la fase 5 de Hispa-486 (DDRAW, DSOUND, DINPUT, WINMM) con programas
# de verdad. No van en el repositorio: se guardan en tests/thirdparty/dl/.
#   - El ejemplo de DirectDraw del flat assembler (EXAMPLES/DDRAW de
#     fasmw17332.zip, licencia de fasm, de tipo BSD): lo baja ya
#     fetch_win32.sh; se ensambla dentro de Hispa-DOS con su FASM.EXE.
#   - TinyPTC para Windows 0.8 (Glenn Fiedler; SourceForge: dominio público /
#     LGPL): fullscreen.exe y windowed.exe, dos ejemplos de 6 KB comprimidos
#     con UPX que cargan DDRAW.DLL con LoadLibraryA y piden 32, 24 o 16 bits
#     (o una ventana con un recortador).
#   - FCE Ultra 0.98.8 para Win32 (GPL): un emulador de NES con DirectDraw 7,
#     DirectInput 7 y DirectSound.
# Downloads free DirectX 2D Win32 programs used only as tests.
set -eu
DIR=$(cd "$(dirname "$0")" && pwd)/dl
"$(dirname "$0")/fetch_win32.sh" > /dev/null 2>&1 || true
mkdir -p "$DIR/dx-tinyptc"
if [ ! -f "$DIR/dx-tinyptc/.ok" ]; then
  curl -sfL --max-time 180 -o "$DIR/dx-tinyptc/tinyptc.zip" \
    "https://downloads.sourceforge.net/project/tinyptc/Windows%20Version/TinyPTC%20Windows%200.8/TinyPTC-Windows-0.8.zip" \
    && (cd "$DIR/dx-tinyptc" && unzip -qo tinyptc.zip && touch .ok) && echo "  tinyptc (win32)" \
    || echo "  (no se pudo bajar TinyPTC)"
fi
mkdir -p "$DIR/dx-fceu"
if [ ! -f "$DIR/dx-fceu/.ok" ]; then
  curl -sfL --max-time 180 -o "$DIR/dx-fceu/fceu.zip" \
    "https://downloads.sourceforge.net/project/fceultra/Binaries/0.98.8%20Win32/fceu-0.98.8.win.zip" \
    && (cd "$DIR/dx-fceu" && unzip -qo fceu.zip && touch .ok) && echo "  fceu (win32)" \
    || echo "  (no se pudo bajar FCE Ultra)"
fi
