/* htmlgen.h -- A3 (gerador + indexadores). API CONGELADA pelo [CTX]. */
#ifndef HTMLGEN_H
#define HTMLGEN_H

#include "txtparse.h"

/*
 * gera_site: escreve o site em dirOut a partir dos n documentos v[0..n-1]
 * (ja lidos com doc_le):
 *   index.html, idx_{tecnica,algoritmo,autores,data,conceitos,problema}.html,
 *   css/estilo.css, p/<slug>.html (1 por doc), c/<slug>.html (1 por conceito:
 *   os c_*.txt e os "stubs").
 * Cria dirOut, p/, c/ e css/ se nao existirem. Nao altera v.
 * Retorno: 0 ok ; != 0 erro, com mensagem em stderr
 *          (1 entrada invalida/colisao de slug, 2 erro de E/S, 3 memoria).
 * Saida deterministica: mesma entrada => mesmos bytes.
 */
int gera_site(Doc *v, int n, const char *dirOut);

#endif
