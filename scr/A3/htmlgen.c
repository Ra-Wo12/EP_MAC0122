/* htmlgen.c -- A3: gerador do site + indexadores. Ver htmlgen.h e NOTAS_A3.txt.
 * Somente libc (+ POSIX mkdir). C99.
 *
 * Plano: (1) classificar docs (p/ ou c_) e agrupar conceitos; (2) calcular
 * ancoras; (3) escrever p/, c/ (c_ e stubs), 6 indices, index.html e css.
 * Invariante de determinismo: toda ordenacao usa chave total (chave, texto,
 * indice), nunca hash nem ponteiro. Nenhuma data/hora/aleatorio na saida.
 * Memoria: strings temporarias vao para um "pool" liberado no fim; se um
 * malloc falhar, err=3 e as paginas restantes sao puladas (sem crash).
 */
#define _POSIX_C_SOURCE 200809L

#include "htmlgen.h"
#include "cdestaque.h"
#include "slug.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define SEM_CHAVE LONG_MAX

static char vazio[1] = "";
static char item_s[] = "item";

/* ------------------------------------------------------------------ tipos */

typedef struct {
    const Doc *d;
    int eh_c;                   /* basename comeca por "c_": pagina de conceito */
    const char *base;           /* basename de d->arq (aponta p/ dentro dele) */
    int pos;                    /* posicao entre os docs p/ (ordem de arquivo) */
    char *slug_alg, *slug_tec;  /* NULL se o campo falta */
    char *id_alg, *id_data, *id_prob;   /* ancoras unicas nos indices */
    long chave;                 /* chave cronologica de Data */
    int *cg;                    /* grupo de conceito de cada d->conceitos[k] */
} Info;

typedef struct {
    char *slug;                 /* slug(texto) */
    const char *nome;           /* texto exibido */
    int nome_c;                 /* nome veio de um c_*.txt */
    char dest;                  /* 'c' c_*.txt | 'p' doc p/ | 's' stub */
    int idoc;                   /* doc de destino (dest 'c' ou 'p') */
    int *cit, ncit;             /* docs p/ que citam este conceito */
    char **pal;                 /* palavras do slug (auto-link) */
    int npal;
} Conc;

typedef struct {
    long num;
    const char *k1, *k2, *txt;
    int idx, pos;
} SItem;

typedef struct {
    Doc *v;
    int n;
    const char *dir;
    Info *inf;
    int *ord, nord;             /* docs p/ por nome de arquivo */
    int *ordc, nordc;           /* docs c_ por nome de arquivo */
    Conc *cc;
    int ncc;
    int *ordconc;               /* conceitos: mais palavras primeiro (auto-link) */
    int *ordcs;                 /* conceitos por slug (idx_conceitos) */
    char *vis;                  /* conceito ja linkado no paragrafo atual */
    const Doc *cur;             /* doc em renderizacao (avisos) */
    void **pool;
    size_t npool, cappool;
    int err;                    /* 0 ok; 1 entrada; 2 E/S; 3 memoria */
} Ctx;

/* ------------------------------------------------------------ utilitarios */

static void erro(Ctx *c, int cod)
{
    if (!c->err) c->err = cod;
}

static void *pool_add(Ctx *c, void *p)
{
    if (!p) { erro(c, 3); return NULL; }
    if (c->npool == c->cappool) {
        size_t nc = c->cappool ? c->cappool * 2 : 256;
        void **np = realloc(c->pool, nc * sizeof *np);
        if (!np) { free(p); erro(c, 3); return NULL; }
        c->pool = np;
        c->cappool = nc;
    }
    c->pool[c->npool++] = p;
    return p;
}

static char *dupn(Ctx *c, const char *s, size_t n)
{
    char *p = malloc(n + 1);
    if (!p) { erro(c, 3); return vazio; }
    memcpy(p, s, n);
    p[n] = '\0';
    return pool_add(c, p) ? p : vazio;
}

static char *xslug(Ctx *c, const char *s)
{
    char *p = slug(s);
    return pool_add(c, p) ? p : vazio;
}

static const char *idof(const char *sl)
{
    return (sl && sl[0]) ? sl : item_s;
}

static const char *basename_of(const char *p)
{
    const char *r = p, *q;
    for (q = p; *q; q++)
        if (*q == '/' || *q == '\\') r = q + 1;
    return r;
}

static int eh_sp(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static void esc(FILE *f, const char *s, size_t n)
{
    size_t i, ini = 0;
    for (i = 0; i < n; i++) {
        const char *r = NULL;
        switch (s[i]) {
        case '&': r = "&amp;"; break;
        case '<': r = "&lt;"; break;
        case '>': r = "&gt;"; break;
        case '"': r = "&quot;"; break;
        default: break;
        }
        if (r) {
            if (i > ini) fwrite(s + ini, 1, i - ini, f);
            fputs(r, f);
            ini = i + 1;
        }
    }
    if (n > ini) fwrite(s + ini, 1, n - ini, f);
}

static void escz(FILE *f, const char *s)
{
    esc(f, s, strlen(s));
}

static int cmp_item(const void *pa, const void *pb)
{
    const SItem *a = pa, *b = pb;
    int r;
    if (a->num != b->num) return a->num < b->num ? -1 : 1;
    r = strcmp(a->k1 ? a->k1 : "", b->k1 ? b->k1 : "");
    if (r) return r;
    r = strcmp(a->k2 ? a->k2 : "", b->k2 ? b->k2 : "");
    if (r) return r;
    return (a->idx > b->idx) - (a->idx < b->idx);
}

static char *mkpath(const char *dir, const char *sub, const char *nome)
{
    size_t n = strlen(dir) + strlen(sub) + strlen(nome) + 3;
    char *p = malloc(n);
    if (!p) return NULL;
    snprintf(p, n, "%s%s%s%s%s", dir, *sub ? "/" : "", sub, *nome ? "/" : "", nome);
    return p;
}

static FILE *abre(Ctx *c, const char *sub, const char *nome)
{
    char *path = mkpath(c->dir, sub, nome);
    FILE *f;
    if (!path) { erro(c, 3); return NULL; }
    f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "gera_site: nao consegui criar '%s'\n", path);
        erro(c, 2);
    }
    free(path);
    return f;
}

static void fecha(Ctx *c, FILE *f)
{
    if (!f) return;
    if (ferror(f)) erro(c, 2);
    if (fclose(f) != 0) erro(c, 2);
}

