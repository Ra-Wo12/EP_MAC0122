/* cdestaque.c -- filtro C -> HTML com destaque de sintaxe (A2).
 *
 * IDEIA
 *   Uma unica passada sobre o texto, com uma maquina de estados escrita a
 *   mao (sem regex).  Em cada posicao decidimos qual "lexema" comeca ali;
 *   se ele merece cor, emitimos <span class="k-..."> ... </span>, senao o
 *   texto fica sem span.  Todo byte de entrada sai exatamente uma vez,
 *   escapado se for & < > ".
 *
 * INVARIANTE (testado em cdestaque_test.c)
 *   strip_tags(unescape(saida)) == entrada, byte a byte.
 *   Em particular: nenhum byte e descartado, duplicado ou reordenado; bytes
 *   >= 0x80 (UTF-8) passam crus, entao acentos nunca sao quebrados; nunca
 *   ha span vazio nem span dentro de span; todo span aberto e fechado
 *   (mesmo com EOF dentro de comentario ou string).
 *
 * COMPLEXIDADE
 *   O(n) no tempo, O(1) de memoria extra (so escreve em FILE *).  A busca
 *   de palavra reservada e binaria sobre tabela ordenada (mesma ideia de
 *   aula_06_busca_binaria.c): O(log 38) por identificador.
 *
 * DECISOES (o enunciado/plano nao define; valem para todo o projeto)
 *   - '#' so e diretiva no inicio da linha (apos espacos/tabs).  Comentario
 *     de bloco antes dele conta como espaco: "/ * x * / #define A" ainda e
 *     diretiva (como no pre-processador).
 *   - span pp = '#' + espacos + nome da diretiva (um span so); em #include,
 *     o header-name <x.h> ou "x.h" e outro span, de classe k-com (a
 *     Wikipedia o pinta como comentario; ver PALETA).  O resto da linha do
 *     #define e codigo normal.  Diretivas: as do plano + "line".
 *     '#' sozinho ou seguido de nome desconhecido: so o '#' recebe span.
 *   - comentario "//" termina no '\n' sem contar '\n' precedido de '\'
 *     (continuacao de linha, como em C99); "/ * ... * /" so fecha em "* /"
 *     achado a partir do 3o byte, logo "/ * / texto * /" NAO fecha em "/ * /".
 *   - string/char sem fechamento termina ANTES do '\n' (um erro de aspas
 *     nao pinta o arquivo inteiro).  EOF fecha o span.
 *   - numero = "pp-number" de C (digitos, letras, '.', '_', sinal depois
 *     de e/E ou, em hexa, p/P): cobre 0x1F 1e-3 3.4f 10UL .5.  Comeca em
 *     digito ou em '.' seguido de digito; "x1" e identificador (sem span).
 *   - NULL, EOF, true, false e nomes de funcao ficam sem span; chr usa a
 *     mesma cor de str.
 *   - bytes >= 0x80 valem como letra de identificador (UTF-8 nao e partido);
 *     um BOM UTF-8 no inicio do arquivo passa cru, sem span.
 *
 * PALETA  (fonte: enunciado "Usar sistema de cores sugeridos pela WikiPedia")
 *   URL .......... https://en.wikipedia.org/wiki/Syntax_highlighting
 *   Acesso ....... 2026-11-08
 *   Origem ....... tabela "Syntax highlighting" do exemplo C (Hello World)
 *                  da pagina.  O TEXTO da pagina nao traz valores de cor; os
 *                  hex foram lidos pela equipe na tabela renderizada (captura
 *                  de tela de 2026-11-08) e conferidos com o estilo padrao do
 *                  Pygments (https://anno-paper.readthedocs.io/_static/
 *                  pygments.css, consultado em 2026-11-08): os 8 hex, o negrito
 *                  e o italico coincidem.  O fundo #f8f8f8 tambem e o do
 *                  Pygments.
 *   Mapeamento ... classe -> cor (token do exemplo)
 *       .k-com  #3D7B7B italico   comentarios       ("// Hello World")
 *                                 header-name       ("<stdio.h>"; no Pygments
 *                                 .cpf tem a mesma cor/estilo de .c1)
 *       .k-pp   #9C6500           diretiva          ("#include")
 *       .k-tipo #B00040           tipos             ("int")
 *       .k-cmd  #008000 negrito   comandos          ("return")
 *       .k-str  #BA2121           "texto"           ("Hello World")
 *       .k-chr  #BA2121           'c'  (nao aparece no exemplo; no Pygments
 *                                 o char usa a mesma cor da string)
 *       .k-num  #666666           numeros           ("0")
 *   Texto base #222222 (borda #dddddd): NAO vem da tabela.
 *   Cores da tabela SEM classe congelada no CTX (nao usadas):
 *       \n         #AA5D1F negrito  (escape dentro da string: fica k-str)
 *       main       #0000FF          (nome de funcao: exige analise sintatica;
 *                                    note que printf nao e colorido no exemplo)
 *   Contraste sobre #f8f8f8: todas as cores usadas >= 4,5:1 (menor: 4,58).
 */
