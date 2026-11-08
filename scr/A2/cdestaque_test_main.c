/* cdestaque_test_main.c -- testes do filtro C -> HTML (A2).
 *
 *   ./cdestaque_test                 casos-limite + palavras reservadas + fuzz
 *   ./cdestaque_test a.c b.c ...     idem, e confere o INVARIANTE em cada
 *                                    arquivo .c dado
 *   ./cdestaque_test txt/NN_x.txt    arquivos .txt: confere cada bloco
 *                                    entre linhas exatas "<<C" e "C>>"
 *                                    (o shell expande o curinga da pasta)
 *
 * INVARIANTE  strip_tags(unescape(saida)) == entrada, byte a byte, e a
 * estrutura dos spans e valida (7 classes, sem aninhar, sem vazio).
 * Sai com 0 se tudo passou, 1 caso contrario.
 *
 * Arquivo com main(): por isso o sufixo _main.c (regra do CTX).
 * Compilar: gcc -std=c99 -Wall -Wextra -fsanitize=address,undefined \
 *           -o cdestaque_test cdestaque_test_main.c cdestaque.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdestaque.h"

static int testes = 0, falhas = 0;

#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        testes++;                                                             \
        if (!(cond)) {                                                        \
            falhas++;                                                         \
            fprintf(stderr, "FALHA %s:%d: ", __FILE__, __LINE__);             \
            fprintf(stderr, __VA_ARGS__);                                     \
            fputc('\n', stderr);                                              \
        }                                                                     \
    } while (0)

static const char PRE[] = "<pre class=\"c\"><code>";
static const char SUF[] = "</code></pre>";

/* ---- captura da saida em memoria (so libc: tmpfile) --------------------- */
static char *ler_tmp(FILE *t, size_t *len) {
    long sz;
    char *b;
    fflush(t);
    sz = ftell(t);
    if (sz < 0) {
        perror("ftell");
        exit(1);
    }
    rewind(t);
    b = malloc((size_t)sz + 1);
    if (!b || fread(b, 1, (size_t)sz, t) != (size_t)sz) {
        perror("leitura do tmpfile");
        exit(1);
    }
    b[sz] = '\0';
    *len = (size_t)sz;
    return b;
}

static char *frag(const char *s, size_t n, size_t *len) {
    FILE *t = tmpfile();
    char *b;
    if (!t) {
        perror("tmpfile");
        exit(1);
    }
    c2html(s, n, t);
    b = ler_tmp(t, len);
    fclose(t);
    return b;
}

static int pref(const char *o, size_t i, size_t fim, const char *lit) {
    size_t l = strlen(lit);
    return i + l <= fim && memcmp(o + i, lit, l) == 0;
}

/* NULL se a estrutura e valida, senao descricao do problema */
static const char *estrutura(const char *o, size_t len) {
    static const char *const cls[] = {"pp\">", "tipo\">", "cmd\">", "com\">",
                                      "str\">", "chr\">", "num\">"};
    size_t lp = strlen(PRE), ls = strlen(SUF), i, fim;
    int aberto = 0;
    if (len < lp + ls || memcmp(o, PRE, lp) != 0 ||
        memcmp(o + len - ls, SUF, ls) != 0)
        return "moldura <pre><code> ausente";
    fim = len - ls;
    for (i = lp; i < fim;) {
        if (o[i] == '<') {
            if (pref(o, i, fim, "<span class=\"k-")) {
                size_t j;
                int ok = 0;
                if (aberto) return "span dentro de span";
                for (j = 0; j < sizeof cls / sizeof cls[0]; j++)
                    if (pref(o, i + 15, fim, cls[j])) {
                        i += 15 + strlen(cls[j]);
                        ok = 1;
                        break;
                    }
                if (!ok) return "classe desconhecida";
                if (pref(o, i, fim, "</span>")) return "span vazio";
                aberto = 1;
            } else if (pref(o, i, fim, "</span>")) {
                if (!aberto) return "fecha span nao aberto";
                aberto = 0;
                i += 7;
            } else
                return "tag inesperada";
        } else if (o[i] == '>' || o[i] == '"') {
            return "> ou \" sem escape";
        } else if (o[i] == '&') {
            if (pref(o, i, fim, "&amp;"))
                i += 5;
            else if (pref(o, i, fim, "&lt;") || pref(o, i, fim, "&gt;"))
                i += 4;
            else if (pref(o, i, fim, "&quot;"))
                i += 6;
            else
                return "& sem escape";
        } else
            i++;
    }
    return aberto ? "span nao fechado" : NULL;
}

