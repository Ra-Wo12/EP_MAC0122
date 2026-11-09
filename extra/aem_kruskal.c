// MAC122 2026
// Algoritmo de Kruskal (1956) - arvore espalhada minima (AEM)
// Refatoracao de algoritmo_kruskal_arv_esp_minima.c (semana 13): o grafo agora vem de arquivo.
//
// Uso:      aem_kruskal [-d] [arquivo]       (default: grafo_k1.txt ; -d = imprime depuracao)
// Compilar: gcc -o aem_kruskal.o aem_kruskal.c
// Se ocorrer "segmentation fault" compilar com:
//   gcc -fsanitize=address -g -o aem_kruskal.o aem_kruskal.c
//
// Formato do arquivo (compartilhado com dijkstra.c):
//   linha 1: "V A" ; depois A linhas "u v w" (vertices 0..V-1, w inteiro) ; linhas iniciadas por '#' = comentario
//
// Ideia: ordenar as arestas por peso (crescente) e examina-las uma vez cada; aceitar a aresta
//   se suas pontas estao em componentes diferentes (union-find), senao ela fecharia um circuito.
// Invariante: no inicio de cada iteracao, as arestas aceitas formam uma floresta contida em alguma AEM.
// Complexidade: qsort O(A log A) + ate 2A buscas e V-1 unioes, cada uma O(log V) (uniao por altura)
//   => O(A log A) = O(A log V), pois A <= V^2. A compressao de caminhos so melhora as buscas.
//
// Comparacao com a versao UGRAPHmstK1() de Feofiloff (IME-USP, "Algoritmo de Kruskal"):
//   https://users.ime.usp.br/~pf/algoritmos_para_grafos/aulas/kruskal.html
//   - mesma ideia e mesma ordem de grandeza: ordena (E log E) e usa union-find, total (E+V) log V.
//   - DIVERGENCIA 1: ele une por TAMANHO (sz[]) e cita compressao de caminhos so como melhoria possivel;
//     aqui: uniao por ALTURA (empate => menor indice vira raiz) + compressao de caminhos.
//   - DIVERGENCIA 2: o laco dele e' "for (i=0; k<V-1; ++i)" e supoe grafo conexo (num grafo desconexo
//     leria alem de e[]); aqui o laco tambem para em A e o grafo desconexo gera floresta + aviso.
//   - DIVERGENCIA 3: ele usa vetores estaticos (ch[1000], e[500000]) e monta e[] a partir de listas de
//     adjacencia (v < w); aqui V e A vem do arquivo e tudo e' malloc/free.

#include <limits.h>
#include <stdio.h>
#include <stdlib.h> // void qsort( void* ptr, size_t count, size_t size, int (*comp)(const void*, const void*) );
#include <string.h>

typedef struct { int u, v, w; } Aresta;


///
/// Leitura do grafo (trecho identico em dijkstra.c: cada programa compila sozinho)

// Proxima linha util (nao vazia e nao comentario). Devolve ponteiro para o 1o caractere util ou NULL no fim.
// buf[] e' sobrescrito; *nlin conta as linhas fisicas lidas (para mensagens de erro).
static char *proxima_linha (FILE *f, char *buf, int tam, int *nlin) {
  int c;
  char *p;
  while (fgets(buf, tam, f) != NULL) {
    (*nlin)++;
    if (strchr(buf, '\n') == NULL && !feof(f)) // linha maior que o buffer: descartar o resto dela
      while ((c = getc(f)) != '\n' && c != EOF) continue;
    p = buf + strspn(buf, " \t\r\n");
    if (*p != '\0' && *p != '#') return p;
    }
  return NULL;
  }

// Le o grafo de 'nome'. Devolve 0 se ok (*par aponta para *pA arestas alocadas) ou -1 se erro (mensagem em stderr).
static int le_grafo (const char *nome, int *pV, int *pA, Aresta **par) {
  FILE *f = fopen(nome, "r");
  char buf[256], *l;
  const char *erro;
  int V, A, i, nlin = 0;
  Aresta *ar = NULL;
  if (f == NULL) { perror(nome); return -1; }
  if ((l = proxima_linha(f, buf, (int)sizeof buf, &nlin)) == NULL || sscanf(l, "%d %d", &V, &A) != 2
      || V < 1 || A < 0 || A > INT_MAX / 2) {
    erro = "esperado \"V A\" (V >= 1, A >= 0)"; goto falha;
    }
  ar = malloc((A > 0 ? (size_t)A : 1) * sizeof *ar);
  if (ar == NULL) { erro = "sem memoria"; goto falha; }
  for (i = 0; i < A; i++) {
    if ((l = proxima_linha(f, buf, (int)sizeof buf, &nlin)) == NULL
        || sscanf(l, "%d %d %d", &ar[i].u, &ar[i].v, &ar[i].w) != 3) {
      erro = "esperado aresta \"u v w\""; goto falha;
      }
    if (ar[i].u < 0 || ar[i].u >= V || ar[i].v < 0 || ar[i].v >= V) {
      erro = "vertice fora de [0, V)"; goto falha;
      }
    }
  fclose(f);
  *pV = V; *pA = A; *par = ar;
  return 0;
 falha:
  fprintf(stderr, "%s:%d: %s\n", nome, nlin, erro);
  free(ar);
  fclose(f);
  return -1;
  }