static int faz_dir(const char *dir, const char *sub)
{
    char *p = mkpath(dir, sub, "");
    int r = 0;
    if (!p) return 3;
    if (mkdir(p, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "gera_site: nao consegui criar a pasta '%s'\n", p);
        r = 2;
    }
    free(p);
    return r;
}

/* ------------------------------------------------------------ chave de Data
 * PRIMEIRO numero ou "seculo <romano>" do texto (R4). "a.C." => negativo;
 * "seculo N" => (N-1)*100; nada achado => SEM_CHAVE (vai ao fim). */
static int romano(const char *p)
{
    int tot = 0, v, nx;
    const char *q;
    for (q = p; *q; q++) {
        switch (*q) {
        case 'I': v = 1; break;    case 'V': v = 5; break;
        case 'X': v = 10; break;   case 'L': v = 50; break;
        case 'C': v = 100; break;  case 'D': v = 500; break;
        case 'M': v = 1000; break; default: return tot;
        }
        switch (q[1]) {
        case 'I': nx = 1; break;    case 'V': nx = 5; break;
        case 'X': nx = 10; break;   case 'L': nx = 50; break;
        case 'C': nx = 100; break;  case 'D': nx = 500; break;
        case 'M': nx = 1000; break; default: nx = 0; break;
        }
        tot += (v < nx) ? -v : v;
    }
    return tot;
}

static long chave_data(const char *s)
{
    const char *p = s, *q;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            long v = 0;
            while (*p >= '0' && *p <= '9') {
                if (v < 100000000L) v = v * 10 + (*p - '0');
                p++;
            }
            q = p;
            while (*q == ' ') q++;
            if (strncmp(q, "a.C.", 4) == 0 || strncmp(q, "a.c.", 4) == 0) v = -v;
            return v;
        }
        if ((*p == 's' || *p == 'S') &&
            (strncmp(p + 1, "\xC3\xA9" "culo", 6) == 0 ||
             strncmp(p + 1, "eculo", 5) == 0)) {
            q = p + (p[1] == 'e' ? 6 : 7);
            while (*q == ' ') q++;
            if (*q >= 'I') {                      /* so maiusculas romanas */
                int r = romano(q);
                if (r > 0) return (long)(r - 1) * 100;
            }
        }
        p++;
    }
    return SEM_CHAVE;
}

/* ------------------------------------------------------------- navegacao */

static void nav(FILE *f, const char *pre)
{
    static const char *const rot[6][2] = {
        {"idx_tecnica", "Técnica"}, {"idx_algoritmo", "Algoritmo"},
        {"idx_autores", "Autores"}, {"idx_data", "Data"},
        {"idx_conceitos", "Conceitos"}, {"idx_problema", "Problema"}
    };
    int i;
    fputs("<header class=\"topo\"><nav aria-label=\"Navegação\">", f);
    fprintf(f, "<a href=\"%sindex.html\">Início</a>", pre);
    for (i = 0; i < 6; i++)
        fprintf(f, " <a href=\"%s%s.html\">%s</a>", pre, rot[i][0], rot[i][1]);
    fputs("</nav></header>\n", f);
}

static void head(FILE *f, const char *titulo, const char *pre)
{
    fputs("<!DOCTYPE html>\n<html lang=\"pt-BR\">\n<head>\n"
          "<meta charset=\"utf-8\">\n"
          "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
          "<title>", f);
    escz(f, titulo);
    fprintf(f, "</title>\n<link rel=\"stylesheet\" href=\"%scss/estilo.css\">\n"
               "</head>\n<body>\n", pre);
    nav(f, pre);
    fputs("<main>\n", f);
}

static void foot(FILE *f)
{
    fputs("</main>\n<footer>Páginas geradas automaticamente a partir dos "
          "arquivos TXT do curso.</footer>\n</body>\n</html>\n", f);
}

static void alink(FILE *f, const char *pre, const char *pasta, const char *sl,
                  const char *texto, const char *classe)
{
    fprintf(f, "<a%s%s%s href=\"%s%s/%s.html\">",
            classe ? " class=\"" : "", classe ? classe : "", classe ? "\"" : "",
            pre, pasta, sl);
    escz(f, texto);
    fputs("</a>", f);
}

static void href_conc(FILE *f, const Conc *g, const char *pre)
{
    fprintf(f, "%s%s/%s.html", pre, g->dest == 'p' ? "p" : "c", idof(g->slug));
}

/* -------------------------------------------------- resolucao de [[slug]] */

/* 'p' doc p/ ; 'c' conceito/c_ ; 0 nao achou. *tit = texto do link. */
static char resolve(const Ctx *c, const char *sl, const char **tit)
{
    int i;
    for (i = 0; i < c->n; i++)
        if (!c->inf[i].eh_c && strcmp(c->inf[i].d->slug, sl) == 0) {
            *tit = c->inf[i].d->titulo;
            return 'p';
        }
    for (i = 0; i < c->n; i++)
        if (c->inf[i].eh_c && strcmp(c->inf[i].d->slug, sl) == 0) {
            *tit = c->inf[i].d->titulo;
            return 'c';
        }
    for (i = 0; i < c->ncc; i++)
        if (c->cc[i].dest == 's' && strcmp(c->cc[i].slug, sl) == 0) {
            *tit = c->cc[i].nome;
            return 'c';
        }
    return 0;
}

/* ------------------------------------------------------------- auto-link */

typedef struct { size_t a, b; char *sl; } Tok;

static int eh_pal(char ch)
{
    unsigned char u = (unsigned char)ch;
    return (u >= '0' && u <= '9') || (u >= 'a' && u <= 'z') ||
           (u >= 'A' && u <= 'Z') || u >= 0x80;
}

static int tok_ok(const Tok *t)
{
    return t->sl && t->sl[0] && !strchr(t->sl, '_');
}

static int casa(const Tok *t, size_t k, const Conc *g, const char *s)
{
    int j;
    for (j = 0; j < g->npal; j++) {
        const Tok *x = &t[k + (size_t)j];
        size_t lp = strlen(g->pal[j]);
        if (!tok_ok(x)) return 0;
        if (strcmp(x->sl, g->pal[j]) != 0) {
            /* plural simples na ultima palavra: "grafos" casa "grafo" */
            if (!(j == g->npal - 1 && lp >= 3 && strlen(x->sl) == lp + 1 &&
                  strncmp(x->sl, g->pal[j], lp) == 0 && x->sl[lp] == 's'))
                return 0;
        }
        if (j > 0) {
            size_t q;
            for (q = t[k + (size_t)j - 1].b; q < x->a; q++)
                if (s[q] != ' ' && s[q] != '\t' && s[q] != '\n' && s[q] != '-')
                    return 0;
        }
    }
    return 1;
}