/* NULL se strip_tags(unescape(o)) == src byte a byte */
static const char *invariante(const char *src, size_t n, const char *o, size_t len) {
    const char *e = estrutura(o, len);
    size_t lp = strlen(PRE), fim = len - strlen(SUF), i, k = 0;
    if (e) return e;
    for (i = lp; i < fim;) {
        char c;
        if (o[i] == '<') {
            while (i < fim && o[i] != '>') i++;
            i++;
            continue;
        }
        if (o[i] == '&') {
            if (pref(o, i, fim, "&amp;")) { c = '&'; i += 5; }
            else if (pref(o, i, fim, "&lt;")) { c = '<'; i += 4; }
            else if (pref(o, i, fim, "&gt;")) { c = '>'; i += 4; }
            else { c = '"'; i += 6; }
        } else
            c = o[i++];
        if (k >= n || src[k] != c) return "texto diferente da entrada";
        k++;
    }
    return k == n ? NULL : "texto mais curto que a entrada";
}

static void confere(const char *nome, const char *src, size_t n) {
    size_t len;
    char *o = frag(src, n, &len);
    const char *e = invariante(src, n, o, len);
    CHECK(e == NULL, "%s: invariante violado (%s)", nome, e ? e : "");
    free(o);
}

/* saida == PRE + corpo + SUF, e o invariante vale */
static void eq(const char *src, const char *corpo) {
    size_t n = strlen(src), len, lc = strlen(corpo);
    char *o = frag(src, n, &len);
    int ok = len == strlen(PRE) + lc + strlen(SUF) &&
             memcmp(o, PRE, strlen(PRE)) == 0 &&
             memcmp(o + strlen(PRE), corpo, lc) == 0 &&
             memcmp(o + len - strlen(SUF), SUF, strlen(SUF)) == 0;
    CHECK(ok, "entrada:\n%s\nesperado:\n%s\nobtido:\n%s", src, corpo, o);
    free(o);
    confere("eq", src, n);
}

static int tem(const char *o, const char *sub) { return strstr(o, sub) != NULL; }

/* ---- testes ------------------------------------------------------------- */
static void t_reservadas(void) {
    static const char *const tipo[] = {
        "int", "char", "float", "double", "long", "short", "unsigned", "signed",
        "void", "_Bool", "bool", "size_t", "FILE", "clock_t", "struct", "union",
        "enum", "typedef", "const", "static", "extern", "register", "volatile",
        "auto", "inline"};
    static const char *const cmd[] = {"if", "else", "for", "while", "do",
                                      "switch", "case", "default", "break",
                                      "continue", "goto", "return", "sizeof"};
    static const char *const nao[] = {"Int", "FILE2", "intx", "_bool", "int_",
                                      "Static", "sizeof2", "x1", "bool_t", "NULL",
                                      "EOF", "main", "printf", "true", "iff",
                                      "dou", "whil", "_Bool1", "ação"};
    size_t k;
    char esperado[128];
    for (k = 0; k < sizeof tipo / sizeof tipo[0]; k++) {
        snprintf(esperado, sizeof esperado, "<span class=\"k-tipo\">%s</span>", tipo[k]);
        eq(tipo[k], esperado);
    }
    for (k = 0; k < sizeof cmd / sizeof cmd[0]; k++) {
        snprintf(esperado, sizeof esperado, "<span class=\"k-cmd\">%s</span>", cmd[k]);
        eq(cmd[k], esperado);
    }
    for (k = 0; k < sizeof nao / sizeof nao[0]; k++) eq(nao[k], nao[k]);
    eq("sizeof(int)",
       "<span class=\"k-cmd\">sizeof</span>(<span class=\"k-tipo\">int</span>)");
}

