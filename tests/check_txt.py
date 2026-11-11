#!/usr/bin/env python3
"""Lint dos TXT e teste dos blocos C (<<C ... C>>) de txt/.

uso (na raiz do repo): python3 tests/check_txt.py [<dir_txt>]      (padrao: txt)

Lint: UTF-8, LF, sem BOM, \\n final; cabecalho = 6 chaves na ordem
(Tecnica Algoritmo Autores Data Conceitos Problema) e linha 7 vazia; >=1 linha
REF:; <<C e C>> balanceados; sem "(verif.)"; sem virgula dentro de parenteses
em Autores/Conceitos.
Blocos: com main() => compila (-std=c99 -Wall -Wextra -pedantic, ASan/UBSan,
0 warnings) e executa (rc=0, sem saida de sanitizer); sem main() => so sintaxe.
"""
import glob
import os
import re
import subprocess
import sys
import tempfile

CHAVES = ["Tecnica", "Algoritmo", "Autores", "Data", "Conceitos", "Problema"]
FLAGS = ["-std=c99", "-Wall", "-Wextra", "-pedantic",
         "-fsanitize=address,undefined"]


def lint(nome, bruto, problemas):
    def erro(msg):
        problemas.append("%s: %s" % (nome, msg))

    if bruto.startswith(b"\xef\xbb\xbf"):
        erro("tem BOM")
    if b"\r" in bruto:
        erro("tem CR (CRLF)")
    try:
        s = bruto.decode("utf-8")
    except UnicodeDecodeError:
        erro("nao e UTF-8")
        return None
    if not s.endswith("\n"):
        erro("sem \\n no final")
    L = s.split("\n")
    achadas = []
    for l in L[:6]:
        m = re.match(r"([A-Za-z]+):", l)
        achadas.append(m.group(1) if m else None)
    if achadas != CHAVES:
        erro("cabecalho %s (esperado %s)" % (achadas, CHAVES))
    if len(L) < 7 or L[6] != "":
        erro("linha 7 deveria ser vazia")
    if not any(l.startswith("REF:") for l in L):
        erro("sem linha REF:")
    if s.count("<<C") != s.count("C>>"):
        erro("<<C e C>> desbalanceados")
    if "(verif.)" in s:
        erro("ainda tem (verif.)")
    for l in L[:6]:
        m = re.match(r"(Autores|Conceitos):\s*(.*)", l)
        if m:
            prof = 0
            for ch in m.group(2):
                prof += ch == "("
                prof -= ch == ")"
                if ch == "," and prof > 0:
                    erro("virgula dentro de parenteses em %s" % m.group(1))
                    break
    return s


def blocos(s):
    L, i, out = s.split("\n"), 0, []
    while i < len(L):
        if L[i].strip() == "<<C":
            j = i + 1
            while j < len(L) and L[j].strip() != "C>>":
                j += 1
            out.append("\n".join(L[i + 1:j]) + "\n")
            i = j
        i += 1
    return out


def testa_bloco(nome, k, src, tmp, problemas):
    base = os.path.join(tmp, "%s_b%d" % (nome, k))
    with open(base + ".c", "w", encoding="utf-8") as fh:
        fh.write(src)
    tem_main = re.search(r"\bmain\s*\(", src) is not None
    if tem_main:
        cmd = ["gcc"] + FLAGS + ["-o", base, base + ".c", "-lm"]
    else:
        cmd = ["gcc"] + FLAGS[:4] + ["-fsyntax-only", "-x", "c", base + ".c"]
    c = subprocess.run(cmd, capture_output=True, text=True)
    rotulo = "%s bloco %d" % (nome, k)
    if c.returncode != 0 or c.stderr.strip():
        problemas.append("%s: compilacao: %s" % (rotulo, c.stderr.strip()[:300]))
        return
    if tem_main:
        try:
            r = subprocess.run([base], capture_output=True, text=True, timeout=10,
                               stdin=subprocess.DEVNULL, cwd=tmp)
        except subprocess.TimeoutExpired:
            problemas.append("%s: execucao passou de 10 s" % rotulo)
            return
        if r.returncode != 0 or r.stderr.strip():
            problemas.append("%s: execucao rc=%d %s" % (rotulo, r.returncode, r.stderr.strip()[:300]))


def main():
    pasta = sys.argv[1] if len(sys.argv) > 1 else "txt"
    arqs = sorted(glob.glob(os.path.join(pasta, "*.txt")))
    if not arqs:
        print("PROBLEMAS: nenhum .txt em '%s'" % pasta)
        return 1
    problemas, nb = [], 0
    with tempfile.TemporaryDirectory() as tmp:
        for a in arqs:
            nome = os.path.basename(a)[:-4]
            with open(a, "rb") as fh:
                s = lint(os.path.basename(a), fh.read(), problemas)
            if s is None:
                continue
            for k, src in enumerate(blocos(s), 1):
                nb += 1
                testa_bloco(nome, k, src, tmp, problemas)
    print("%d TXT, %d blocos C" % (len(arqs), nb))
    for p in problemas:
        print("  " + p)
    print("PROBLEMAS: %d" % len(problemas))
    return 1 if problemas else 0


if __name__ == "__main__":
    sys.exit(main())
