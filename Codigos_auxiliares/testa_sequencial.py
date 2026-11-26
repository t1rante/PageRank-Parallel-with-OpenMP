#!/usr/bin/env python3
"""Teste didático da versão sequencial.

Uma rodada inicial aquece o processador. Depois são feitas cinco rodadas
válidas, em ordem aleatória, para cada grafo de 1 milhão a 10 milhões de nós.
"""
import csv
import os
import random
import subprocess

EXECUCOES = 5
TAMANHOS = range(1, 11)
TIPOS = ("uniforme", "desigual")
CAMPOS = (
    "leitura", "calculo", "escrita", "total", "voltas",
    "votacao", "damping", "residual", "copia",
)
SAIDA = "/tmp/pagerank_saida.txt"


def executar(tipo, tamanho):
    """Executa um programa em um núcleo rápido e devolve os tempos medidos."""
    entrada = f"{tipo}_{tamanho}M.txt"
    comando = ["taskset", "-c", "0", "./pagerank_sequencial", entrada, SAIDA]
    processo = subprocess.run(comando, capture_output=True, text=True, check=True)

    valores = processo.stderr.strip().split(",")
    return dict(zip(CAMPOS, map(float, valores)))


def contar_links(tipo, tamanho):
    """Conta a quantidade de links existentes no grafo."""
    with open(f"{tipo}_{tamanho}M.txt", encoding="utf-8") as arquivo:
        next(arquivo)
        return sum(int(linha.split(maxsplit=1)[0]) for linha in arquivo)


# A ordem fixa torna o teste repetível.
random.seed(42)
grafos = [(tipo, tamanho) for tipo in TIPOS for tamanho in TAMANHOS]
medidas = []

# rodada 0: aquecimento; seu resultado não é salvo
for rodada in range(EXECUCOES + 1):
    random.shuffle(grafos)

    for tipo, tamanho in grafos:
        tempos = executar(tipo, tamanho)
        if rodada > 0:
            medidas.append({
                "rodada": rodada,
                "tipo": tipo,
                "milhoes": tamanho,
                **tempos,
            })

    print(f"rodada {rodada} de {EXECUCOES}", flush=True)

os.remove(SAIDA)

with open("resultados/sequencial.csv", "w", newline="", encoding="utf-8") as arquivo:
    escritor = csv.DictWriter(arquivo, fieldnames=medidas[0].keys())
    escritor.writeheader()
    escritor.writerows(medidas)

print(f"medidas sequenciais gravadas em resultados/sequencial.csv ({len(medidas)} linhas)")
