# EP_MAC0122
Repositório do EP de MAC0122

Enunciado:

O objetivo principal deste EP é implementar um sistema em C que gera páginas HTML de um curso de MAC122, baseado no material estudado nesta edição do MAC122.

As entradas para o sistema é um conjunto de páginas TXT com alguns rótulos marcadores no início de cada arquivo (abaixo explicado).
As saídas do sistema será um conjunto de páginas HTML, com um "index.html" com apontadores para outros 6 páginas indexadoras:

Tecnica: método guloso, divisão-e-conquista etc
Algoritmo: ordenador seleção direta, ... ordenador por fusão, Kruskal (árvore espalhada mínima), ...
Autores: Joseph Kruskal (árvore espalhada mínima), Edsger W. Dijkstra (distância mínima), ...
Data: 1956: Kruskal (árvore espalhada mínima), 1956: Dijkstra (distância mínima), ...
Conceitos: Grafos, Árvores, recursividade, árvore espalhada mínima...
Problema: Dado ....
Um dos módulos obrigatórios é um filtro que processa código C devolvendo um HTML correspondente, com o código colorido, separando por cores os tipos de itens léxicos, como:

pragma (#include, #define, ...);
tipos de variáveis (int, float, ...);
comandos (if, for, ...),
comentários (// coment1 /* coment2 */),
cadeia de caracteres ("texto\n").
Usar sistema de cores sugeridos pela WikiPedia.
LINK: https://en.wikipedia.org/wiki/Syntax_highlighting

Os arquivos TXT devem ser preparandos usando nosso material didático e complementados com outros (preferencialmente livros, as referências devem ser apresentadas).
No início do arquivo TXT podem existir os marcadores (se vazio, deve ser ignorado), veja um exemplo para o TXT sobre o algoritmo de Kruskal:

Tecnica: Método guloso
Algoritmo: Kruskal para árvore espalhada mínima
Autores: Joseph Kruskal
Data: 1956
Conceitos: Grafos, Árvores, árvore espalhada mínima, union-find
Problema: Dado um grafo não dirigido com pesos nas arestas, determinar sua árvore espalhada mínima (ligar com a página explicativa dos conceitos de "grafo", "árvore" e "árvore espalhada mínima").
O que entregar.

na área de texto digitar (antes de 29/10 os NUSP e nomes do 4 membro do grupo);
um arquivo explicando o que cada membro do grupo fez (1 página);
um arquivo explicando como foram os diálogos com os mecanimos de IA;
um arquivo explicando as maiores dificuldades encontradas em cada IA usada (iniciar apontando quais foram usadas e quais foram melhores/piores);
um arquivo explicando como compilar cada código C usado (e.g. "para o Kruskal, usar 'aem_kruskal.c' e 'grafo_k1.txt' em um diretório e compilar com "gcc -o aem_kruskal.o aem_kruskal.c");
um arquivo compactado com os fontes TXT (conteúdo do MAC122 usados para processar - na fase 1, todo o conteúdo da avaliação 1);
um arquivo compactado com os fontes C (todos os código C que devem funcionar) e arquivos auxiliares (e.g. TXT usado para criar um grafo a ser processado pelo Kruskal.

Organização do Repositório, de acordo com as atribuições de cada IA
repo/
├─ txt/            00_… c_*.txt … 18_dijkstra.txt   ← fonte do conteúdo (A4)
├─ src/            slug, txtparse, cdestaque, htmlgen, gera_main   (A1-A3)
├─ extra/          aem_kruskal.c, dijkstra.c, grafo_k1.txt, grafo_k2.txt   (A5)
├─ aula/           .c e .txt de apoio dos zips
├─ entrega/        membros, diálogos, dificuldades, como_compilar   (A6)
├─ tests/          check_links, roundtrip   (A7)
├─ .github/workflows/site.yml
└─ .gitignore      gera  *.o  site/
