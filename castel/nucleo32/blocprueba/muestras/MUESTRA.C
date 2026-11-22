#include <stdio.h> /* la de siempre */
#ifdef DEBUG
int x = 0x1F;
#endif
/* un comentario
   de dos líneas */ static int f(char *s) {
    if (s[0] == '\'') { return 1; }
    printf("%d \"%s\"\n", 3, s); // fin
    return 2.5e+3;
}