///
/// Funcoes auxiliares de depuracao (so' com -d)

static void imprime_arestas (const Aresta *ar, int n) {
  int i;
  printf("Arestas ordenadas: %d\n", n);
  for (i = 0; i < n; i++) printf("%d-%d (%d)\n", ar[i].u, ar[i].v, ar[i].w);
  }

static void imprime_vertices_alturas (const int pai[], const int alt[], int n) {
  int i;
  printf(" + pai[]   = { "); for (i = 0; i < n; i++) printf("%3d, ", pai[i]); printf("}\n");
  printf(" + altura[] = { "); for (i = 0; i < n; i++) printf("%3d, ", alt[i]); printf("}\n");
  }


///
/// Union-Find (cada conjunto = componente conexa da floresta); pai[i] == -1 => i e' raiz

// Encontrar o representante do conjunto, reduzindo o caminho no processo.
// Devolve -1 se detectar laco (nao deve acontecer).
static int busca_pai (int pai[], int i) {
  int i0 = i, j;
  while ((j = pai[i]) != -1) { // enquanto NAO raiz - CUIDADO: NAO usar "j = pai[i] != -1" (j receberia 0 ou 1)
    if (pai[j] != -1) // ainda tem elemento acima
      pai[i] = pai[j]; // compressao de caminho
    if (pai[i] == i) { fprintf(stderr, "[busca_pai(%d)] ERRO! pai[%d]==%d: laco!\n", i0, i, i); return -1; }
    i = j; // subir na arvore (o ultimo no' e' o representante)
    }
  return i;
  }


///
/// Algoritmo de Kruskal

// Usado por qsort: negativo se a < b ; positivo se a > b ; 0 se empate (por peso)
static int compare_arestas (const void *a, const void *b) {
  const Aresta *ea = a, *eb = b;
  return (ea->w > eb->w) - (ea->w < eb->w);
  }

// Devolve 0 se ok (peso total em *peso) ou -1 se erro interno. Reordena ar[].
// pai[0..V-1] deve estar todo em -1 e alt[0..V-1] todo em 0.
static int kruskal_aem (Aresta *ar, int pai[], int alt[], int A, int V, int debug, long long *peso) {
  int i, rx, ry, usadas = 0;
  *peso = 0;
  // Passo 1: ordenar as arestas pelo peso (crescente)
  qsort(ar, (size_t)A, sizeof *ar, compare_arestas);
  if (debug) imprime_arestas(ar, A);
  puts("Arestas na arvore espalhada minima:");
  // Passo 2: examinar as arestas em ordem crescente, pulando as que formam ciclo
  for (i = 0; i < A && usadas < V - 1; i++) {
    rx = busca_pai(pai, ar[i].u);
    ry = busca_pai(pai, ar[i].v);
    if (rx < 0 || ry < 0 || rx >= V || ry >= V) { fprintf(stderr, "Erro: rx=%d, ry=%d fora de [0,%d)\n", rx, ry, V); return -1; }
    if (rx != ry) { // estao em componentes distintas
      printf("  %d -- %d  (peso %d)\n", ar[i].u, ar[i].v, ar[i].w);
      *peso += ar[i].w;
      usadas++;
      // Uniao das componentes rx e ry (arvore de maior altura recebe a menor)
      if (alt[rx] < alt[ry]) pai[rx] = ry;
      else if (alt[rx] > alt[ry]) pai[ry] = rx;
      else { // empatou: pendure sob a raiz de menor indice
        if (rx < ry) { pai[ry] = rx; alt[rx]++; }
        else { pai[rx] = ry; alt[ry]++; }
        }
      }
    }
  if (debug) imprime_vertices_alturas(pai, alt, V);
  if (usadas != V - 1)
    printf("Atencao: o grafo NAO e' conexo (%d componentes): o resultado e' uma floresta geradora minima, nao uma arvore.\n", V - usadas);
  printf("Peso total da AEM: %lld\n", *peso);
  return 0;
  }


int main (int argc, char *argv[]) {
  const char *nome = NULL;
  int debug = 0, V, A, i, status = 1;
  int *pai, *alt;
  Aresta *ar;
  long long peso;
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) debug = 1;
    else if (argv[i][0] != '-' && nome == NULL) nome = argv[i];
    else { fprintf(stderr, "uso: %s [-d] [arquivo]\n", argv[0]); return 2; }
    }
  if (nome == NULL) nome = "grafo_k1.txt";
  if (le_grafo(nome, &V, &A, &ar) != 0) return 1;
  pai = malloc((size_t)V * sizeof *pai);
  alt = malloc((size_t)V * sizeof *alt);
  if (pai == NULL || alt == NULL) fprintf(stderr, "sem memoria\n");
  else {
    for (i = 0; i < V; i++) { pai[i] = -1; alt[i] = 0; } // cada vertice e' raiz de si mesmo
    printf("Total de vertices e arestas: %d, %d\n", V, A);
    status = (kruskal_aem(ar, pai, alt, A, V, debug, &peso) == 0) ? 0 : 1;
    }
  free(pai); free(alt); free(ar);
  return status;
  }
