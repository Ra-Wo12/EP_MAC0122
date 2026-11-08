/* txtparse_test.c -- A1 (parser): testes de slug() e doc_le().
 *
 *   sem argumentos : roda os testes automaticos (cria/apaga _tp_tmp.txt)
 *   com arquivos   : ./txtparse_test arq1.txt arq2.txt ...  (no shell, passe
 *                    todos os .txt da pasta txt/) -> le cada um com doc_le,
 *                    imprime um resumo e sai com 1 se algum falhar.
 *
 * gcc -std=c99 -Wall -Wextra -fsanitize=address,undefined \
 *     -o txtparse_test txtparse_test.c txtparse.c slug.c
 */
#include "slug.h"
#include "txtparse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TMP "_tp_tmp.txt"

static int falhas = 0, total = 0;

#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        total++;                                                            \
        if (!(cond)) {                                                      \
            falhas++;                                                       \
            printf("FALHOU [%s:%d] %s\n", __FILE__, __LINE__, msg);         \
        }                                                                   \
    } while (0)

static int igual(const char *a, const char *b)
{
    if (!a || !b) return a == b;
    return strcmp(a, b) == 0;
}

static void grava(const void *dados, size_t n)
{
    FILE *f = fopen(TMP, "wb");
    if (!f) { perror(TMP); exit(2); }
    fwrite(dados, 1, n, f);
    fclose(f);
}

static void grava_s(const char *s) { grava(s, strlen(s)); }

static void t_slug(const char *in, const char *esperado)
{
    char *s = slug(in);
    total++;
    if (!s || strcmp(s, esperado) != 0) {
        falhas++;
        printf("FALHOU slug(\"%s\") = \"%s\" (esperado \"%s\")\n",
               in ? in : "(null)", s ? s : "(null)", esperado);
    }
    free(s);
}

static void testes_slug(void)
{
    t_slug("Kruskal para árvore espalhada mínima",
           "kruskal_para_arvore_espalhada_minima");
    t_slug("union-find", "union_find");
    t_slug("  Ça va?!  ", "ca_va");
    t_slug("Técnica", "tecnica");
    t_slug("ÁÉÍÓÚ ÀÈÌÒÙ ÂÊÎÔÛ ÃÕ ÄËÏÖÜ Ç Ñ", "aeiou_aeiou_aeiou_ao_aeiou_c_n");
    t_slug("áéíóú àèìòù âêîôû ãõ äëïöü ç ñ", "aeiou_aeiou_aeiou_ao_aeiou_c_n");
    t_slug("Straße Æther Œuvre Łódź Đ", "strasse_aether_oeuvre_lodz_d");
    t_slug("a\xCC\x81rvore", "arvore");              /* NFD: a + acento */
    t_slug("O(n²) vs O(n log n)", "o_n_vs_o_n_log_n");
    t_slug("__a___b__", "a_b");
    t_slug("--- !!! ---", "");
    t_slug("", "");
    t_slug(NULL, "");
    t_slug("abc\xFF" "def", "abc_def");               /* byte invalido */
    t_slug("abc\xC3", "abc");                          /* truncado no fim */
    t_slug("x\xE2\x8C\x8A y", "x_y");                  /* simbolo fora do latim */
    t_slug("ordena_fusao", "ordena_fusao");
    t_slug("Passagem por referência", "passagem_por_referencia");
    t_slug("c. 300 a.C.", "c_300_a_c");
}

