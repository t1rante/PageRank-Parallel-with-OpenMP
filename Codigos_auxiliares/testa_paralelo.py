#!/usr/bin/env python3
"""Compara o PageRank paralelo com o sequencial.

Os passos são:
1. executar o sequencial e guardar uma cópia de cada saída;
2. executar o paralelo com 1, 2, 4, 8 e 12 threads;
3. comparar diretamente os arquivos produzidos.
"""
import csv
import os
import random
import subprocess

EXECUCOES = 5
THREADS = (1, 2, 4, 8, 12)
TAMANHOS = range(1, 11)
TIPOS = ("uniforme", "desigual")
CAMPOS = (
    "leitura", "calculo", "escrita", "total", "voltas",
    "votacao", "damping", "residual", "copia", "inversao",
)
SAIDA_PARALELA = "/tmp/pagerank_saida.txt"


def executar(comando, tipo, tamanho, saida):
    """Executa um programa e devolve somente os tempos medidos."""
    processo = subprocess.run(
        comando + [f"{tipo}_{tamanho}M.txt", saida],
        capture_output=True,
        text=True,
        check=True,
    )
    return dict(zip(CAMPOS, map(float, processo.stderr.strip().split(","))))


def arquivos_identicos(arquivo_a, arquivo_b):
    """Compara os dois arquivos diretamente, byte a byte."""
    with open(arquivo_a, "rb") as a, open(arquivo_b, "rb") as b:
        while True:
            parte_a = a.read(1024 * 1024)
            parte_b = b.read(1024 * 1024)
            if parte_a != parte_b:
                return False
            if not parte_a:
                return True


def salvar_csv(nome, linhas):
    with open(nome, "w", newline="", encoding="utf-8") as arquivo:
        escritor = csv.DictWriter(arquivo, fieldnames=linhas[0].keys())
        escritor.writeheader()
        escritor.writerows(linhas)


random.seed(42)

gabaritos = {}
controle = []
for tipo in TIPOS:
    for tamanho in TAMANHOS:
        temporario = f"/tmp/pagerank_sequencial_{tipo}_{tamanho}M.txt"
        tempos = executar(
            ["taskset", "-c", "0", "./pagerank_sequencial"],
            tipo,
            tamanho,
            temporario,
        )
        gabaritos[tipo, tamanho] = temporario
        controle.append({"tipo": tipo, "milhoes": tamanho, **tempos})

salvar_csv("resultados/controle_sequencial.csv", controle)
print("rodada 0 (sequencial): gabaritos criados", flush=True)

configuracoes = [
    (tipo, tamanho, threads)
    for tipo in TIPOS
    for tamanho in TAMANHOS
    for threads in THREADS
]
medidas = []

for rodada in range(1, EXECUCOES + 1):
    random.shuffle(configuracoes)

    for tipo, tamanho, threads in configuracoes:
        os.environ["OMP_NUM_THREADS"] = str(threads)
        tempos = executar(["./pagerank_paralelo"], tipo, tamanho, SAIDA_PARALELA)
        medidas.append({
            "rodada": rodada,
            "tipo": tipo,
            "milhoes": tamanho,
            "threads": threads,
            "identico": arquivos_identicos(
                gabaritos[tipo, tamanho], SAIDA_PARALELA
            ),
            **tempos,
        })

    salvar_csv("resultados/paralelo.csv", medidas)
    diferentes = sum(not linha["identico"] for linha in medidas)
    print(
        f"rodada {rodada} de {EXECUCOES}: {diferentes} saídas diferentes de {len(medidas)}",
        flush=True,
    )

os.remove(SAIDA_PARALELA)
for arquivo in gabaritos.values():
    os.remove(arquivo)