static int eh_self(const Ctx *c, const Conc *g, int self)
{
    (void)c;
    return self >= 0 && (g->dest == 'c' || g->dest == 'p') && g->idoc == self;
}

/* Escreve [s,s+n) escapado; a 1a ocorrencia (por paragrafo) de cada nome de
 * conceito vira link. Casamento por palavra inteira, sem acento/caixa. */
static void autolink(Ctx *c, FILE *f, const char *s, size_t n, const char *pre,
                     int self)
{
    size_t i = 0, nt = 0, k, saida = 0, a, b;
    Tok *t;
    int m;

    for (i = 0; i < n;) {
        if (eh_pal(s[i])) {
            while (i < n && eh_pal(s[i])) i++;
            nt++;
        } else i++;
    }
    t = calloc(nt + 1, sizeof *t);
    if (!t || c->ncc == 0 || nt == 0) {
        if (!t) erro(c, 3);
        free(t);
        esc(f, s, n);
        return;
    }
    nt = 0;
    for (i = 0; i < n;) {
        if (eh_pal(s[i])) {
            size_t ini = i;
            char *tmp;
            while (i < n && eh_pal(s[i])) i++;
            t[nt].a = ini;
            t[nt].b = i;
            tmp = malloc(i - ini + 1);
            if (tmp) {
                memcpy(tmp, s + ini, i - ini);
                tmp[i - ini] = '\0';
                t[nt].sl = slug(tmp);
                free(tmp);
            }
            nt++;
        } else i++;
    }
    for (k = 0; k < nt;) {
        int hit = -1;
        if (tok_ok(&t[k])) {
            for (m = 0; m < c->ncc; m++) {
                const Conc *g = &c->cc[c->ordconc[m]];
                if (g->npal == 0 || k + (size_t)g->npal > nt) continue;
                if (casa(t, k, g, s)) { hit = c->ordconc[m]; break; }
            }
        }
        if (hit < 0) { k++; continue; }
        a = t[k].a;
        b = t[k + (size_t)c->cc[hit].npal - 1].b;
        if (!c->vis[hit] && !eh_self(c, &c->cc[hit], self)) {
            esc(f, s + saida, a - saida);
            fputs("<a class=\"conc\" href=\"", f);
            href_conc(f, &c->cc[hit], pre);
            fputs("\">", f);
            esc(f, s + a, b - a);
            fputs("</a>", f);
            saida = b;
            c->vis[hit] = 1;
        }
        k += (size_t)c->cc[hit].npal;
    }
    esc(f, s + saida, n - saida);
    for (k = 0; k < nt; k++) free(t[k].sl);
    free(t);
}

/* Texto de paragrafo: [[slug]] vira link; o resto passa por autolink. */
static void inline_txt(Ctx *c, FILE *f, const char *s, size_t n, const char *pre,
                       int self)
{
    size_t i = 0, ini = 0;
    while (i + 1 < n) {
        if (s[i] == '[' && s[i + 1] == '[') {
            size_t j = i + 2;
            while (j + 1 < n && s[j] != '\n' && s[j] != '[' &&
                   !(s[j] == ']' && s[j + 1] == ']')) j++;
            if (j + 1 < n && s[j] == ']' && s[j + 1] == ']' && j > i + 2) {
                char *nome = dupn(c, s + i + 2, j - i - 2), *sl;
                const char *tit = NULL;
                char k;
                autolink(c, f, s + ini, i - ini, pre, self);
                sl = nome;
                k = resolve(c, sl, &tit);
                if (k) {
                    alink(f, pre, k == 'p' ? "p" : "c", sl, tit, "ref");
                } else {
                    size_t q;
                    fprintf(stderr, "aviso: %s: [[%s]] sem destino\n",
                            c->cur ? c->cur->arq : "?", sl);
                    for (q = 0; sl[q]; q++) fputc(sl[q] == '_' ? ' ' : sl[q], f);
                }
                i = ini = j + 2;
                continue;
            }
        }
        i++;
    }
    autolink(c, f, s + ini, n - ini, pre, self);
}

/* REF: texto escapado; http(s)://... vira <a>. */
static void emit_ref(FILE *f, const char *s, size_t n)
{
    size_t i = 0, ini = 0;
    while (i < n) {
        if ((n - i > 8 && strncmp(s + i, "http://", 7) == 0) ||
            (n - i > 9 && strncmp(s + i, "https://", 8) == 0)) {
            size_t j = i;
            while (j < n && !eh_sp(s[j]) && s[j] != '<' && s[j] != '>') j++;
            while (j > i + 8 && strchr(".,;:)]'\"", s[j - 1])) j--;
            esc(f, s + ini, i - ini);
            fputs("<a href=\"", f);
            esc(f, s + i, j - i);
            fputs("\" rel=\"noopener\">", f);
            esc(f, s + i, j - i);
            fputs("</a>", f);
            i = ini = j;
            continue;
        }
        i++;
    }
    esc(f, s + ini, n - ini);
}

/* ----------------------------------------------------------------- corpo */

enum { M_NADA, M_P, M_PRE, M_UL };

static void fecha_modo(FILE *f, int *modo)
{
    if (*modo == M_P) fputs("</p>\n", f);
    else if (*modo == M_PRE) fputs("</pre>\n", f);
    else if (*modo == M_UL) fputs("</ul>\n", f);
    *modo = M_NADA;
}

