# Seguridad

## Cómo avisar de un fallo de seguridad

**No abras una *issue* pública** para un fallo que alguien podría usar contra otros usuarios: por ejemplo, que una página web lea datos de otra, que un programa se salte los permisos, que una actualización se pueda falsificar o que se pueda escribir fuera de un búfer.

Avísanos **en privado**, con los avisos privados de GitHub:

- en este repositorio, pestaña **Security** › **Report a vulnerability**;
- o directamente en <https://github.com/instcide-Es/hispados/security/advisories/new>.

Lo que escribas ahí solo lo ven los responsables del proyecto y tú. Hace falta una cuenta de GitHub (es gratis). Si el fallo no es de seguridad (algo que no funciona, un equipo que no arranca), mejor una *issue* normal, con sus plantillas.

Cuéntanos:

1. qué sistema y qué versión (Hispa-8086 0.14, Hispa-386 0.9.30, Hispa-Pentium 0.8.12…; en Hispa-8086, 386 y 486 la dice la orden `VER`);
2. qué pasa y cómo se provoca (los pasos, o un archivo de prueba);
3. qué consecuencias crees que tiene.

## Qué haremos

- Te contestaremos en unos **7 días**.
- Te diremos si lo hemos podido reproducir y cuándo calculamos que estará arreglado.
- Cuando salga el arreglo, las novedades de la web dirán «esta versión corrige fallos de seguridad: actualiza», sin dar los detalles hasta que la mayoría haya actualizado. Después contaremos qué era y, si quieres, quién lo encontró.
- Te pedimos que no cuentes los detalles en público hasta **90 días** después del aviso, o hasta que salga el arreglo, lo que pase antes.

## Qué versiones reciben arreglos

| Sistema | Versión con arreglos de seguridad |
|---|---|
| Hispa-8086 | la última publicada |
| Hispa-386 e Hispa-486 | la última 0.9.x publicada (es una versión de pruebas) |
| Hispa-Pentium | la última 0.8.x publicada (lo instalado se pone al día con «Actualizar todo») |
| Hispa-64 | todavía en desarrollo: sin versión publicada |

## Contra qué protege cada sistema

- **Hispa-8086, Hispa-386 e Hispa-486** son de un solo usuario, como MS-DOS: un programa puede hacerlo todo, y eso no es un fallo. Lo que sí se protege es lo que llega de fuera: la red, las páginas web, las actualizaciones y los archivos.
- **Hispa-Pentium** tiene usuarios y permisos: un usuario normal no debería poder leer lo de otro ni cambiar el sistema. Sus actualizaciones van firmadas (Ed25519) y no se instala nada sin esa firma.
- **Hispa-64** está en desarrollo y todavía no tiene usuarios.

## Las descargas

Cada publicación trae un `SHA256SUMS.txt` con la huella de cada archivo. Cuando lleve además su firma (`SHA256SUMS.txt.sig`), se comprueba con nuestra clave pública, que está en <https://hispados.es/hispados-firmantes.txt>; cómo hacerlo, paso a paso, en <https://hispados.es/descargas.html#instalar-sumas>.
