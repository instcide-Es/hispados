## Qué cambia

<!-- En pocas líneas: qué hace esta PR y por qué. Si cierra una issue, pon «Cierra #número». -->

## Dónde lo has probado

<!-- QEMU, VirtualBox, 86Box, DOSBox, un equipo real (cuál; por BIOS o por UEFI)... -->

## Comprobaciones

- [ ] No contiene código de Microsoft ni de desensamblados; se basa en documentación pública.
- [ ] Las pruebas pasan: `make test` (Hispa-386/486) o la parte de `castel/pruebas.sh` que toca (Castel, Hispa-Pentium), o explico abajo qué falla y por qué.
- [ ] Lo nuevo lleva su prueba (en `tests/` o en `castel/pruebas.sh`).
- [ ] No lleva datos personales (correos, contraseñas, claves de licencia) en registros ni capturas.
- [ ] Los textos que ve el usuario están en español.