static void render_corpo(Ctx *c, FILE *f, int self, const char *pre)
{
    const char *s = c->inf[self].d->corpo;
    size_t n = strlen(s), p = 0, *ref = NULL, nref = 0, capref = 0, i;
    int modo = M_NADA;

    c->cur = c->inf[self].d;
    while (p < n) {
        size_t e = p, prox, a, b, larg = 0, ind = 0;

        while (e < n && s[e] != '\n') e++;
        prox = (e < n) ? e + 1 : n;
        a = p;
        b = e;
        while (b > a && eh_sp(s[b - 1])) b--;
        if (b == a) { fecha_modo(f, &modo); p = prox; continue; }

        if (b - a == 3 && memcmp(s + a, "<<C", 3) == 0) {   /* bloco de codigo */
            size_t bs = prox, q = prox, be = n, blen;
            int achou = 0;
            fecha_modo(f, &modo);
            p = n;
            while (q < n) {
                size_t qe = q, qb;
                while (qe < n && s[qe] != '\n') qe++;
                qb = qe;
                while (qb > q && eh_sp(s[qb - 1])) qb--;
                if (qb - q == 3 && memcmp(s + q, "C>>", 3) == 0) {
                    be = q;
                    achou = 1;
                    p = (qe < n) ? qe + 1 : n;
                    break;
                }
                q = (qe < n) ? qe + 1 : n;
            }
            if (!achou)
                fprintf(stderr, "aviso: %s: bloco <<C sem C>>\n", c->cur->arq);
            blen = be - bs;
            if (blen > 0 && s[be - 1] == '\n') blen--;
            c2html(s + bs, blen, f);
            fputc('\n', f);
            continue;
        }
        if (b - a >= 4 && memcmp(s + a, "REF:", 4) == 0) {      /* referencia */
            size_t ra = a + 4;
            fecha_modo(f, &modo);
            while (ra < b && eh_sp(s[ra])) ra++;
            if (ra < b) {
                if (nref == capref) {
                    size_t nc = capref ? capref * 2 : 8;
                    size_t *nr = realloc(ref, nc * 2 * sizeof *ref);
                    if (!nr) { erro(c, 3); break; }
                    ref = nr;
                    capref = nc;
                }
                ref[2 * nref] = ra;
                ref[2 * nref + 1] = b;
                nref++;
            }
            p = prox;
            continue;
        }
        while (a + ind < b && (s[a + ind] == ' ' || s[a + ind] == '\t')) {
            larg += (s[a + ind] == '\t') ? 4 : 1;
            ind++;
        }
        if (larg >= 2) {                                   /* pseudocodigo */
            if (modo != M_PRE) {
                fecha_modo(f, &modo);
                fputs("<pre class=\"pseudo\">", f);
                modo = M_PRE;
            } else fputc('\n', f);
            esc(f, s + a, b - a);
        } else if (ind <= 1 && b - (a + ind) >= 2 && s[a + ind] == '-' &&
                   s[a + ind + 1] == ' ') {                /* item de lista */
            size_t ia = a + ind + 2;
            while (ia < b && eh_sp(s[ia])) ia++;
            if (modo != M_UL) {
                fecha_modo(f, &modo);
                fputs("<ul>\n", f);
                modo = M_UL;
            }
            memset(c->vis, 0, (size_t)c->ncc);
            fputs("<li>", f);
            inline_txt(c, f, s + ia, b - ia, pre, self);
            fputs("</li>\n", f);
        } else {                                           /* paragrafo */
            if (modo != M_P) {
                fecha_modo(f, &modo);
                fputs("<p>", f);
                modo = M_P;
                memset(c->vis, 0, (size_t)c->ncc);
            } else fputc('\n', f);
            inline_txt(c, f, s + a + ind, b - (a + ind), pre, self);
        }
        p = prox;
    }
    fecha_modo(f, &modo);
    if (nref > 0) {
        fputs("<h2>Referências</h2>\n<ol class=\"refs\">\n", f);
        for (i = 0; i < nref; i++) {
            fputs("<li>", f);
            emit_ref(f, s + ref[2 * i], ref[2 * i + 1] - ref[2 * i]);
            fputs("</li>\n", f);
        }
        fputs("</ol>\n", f);
    }
    free(ref);
}

/* ------------------------------------------------------------ preparacao */

static void add_citer(Ctx *c, int g, int doc)
{
    Conc *q = &c->cc[g];
    int *np = realloc(q->cit, (size_t)(q->ncit + 1) * sizeof *np);
    if (!np) { erro(c, 3); return; }
    q->cit = np;
    q->cit[q->ncit++] = doc;
}

/* ancora unica: slug(base), com "_2", "_3"... se ja usada neste indice */
static char *uniq_id(Ctx *c, const char *base, char **used, int *nu)
{
    char *sl = xslug(c, base);
    const char *b = idof(sl);
    char *cand = (char *)b;
    int k = 1, j, dup;

    for (;;) {
        dup = 0;
        for (j = 0; j < *nu; j++)
            if (strcmp(used[j], cand) == 0) { dup = 1; break; }
        if (!dup) break;
        {
            char suf[24];
            size_t lb = strlen(b);
            char *t;
            snprintf(suf, sizeof suf, "_%d", ++k);
            t = malloc(lb + strlen(suf) + 1);
            if (!t) { erro(c, 3); break; }
            memcpy(t, b, lb);
            strcpy(t + lb, suf);
            cand = pool_add(c, t);
            if (!cand) break;
        }
    }
    if (!cand) cand = item_s;
    used[(*nu)++] = cand;
    return cand;
}

