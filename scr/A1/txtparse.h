/* txtparse.h -- A1 (parser). API CONGELADA pelo [CTX]: nao alterar. */
#ifndef TXTPARSE_H
#define TXTPARSE_H

typedef struct {
    char *arq, *slug, *titulo, *tecnica, *algoritmo, *data, *problema, *corpo;
    char **autores, **conceitos;
    int nAut, nConc;
} Doc;

/*
 * doc_le: le um TXT (formato [CTX]/[A4-OUT]) e preenche *d.
 * Retorno: 0 ok ; <0 erro (-1 argumento invalido, -2 nao abriu,
 *          -3 erro de leitura, -4 sem memoria). Em erro *d fica zerado
 *          (nada vazado; doc_libera continua seguro).
 *
 * Campos apos sucesso:
 *   arq        copia do path recebido                       (nunca NULL)
 *   slug       do nome do arquivo, sem pasta/.txt/"NN_"/"c_" (nunca NULL)
 *   titulo     Algoritmo | Problema | 1o conceito | 1a linha do corpo |
 *              nome do arquivo (primeiro que existir)       (nunca NULL)
 *   corpo      texto apos o cabecalho, LF, sem brancos nas pontas ("" se
 *              nao ha corpo)                                (nunca NULL)
 *   tecnica, algoritmo, data, problema
 *              texto do marcador; NULL se ausente ou vazio (= ignorar)
 *   autores, conceitos (+ nAut, nConc)
 *              multivalor por ',', trim, sem vazios, sem repetidos
 *              (compara pelo slug); NULL e n=0 se ausente ou vazio.
 */
int  doc_le(const char *path, Doc *d);
void doc_libera(Doc *d);   /* free de tudo + zera; aceita Doc zerado */

#endif
