/* cdestaque_main.c -- CLI do filtro C -> HTML.
 *
 *   cdestaque [-f] arq.c > saida.html
 *     -f   so o fragmento <pre class="c">...</pre> (sem pagina)
 *     "-"  como nome do arquivo le da entrada padrao
 *
 * Compilar: gcc -std=c99 -Wall -o cdestaque cdestaque.c cdestaque_main.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdestaque.h"

static void uso(void) {
    fputs("uso: cdestaque [-f] arq.c > saida.html\n"
          "  -f   emite so o fragmento <pre class=\"c\">...</pre>\n"
          "  -    le da entrada padrao\n",
          stderr);
}

/* le o fluxo inteiro; 0 ok, -1 erro (sem vazar) */
static int le_tudo(FILE *f, char **buf, size_t *n) {
    size_t cap = 4096, len = 0;
    char *b = malloc(cap);
    if (!b) return -1;
    for (;;) {
        size_t r = fread(b + len, 1, cap - len, f);
        len += r;
        if (len < cap) break; /* fim ou erro */
        {
            char *nb = realloc(b, cap * 2);
            if (!nb) {
                free(b);
                return -1;
            }
            b = nb;
            cap *= 2;
        }
    }
    if (ferror(f)) {
        free(b);
        return -1;
    }
    *buf = b;
    *n = len;
    return 0;
}

int main(int argc, char **argv) {
    int frag = 0, i;
    const char *arq = NULL;
    char *src = NULL;
    size_t n = 0;
    FILE *f;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0)
            frag = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            uso();
            return 0;
        } else if (!arq)
            arq = argv[i];
        else {
            uso();
            return 2;
        }
    }
    if (!arq) {
        uso();
        return 2;
    }

    f = (strcmp(arq, "-") == 0) ? stdin : fopen(arq, "rb");
    if (!f) {
        fprintf(stderr, "cdestaque: nao abriu '%s'\n", arq);
        return 1;
    }
    if (le_tudo(f, &src, &n) < 0) {
        fprintf(stderr, "cdestaque: erro lendo '%s'\n", arq);
        if (f != stdin) fclose(f);
        return 1;
    }
    if (f != stdin) fclose(f);

    if (frag) {
        c2html(src, n, stdout);
        fputc('\n', stdout);
    } else {
        const char *base = strrchr(arq, '/');
        c_pagina(src, n, stdout, strcmp(arq, "-") == 0 ? "stdin" : (base ? base + 1 : arq));
    }
    free(src);
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fputs("cdestaque: erro escrevendo a saida\n", stderr);
        return 1;
    }
    return 0;
}