static int prepara(Ctx *c)
{
    int i, j, k, g, pass;
    size_t total = 0;
    SItem *si = calloc((size_t)c->n, sizeof *si);

    c->inf = calloc((size_t)c->n, sizeof *c->inf);
    c->ord = calloc((size_t)c->n, sizeof *c->ord);
    c->ordc = calloc((size_t)c->n, sizeof *c->ordc);
    if (!si || !c->inf || !c->ord || !c->ordc) { free(si); return 3; }

    for (i = 0; i < c->n; i++) {
        Info *x = &c->inf[i];
        x->d = &c->v[i];
        x->base = basename_of(x->d->arq);
        x->eh_c = strncmp(x->base, "c_", 2) == 0;     /* R1 */
        x->pos = -1;
        x->slug_alg = x->d->algoritmo ? xslug(c, x->d->algoritmo) : NULL;
        x->slug_tec = x->d->tecnica ? xslug(c, x->d->tecnica) : NULL;
        x->chave = x->d->data ? chave_data(x->d->data) : SEM_CHAVE;
        total += (size_t)x->d->nConc;
        si[i].k1 = x->base;
        si[i].idx = i;
    }
    if (c->err) { free(si); return c->err; }

    for (i = 0; i < c->n; i++)                         /* colisao na mesma pasta */
        for (j = i + 1; j < c->n; j++)
            if (c->inf[i].eh_c == c->inf[j].eh_c &&
                strcmp(c->inf[i].d->slug, c->inf[j].d->slug) == 0) {
                fprintf(stderr, "gera_site: slug '%s' repetido em '%s' e '%s'\n",
                        c->inf[i].d->slug, c->inf[i].d->arq, c->inf[j].d->arq);
                free(si);
                return 1;
            }

    qsort(si, (size_t)c->n, sizeof *si, cmp_item);     /* ordem de arquivo */
    for (k = 0; k < c->n; k++) {
        i = si[k].idx;
        if (c->inf[i].eh_c) c->ordc[c->nordc++] = i;
        else { c->inf[i].pos = c->nord; c->ord[c->nord++] = i; }
    }
    free(si);

    /* grupos de conceito (R2): primeiro os docs p/, depois os c_ */
    c->cc = calloc(total ? total : 1, sizeof *c->cc);
    if (!c->cc) return 3;
    for (pass = 0; pass < 2; pass++) {
        int cnt = pass ? c->nordc : c->nord;
        const int *lst = pass ? c->ordc : c->ord;
        for (k = 0; k < cnt; k++) {
            Info *x = &c->inf[lst[k]];
            int q;
            x->cg = pool_add(c, calloc((size_t)(x->d->nConc ? x->d->nConc : 1),
                                       sizeof(int)));
            if (!x->cg) return 3;
            for (q = 0; q < x->d->nConc; q++) {
                const char *txt = x->d->conceitos[q];
                char *sl = xslug(c, txt);
                if (c->err) return c->err;
                for (g = 0; g < c->ncc; g++)
                    if (strcmp(c->cc[g].slug, sl) == 0) break;
                if (g == c->ncc) {
                    Conc *cn = &c->cc[c->ncc++];
                    cn->slug = sl;
                    cn->nome = txt;
                    cn->nome_c = x->eh_c;
                    cn->dest = 's';
                    cn->idoc = -1;
                } else if (x->eh_c && !c->cc[g].nome_c) {
                    c->cc[g].nome = txt;               /* nome do proprio c_ */
                    c->cc[g].nome_c = 1;
                }
                x->cg[q] = g;
                if (!x->eh_c) add_citer(c, g, lst[k]);
            }
        }
    }
    if (c->err) return c->err;

    for (g = 0; g < c->ncc; g++) {                     /* destino: c_, doc, stub */
        Conc *q = &c->cc[g];
        for (i = 0; i < c->n; i++)
            if (c->inf[i].eh_c && strcmp(c->inf[i].d->slug, q->slug) == 0) {
                q->dest = 'c';
                q->idoc = i;
                break;
            }
        if (q->dest == 's')
            for (i = 0; i < c->n; i++)
                if (!c->inf[i].eh_c && strcmp(c->inf[i].d->slug, q->slug) == 0) {
                    q->dest = 'p';
                    q->idoc = i;
                    break;
                }
        if (q->slug[0]) {                              /* palavras (auto-link) */
            char *w = dupn(c, q->slug, strlen(q->slug)), *p;
            size_t cnt = 1;
            for (p = w; *p; p++)
                if (*p == '_') cnt++;
            q->pal = pool_add(c, malloc(cnt * sizeof *q->pal));
            if (!q->pal) return 3;
            q->pal[q->npal++] = w;
            for (p = w; *p; p++)
                if (*p == '_') { *p = '\0'; q->pal[q->npal++] = p + 1; }
        }
    }
    if (c->err) return c->err;

    c->ordconc = calloc((size_t)(c->ncc ? c->ncc : 1), sizeof(int));
    c->ordcs = calloc((size_t)(c->ncc ? c->ncc : 1), sizeof(int));
    c->vis = calloc((size_t)(c->ncc ? c->ncc : 1), 1);
    si = calloc((size_t)(c->ncc ? c->ncc : 1), sizeof *si);
    if (!c->ordconc || !c->ordcs || !c->vis || !si) { free(si); return 3; }
    for (g = 0; g < c->ncc; g++) {
        si[g].num = -(long)c->cc[g].npal;
        si[g].k1 = c->cc[g].slug;
        si[g].idx = g;
    }
    qsort(si, (size_t)c->ncc, sizeof *si, cmp_item);
    for (g = 0; g < c->ncc; g++) c->ordconc[g] = si[g].idx;
    for (g = 0; g < c->ncc; g++) si[g].num = 0;
    qsort(si, (size_t)c->ncc, sizeof *si, cmp_item);
    for (g = 0; g < c->ncc; g++) c->ordcs[g] = si[g].idx;
    free(si);

    {                                                  /* ancoras unicas */
        char **u = calloc(3 * (size_t)c->n, sizeof *u);
        int nu[3] = {0, 0, 0};
        if (!u) return 3;
        for (k = 0; k < c->nord; k++) {
            Info *x = &c->inf[c->ord[k]];
            if (x->d->algoritmo)
                x->id_alg = uniq_id(c, x->d->algoritmo, u, &nu[0]);
            if (x->d->data)
                x->id_data = uniq_id(c, x->d->data, u + c->n, &nu[1]);
            if (x->d->problema)
                x->id_prob = uniq_id(c, x->d->problema, u + 2 * c->n, &nu[2]);
        }
        free(u);
    }
    return c->err;
}

/* ---------------------------------------------------------------- paginas */

static FILE *abre_pagina(Ctx *c, const char *sub, const char *sl)
{
    size_t m = strlen(sl) + 6;
    char *nm = malloc(m);
    FILE *f;
    if (!nm) { erro(c, 3); return NULL; }
    snprintf(nm, m, "%s.html", sl);
    f = abre(c, sub, nm);
    free(nm);
    return f;
}

static const char *nome_doc(const Doc *d)
{
    return d->algoritmo ? d->algoritmo : d->titulo;
}

static void citado_em(const Ctx *c, FILE *f, const Conc *g)
{
    int k;
    if (!g || g->ncit == 0) return;
    fputs("<h2>Citado em</h2>\n<ul>\n", f);
    for (k = 0; k < g->ncit; k++) {
        const Doc *d = c->inf[g->cit[k]].d;
        fputs("<li>", f);
        alink(f, "../", "p", d->slug, nome_doc(d), NULL);
        fputs("</li>\n", f);
    }
    fputs("</ul>\n", f);
}

