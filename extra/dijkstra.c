// MAC122 2026
// Algoritmo de Dijkstra (concebido em 1956, publicado em 1959) - distancia minima de uma origem a todos os vertices
//
// Uso:      dijkstra -s S [-d] arquivo     (S = vertice de origem ; -d = grafo DIRIGIDO; default: nao dirigido)
// Compilar: gcc -o dijkstra.o dijkstra.c
// Se ocorrer "segmentation fault" compilar com:
//   gcc -fsanitize=address -g -o dijkstra.o dijkstra.c
//
// Formato do arquivo (compartilhado com aem_kruskal.c):
//   linha 1: "V A" ; depois A linhas "u v w" (vertices 0..V-1, w inteiro >= 0) ; linhas iniciadas por '#' = comentario
//   No modo nao dirigido cada aresta vale nos dois sentidos; com -d vale so' de u para v.
//
// Pseudocodigo (enunciado):
//   dijkstra (Graph, dist[], prev[], source) {
//     for each v: dist[v] <- INF; prev[v] <- UNDEF; add v to Q
//     dist[source] <- 0
//     while (Q not empty) {
//       u <- vertice de Q com menor dist; remove u de Q
//       for each aresta (u,v) { alt <- dist[u] + Dist(u,v); if (alt < dist[v]) { dist[v] <- alt; prev[v] <- u } }
//     } }
//
// Ideia: guloso - o vertice de Q com menor dist ja' esta' com a distancia definitiva.
// Invariante: no inicio de cada iteracao, para todo v fora de Q, dist[v] e' a distancia minima de source a v
//   (vale porque w >= 0: um caminho que passe por Q so' pode ficar mais longo). Por isso w < 0 e' rejeitado.
// Complexidade: Q = vetor de flags => achar o minimo custa O(V); V vezes => O(V^2); relaxar todas as arestas O(A).
//   Total O(V^2 + A). (Com monte/fila de prioridade seria O((V + A) log V).)
//
// Representacao: INF = INT_MAX/2. Distancias >= INF contam como "inacessivel". A soma dist[u] + w e' feita
//   em long long, entao nao ha' overflow mesmo com w grande.

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INF (INT_MAX / 2)

typedef struct { int u, v, w; } Aresta;

// Grafo por listas de adjacencia em vetores: arcos e = 0..M-1 ; head[u] = 1o arco de u ; prox[e] = proximo arco (-1 = fim)
typedef struct { int V; int *head, *prox, *dest, *peso; } Grafo;


///
/// Leitura do grafo (trecho identico em aem_kruskal.c: cada programa compila sozinho)

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
/// Grafo: montagem a partir da lista de arestas

static void libera_grafo (Grafo *g) { free(g->head); free(g->prox); free(g->dest); free(g->peso); }

static void insere_arco (Grafo *g, int *m, int u, int v, int w) {
  g->dest[*m] = v; g->peso[*m] = w;
  g->prox[*m] = g->head[u]; g->head[u] = *m; // insere no inicio da lista de u
  (*m)++;
  }

// Devolve 0 se ok, -1 se faltar memoria. Pre-condicao: pesos ja' validados (>= 0).
static int monta_grafo (Grafo *g, const Aresta *ar, int V, int A, int dirigido) {
  size_t M = (size_t)A * (dirigido ? 1 : 2);
  int i, m = 0;
  g->V = V;
  g->head = malloc((size_t)V * sizeof *g->head);
  g->prox = malloc((M > 0 ? M : 1) * sizeof *g->prox);
  g->dest = malloc((M > 0 ? M : 1) * sizeof *g->dest);
  g->peso = malloc((M > 0 ? M : 1) * sizeof *g->peso);
  if (!g->head || !g->prox || !g->dest || !g->peso) { libera_grafo(g); return -1; }
  for (i = 0; i < V; i++) g->head[i] = -1;
  for (i = 0; i < A; i++) {
    insere_arco(g, &m, ar[i].u, ar[i].v, ar[i].w);
    if (!dirigido) insere_arco(g, &m, ar[i].v, ar[i].u, ar[i].w);
    }
  return 0;
  }


///
/// Dijkstra

