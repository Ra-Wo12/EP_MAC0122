#!/bin/sh
# run_tests -- bateria completa do projeto (A7). Rodar na RAIZ do repo:
#   sh tests/run_tests.sh
# Para no primeiro erro (set -e). Gera lixo so em build/ e out_test/ (no .gitignore).
set -eu

CF="-std=c99 -Wall -Wextra -pedantic -Werror -fsanitize=address,undefined"
B=build
rm -rf "$B" out_test out_test2 txt_neg
mkdir -p "$B"

echo "== 1. compilar (0 warnings; -Werror + ASan/UBSan) =="
gcc $CF -Iscr/A1 -Iscr/A2 -Iscr/A3 -o $B/gera \
    scr/A3/gera_main.c scr/A3/htmlgen.c scr/A1/txtparse.c scr/A1/slug.c scr/A2/cdestaque.c
gcc $CF -Iscr/A2 -o $B/cdestaque scr/A2/cdestaque.c scr/A2/cdestaque_main.c
gcc $CF -Iscr/A2 -o $B/cdestaque_test scr/A2/cdestaque_test_main.c scr/A2/cdestaque.c
gcc $CF -Iscr/A1 -o $B/txtparse_test scr/A1/txtparse_test.c scr/A1/txtparse.c scr/A1/slug.c
gcc $CF -o $B/aem_kruskal extra/aem_kruskal.c
gcc $CF -o $B/dijkstra extra/dijkstra.c

echo "== 2. testes unitarios (A1, A2) =="
$B/txtparse_test
$B/cdestaque_test
$B/cdestaque_test txt/*.txt | tail -1

echo "== 3. lint dos TXT + blocos C compilam e rodam =="
python3 tests/check_txt.py txt

echo "== 4. extra (A5): oraculos =="
$B/aem_kruskal extra/grafo_k1.txt | grep -q "Peso total da AEM: 12"
$B/aem_kruskal extra/grafo_k2.txt | grep -q "Peso total da AEM: 39"
$B/dijkstra -s 0 extra/grafo_k1.txt | grep -q "dist = \[0, 2, 5, 3, 7, 4, 5\]"
echo "kruskal k1=12, k2=39; dijkstra(k1,S=0)=[0,2,5,3,7,4,5]: ok"

echo "== 5. gerar o site duas vezes (determinismo) =="
$B/gera txt out_test
$B/gera txt out_test2
diff -r out_test out_test2
echo "saidas identicas"

echo "== 6. links, ancoras e HTML do site =="
sh tests/check_links.sh out_test txt

echo "== 7. [[slug]] sem destino tem que FALHAR o gerador (N1) =="
mkdir txt_neg
printf 'Algoritmo: Teste\n\nveja [[nao_existe_mesmo]]\n' > txt_neg/01_teste.txt
if $B/gera txt_neg out_neg 2>/dev/null; then
    echo "FALHOU: gera devolveu 0 com [[slug]] sem destino"
    exit 1
fi
rm -rf txt_neg out_neg
echo "gera devolveu erro, como esperado"

rm -rf out_test2
echo
echo "TUDO OK"
