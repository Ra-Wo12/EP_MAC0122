/* gera_main.c -- A3. Unico main do gerador.
 *
 *   ./gera <dir_txt> <dir_out>        ex.: ./gera txt site
 *
 * Le todos os *.txt de <dir_txt> (ignora o resto), em ordem de nome
 * (strcmp), com doc_le, e chama gera_site.
 */
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "htmlgen.h"
#include "txtparse.h"

static int cmp_nome(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static int termina_em_txt(const char *s)
{
    size_t n = strlen(s);
    return n > 4 && strcmp(s + n - 4, ".txt") == 0;
}

int main(int argc, char **argv)
{
    DIR *dir;
    struct dirent *e;
    char **nomes = NULL, **tmp;
    size_t nn = 0, cap = 0, i;
    Doc *v = NULL;
    int rc = 1;

    if (argc != 3) {
        fputs("uso: gera <dir_txt> <dir_out>\n", stderr);
        return 2;
    }
    dir = opendir(argv[1]);
    if (!dir) {
        fprintf(stderr, "gera: nao abriu a pasta '%s'\n", argv[1]);
        return 1;
    }
    while ((e = readdir(dir)) != NULL) {
        if (!termina_em_txt(e->d_name)) continue;
        if (nn == cap) {
            cap = cap ? cap * 2 : 64;
            tmp = realloc(nomes, cap * sizeof *nomes);
            if (!tmp) { fputs("gera: sem memoria\n", stderr); goto fim; }
            nomes = tmp;
        }
        nomes[nn] = strdup(e->d_name);
        if (!nomes[nn]) { fputs("gera: sem memoria\n", stderr); goto fim; }
        nn++;
    }
    if (nn == 0) {
        fprintf(stderr, "gera: nenhum .txt em '%s'\n", argv[1]);
        goto fim;
    }
    qsort(nomes, nn, sizeof *nomes, cmp_nome);

    v = calloc(nn, sizeof *v);
    if (!v) { fputs("gera: sem memoria\n", stderr); goto fim; }
    for (i = 0; i < nn; i++) {
        size_t m = strlen(argv[1]) + strlen(nomes[i]) + 2;
        char *path = malloc(m);
        int r;
        if (!path) { fputs("gera: sem memoria\n", stderr); goto fim; }
        snprintf(path, m, "%s/%s", argv[1], nomes[i]);
        r = doc_le(path, &v[i]);
        if (r < 0) fprintf(stderr, "gera: erro %d lendo '%s'\n", r, path);
        free(path);
        if (r < 0) goto fim;
    }
    rc = gera_site(v, (int)nn, argv[2]) == 0 ? 0 : 1;

fim:
    if (v) {
        for (i = 0; i < nn; i++) doc_libera(&v[i]);
        free(v);
    }
    for (i = 0; i < nn; i++) free(nomes[i]);
    free(nomes);
    closedir(dir);
    return rc;
}
