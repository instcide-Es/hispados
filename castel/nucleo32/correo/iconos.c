/* iconos.c - Correo: los dibujitos de la lista y de la barra (12 x 12): la
 * estrella de los destacados, el clip de los adjuntos, el aviso, el sobre de
 * los sin leer, las flechas del orden (▲ y ▼: la letra del sistema no las
 * tiene en la 437 de Hispa a la vista), el candado de las conexiones
 * cifradas. Así nada se dice solo con el color (daltonismo). Y el icono del
 * programa (32 x 32), dibujado aquí: un sobre con un sello verde.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ventana.h"

static const char *const dibujos[][12] = {
	{ "     ##     ", "     ##     ", "    ####    ", "    ####    ", "############", " ########## ",
	  "  ########  ", "  ########  ", " ####  #### ", " ###    ### ", "##        ##", "            " },
	{ "     ##     ", "    #  #    ", "    #  #    ", "#####  #####", "#          #", " #        # ",
	  "  #      #  ", "  #      #  ", " #   ##   # ", " #  #  #  # ", "####    ####", "            " },
	{ "    ####    ", "   #    #   ", "   #  # #   ", "   #  # #   ", "   #  # #   ", "   #  # #   ",
	  "   #  # #   ", "   #  # #   ", "   #  # #   ", "   #    #   ", "    #  #    ", "     ##     " },
	{ "     ##     ", "    ####    ", "    #  #    ", "   ## # #   ", "   #  #  #  ", "  ##  #  ## ",
	  "  #   #   # ", " ##       ##", " #    #    #", "##    #    #", "############", "            " },
	{ "            ", "############", "##        ##", "# #      # #", "#  #    #  #", "#   #  #   #",
	  "#    ##    #", "#          #", "#          #", "#          #", "############", "            " },
	{ "            ", "            ", "     ##     ", "    ####    ", "   ######   ", "  ########  ",
	  " ########## ", "############", "            ", "            ", "            ", "            " },
	{ "            ", "            ", "            ", "            ", "############", " ########## ",
	  "  ########  ", "   ######   ", "    ####    ", "     ##     ", "            ", "            " },
	{ "    #       ", "   ##       ", "  #######   ", " ########## ", "  ######  # ", "   ##    # #",
	  "    #     ##", "           #", "           #", "          # ", "        ##  ", "            " },
	{ "    ####    ", "   #    #   ", "  #      #  ", "  #      #  ", " ########## ", " ########## ",
	  " ####  #### ", " ####  #### ", " ##### #### ", " ########## ", " ########## ", "            " },
	{ "    ####    ", "   #    #   ", "  #      #  ", "         #  ", " ########## ", " ########## ",
	  " ####  #### ", " ####  #### ", " ##### #### ", " ########## ", " ########## ", "            " },
	{ "            ", " #          ", " #          ", " #          ", " #          ", " #       #  ",
	  " ######### #", "         #  ", "            ", "            ", "            ", "            " },
};

void icono(int cual, int x, int y, u32 tinta, u32 fondo)
{
	static u32 p[144];
	int i, j;

	if (cual < 0 || cual >= (int)(sizeof dibujos / sizeof dibujos[0]))
		return;
	/* (un mapa de puntos: una sola llamada al núcleo) */
	for (j = 0; j < 12; j++)
		for (i = 0; i < 12; i++)
			p[j * 12 + i] = dibujos[cual][j][i] == '#' ? tinta : fondo;
	ui_mapa(x, y, 12, 12, p);
}

void icono_correo(int x, int y)
{
	/* un sobre blanco con borde azul y un sello verde */
	ui_rect(x + 2, y + 7, 28, 20, C_AZUL);
	ui_rect(x + 3, y + 8, 26, 18, C_PAPEL);
	ui_linea(x + 3, y + 8, x + 16, y + 18, C_AZUL);
	ui_linea(x + 28, y + 8, x + 16, y + 18, C_AZUL);
	ui_rect(x + 22, y + 10, 5, 5, C_VERDE);
}
