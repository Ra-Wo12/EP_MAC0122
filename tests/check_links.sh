#!/bin/sh
# check_links -- confere o site gerado (links, ancoras, HTML basico).
# uso (na raiz do repo):  sh tests/check_links.sh <dir_site> [<dir_txt>]
# Com <dir_txt>, confere tambem a contagem de paginas p/ (1 por TXT que nao e c_*.txt).
# Sai com 0 se tudo ok, 1 se achou problema, 2 se o uso estiver errado.
[ $# -ge 1 ] || { echo "uso: sh tests/check_links.sh <dir_site> [<dir_txt>]" >&2; exit 2; }
exec python3 "$(dirname "$0")/check_site.py" "$@"