static void pagina_doc(Ctx *c, int i)
{
    const Info *x = &c->inf[i];
    const Doc *d = x->d;
    FILE *f = abre_pagina(c, "p", d->slug);
    int k, tem;

    if (!f) return;
    head(f, d->titulo, "../");
    fputs("<article>\n<h1>", f);
    escz(f, d->titulo);
    fputs("</h1>\n", f);

    tem = d->algoritmo || d->tecnica || d->nAut || d->data || d->problema ||
          d->nConc;
    if (tem) {
        fputs("<table class=\"meta\">\n", f);
        if (d->algoritmo) {
            fputs("<tr><th>Algoritmo</th><td><a href=\"../idx_algoritmo.html#", f);
            fprintf(f, "%s\">", x->id_alg);
            escz(f, d->algoritmo);
            fputs("</a></td></tr>\n", f);
        }
        if (d->tecnica) {
            fputs("<tr><th>Técnica</th><td>", f);
            fprintf(f, "<a href=\"../idx_tecnica.html#%s\">", idof(x->slug_tec));
            escz(f, d->tecnica);
            fputs("</a></td></tr>\n", f);
        }
        if (d->nAut) {
            fputs("<tr><th>Autores</th><td>", f);
            for (k = 0; k < d->nAut; k++) {
                if (k) fputs(", ", f);
                fprintf(f, "<a href=\"../idx_autores.html#%s\">",
                        idof(xslug(c, d->autores[k])));
                escz(f, d->autores[k]);
                fputs("</a>", f);
            }
            fputs("</td></tr>\n", f);
        }
        if (d->data) {
            fputs("<tr><th>Data</th><td>", f);
            fprintf(f, "<a href=\"../idx_data.html#%s\">", x->id_data);
            escz(f, d->data);
            fputs("</a></td></tr>\n", f);
        }
        if (d->problema) {
            c->cur = d;
            fputs("<tr><th><a href=\"../idx_problema.html#", f);
            fprintf(f, "%s\">Problema</a></th><td>", x->id_prob);
            memset(c->vis, 0, (size_t)c->ncc);
            inline_txt(c, f, d->problema, strlen(d->problema), "../", i);
            fputs("</td></tr>\n", f);
        }
        if (d->nConc) {
            fputs("<tr><th>Conceitos</th><td>", f);
            for (k = 0; k < d->nConc; k++) {
                if (k) fputs(", ", f);
                fputs("<a href=\"", f);
                href_conc(f, &c->cc[x->cg[k]], "../");
                fputs("\">", f);
                escz(f, d->conceitos[k]);
                fputs("</a>", f);
            }
            fputs("</td></tr>\n", f);
        }
        fputs("</table>\n", f);
    }

    render_corpo(c, f, i, "../");
    fputs("</article>\n<nav class=\"prox\" aria-label=\"Páginas vizinhas\">", f);
    if (x->pos > 0) {
        const Doc *p = c->inf[c->ord[x->pos - 1]].d;
        fprintf(f, "<a rel=\"prev\" href=\"%s.html\">← ", p->slug);
        escz(f, nome_doc(p));
        fputs("</a>", f);
    } else fputs("<span class=\"off\">← anterior</span>", f);
    if (x->pos + 1 < c->nord) {
        const Doc *p = c->inf[c->ord[x->pos + 1]].d;
        fprintf(f, "<a rel=\"next\" href=\"%s.html\">", p->slug);
        escz(f, nome_doc(p));
        fputs(" →</a>", f);
    } else fputs("<span class=\"off\">próxima →</span>", f);
    fputs("</nav>\n", f);
    foot(f);
    fecha(c, f);
}

static void pagina_conceito_c(Ctx *c, int i)       /* c/<slug>.html de um c_*.txt */
{
    const Doc *d = c->inf[i].d;
    FILE *f = abre_pagina(c, "c", d->slug);
    int g;

    if (!f) return;
    head(f, d->titulo, "../");
    fputs("<article>\n<h1>", f);
    escz(f, d->titulo);
    fputs("</h1>\n", f);
    render_corpo(c, f, i, "../");
    for (g = 0; g < c->ncc; g++)
        if (c->cc[g].dest == 'c' && c->cc[g].idoc == i) {
            citado_em(c, f, &c->cc[g]);
            break;
        }
    fputs("</article>\n", f);
    foot(f);
    fecha(c, f);
}

static void pagina_stub(Ctx *c, const Conc *g)
{
    FILE *f = abre_pagina(c, "c", idof(g->slug));
    if (!f) return;
    head(f, g->nome, "../");
    fputs("<article>\n<h1>", f);
    escz(f, g->nome);
    fputs("</h1>\n<p>Este conceito ainda não tem uma página própria. "
          "Veja onde ele aparece:</p>\n", f);
    citado_em(c, f, g);
    fputs("</article>\n", f);
    foot(f);
    fecha(c, f);
}

/* ---------------------------------------------------------------- indices */

static FILE *abre_idx(Ctx *c, const char *arq, const char *titulo)
{
    FILE *f = abre(c, "", arq);
    if (!f) return NULL;
    head(f, titulo, "");
    fputs("<h1>", f);
    escz(f, titulo);
    fputs("</h1>\n", f);
    return f;
}

static void fim_idx(Ctx *c, FILE *f)
{
    foot(f);
    fecha(c, f);
}

static void li_doc(FILE *f, const Doc *d)
{
    alink(f, "", "p", d->slug, nome_doc(d), NULL);
}

static void idx_tecnica(Ctx *c)
{
    SItem *si = calloc((size_t)(c->nord ? c->nord : 1), sizeof *si);
    int m = 0, a, b, k, best;
    FILE *f;

    if (!si) { erro(c, 3); return; }
    for (a = 0; a < c->nord; a++) {
        const Info *x = &c->inf[c->ord[a]];
        if (!x->slug_tec) continue;
        si[m].k1 = x->slug_tec;
        si[m].k2 = x->slug_alg ? x->slug_alg : "";
        si[m].idx = c->ord[a];
        si[m].pos = x->pos;
        m++;
    }
    qsort(si, (size_t)m, sizeof *si, cmp_item);
    f = abre_idx(c, "idx_tecnica.html", "Índice de técnicas");
    if (!f) { free(si); return; }
    if (m == 0) fputs("<p>Nenhuma entrada.</p>\n", f);
    for (a = 0; a < m; a = b) {
        b = a + 1;
        while (b < m && strcmp(si[b].k1, si[a].k1) == 0) b++;
        best = a;
        for (k = a; k < b; k++)
            if (si[k].pos < si[best].pos) best = k;     /* 1o texto visto */
        fprintf(f, "<section>\n<h2 id=\"%s\">", idof(si[a].k1));
        escz(f, c->inf[si[best].idx].d->tecnica);
        fputs("</h2>\n<ul>\n", f);
        for (k = a; k < b; k++) {
            fputs("<li>", f);
            li_doc(f, c->inf[si[k].idx].d);
            fputs("</li>\n", f);
        }
        fputs("</ul>\n</section>\n", f);
    }
    fim_idx(c, f);
    free(si);
}

