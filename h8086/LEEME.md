# Hispa-8086

El sistema de la familia Hispa para los PC, XT y AT (8086, 8088, V20, 80186 y
286). Todo en ensamblador de 8086. El plan y las decisiones están en
[docs/8086/HISPA-8086.md](../docs/8086/HISPA-8086.md); el estudio previo, en
[docs/8086/ESTUDIO.md](../docs/8086/ESTUDIO.md).

```
make            # los disquetes de 360 K, 720 K, 1,2 M y 1,44 M, en build/
make prueba     # los arranca en QEMU y comprueba lo que sale
```

`make` hace también el disquete de juegos, `build/hispa8086-juegos.img` (se
pone en B:): de momento, **Marcianada** (`MARCIANA`), de marcianitos; ver
[docs/8086/JUEGO-MARCIANADA.md](../docs/8086/JUEGO-MARCIANADA.md).

Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