static void testes_doc(void)
{
    Doc d;
    char *big;
    size_t i;

    /* 1. EXEMPLO-OURO do enunciado (nConc=4, nAut=1) */
    grava_s("Tecnica: Método guloso\n"
            "Algoritmo: Kruskal para árvore espalhada mínima\n"
            "Autores: Joseph Kruskal\n"
            "Data: 1956\n"
            "Conceitos: Grafos, Árvores, árvore espalhada mínima, union-find\n"
            "Problema: Dado um grafo não dirigido com pesos nas arestas, "
            "determinar sua árvore espalhada mínima\n"
            "\n"
            "Primeiro parágrafo.\n\nSegundo.\n");
    CHECK(doc_le(TMP, &d) == 0, "ouro: doc_le");
    CHECK(d.nConc == 4 && d.nAut == 1, "ouro: nConc=4 nAut=1");
    CHECK(igual(d.tecnica, "Método guloso"), "ouro: tecnica");
    CHECK(igual(d.algoritmo, "Kruskal para árvore espalhada mínima"),
          "ouro: algoritmo");
    CHECK(igual(d.autores[0], "Joseph Kruskal"), "ouro: autor");
    CHECK(igual(d.data, "1956"), "ouro: data");
    CHECK(igual(d.conceitos[0], "Grafos") && igual(d.conceitos[1], "Árvores") &&
          igual(d.conceitos[2], "árvore espalhada mínima") &&
          igual(d.conceitos[3], "union-find"), "ouro: conceitos");
    CHECK(d.problema && strncmp(d.problema, "Dado um grafo", 13) == 0,
          "ouro: problema");
    CHECK(igual(d.titulo, d.algoritmo), "ouro: titulo = algoritmo");
    CHECK(igual(d.corpo, "Primeiro parágrafo.\n\nSegundo."), "ouro: corpo");
    CHECK(igual(d.slug, "tp_tmp"), "ouro: slug do arquivo");
    doc_libera(&d);
    CHECK(d.corpo == NULL && d.nConc == 0, "libera zera");
    doc_libera(&d);                                   /* 2x e seguro */

    /* 2. marcador ausente: tudo e corpo; titulo = 1a linha */
    grava_s("Introdução à busca\nsegunda linha\n");
    CHECK(doc_le(TMP, &d) == 0, "sem marcador");
    CHECK(!d.tecnica && !d.algoritmo && !d.autores && d.nAut == 0 &&
          d.nConc == 0, "sem marcador: campos NULL");
    CHECK(igual(d.corpo, "Introdução à busca\nsegunda linha"),
          "sem marcador: corpo");
    CHECK(igual(d.titulo, "Introdução à busca"), "sem marcador: titulo");
    doc_libera(&d);

    /* 3. marcador vazio e ignorado (qualquer ordem, qualquer chave) */
    grava_s("Tecnica:\nAlgoritmo:   \nAutores:\nData:\nConceitos: , ,\n"
            "Problema: P1\n\ncorpo\n");
    CHECK(doc_le(TMP, &d) == 0, "marcador vazio");
    CHECK(!d.tecnica && !d.algoritmo && !d.data && !d.autores &&
          !d.conceitos && d.nAut == 0 && d.nConc == 0,
          "marcador vazio: ignorados");
    CHECK(igual(d.problema, "P1") && igual(d.titulo, "P1") &&
          igual(d.corpo, "corpo"), "marcador vazio: resto ok");
    doc_libera(&d);

    /* 4. 'Problema: a: b' corta so no 1o ':' */
    grava_s("Problema: a: b: c\n\nx\n");
    CHECK(doc_le(TMP, &d) == 0 && igual(d.problema, "a: b: c"),
          "problema com ':' no valor");
    doc_libera(&d);

    /* 5. CRLF */
    grava_s("Tecnica: T\r\nAlgoritmo: A\r\n\r\nlinha1\r\nlinha2\r\n");
    CHECK(doc_le(TMP, &d) == 0, "crlf");
    CHECK(igual(d.tecnica, "T") && igual(d.algoritmo, "A") &&
          igual(d.corpo, "linha1\nlinha2"), "crlf: valores sem \\r");
    doc_libera(&d);

    /* 6. BOM */
    grava_s("\xEF\xBB\xBF" "Tecnica: T\nAlgoritmo: A\n\ncorpo\n");
    CHECK(doc_le(TMP, &d) == 0 && igual(d.tecnica, "T") &&
          igual(d.algoritmo, "A"), "bom: 1a chave reconhecida");
    doc_libera(&d);

    /* 7. sem corpo (cabecalho + linha vazia) e 8. so cabecalho (sem \n) */
    grava_s("Algoritmo: A\n\n");
    CHECK(doc_le(TMP, &d) == 0 && igual(d.corpo, "") && igual(d.titulo, "A"),
          "sem corpo");
    doc_libera(&d);
    grava_s("Algoritmo: A\nData: 1956");
    CHECK(doc_le(TMP, &d) == 0 && igual(d.corpo, "") && igual(d.data, "1956"),
          "so cabecalho, sem \\n final");
    doc_libera(&d);

    /* 9. chave com acento / caixa diferente / espacos */
    grava_s("Técnica: Guloso\nALGORITMO :  Alg X \ndata: 1956\n\nc\n");
    CHECK(doc_le(TMP, &d) == 0, "chaves variadas");
    CHECK(igual(d.tecnica, "Guloso") && igual(d.algoritmo, "Alg X") &&
          igual(d.data, "1956"), "chaves variadas: valores");
    doc_libera(&d);

    /* 10. arquivo vazio / so brancos */
    grava("", 0);
    CHECK(doc_le(TMP, &d) == 0 && igual(d.corpo, "") && !d.algoritmo &&
          igual(d.titulo, "_tp_tmp"), "arquivo vazio");
    doc_libera(&d);
    grava_s("  \n\n\t\n");
    CHECK(doc_le(TMP, &d) == 0 && igual(d.corpo, ""), "so brancos");
    doc_libera(&d);

    /* 11. linhas de 100 KB (valor de cabecalho e linha de corpo) */
    big = malloc(100001);
    CHECK(big != NULL, "malloc teste");
    if (big) {
        FILE *f;
        for (i = 0; i < 100000; i++) big[i] = (char)('a' + i % 26);
        big[100000] = '\0';
        f = fopen(TMP, "wb");
        fprintf(f, "Problema: %s\n\n%s\n", big, big);
        fclose(f);
        CHECK(doc_le(TMP, &d) == 0 && d.problema && strlen(d.problema) == 100000
              && strlen(d.corpo) == 100000, "linha de 100KB");
        doc_libera(&d);
        /* 100KB sem nenhum ':' na 1a linha: e corpo */
        f = fopen(TMP, "wb");
        fprintf(f, "%s\n", big);
        fclose(f);
        CHECK(doc_le(TMP, &d) == 0 && !d.problema && strlen(d.corpo) == 100000,
              "100KB sem chave => corpo");
        doc_libera(&d);
        free(big);
    }

    /* 12. arquivo inexistente / argumentos invalidos */
    CHECK(doc_le("nao_existe_xyz.txt", &d) < 0 && d.arq == NULL &&
          d.corpo == NULL, "inexistente: <0 e Doc zerado");
    CHECK(doc_le(NULL, &d) < 0, "path NULL");
    CHECK(doc_le(TMP, NULL) < 0, "Doc NULL");

    /* 13. dedupe sem diferenciar caixa/acento; vazios descartados */
    grava_s("Conceitos: Grafos, grafos, , Árvores, ARVORES, Árvore, grafos \n"
            "Autores: A. Um,A. UM , B. Dois,\n\nx\n");
    CHECK(doc_le(TMP, &d) == 0, "dedupe");
    CHECK(d.nConc == 3 && igual(d.conceitos[0], "Grafos") &&
          igual(d.conceitos[1], "Árvores") && igual(d.conceitos[2], "Árvore"),
          "dedupe: conceitos (1a ocorrencia vence)");
    CHECK(d.nAut == 2 && igual(d.autores[0], "A. Um") &&
          igual(d.autores[1], "B. Dois"), "dedupe: autores");
    doc_libera(&d);

    /* 14. conceito (c_*.txt): titulo = conceito; slug sem "c_" */
    {
        FILE *f = fopen("c_arvore_espalhada_minima.txt", "wb");
        fputs("Tecnica:\nAlgoritmo:\nAutores:\nData:\n"
              "Conceitos: árvore espalhada mínima\nProblema:\n\nTexto.\n", f);
        fclose(f);
        CHECK(doc_le("c_arvore_espalhada_minima.txt", &d) == 0 &&
              igual(d.titulo, "árvore espalhada mínima") &&
              igual(d.slug, "arvore_espalhada_minima") && d.nConc == 1,
              "conceito c_*");
        doc_libera(&d);
        remove("c_arvore_espalhada_minima.txt");
    }

    /* 15. slug do algoritmo: sem pasta, sem NN_ e sem .txt */
    {
        FILE *f = fopen("17_kruskal.txt", "wb");
        fputs("Algoritmo: Kruskal\n\n", f);
        fclose(f);
        CHECK(doc_le("./17_kruskal.txt", &d) == 0 && igual(d.slug, "kruskal") &&
              igual(d.arq, "./17_kruskal.txt"), "slug 17_kruskal");
        doc_libera(&d);
        remove("17_kruskal.txt");
    }

    /* 16. 1a linha nao-chave comeca o corpo (e nao some); corpo preserva
     *     indentacao, <<C ... C>> e [[s]] byte a byte */
    grava_s("Algoritmo: A\n\nNota: isto e corpo\n"
            "  <<C\n#include <stdio.h>\nint main(void){ return 0; }\nC>>\n"
            "    indentado  \n- item\nREF: livro [[pilha]]\n\n\n");
    CHECK(doc_le(TMP, &d) == 0, "corpo literal");
    CHECK(igual(d.corpo, "Nota: isto e corpo\n"
                         "  <<C\n#include <stdio.h>\nint main(void){ return 0; }\nC>>\n"
                         "    indentado  \n- item\nREF: livro [[pilha]]"),
          "corpo preservado (so apara o fim)");
    doc_libera(&d);

    /* 17. cabecalho termina em linha sem chave valida (sem linha vazia) */
    grava_s("Algoritmo: A\nUm paragrafo logo apos\nOutro\n");
    CHECK(doc_le(TMP, &d) == 0 && igual(d.algoritmo, "A") &&
          igual(d.corpo, "Um paragrafo logo apos\nOutro"),
          "cabecalho sem linha vazia");
    doc_libera(&d);

    /* 18. chave repetida: ultimo valor nao vazio vence; vazio nao apaga */
    grava_s("Data: 1\nData: 2\nData:\n\n");
    CHECK(doc_le(TMP, &d) == 0 && igual(d.data, "2"), "chave repetida");
    doc_libera(&d);

    /* 19. bytes NUL e UTF-8 invalido no meio do arquivo nao quebram */
    grava("Algoritmo: A\0B\n\nco\xFF" "rpo\0\n", 22);
    CHECK(doc_le(TMP, &d) == 0 && d.algoritmo && d.corpo, "NUL / UTF-8 ruim");
    doc_libera(&d);

    remove(TMP);
}

static int resumo(int argc, char **argv)
{
    int i, ruins = 0;
    for (i = 1; i < argc; i++) {
        Doc d;
        int rc = doc_le(argv[i], &d);
        if (rc < 0) {
            printf("ERRO %d  %s\n", rc, argv[i]);
            ruins++;
            continue;
        }
        printf("%-40s slug=%-28s aut=%d conc=%d corpo=%luB  \"%s\"\n",
               argv[i], d.slug, d.nAut, d.nConc,
               (unsigned long)strlen(d.corpo), d.titulo);
        doc_libera(&d);
    }
    printf("%d arquivo(s), %d erro(s)\n", argc - 1, ruins);
    return ruins ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc > 1) return resumo(argc, argv);
    testes_slug();
    testes_doc();
    printf("%d/%d verificacoes OK\n", total - falhas, total);
    return falhas ? 1 : 0;
}
