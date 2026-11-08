/* slug.h -- A1 (parser). Unica implementacao de slug do projeto (CTX). */
#ifndef SLUG_H
#define SLUG_H

/*
 * slug(s): devolve NOVA string (malloc; quem chama faz free) com:
 *   - minusculas ASCII [a-z0-9] ;
 *   - acentos removidos (UTF-8 decodificado "na mao": a->a, c->c, ...;
 *     tambem aceita texto ja decomposto/NFD: letra + marca combinante);
 *   - qualquer outro caractere (espaco, hifen, pontuacao, simbolos,
 *     bytes UTF-8 invalidos) vira separador '_' ;
 *   - sem '_' repetido e sem '_' na borda.
 * Exemplos: "Arvore espalhada minima" -> "arvore_espalhada_minima"
 *           "union-find" -> "union_find" ; "  Ca va?! " -> "ca_va".
 * s == NULL ou sem nenhum caractere aproveitavel -> "" (string vazia,
 * tambem alocada). Devolve NULL SOMENTE se faltar memoria.
 */
char *slug(const char *s);

#endif