#include "cdestaque.h"

#include <string.h>

/* ---- palavras reservadas: tabela ORDENADA por strcmp (ASCII) ----------- */
typedef struct {
    const char *w;
    char cls; /* 'T' = tipo/qualificador, 'C' = comando de controle */
} Kw;

static const Kw KW[] = {
    {"FILE", 'T'},     {"_Bool", 'T'},   {"auto", 'T'},     {"bool", 'T'},
    {"break", 'C'},    {"case", 'C'},    {"char", 'T'},     {"clock_t", 'T'},
    {"const", 'T'},    {"continue", 'C'},{"default", 'C'},  {"do", 'C'},
    {"double", 'T'},   {"else", 'C'},    {"enum", 'T'},     {"extern", 'T'},
    {"float", 'T'},    {"for", 'C'},     {"goto", 'C'},     {"if", 'C'},
    {"inline", 'T'},   {"int", 'T'},     {"long", 'T'},     {"register", 'T'},
    {"return", 'C'},   {"short", 'T'},   {"signed", 'T'},   {"size_t", 'T'},
    {"sizeof", 'C'},   {"static", 'T'},  {"struct", 'T'},   {"switch", 'C'},
    {"typedef", 'T'},  {"union", 'T'},   {"unsigned", 'T'}, {"void", 'T'},
    {"volatile", 'T'}, {"while", 'C'}};
#define NKW (sizeof KW / sizeof KW[0])

static const char *const DIRETIVAS[] = {
    "define", "elif",    "else",  "endif",  "error", "if",
    "ifdef",  "ifndef",  "include", "line", "pragma", "undef"};
#define NDIR (sizeof DIRETIVAS / sizeof DIRETIVAS[0])

static const char CL_PP[] = "k-pp";
static const char CL_TIPO[] = "k-tipo";
static const char CL_CMD[] = "k-cmd";
static const char CL_COM[] = "k-com";
static const char CL_STR[] = "k-str";
static const char CL_CHR[] = "k-chr";
static const char CL_NUM[] = "k-num";