static void t_limite(void) {
    /* comentarios */
    eq("/*/ texto */ int x;",
       "<span class=\"k-com\">/*/ texto */</span> <span class=\"k-tipo\">int</span> x;");
    eq("/*//DEPURA x*/ y", "<span class=\"k-com\">/*//DEPURA x*/</span> y");
    eq("/* a \" b */", "<span class=\"k-com\">/* a &quot; b */</span>");
    eq("/* a\nb */ c", "<span class=\"k-com\">/* a\nb */</span> c");
    eq("// a\nint", "<span class=\"k-com\">// a</span>\n<span class=\"k-tipo\">int</span>");
    eq("// a \\\nint x\nint",
       "<span class=\"k-com\">// a \\\nint x</span>\n<span class=\"k-tipo\">int</span>");
    eq("int a; /* aberto", "<span class=\"k-tipo\">int</span> a; <span class=\"k-com\">/* aberto</span>");
    eq("/*", "<span class=\"k-com\">/*</span>");
    eq("/", "/");
    eq("a / b", "a / b");
    /* strings e chars */
    eq("char *s = \"//x /* y\";",
       "<span class=\"k-tipo\">char</span> *s = <span class=\"k-str\">&quot;//x /* y&quot;</span>;");
    eq("'\\''", "<span class=\"k-chr\">'\\''</span>");
    eq("'\\\\'", "<span class=\"k-chr\">'\\\\'</span>");
    eq("'\"'", "<span class=\"k-chr\">'&quot;'</span>");
    eq("\"\\\\\" x", "<span class=\"k-str\">&quot;\\\\&quot;</span> x");
    eq("\"a\\\"b\"", "<span class=\"k-str\">&quot;a\\&quot;b&quot;</span>");
    eq("\"abc", "<span class=\"k-str\">&quot;abc</span>");
    eq("\"ab\nint", "<span class=\"k-str\">&quot;ab</span>\n<span class=\"k-tipo\">int</span>");
    eq("'", "<span class=\"k-chr\">'</span>");
    /* pre-processador */
    eq("#include <stdio.h>  /* c */",
       "<span class=\"k-pp\">#include</span> <span class=\"k-com\">&lt;stdio.h&gt;</span>"
       "  <span class=\"k-com\">/* c */</span>");
    eq("#include \"x.h\"",
       "<span class=\"k-pp\">#include</span> <span class=\"k-com\">&quot;x.h&quot;</span>");
    eq("  # define N 10",
       "  <span class=\"k-pp\"># define</span> N <span class=\"k-num\">10</span>");
    eq("#define M(a) \\\n#a", "<span class=\"k-pp\">#define</span> M(a) \\\n#a");
    eq("/* x */ #define A", "<span class=\"k-com\">/* x */</span> <span class=\"k-pp\">#define</span> A");
    eq("a # b", "a # b");
    eq("#pragma once", "<span class=\"k-pp\">#pragma</span> once");
    eq("#\nx", "<span class=\"k-pp\">#</span>\nx");
    eq("#foo", "<span class=\"k-pp\">#</span>foo");
    eq("#include MACRO", "<span class=\"k-pp\">#include</span> MACRO");
    eq("#include <abc", "<span class=\"k-pp\">#include</span> &lt;abc");
    eq("#ifdef A\n#endif",
       "<span class=\"k-pp\">#ifdef</span> A\n<span class=\"k-pp\">#endif</span>");
    /* numeros */
    eq("0x1F 1e-3 3.4f 10UL .5 x1 a[0].b",
       "<span class=\"k-num\">0x1F</span> <span class=\"k-num\">1e-3</span> "
       "<span class=\"k-num\">3.4f</span> <span class=\"k-num\">10UL</span> "
       "<span class=\"k-num\">.5</span> x1 a[<span class=\"k-num\">0</span>].b");
    eq("3+4", "<span class=\"k-num\">3</span>+<span class=\"k-num\">4</span>");
    eq("0x1e+3", "<span class=\"k-num\">0x1e</span>+<span class=\"k-num\">3</span>");
    eq("1e+3", "<span class=\"k-num\">1e+3</span>");
    /* escape */
    eq("x<y && a>b", "x&lt;y &amp;&amp; a&gt;b");
    /* UTF-8, CRLF, tabs, vazio */
    eq("/* ação ≤ ⇒ */", "<span class=\"k-com\">/* ação ≤ ⇒ */</span>");
    eq("char *s = \"é\";",
       "<span class=\"k-tipo\">char</span> *s = <span class=\"k-str\">&quot;é&quot;</span>;");
    eq("int x;\r\nreturn 0;\r\n",
       "<span class=\"k-tipo\">int</span> x;\r\n<span class=\"k-cmd\">return</span> "
       "<span class=\"k-num\">0</span>;\r\n");
    eq("\tif\t(x)", "\t<span class=\"k-cmd\">if</span>\t(x)");
    eq("", "");
    {
        const char bom[] = "\xEF\xBB\xBF" "int x;";
        eq(bom, "\xEF\xBB\xBF<span class=\"k-tipo\">int</span> x;");
    }
    {
        const char nul[] = {'a', '\0', 'b'};
        confere("NUL no meio", nul, sizeof nul);
    }
    confere("NULL com n=0", NULL, 0);
}

