/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/editline/readline.h - Hispa-64, etapa 10 g (docs/64/HISPA-64.md,
 * sección 24): lo poco de libedit que pide «js» de Ladybird
 * (Utilities/js.cpp) para su indicador: leer una línea (sin editarla: la
 * consola de Hispa-64 ya deja borrar), sin historia ni completar. Hispa-64
 * no lleva libedit.
 */
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* rl_line_buffer = 0;
static int rl_attempted_completion_over = 0;
static char** (*rl_attempted_completion_function)(char const*, int, int) = 0;

static inline char* readline(char const* indicador)
{
    size_t n = 0;
    char* linea = 0;
    if (indicador) {
        fputs(indicador, stdout);
        fflush(stdout);
    }
    ssize_t l = getline(&linea, &n, stdin);
    if (l < 0) {
        free(linea);
        return 0;
    }
    if (l > 0 && linea[l - 1] == '\n')
        linea[l - 1] = 0;
    rl_line_buffer = linea;
    return linea;
}
static inline int add_history(char const*) { return 0; }
static inline int read_history(char const*) { return 0; }
static inline int write_history(char const*) { return 0; }
