/* cdestaque.h -- filtro C -> HTML com destaque de sintaxe (A2).
 *
 * API CONGELADA (00_CTX): nao mudar assinaturas.
 *
 *   c2html     emite  <pre class="c"><code>...</code></pre>  (sem '\n' final).
 *              Invariante: remover as tags e desfazer o escape (&amp; &lt;
 *              &gt; &quot;) devolve a entrada byte a byte.
 *   c_pagina   pagina HTML5 completa (lang=pt-BR, CSS inline).
 *   cdestaque_css  unica fonte da paleta; A3 a usa em css/estilo.css.
 *
 * Classes de span: k-pp k-tipo k-cmd k-com k-str k-chr k-num.
 */
#ifndef CDESTAQUE_H
#define CDESTAQUE_H

#include <stddef.h>
#include <stdio.h>

void c2html(const char *src, size_t n, FILE *o);
void c_pagina(const char *src, size_t n, FILE *o, const char *titulo);
const char *cdestaque_css(void);

#endif /* CDESTAQUE_H */