static void t_css_pagina(void) {
    static const char *const cl[] = {".k-pp", ".k-tipo", ".k-cmd", ".k-com",
                                     ".k-str", ".k-chr", ".k-num"};
    size_t k, len;
    FILE *t = tmpfile();
    char *o;
    const char *css = cdestaque_css();
    CHECK(css && *css, "cdestaque_css vazio");
    {
        static const char *const hex[] = {"#3D7B7B", "#9C6500", "#B00040",
                                          "#008000", "#BA2121", "#666666"};
        for (k = 0; k < sizeof hex / sizeof hex[0]; k++)
            CHECK(tem(css, hex[k]), "CSS sem a cor %s da Wikipedia", hex[k]);
        CHECK(tem(css, ".k-cmd{color:#008000;font-weight:bold}"), "cmd sem negrito");
        CHECK(tem(css, ".k-com{color:#3D7B7B;font-style:italic}"), "com sem italico");
    }
    for (k = 0; k < sizeof cl / sizeof cl[0]; k++)
        CHECK(tem(css, cl[k]), "CSS sem a classe %s", cl[k]);
    CHECK(t != NULL, "tmpfile");
    if (!t) return;
    c_pagina("int x = 1 < 2;", 14, t, "a<b & \"c\"");
    o = ler_tmp(t, &len);
    fclose(t);
    CHECK(tem(o, "<!DOCTYPE html>"), "sem doctype");
    CHECK(tem(o, "<html lang=\"pt-BR\">"), "sem lang=pt-BR");
    CHECK(tem(o, "<meta charset=\"utf-8\">"), "sem charset");
    CHECK(tem(o, "<title>a&lt;b &amp; &quot;c&quot;</title>"), "titulo nao escapado");
    CHECK(tem(o, "<style>") && tem(o, ".k-cmd{color:"), "CSS nao inline");
    CHECK(tem(o, "<span class=\"k-tipo\">int</span> x = <span class=\"k-num\">1</span> &lt; "
                 "<span class=\"k-num\">2</span>;"),
          "codigo nao destacado na pagina");
    free(o);
}

static void t_grande(void) {
    size_t n = 200000, i;
    char *s = malloc(n + 1);
    if (!s) exit(1);
    for (i = 0; i < n; i++) s[i] = "x<y&&\"a\"'b'/*c*/ int0x1"[i % 24];
    confere("linha de 200KB", s, n);
    memset(s, '/', n);
    s[1] = '*';
    confere("comentario de 200KB aberto", s, n);
    memset(s, 'a', n);
    confere("identificador de 200KB", s, n);
    free(s);
}