static void idx_algoritmo(Ctx *c)
{
    SItem *si = calloc((size_t)(c->nord ? c->nord : 1), sizeof *si);
    int m = 0, a;
    FILE *f;

    if (!si) { erro(c, 3); return; }
    for (a = 0; a < c->nord; a++) {
        const Info *x = &c->inf[c->ord[a]];
        if (!x->d->algoritmo) continue;
        si[m].k1 = x->slug_alg;
        si[m].idx = c->ord[a];
        m++;
    }
    qsort(si, (size_t)m, sizeof *si, cmp_item);
    f = abre_idx(c, "idx_algoritmo.html", "Índice de algoritmos");
    if (!f) { free(si); return; }
    if (m == 0) fputs("<p>Nenhuma entrada.</p>\n", f);
    else fputs("<ul>\n", f);
    for (a = 0; a < m; a++) {
        const Info *x = &c->inf[si[a].idx];
        fprintf(f, "<li id=\"%s\">", x->id_alg);
        li_doc(f, x->d);
        fputs("</li>\n", f);
    }
    if (m) fputs("</ul>\n", f);
    fim_idx(c, f);
    free(si);
}

static void idx_autores(Ctx *c)
{
    size_t tot = 0;
    SItem *si;
    int m = 0, a, b, k, best;
    FILE *f;

    for (a = 0; a < c->nord; a++) tot += (size_t)c->inf[c->ord[a]].d->nAut;
    si = calloc(tot ? tot : 1, sizeof *si);
    if (!si) { erro(c, 3); return; }
    for (a = 0; a < c->nord; a++) {
        const Info *x = &c->inf[c->ord[a]];
        for (k = 0; k < x->d->nAut; k++) {
            si[m].k1 = xslug(c, x->d->autores[k]);
            si[m].k2 = x->slug_alg ? x->slug_alg : "";
            si[m].txt = x->d->autores[k];
            si[m].idx = c->ord[a];
            si[m].pos = x->pos;
            m++;
        }
    }
    qsort(si, (size_t)m, sizeof *si, cmp_item);
    f = abre_idx(c, "idx_autores.html", "Índice de autores");
    if (!f) { free(si); return; }
    if (m == 0) fputs("<p>Nenhuma entrada.</p>\n", f);
    for (a = 0; a < m; a = b) {
        b = a + 1;
        while (b < m && strcmp(si[b].k1, si[a].k1) == 0) b++;
        best = a;
        for (k = a; k < b; k++)
            if (si[k].pos < si[best].pos) best = k;
        fprintf(f, "<section>\n<h2 id=\"%s\">", idof(si[a].k1));
        escz(f, si[best].txt);
        fputs("</h2>\n<ul>\n", f);
        for (k = a; k < b; k++) {
            const Doc *d = c->inf[si[k].idx].d;
            fputs("<li>", f);
            li_doc(f, d);
            if (d->data) {
                fputs(" — ", f);
                escz(f, d->data);
            }
            fputs("</li>\n", f);
        }
        fputs("</ul>\n</section>\n", f);
    }
    fim_idx(c, f);
    free(si);
}

static void idx_data(Ctx *c)
{
    SItem *si = calloc((size_t)(c->nord ? c->nord : 1), sizeof *si);
    int m = 0, a, k;
    FILE *f;

    if (!si) { erro(c, 3); return; }
    for (a = 0; a < c->nord; a++) {
        const Info *x = &c->inf[c->ord[a]];
        if (!x->d->data) continue;
        si[m].num = x->chave;
        si[m].k1 = x->slug_alg ? x->slug_alg : "";
        si[m].idx = c->ord[a];
        m++;
    }
    qsort(si, (size_t)m, sizeof *si, cmp_item);
    f = abre_idx(c, "idx_data.html", "Índice cronológico");
    if (!f) { free(si); return; }
    if (m == 0) fputs("<p>Nenhuma entrada.</p>\n", f);
    else fputs("<ul>\n", f);
    for (a = 0; a < m; a++) {
        const Info *x = &c->inf[si[a].idx];
        const Doc *d = x->d;
        fprintf(f, "<li id=\"%s\">", x->id_data);
        escz(f, d->data);
        fputs(": ", f);
        for (k = 0; k < d->nAut; k++) {
            if (k) fputs(", ", f);
            escz(f, d->autores[k]);
        }
        if (d->nAut) fputc(' ', f);
        fputc('(', f);
        li_doc(f, d);
        fputs(")</li>\n", f);
    }
    if (m) fputs("</ul>\n", f);
    fim_idx(c, f);
    free(si);
}

static void idx_conceitos(Ctx *c)
{
    int a, k;
    FILE *f = abre_idx(c, "idx_conceitos.html", "Índice de conceitos");

    if (!f) return;
    if (c->ncc == 0) fputs("<p>Nenhuma entrada.</p>\n", f);
    else fputs("<ul>\n", f);
    for (a = 0; a < c->ncc; a++) {
        const Conc *g = &c->cc[c->ordcs[a]];
        fprintf(f, "<li id=\"%s\"><a href=\"", idof(g->slug));
        href_conc(f, g, "");
        fputs("\">", f);
        escz(f, g->nome);
        fputs("</a>", f);
        if (g->ncit > 0) {
            fputs(" — citado em: ", f);
            for (k = 0; k < g->ncit; k++) {
                if (k) fputs(", ", f);
                li_doc(f, c->inf[g->cit[k]].d);
            }
        }
        fputs("</li>\n", f);
    }
    if (c->ncc) fputs("</ul>\n", f);
    fim_idx(c, f);
}

static void idx_problema(Ctx *c)
{
    int a, m = 0;
    FILE *f = abre_idx(c, "idx_problema.html", "Índice de problemas");

    if (!f) return;
    for (a = 0; a < c->nord; a++)
        if (c->inf[c->ord[a]].d->problema) m++;
    if (m == 0) fputs("<p>Nenhuma entrada.</p>\n", f);
    else fputs("<ul>\n", f);
    for (a = 0; a < c->nord; a++) {
        const Info *x = &c->inf[c->ord[a]];
        if (!x->d->problema) continue;
        fprintf(f, "<li id=\"%s\">", x->id_prob);
        li_doc(f, x->d);
        fputs(": ", f);
        escz(f, x->d->problema);
        fputs("</li>\n", f);
    }
    if (m) fputs("</ul>\n", f);
    fim_idx(c, f);
}

