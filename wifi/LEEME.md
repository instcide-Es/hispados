# wifi/ — el Wi-Fi de Hispa-486

Lo explica [docs/WIFI.md](../docs/WIFI.md). Aquí, qué hay en cada carpeta y de
dónde sale.

| Carpeta | Qué | De quién | Licencia |
|---|---|---|---|
| `nucleo/` | La gestión 802.11, WPA2, la radio simulada, la unión con brcmsmac y las llamadas del residente | Hispa-DOS | GPL-2.0 o posterior |
| `compat/` | Lo que los fuentes de Linux esperan de su núcleo, hecho para Hispa-486 | Hispa-DOS | GPL-2.0 o posterior |
| `linux/brcmsmac/`, `linux/brcmutil/`, `linux/brcminc/` | El controlador de las Broadcom 802.11n *softMAC* | Broadcom y contribuidores de Linux | ISC (el texto, al principio de cada archivo) |
| `linux/bcma/`, `linux/include/linux/bcma/`, `linux/include/linux/ssb/`, `linux/cordic.c`, `linux/include/linux/cordic.h` | El bus interno de los chips de Broadcom, las definiciones de la SPROM y el cálculo CORDIC | contribuidores de Linux | GPL-2.0 (la del núcleo de Linux: «solo versión 2»; el texto, en `/LICENSE`) |
| `nucleo/sdio.c`, `nucleo/radio_rtl_pc.c` | (RTL8723BS) El SDIO sobre una controladora SDHCI y dónde está la de la Realtek en un equipo de verdad | Hispa-DOS | GPL-2.0 o posterior |
| `nucleo/radio_rtl.c`, `linux/rtl8723bs/` | (RTL8723BS) El controlador de la Realtek RTL8723BS (SDIO) y sus tablas (MAC_REG, PHY_REG, AGC_TAB, RadioA): lo mínimo de `drivers/staging/rtl8723bs` de **Linux 6.6** (etiqueta `v6.6`), con sus registros, valores y orden; las tablas, tal cual de `hal/HalHWImg8723B_*.c` | Realtek (Copyright(c) 2007 - 2012 Realtek Corporation) y contribuidores de Linux | GPL-2.0 («solo versión 2»: el aviso, al principio de cada archivo) |
| `firmware/` | El microcódigo de la tarjeta | Broadcom | `firmware/LICENCE.broadcom_bcm43xx`: se puede usar y repartir completo y sin modificar; no es software libre |

## Los fuentes de Linux

Son los de **Linux 5.10** (etiqueta `v5.10` de
`git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git`), **sin
modificar**:

- `drivers/net/wireless/broadcom/brcm80211/brcmsmac/` → `linux/brcmsmac/`,
  menos `Makefile`, `debug.c`, `led.c` y las trazas (`brcms_trace_*`). El PHY
  LCN de la BCM4313 (`phy/phy_lcn.c` y `phy/phytbl_lcn.c`, Copyright (c) 2010
  Broadcom Corporation, ISC: el aviso, al principio de cada uno) entró el
  24/11/2026, también de la etiqueta `v5.10` y sin modificar; antes
  estaba fuera y lo que el resto llama de él eran funciones vacías en
  `nucleo/radio_brcm.c`.
- `drivers/net/wireless/broadcom/brcm80211/brcmutil/` → `linux/brcmutil/` y
  `.../brcm80211/include/` → `linux/brcminc/`.
- De `drivers/bcma/`: `scan.c`, `scan.h`, `core.c`, `sprom.c`,
  `driver_chipcommon.c`, `driver_chipcommon_pmu.c`, `driver_pci.c`,
  `host_pci.c` y `bcma_private.h` → `linux/bcma/`. No se usa `main.c` (el
  alta de dispositivos de Linux): lo que hace para una tarjeta PCIe está en
  `nucleo/radio_brcm.c`.
- `include/linux/bcma/` entero, `include/linux/ssb/ssb_regs.h` y, de
  `include/linux/ssb/ssb.h`, solo las estructuras de la SPROM.
- `lib/math/cordic.c` e `include/linux/cordic.h`.

Para pasar a otra versión de Linux: copiar los mismos archivos encima y
compilar; lo que falte saldrá como error en `compat/hispa_linux.h` o
`compat/hispa_80211.h`.

## El microcódigo

`firmware/bcm43xx-0.fw` (96.224 octetos, SHA-256
`24668c702e723d2b1caebda047ee975ce16d7edf23788bc447dc12bcaf246b0f`) y
`firmware/bcm43xx_hdr-0.fw` (180 octetos, SHA-256
`4aaa2ba62983d64b65f351908c59062a8a7c99100d190904c0e780d7c30f0dd0`): los
`brcm/bcm43xx-0.fw` y `brcm/bcm43xx_hdr-0.fw` de linux-firmware (versión
610.812), tal cual. En el sistema instalado se llaman `BCM43XX.FW` y
`BCM43XXH.FW`, con la licencia en `BCM43XX.TXT`.
