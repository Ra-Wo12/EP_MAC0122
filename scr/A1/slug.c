/* slug.c -- A1 (parser). Ver slug.h. Somente libc, C99. */
#include "slug.h"

#include <stdlib.h>
#include <string.h>

/* Latin-1 Supplement U+00C0..U+00FF: letra-base; '*' = 2 letras (ver
 * casos_especiais); '-' = nao e letra (multiplicacao, divisao). */
static const char LAT1[] =
    "aaaaaa*c" "eeeeiiii" "dnooooo-" "ouuuuy**"
    "aaaaaa*c" "eeeeiiii" "dnooooo-" "ouuuuy*y";

/* Latin Extended-A U+0100..U+017F (128 entradas). */
static const char LATA[] =
    "aaaaaa" "cccccccc" "dddd" "eeeeeeeeee" "gggggggg" "hhhh"
    "iiiiiiiiii" "**" "jj" "kkk" "llllllllll" "nnnnnnnnn"
    "oooooo" "**" "rrrrrr" "ssssssss" "tttttt" "uuuuuuuuuuuu"
    "ww" "yyy" "zzzzzz" "s";

/* Garante em tempo de compilacao que as tabelas tem o tamanho certo. */
typedef char slug_chk_lat1[(sizeof LAT1 - 1 == 64) ? 1 : -1];
typedef char slug_chk_lata[(sizeof LATA - 1 == 128) ? 1 : -1];

/* Ligaduras/letras que viram 2 caracteres. */
static const char *casos_especiais(unsigned cp)
{
    switch (cp) {
    case 0xC6: case 0xE6: return "ae";
    case 0xDE: case 0xFE: return "th";
    case 0xDF:            return "ss";
    case 0x132: case 0x133: return "ij";
    case 0x152: case 0x153: return "oe";
    default:              return "";
    }
}

/*
 * Dobra um ponto de codigo para ASCII.
 * Retorno: n>0 -> n letras/digitos gravados em out ;
 *          0   -> separador ;
 *         -1   -> marca combinante (ignorar, nao conta como separador).
 */
static int dobra(unsigned cp, char out[3])
{
    char c = 0;
    const char *e;

    if (cp < 0x80) {
        if (cp >= 'A' && cp <= 'Z') cp += 'a' - 'A';
        if ((cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9')) {
            out[0] = (char)cp;
            return 1;
        }
        return 0;
    }
    if (cp == 0xAA) c = 'a';
    else if (cp == 0xBA) c = 'o';
    else if (cp >= 0xC0 && cp <= 0xFF) c = LAT1[cp - 0xC0];
    else if (cp >= 0x100 && cp <= 0x17F) c = LATA[cp - 0x100];
    else if (cp >= 0x300 && cp <= 0x36F) return -1;
    else return 0;

    if (c == '-' || c == 0) return 0;
    if (c == '*') {
        e = casos_especiais(cp);
        out[0] = e[0];
        out[1] = e[1];
        return 2;
    }
    out[0] = c;
    return 1;
}

/* Decodifica 1 caractere UTF-8 de s (s[0] != 0). *adv = bytes consumidos.
 * Sequencia invalida/overlong/truncada -> devolve 0xFFFFFFFF, adv = 1. */
static unsigned utf8_dec(const unsigned char *s, size_t *adv)
{
    unsigned c = s[0], cp;
    size_t n, i;

    *adv = 1;
    if (c < 0x80) return c;
    if (c >= 0xC2 && c <= 0xDF) { n = 2; cp = c & 0x1F; }
    else if (c >= 0xE0 && c <= 0xEF) { n = 3; cp = c & 0x0F; }
    else if (c >= 0xF0 && c <= 0xF4) { n = 4; cp = c & 0x07; }
    else return 0xFFFFFFFFu;

    for (i = 1; i < n; i++) {
        if ((s[i] & 0xC0) != 0x80) return 0xFFFFFFFFu; /* inclui '\0' */
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    if ((n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) || cp > 0x10FFFF)
        return 0xFFFFFFFFu;
    *adv = n;
    return cp;
}

char *slug(const char *s)
{
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    char *out = malloc(strlen((const char *)p) + 1);
    size_t o = 0, adv;
    int sep = 0, k, i;
    char buf[3];
    unsigned cp;

    if (!out) return NULL;
    /* Cada ponto de codigo gera no maximo 2 chars e, quando gera 2, ocupa
     * >= 2 bytes de entrada; o '_' separador so entra entre letras.
     * Logo a saida nunca passa de strlen(s) + 1 bytes. */
    while (*p) {
        cp = utf8_dec(p, &adv);
        p += adv;
        k = (cp == 0xFFFFFFFFu) ? 0 : dobra(cp, buf);
        if (k < 0) continue;
        if (k == 0) { sep = 1; continue; }
        if (sep && o > 0) out[o++] = '_';
        sep = 0;
        for (i = 0; i < k; i++) out[o++] = buf[i];
    }
    out[o] = '\0';
    return out;
}
