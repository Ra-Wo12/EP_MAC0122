#!/usr/bin/env python3
"""Confere o site gerado por ./gera (chamado por tests/check_links.sh).

uso: python3 tests/check_site.py <dir_site> [<dir_txt>]

Confere: arquivos obrigatorios; todo href/src interno aponta para arquivo
existente (e #ancora existente); <!DOCTYPE html>, lang="pt-BR", meta charset;
tags balanceadas; nenhum <li> vazio; nenhum "[[", "<<C" ou "C>>" no texto
(fora de <pre>); com <dir_txt>: numero de p/ = numero de TXT que nao sao c_*.txt.
"""
import os
import re
import sys
from html.parser import HTMLParser

VOID = {"meta", "link", "br", "hr", "img", "input", "area", "base", "col",
        "embed", "source", "track", "wbr"}
INDICES = ["tecnica", "algoritmo", "autores", "data", "conceitos", "problema"]


class Pagina(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=False)
        self.links, self.ids, self.stack, self.erros = [], set(), [], []
        self.texto, self.li_vazio, self._li, self._pre = [], 0, [], 0

    def handle_starttag(self, tag, attrs):
        d = dict(attrs)
        for k in ("href", "src"):
            if k in d:
                self.links.append(d[k])
        if "id" in d:
            self.ids.add(d["id"])
        if tag in VOID:
            return
        self.stack.append(tag)
        if tag == "li":
            self._li.append(False)
        if tag == "pre":
            self._pre += 1

    def handle_startendtag(self, tag, attrs):
        self.handle_starttag(tag, attrs)

    def handle_endtag(self, tag):
        if tag in VOID:
            return
        if not self.stack or self.stack[-1] != tag:
            self.erros.append("tag </%s> sem par (pilha: %s)" % (tag, self.stack[-3:]))
            if tag not in self.stack:
                return
            while self.stack[-1] != tag:
                self.stack.pop()
        self.stack.pop()
        if tag == "li" and self._li and not self._li.pop():
            self.li_vazio += 1
        if tag == "pre":
            self._pre -= 1

    def _marca(self):
        if self._li:
            self._li[-1] = True

    def handle_data(self, data):
        if data.strip():
            self._marca()
            if not self._pre:
                self.texto.append(data)

    def handle_entityref(self, nome):
        self._marca()

    def handle_charref(self, nome):
        self._marca()


def main():
    if len(sys.argv) < 2:
        print("uso: check_site.py <dir_site> [<dir_txt>]", file=sys.stderr)
        return 2
    raiz = sys.argv[1]
    problemas = []

    def erro(arq, msg):
        problemas.append("%s: %s" % (arq, msg))

    if not os.path.isdir(raiz):
        print("PROBLEMAS: pasta '%s' nao existe" % raiz)
        return 1
    for f in ["index.html", "css/estilo.css"] + ["idx_%s.html" % i for i in INDICES]:
        if not os.path.isfile(os.path.join(raiz, f)):
            erro(f, "arquivo obrigatorio ausente")

    paginas = {}
    for pasta, _, arqs in os.walk(raiz):
        for a in arqs:
            if a.endswith(".html"):
                caminho = os.path.join(pasta, a)
                with open(caminho, "rb") as fh:
                    try:
                        s = fh.read().decode("utf-8")
                    except UnicodeDecodeError:
                        erro(os.path.relpath(caminho, raiz), "nao e UTF-8")
                        continue
                p = Pagina()
                p.feed(s)
                paginas[caminho] = (p, s)

    n_links = 0
    for caminho, (p, s) in sorted(paginas.items()):
        rel = os.path.relpath(caminho, raiz)
        if not s.startswith("<!DOCTYPE html>"):
            erro(rel, "nao comeca com <!DOCTYPE html>")
        if '<html lang="pt-BR"' not in s:
            erro(rel, 'sem <html lang="pt-BR">')
        if not re.search(r'<meta charset="utf-8"', s, re.I):
            erro(rel, "sem <meta charset=utf-8>")
        if p.stack:
            erro(rel, "tags nao fechadas: %s" % p.stack)
        for e in p.erros[:3]:
            erro(rel, e)
        if p.li_vazio:
            erro(rel, "%d <li> vazio(s)" % p.li_vazio)
        texto = "".join(p.texto)
        for marca in ("[[", "<<C", "C>>"):
            if marca in texto:
                erro(rel, "sobrou %r no texto" % marca)
        for u in p.links:
            if re.match(r"^(https?:|mailto:)", u, re.I):
                continue
            n_links += 1
            if u.startswith("/") or re.match(r"^[a-z][a-z0-9+.-]*:", u, re.I):
                erro(rel, "link absoluto/esquema: %s" % u)
                continue
            alvo_rel, _, frag = u.partition("#")
            alvo = os.path.normpath(os.path.join(os.path.dirname(caminho), alvo_rel)) \
                if alvo_rel else caminho
            if not os.path.isfile(alvo):
                erro(rel, "LINK QUEBRADO: %s" % u)
            elif frag and alvo.endswith(".html") and alvo in paginas \
                    and frag not in paginas[alvo][0].ids:
                erro(rel, "ANCORA INEXISTENTE: %s" % u)

    if len(sys.argv) > 2:
        txts = [f for f in os.listdir(sys.argv[2]) if f.endswith(".txt")]
        esperado = len([f for f in txts if not f.startswith("c_")])
        pdir = os.path.join(raiz, "p")
        obtido = len(os.listdir(pdir)) if os.path.isdir(pdir) else 0
        if esperado != obtido:
            erro("p/", "%d paginas, esperado %d (1 por TXT que nao e c_*)" % (obtido, esperado))

    print("%d paginas HTML, %d links internos conferidos" % (len(paginas), n_links))
    for m in problemas:
        print("  " + m)
    print("PROBLEMAS: %d" % len(problemas))
    return 1 if problemas else 0


if __name__ == "__main__":
    sys.exit(main())