/* ---- classificacao de caracteres (ASCII fixo, independe de locale) ----- */
static int e_letra(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static int e_digito(unsigned char c) { return c >= '0' && c <= '9'; }
/* bytes >= 0x80 contam como letra de identificador: UTF-8 nunca e partido */
static int e_idc(unsigned char c) {
    return e_letra(c) || e_digito(c) || c == '_' || c >= 0x80;
}
static int e_branco(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

/* 0 = nao e reservada, 1 = tipo, 2 = comando */
static int kw_busca(const char *p, size_t len) {
    char buf[16];
    size_t lo = 0, hi = NKW;
    if (len == 0 || len >= sizeof buf) return 0;
    memcpy(buf, p, len);
    buf[len] = '\0';
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = strcmp(buf, KW[mid].w);
        if (c == 0) return KW[mid].cls == 'T' ? 1 : 2;
        if (c < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    return 0;
}

/* ---- saida ------------------------------------------------------------- */
/* escreve p[0..n) escapando & < > " */
static void esc(FILE *o, const char *p, size_t n) {
    size_t i, ini = 0;
    for (i = 0; i < n; i++) {
        const char *r = NULL;
        switch (p[i]) {
        case '&': r = "&amp;"; break;
        case '<': r = "&lt;"; break;
        case '>': r = "&gt;"; break;
        case '"': r = "&quot;"; break;
        default: break;
        }
        if (r) {
            if (i > ini) fwrite(p + ini, 1, i - ini, o);
            fputs(r, o);
            ini = i + 1;
        }
    }
    if (n > ini) fwrite(p + ini, 1, n - ini, o);
}

/* nunca emite span vazio */
static void span(FILE *o, const char *cls, const char *p, size_t n) {
    if (n == 0) return;
    fputs("<span class=\"", o);
    fputs(cls, o);
    fputs("\">", o);
    esc(o, p, n);
    fputs("</span>", o);
}

/* ---- fins de lexema: cada funcao devolve o indice logo APOS o lexema ---- */

/* i aponta para "/ *".  Procura "* /" a partir de i+2: "/ * /" nao fecha. */
static size_t fim_bloco(const char *s, size_t n, size_t i) {
    size_t j = i + 2;
    while (j + 1 < n) {
        if (s[j] == '*' && s[j + 1] == '/') return j + 2;
        j++;
    }
    return n; /* EOF dentro do comentario */
}

/* i aponta para "//".  Devolve o indice do '\n' que termina (nao incluso). */
static size_t fim_linha(const char *s, size_t n, size_t i) {
    size_t j = i + 2;
    while (j < n) {
        if (s[j] == '\n') {
            size_t p = j;
            if (p > 0 && s[p - 1] == '\r') p--;
            if (p > i + 2 && s[p - 1] == '\\') { /* continuacao */
                j++;
                continue;
            }
            return j;
        }
        j++;
    }
    return n;
}

/* i aponta para a abertura q ('"' ou '\''). */
static size_t fim_lit(const char *s, size_t n, size_t i, char q) {
    size_t j = i + 1;
    while (j < n) {
        char c = s[j];
        if (c == '\\') {
            if (j + 1 >= n) return n;
            if (s[j + 1] == '\r' && j + 2 < n && s[j + 2] == '\n')
                j += 3;
            else
                j += 2;
            continue;
        }
        if (c == q) return j + 1;
        if (c == '\n') return j; /* sem fechamento: para antes do '\n' */
        j++;
    }
    return n;
}

static size_t fim_num(const char *s, size_t n, size_t i) {
    size_t j = i;
    int hexa = 0;
    if (s[j] == '0' && j + 1 < n && (s[j + 1] == 'x' || s[j + 1] == 'X')) {
        hexa = 1;
        j += 2;
    }
    while (j < n) {
        unsigned char c = (unsigned char)s[j];
        if (e_letra(c) || e_digito(c) || c == '.' || c == '_') {
            j++;
            if (j < n && (s[j] == '+' || s[j] == '-') &&
                (hexa ? (c == 'p' || c == 'P') : (c == 'e' || c == 'E')))
                j++;
            continue;
        }
        break;
    }
    return j;
}

static int e_diretiva(const char *p, size_t len) {
    size_t k;
    for (k = 0; k < NDIR; k++)
        if (strlen(DIRETIVAS[k]) == len && memcmp(DIRETIVAS[k], p, len) == 0)
            return 1;
    return 0;
}

/* ---- API --------------------------------------------------------------- */
void c2html(const char *s, size_t n, FILE *o) {
    size_t i = 0;
    size_t ps = 0; /* inicio do texto simples ainda nao escrito */
    int ini_linha = 1; /* so brancos desde o ultimo '\n' (ou inicio) */

#define FLUSH(ate)                                                            \
    do {                                                                      \
        if ((ate) > ps) esc(o, s + ps, (ate) - ps);                           \
    } while (0)

    if (!s) n = 0;
    fputs("<pre class=\"c\"><code>", o);
    /* BOM UTF-8 no inicio: passa cru, sem grudar no 1o identificador */
    if (n >= 3 && memcmp(s, "\xEF\xBB\xBF", 3) == 0) i = 3;

    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        size_t e;

        if (c == '\n') {
            size_t k = i; /* '\' antes do '\n' (com ou sem '\r') = continuacao */
            if (k > 0 && s[k - 1] == '\r') k--;
            ini_linha = !(k > 0 && s[k - 1] == '\\');
            i++;
            continue;
        }
        if (e_branco(c)) {
            i++;
            continue;
        }

        /* comentarios: valem como espaco, nao mexem em ini_linha */
        if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            e = fim_bloco(s, n, i);
            FLUSH(i);
            span(o, CL_COM, s + i, e - i);
            i = ps = e;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '/') {
            e = fim_linha(s, n, i);
            FLUSH(i);
            span(o, CL_COM, s + i, e - i);
            i = ps = e;
            continue;
        }

        if (c == '#' && ini_linha) {
            size_t j = i + 1, d;
            ini_linha = 0;
            while (j < n && (s[j] == ' ' || s[j] == '\t')) j++;
            d = j;
            while (d < n && e_letra((unsigned char)s[d])) d++;
            FLUSH(i);
            if (e_diretiva(s + j, d - j)) {
                int inclui = (d - j == 7 && memcmp(s + j, "include", 7) == 0);
                span(o, CL_PP, s + i, d - i);
                i = ps = d;
                if (inclui) {
                    size_t h = i;
                    while (h < n && (s[h] == ' ' || s[h] == '\t')) h++;
                    if (h < n && (s[h] == '<' || s[h] == '"')) {
                        char fecha = (s[h] == '<') ? '>' : '"';
                        size_t t = h + 1;
                        while (t < n && s[t] != fecha && s[t] != '\n') t++;
                        if (t < n && s[t] == fecha) {
                            FLUSH(h); /* brancos entre #include e o header */
                            span(o, CL_COM, s + h, t + 1 - h); /* cor do .cpf */
                            i = ps = t + 1;
                        }
                    }
                }
            } else {
                span(o, CL_PP, s + i, 1); /* so o '#' */
                i = ps = i + 1;
            }
            continue;
        }

        ini_linha = 0;

        if (c == '"' || c == '\'') {
            e = fim_lit(s, n, i, (char)c);
            FLUSH(i);
            span(o, c == '"' ? CL_STR : CL_CHR, s + i, e - i);
            i = ps = e;
            continue;
        }
        if (e_digito(c) || (c == '.' && i + 1 < n && e_digito((unsigned char)s[i + 1]))) {
            e = fim_num(s, n, i);
            FLUSH(i);
            span(o, CL_NUM, s + i, e - i);
            i = ps = e;
            continue;
        }
        if (e_letra(c) || c == '_' || c >= 0x80) {
            int r;
            e = i + 1;
            while (e < n && e_idc((unsigned char)s[e])) e++;
            r = kw_busca(s + i, e - i);
            if (r) {
                FLUSH(i);
                span(o, r == 1 ? CL_TIPO : CL_CMD, s + i, e - i);
                ps = e;
            }
            i = e;
            continue;
        }
        i++; /* pontuacao: sem span */
    }
    FLUSH(n);
    fputs("</code></pre>", o);
#undef FLUSH
}

const char *cdestaque_css(void) {
    /* Unica fonte da paleta (ver bloco PALETA no topo deste arquivo). */
    return "pre.c{background:#f8f8f8;color:#222222;border:1px solid #dddddd;"
           "border-radius:4px;padding:.75em 1em;overflow-x:auto;"
           "line-height:1.4;tab-size:4}\n"
           "pre.c code{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,"
           "'Liberation Mono',monospace;font-size:.9em;background:none}\n"
           ".k-pp{color:#9C6500}\n"
           ".k-tipo{color:#B00040}\n"
           ".k-cmd{color:#008000;font-weight:bold}\n"
           ".k-com{color:#3D7B7B;font-style:italic}\n"
           ".k-str,.k-chr{color:#BA2121}\n"
           ".k-num{color:#666666}\n";
}

void c_pagina(const char *src, size_t n, FILE *o, const char *titulo) {
    if (!titulo || !*titulo) titulo = "C";
    fputs("<!DOCTYPE html>\n<html lang=\"pt-BR\">\n<head>\n"
          "<meta charset=\"utf-8\">\n"
          "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
          "<title>",
          o);
    esc(o, titulo, strlen(titulo));
    fputs("</title>\n<style>\n"
          "body{font-family:system-ui,sans-serif;margin:1.5em auto;"
          "max-width:60em;padding:0 1em}\n",
          o);
    fputs(cdestaque_css(), o);
    fputs("</style>\n</head>\n<body>\n<h1>", o);
    esc(o, titulo, strlen(titulo));
    fputs("</h1>\n", o);
    c2html(src, n, o);
    fputs("\n</body>\n</html>\n", o);
}