/* fuzz deterministico: pedacos "perigosos" colados ao acaso */
static void t_fuzz(void) {
    static const char *const tok[] = {
        "int ", "if", "#include ", "<stdio.h>", "\"", "'", "/*", "*/", "//",
        "\\\n", "\r\n", "0x1F", "1e-3", "\n", "#define A ", "'\\''", "\"\\\"\"",
        "é", "ação", "ints", " ", "\t", "&", "<", ">", "x1", ".", "e+", "-",
        "\\", "#", "/", "*", "return;", "NULL", "0", "p-"};
    const size_t nt = sizeof tok / sizeof tok[0];
    unsigned long long st = 88172645463325252ULL;
    int it;
    for (it = 0; it < 5000; it++) {
        char buf[512];
        size_t len = 0, ntok, k;
        st = st * 6364136223846793005ULL + 1442695040888963407ULL;
        ntok = 1 + (size_t)((st >> 33) % 40);
        for (k = 0; k < ntok; k++) {
            const char *p;
            size_t pl;
            st = st * 6364136223846793005ULL + 1442695040888963407ULL;
            p = tok[(st >> 33) % nt];
            pl = strlen(p);
            if (len + pl >= sizeof buf) break;
            memcpy(buf + len, p, pl);
            len += pl;
        }
        confere("fuzz", buf, len);
        if (falhas > 20) return; /* nao inundar o terminal */
    }
}

/* ---- corpus ------------------------------------------------------------- */
static char *le_arquivo(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    size_t cap = 8192, len = 0;
    char *b;
    if (!f) return NULL;
    b = malloc(cap);
    while (b) {
        size_t r = fread(b + len, 1, cap - len, f);
        len += r;
        if (len < cap) break;
        {
            char *nb = realloc(b, cap * 2);
            if (!nb) {
                free(b);
                b = NULL;
                break;
            }
            b = nb;
            cap *= 2;
        }
    }
    fclose(f);
    *n = len;
    return b;
}

static int termina_em(const char *s, const char *suf) {
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && strcmp(s + a - b, suf) == 0;
}

/* confere cada bloco entre linhas exatas "<<C" e "C>>" */
static int blocos_txt(const char *nome, const char *s, size_t n) {
    size_t i = 0, cap = 0, len = 0;
    int dentro = 0, nb = 0;
    char *blk = NULL;
    while (i < n) {
        size_t j = i, fimlin, prox;
        while (j < n && s[j] != '\n') j++;
        prox = (j < n) ? j + 1 : j;
        fimlin = j;
        if (fimlin > i && s[fimlin - 1] == '\r') fimlin--;
        if (!dentro && fimlin - i == 3 && memcmp(s + i, "<<C", 3) == 0) {
            dentro = 1;
            len = 0;
        } else if (dentro && fimlin - i == 3 && memcmp(s + i, "C>>", 3) == 0) {
            char rot[256];
            snprintf(rot, sizeof rot, "%s bloco %d", nome, ++nb);
            confere(rot, blk ? blk : "", len);
            dentro = 0;
        } else if (dentro) {
            size_t l = prox - i;
            if (len + l > cap) {
                char *t;
                cap = (len + l) * 2 + 64;
                t = realloc(blk, cap);
                if (!t) {
                    free(blk);
                    return -1;
                }
                blk = t;
            }
            memcpy(blk + len, s + i, l);
            len += l;
        }
        i = prox;
    }
    CHECK(!dentro, "%s: bloco <<C sem C>>", nome);
    free(blk);
    return nb;
}

int main(int argc, char **argv) {
    int a;
    t_reservadas();
    t_limite();
    t_css_pagina();
    t_grande();
    t_fuzz();
    printf("embutidos: %d verificacoes, %d falhas\n", testes, falhas);

    for (a = 1; a < argc; a++) {
        size_t n = 0;
        int antes = falhas;
        char *s = le_arquivo(argv[a], &n);
        if (!s) {
            fprintf(stderr, "nao leu %s\n", argv[a]);
            falhas++;
            continue;
        }
        if (termina_em(argv[a], ".txt")) {
            int nb = blocos_txt(argv[a], s, n);
            printf("%-40s %d bloco(s) C %s\n", argv[a], nb, falhas == antes ? "ok" : "FALHOU");
        } else {
            confere(argv[a], s, n);
            printf("%-40s %zu bytes %s\n", argv[a], n, falhas == antes ? "ok" : "FALHOU");
        }
        free(s);
    }
    printf("PROBLEMAS: %d\n", falhas);
    return falhas ? 1 : 0;
}