static void pagina_inicial(Ctx *c)
{
    static const char *const desc[6][3] = {
        {"idx_tecnica", "Técnica", "algoritmos agrupados pela técnica de projeto"},
        {"idx_algoritmo", "Algoritmo", "todos os algoritmos, em ordem alfabética"},
        {"idx_autores", "Autores", "quem criou ou formalizou cada algoritmo"},
        {"idx_data", "Data", "ordem cronológica das origens"},
        {"idx_conceitos", "Conceitos", "conceitos e onde aparecem"},
        {"idx_problema", "Problema", "o problema que cada algoritmo resolve"}
    };
    FILE *f = abre(c, "", "index.html");
    int a;

    if (!f) return;
    head(f, "MAC0122 — página inicial", "");
    fputs("<h1>MAC0122 — material do curso</h1>\n"
          "<p>Páginas geradas a partir dos arquivos TXT do curso. "
          "Escolha um índice ou siga a trilha de estudo.</p>\n"
          "<h2>Índices</h2>\n<ul>\n", f);
    for (a = 0; a < 6; a++)
        fprintf(f, "<li><a href=\"%s.html\">%s</a> — %s</li>\n", desc[a][0],
                desc[a][1], desc[a][2]);
    fputs("</ul>\n", f);
    if (c->nord > 0) {
        fputs("<h2>Trilha de estudo</h2>\n<ol class=\"trilha\">\n", f);
        for (a = 0; a < c->nord; a++) {
            fputs("<li>", f);
            li_doc(f, c->inf[c->ord[a]].d);
            fputs("</li>\n", f);
        }
        fputs("</ol>\n", f);
    }
    if (c->nordc > 0) {
        fputs("<h2>Conceitos</h2>\n<ul>\n", f);
        for (a = 0; a < c->nordc; a++) {
            const Doc *d = c->inf[c->ordc[a]].d;
            fputs("<li>", f);
            alink(f, "", "c", d->slug, d->titulo, NULL);
            fputs("</li>\n", f);
        }
        fputs("</ul>\n", f);
    }
    foot(f);
    fecha(c, f);
}

static const char *const CSS_BASE[] = {
    ":root{color-scheme:light}\n"
    "*{box-sizing:border-box}\n"
    "body{margin:0;background:#fff;color:#222;"
    "font:1rem/1.6 system-ui,-apple-system,\"Segoe UI\",Roboto,sans-serif}\n"
    ".topo{background:#1f3a5f;padding:.6em 1em}\n"
    ".topo nav{max-width:60em;margin:0 auto;display:flex;flex-wrap:wrap;"
    "gap:.3em 1.1em}\n"
    ".topo a{color:#fff;text-decoration:none;font-weight:600}\n"
    ".topo a:hover,.topo a:focus{text-decoration:underline}\n"
    "main{max-width:60em;margin:0 auto;padding:1em 1em 2em}\n"
    "footer{max-width:60em;margin:0 auto;padding:1em;color:#555;"
    "font-size:.85em;border-top:1px solid #ddd}\n",

    "h1{line-height:1.25;margin:.6em 0 .5em}\n"
    "h2{margin:1.6em 0 .5em;border-bottom:1px solid #ddd;padding-bottom:.2em}\n"
    "a{color:#0b5cad}\n"
    "a.conc{text-decoration:underline dotted}\n"
    ":target{background:#fff8c5}\n"
    "li{margin:.25em 0}\n"
    "table.meta{border-collapse:collapse;margin:1em 0;width:100%}\n"
    "table.meta th{text-align:left;vertical-align:top;white-space:nowrap;"
    "padding:.4em .8em .4em 0;color:#444}\n"
    "table.meta td{padding:.4em 0}\n"
    "table.meta tr{border-bottom:1px solid #eee}\n",

    "pre.pseudo{background:#f4f6f8;border:1px solid #ddd;border-radius:4px;"
    "padding:.75em 1em;overflow-x:auto;line-height:1.4;"
    "font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,"
    "'Liberation Mono',monospace;font-size:.9em}\n"
    ".prox{display:flex;justify-content:space-between;gap:1em;"
    "margin:2em 0 0;padding-top:1em;border-top:1px solid #ddd}\n"
    ".prox .off{color:#767676}\n"
    "ol.refs li{margin:.4em 0;overflow-wrap:anywhere}\n"
    "/* ---- destaque de sintaxe (paleta de cdestaque_css) ---- */\n",
    NULL
};

static void escreve_css(Ctx *c)
{
    FILE *f = abre(c, "css", "estilo.css");
    int i;
    if (!f) return;
    for (i = 0; CSS_BASE[i]; i++) fputs(CSS_BASE[i], f);
    fputs(cdestaque_css(), f);
    fecha(c, f);
}

/* ------------------------------------------------------------------- API */

int gera_site(Doc *v, int n, const char *dirOut)
{
    Ctx c;
    int r, i;

    if (!v || n <= 0 || !dirOut) {
        fputs("gera_site: argumentos invalidos\n", stderr);
        return 1;
    }
    memset(&c, 0, sizeof c);
    c.v = v;
    c.n = n;
    c.dir = dirOut;

    r = faz_dir(dirOut, "");
    if (!r) r = faz_dir(dirOut, "p");
    if (!r) r = faz_dir(dirOut, "c");
    if (!r) r = faz_dir(dirOut, "css");
    if (r) return r;

    r = prepara(&c);
    if (r) erro(&c, r);
    if (!c.err) {
        for (i = 0; i < c.nord && !c.err; i++) pagina_doc(&c, c.ord[i]);
        for (i = 0; i < c.nordc && !c.err; i++) pagina_conceito_c(&c, c.ordc[i]);
        for (i = 0; i < c.ncc && !c.err; i++)
            if (c.cc[c.ordcs[i]].dest == 's') pagina_stub(&c, &c.cc[c.ordcs[i]]);
        if (!c.err) idx_tecnica(&c);
        if (!c.err) idx_algoritmo(&c);
        if (!c.err) idx_autores(&c);
        if (!c.err) idx_data(&c);
        if (!c.err) idx_conceitos(&c);
        if (!c.err) idx_problema(&c);
        if (!c.err) pagina_inicial(&c);
        if (!c.err) escreve_css(&c);
    }

    for (i = 0; i < c.ncc; i++) free(c.cc[i].cit);
    free(c.cc);
    free(c.inf);
    free(c.ord);
    free(c.ordc);
    free(c.ordconc);
    free(c.ordcs);
    free(c.vis);
    for (i = 0; (size_t)i < c.npool; i++) free(c.pool[i]);
    free(c.pool);
    return c.err;
}
