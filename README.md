# EP_MAC0122
Repositório do EP de MAC0122

Caso sua IA peça para deixar instruções para as outras IAs, mude o README e avise no grupo. Depois vamos conectar alguma IA para organizar tudo.

IMPORTANTE!
 Ao realizar o zip referente a sua etapa, APAGAR o arquivo com nome referente, pois este é apenas um arquivo vazio. Colar a pasta que contém os conteúdos do seu A[i} por meio de "add files via upload", com nome IDENTICO ao arquivo vazio que foi apagado. Deve ficar igual ao txt do A[4]. Qualquer coisa, perguntar à sua IA como colocar corretamente seu arquivo no github, com um print desta tela, com o README e os arquivos ao lado, isso deve bastar.
 
Instrução para a criação e desenvolvimentos:
 A IA (Claude) gerou 7 blocos independentes, que podem ser trabalhados individualmente. PORÉM, para cada bloco, colar no prompt também o [CTX] e o bloco da própria etapa (A3 também leva o A3_ANEXO.txt), que contextualiza a IA para o projeto e sua realização.
 Quando for desenvolver, analisar a estrutura definida ao final deste README. Resumidamente, sabendo o arquivo que for trabalhar, colocar "/" após o arquivo raiz, por exemplo, se for desenvolver para o "scr", colocar no nome do arquivo uma "/" após ele, verá que ficou com mesma estrutura de uma pasta, colocar nelas arquivos separados, .txt, .c e etc. Qualquer dúvida, perguntar para mim (Rafael) que posso ou responder, ou perguntar para o Claude o que ele planejou. Lembrem de anotar resumos das conversas e dificuldades com as IAs para o relatório.

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
