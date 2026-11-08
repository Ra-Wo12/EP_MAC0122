/* txtparse.c -- A1 (parser). Ver txtparse.h. Somente libc, C99.
 *
 * Ideia: ler o arquivo inteiro, normalizar (BOM, CRLF), consumir o
 * cabecalho linha a linha ("Chave: valor") ate a 1a linha vazia ou sem
 * chave valida, e entregar o resto como corpo.
 * Invariante: o texto vive num unico buffer; todas as "linhas" sao pares
 * de ponteiros [a,b) nesse buffer -> nenhum limite fixo de tamanho de linha.
 * Custo: O(tamanho do arquivo) (+ O(k^2) no dedupe de k itens de um
 * multivalor, k pequeno).
 */
#include "txtparse.h"
#include "slug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { K_TEC, K_ALG, K_AUT, K_DATA, K_CONC, K_PROB };
#define K_INVALIDA (-1)
#define K_SEM_MEMORIA (-2)

static char *dup_n(const char *s, size_t n)
{
    char *p = malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static int eh_branco(char c)
{
    return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' ||
           c == '\n';
}

/* Encurta [*a,*b) tirando brancos das duas pontas. */
static void trim(const char **a, const char **b)
{
    while (*a < *b && eh_branco(**a)) (*a)++;
    while (*b > *a && eh_branco((*b)[-1])) (*b)--;
}

/* ---------- leitura do arquivo ---------- */

/* Le tudo para *out (terminado em '\0'). 0 ok, -2 nao abriu, -3 leitura,
 * -4 memoria. */
static int le_arquivo(const char *path, char **out, size_t *len)
{
    FILE *f = fopen(path, "rb");
    char *buf, *nb;
    size_t cap = 8192, n = 0, r;

    if (!f) return -2;
    buf = malloc(cap);
    if (!buf) { fclose(f); return -4; }
    for (;;) {
        if (cap - n < 4096 + 1) {
            if (cap > ((size_t)-1) / 2) { free(buf); fclose(f); return -4; }
            nb = realloc(buf, cap * 2);
            if (!nb) { free(buf); fclose(f); return -4; }
            buf = nb;
            cap *= 2;
        }
        r = fread(buf + n, 1, cap - n - 1, f);
        n += r;
        if (r == 0) break;
    }
    if (ferror(f)) { free(buf); fclose(f); return -3; }
    fclose(f);
    buf[n] = '\0';
    *out = buf;
    *len = n;
    return 0;
}

/* Normaliza no proprio buffer: tira BOM UTF-8, NUL -> ' ', CRLF/CR -> LF.
 * Devolve o novo comprimento; retorna ponteiro de inicio em *ini. */
static size_t normaliza(char *buf, size_t n, char **ini)
{
    size_t i = 0, j = 0;

    if (n >= 3 && (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF)
        i = 3;
    for (; i < n; i++) {
        char c = buf[i];
        if (c == '\0') c = ' ';
        if (c == '\r') {
            if (i + 1 < n && buf[i + 1] == '\n') continue; /* CRLF */
            c = '\n';                                      /* CR solto */
        }
        buf[j++] = c;
    }
    buf[j] = '\0';
    *ini = buf;
    return j;
}

/* ---------- campos do cabecalho ---------- */

/* Qual chave e [a,b)? K_* ou K_INVALIDA; K_SEM_MEMORIA se malloc falhar. */
static int chave_id(const char *a, const char *b)
{
    static const char *const nomes[] = {
        "tecnica", "algoritmo", "autores", "data", "conceitos", "problema"
    };
    char *s;
    int i, id = K_INVALIDA;

    if (b - a < 1 || b - a > 32) return K_INVALIDA;
    {
        char *t = dup_n(a, (size_t)(b - a));
        if (!t) return K_SEM_MEMORIA;
        s = slug(t);               /* case-insens. e sem acento: "Tecnica" */
        free(t);
    }
    if (!s) return K_SEM_MEMORIA;
    for (i = 0; i < 6; i++)
        if (strcmp(s, nomes[i]) == 0) { id = i; break; }
    free(s);
    return id;
}

/* Campo unico: guarda [a,b) (ja aparado, nao vazio). 0 ok, -4 memoria. */
static int set_unico(char **dst, const char *a, const char *b)
{
    char *p = dup_n(a, (size_t)(b - a));
    if (!p) return -4;
    free(*dst);
    *dst = p;
    return 0;
}

static void libera_vetor(char **v, int n)
{
    int i;
    if (!v) return;
    for (i = 0; i < n; i++) free(v[i]);
    free(v);
}

/* Multivalor: separa [a,b) por ',', apara, descarta vazios e repetidos
 * (repetido = mesmo slug, ou seja, ignora caixa e acento). Substitui o
 * conteudo anterior. 0 ok, -4 memoria. */
static int set_multi(char ***dst, int *cnt, const char *a, const char *b)
{
    size_t max = 1, k;
    const char *p, *q;
    char **v, **sl;
    int n = 0, i, rc = 0;

    for (p = a; p < b; p++)
        if (*p == ',') max++;
    v = calloc(max, sizeof *v);
    sl = calloc(max, sizeof *sl);
    if (!v || !sl) { free(v); free(sl); return -4; }

    for (p = a; p <= b && rc == 0; p = q + 1) {
        const char *ia, *ib;
        char *item, *sg;
        int dup = 0;

        q = memchr(p, ',', (size_t)(b - p));
        if (!q) q = b;
        ia = p; ib = q;
        trim(&ia, &ib);
        if (ia < ib) {
            item = dup_n(ia, (size_t)(ib - ia));
            sg = item ? slug(item) : NULL;
            if (!item || !sg) { free(item); free(sg); rc = -4; break; }
            for (i = 0; i < n; i++)
                if (strcmp(sl[i], sg) == 0) { dup = 1; break; }
            if (dup) { free(item); free(sg); }
            else { v[n] = item; sl[n] = sg; n++; }
        }
        if (q == b) break;
    }
    for (k = 0; k < (size_t)n; k++) free(sl[k]);
    free(sl);
    if (rc < 0) { libera_vetor(v, n); return rc; }
    if (n == 0) { free(v); v = NULL; }
    libera_vetor(*dst, *cnt);
    *dst = v;
    *cnt = n;
    return 0;
}

/* ---------- nome do arquivo -> slug / titulo de reserva ---------- */

/* [*a,*b) = nome do arquivo sem pasta e sem a extensao final. */
static void nome_base(const char *path, const char **a, const char **b)
{
    const char *p, *dot = NULL;

    *a = path;
    for (p = path; *p; p++)
        if (*p == '/' || *p == '\\') *a = p + 1;
    *b = p;
    for (p = *a; *p; p++)
        if (*p == '.') dot = p;
    if (dot && dot > *a) *b = dot;
}

/* slug do documento: nome base sem "NN_" inicial (algoritmos) e sem "c_"
 * inicial (conceitos). Se sobrar vazio, usa o nome base inteiro. */
static char *slug_do_arquivo(const char *a, const char *b)
{
    const char *p = a;
    char *t, *s;

    if (b - p >= 2 && p[0] == 'c' && p[1] == '_') {
        p += 2;
    } else {
        const char *q = p;
        while (q < b && *q >= '0' && *q <= '9') q++;
        if (q > p && q < b && *q == '_') p = q + 1;
    }
    t = dup_n(p, (size_t)(b - p));
    if (!t) return NULL;
    s = slug(t);
    free(t);
    if (s && s[0] == '\0') {
        free(s);
        t = dup_n(a, (size_t)(b - a));
        if (!t) return NULL;
        s = slug(t);
        free(t);
        if (s && s[0] == '\0') { free(s); s = dup_n("sem_nome", 8); }
    }
    return s;
}

static char *escolhe_titulo(const Doc *d, const char *na, const char *nb)
{
    const char *a, *b, *e;

    if (d->algoritmo) return dup_n(d->algoritmo, strlen(d->algoritmo));
    if (d->problema)  return dup_n(d->problema, strlen(d->problema));
    if (d->nConc > 0) return dup_n(d->conceitos[0], strlen(d->conceitos[0]));
    a = d->corpo;
    b = a + strlen(a);
    e = memchr(a, '\n', (size_t)(b - a));
    if (e) b = e;
    trim(&a, &b);
    if (a < b) return dup_n(a, (size_t)(b - a));
    return dup_n(na, (size_t)(nb - na));
}

/* ---------- API ---------- */

void doc_libera(Doc *d)
{
    if (!d) return;
    free(d->arq); free(d->slug); free(d->titulo); free(d->tecnica);
    free(d->algoritmo); free(d->data); free(d->problema); free(d->corpo);
    libera_vetor(d->autores, d->nAut);
    libera_vetor(d->conceitos, d->nConc);
    memset(d, 0, sizeof *d);
}

int doc_le(const char *path, Doc *d)
{
    char *raw = NULL, *txt;
    size_t len = 0, n, p, q;
    const char *na, *nb;
    int rc;

    if (!d) return -1;
    memset(d, 0, sizeof *d);
    if (!path) return -1;

    rc = le_arquivo(path, &raw, &len);
    if (rc < 0) return rc;
    n = normaliza(raw, len, &txt);

    /* --- cabecalho --- */
    p = 0;
    while (p < n) {
        const char *e = memchr(txt + p, '\n', n - p);
        size_t fim = e ? (size_t)(e - txt) : n;
        const char *a = txt + p, *b = txt + fim, *dp;
        const char *va, *vb;
        int id;

        trim(&a, &b);
        if (a == b) { p = e ? fim + 1 : n; break; }  /* linha vazia: fim */
        dp = memchr(a, ':', (size_t)(b - a));
        if (!dp) break;                               /* sem "Chave:" */
        {
            const char *ka = a, *kb = dp;
            trim(&ka, &kb);
            id = chave_id(ka, kb);
        }
        if (id == K_SEM_MEMORIA) { rc = -4; goto erro; }
        if (id == K_INVALIDA) break;                  /* comeca o corpo */

        va = dp + 1; vb = b;
        trim(&va, &vb);
        if (va < vb) {                                /* vazio => ignora */
            switch (id) {
            case K_TEC:  rc = set_unico(&d->tecnica, va, vb); break;
            case K_ALG:  rc = set_unico(&d->algoritmo, va, vb); break;
            case K_DATA: rc = set_unico(&d->data, va, vb); break;
            case K_PROB: rc = set_unico(&d->problema, va, vb); break;
            case K_AUT:  rc = set_multi(&d->autores, &d->nAut, va, vb); break;
            default:     rc = set_multi(&d->conceitos, &d->nConc, va, vb);
            }
            if (rc < 0) goto erro;
        }
        p = e ? fim + 1 : n;
    }

    /* --- corpo: pula linhas em branco iniciais, apara o fim --- */
    while (p < n) {
        const char *e = memchr(txt + p, '\n', n - p);
        size_t fim = e ? (size_t)(e - txt) : n, i;
        int vazia = 1;
        for (i = p; i < fim; i++)
            if (!eh_branco(txt[i])) { vazia = 0; break; }
        if (!vazia) break;
        p = e ? fim + 1 : n;
    }
    q = n;
    while (q > p && eh_branco(txt[q - 1])) q--;
    d->corpo = dup_n(txt + p, q - p);
    if (!d->corpo) { rc = -4; goto erro; }

    /* --- arq, slug, titulo --- */
    d->arq = dup_n(path, strlen(path));
    nome_base(path, &na, &nb);
    d->slug = slug_do_arquivo(na, nb);
    if (!d->arq || !d->slug) { rc = -4; goto erro; }
    d->titulo = escolhe_titulo(d, na, nb);
    if (!d->titulo) { rc = -4; goto erro; }

    free(raw);
    return 0;

erro:
    free(raw);
    doc_libera(d);
    return rc;
}