// Calcula dist[0..V-1] e prev[0..V-1] (-1 = sem antecessor). Devolve 0 se ok, -1 se faltar memoria.
static int dijkstra (const Grafo *g, int dist[], int prev[], int source) {
  char *emQ = malloc((size_t)g->V); // emQ[v] == 1 <=> v ainda esta' em Q
  int v, u, e, restam = g->V;
  long long alt;
  if (emQ == NULL) return -1;
  for (v = 0; v < g->V; v++) { dist[v] = INF; prev[v] = -1; emQ[v] = 1; }
  dist[source] = 0;
  while (restam > 0) {
    u = -1; // argmin de dist em Q (empate: menor indice)
    for (v = 0; v < g->V; v++)
      if (emQ[v] && (u < 0 || dist[v] < dist[u])) u = v;
    if (dist[u] == INF) break; // o que sobrou em Q e' inacessivel
    emQ[u] = 0; restam--;
    for (e = g->head[u]; e != -1; e = g->prox[e]) { // para cada aresta (u,v)
      v = g->dest[e];
      alt = (long long)dist[u] + g->peso[e];
      if (alt < dist[v]) { dist[v] = (int)alt; prev[v] = u; }
      }
    }
  free(emQ);
  return 0;
  }

// Imprime o caminho source -> v seguindo prev[] de tras para frente (recursao inverte a ordem).
static void imprime_caminho (const int prev[], int v) {
  if (prev[v] != -1) { imprime_caminho(prev, prev[v]); printf(" -> "); }
  printf("%d", v);
  }


int main (int argc, char *argv[]) {
  const char *nome = NULL;
  int dirigido = 0, tem_s = 0, source = 0, V, A, i, status = 1;
  int *dist = NULL, *prev = NULL;
  char *fim;
  Aresta *ar;
  Grafo g;
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) dirigido = 1;
    else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc && !tem_s) {
      long s = strtol(argv[++i], &fim, 10);
      if (*fim != '\0' || fim == argv[i] || s < 0 || s > INT_MAX) { fprintf(stderr, "origem invalida: %s\n", argv[i]); return 2; }
      source = (int)s; tem_s = 1;
      }
    else if (argv[i][0] != '-' && nome == NULL) nome = argv[i];
    else { tem_s = 0; nome = NULL; break; }
    }
  if (!tem_s || nome == NULL) { fprintf(stderr, "uso: %s -s S [-d] arquivo\n", argv[0]); return 2; }
  if (le_grafo(nome, &V, &A, &ar) != 0) return 1;
  if (source >= V) { fprintf(stderr, "origem %d fora de [0, %d)\n", source, V); free(ar); return 1; }
  for (i = 0; i < A; i++)
    if (ar[i].w < 0) { fprintf(stderr, "%s: aresta %d (%d %d %d): peso negativo; Dijkstra exige w >= 0\n", nome, i, ar[i].u, ar[i].v, ar[i].w); free(ar); return 1; }
  if (monta_grafo(&g, ar, V, A, dirigido) != 0) { fprintf(stderr, "sem memoria\n"); free(ar); return 1; }
  free(ar);
  dist = malloc((size_t)V * sizeof *dist);
  prev = malloc((size_t)V * sizeof *prev);
  if (dist == NULL || prev == NULL || dijkstra(&g, dist, prev, source) != 0) fprintf(stderr, "sem memoria\n");
  else {
    printf("Dijkstra: origem = %d, grafo %s, V=%d, A=%d\n", source, dirigido ? "dirigido" : "nao dirigido", V, A);
    printf("  v    dist  caminho\n");
    for (i = 0; i < V; i++) {
      if (dist[i] == INF) printf("%3d     INF  (inacessivel)\n", i);
      else { printf("%3d %7d  ", i, dist[i]); imprime_caminho(prev, i); printf("\n"); }
      }
    printf("dist = [");
    for (i = 0; i < V; i++) {
      if (i > 0) printf(", ");
      if (dist[i] == INF) printf("INF"); else printf("%d", dist[i]);
      }
    printf("]\n");
    status = 0;
    }
  free(dist); free(prev); libera_grafo(&g);
  return status;
  }
