#!/usr/bin/env python3
"""Le os resultados brutos e monta os gráficos finais.

  speedup = tempo do sequencial / tempo do paralelo

Os gráficos precisam do matplotlib (pip install matplotlib).
"""
import csv
from statistics import mean, stdev

THREADS  = ["1", "2", "4", "8", "12"]
TAMANHOS = range(1, 11)
TIPOS    = ["uniforme", "desigual"]

sequencial = list(csv.DictReader(open("resultados/sequencial.csv")))
paralelo   = list(csv.DictReader(open("resultados/paralelo.csv")))

def tempos(tipo, n, threads=None, campo="calculo"):
    """As 5 medidas de um grafo. Sem threads = sequencial."""
    linhas = sequencial if threads is None else paralelo
    return [float(l[campo]) for l in linhas
            if l["tipo"] == tipo and l["milhoes"] == str(n)
            and (threads is None or l["threads"] == threads)]

def speedup(tipo, n, threads):
    return mean(tempos(tipo, n)) / mean(tempos(tipo, n, threads))

def virgula(x, casas=2):
    return f"{x:.{casas}f}".replace(".", ",")

# O README.md é o único relatório Markdown; esta etapa cria apenas os gráficos.
# ---- graficos ---------------------------------------------------------------
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

FUNDO, TINTA, SUAVE, GRADE = "#fcfcfb", "#0b0b0b", "#52514e", "#e4e3df"
CORES = {1: "#2a78d6", 5: "#eb6834", 10: "#1baf7a"}      # uma cor por tamanho
plt.rcParams.update({"font.size": 11, "text.color": TINTA, "axes.labelcolor": SUAVE,
                     "xtick.color": SUAVE, "ytick.color": SUAVE, "axes.edgecolor": GRADE,
                     "figure.facecolor": FUNDO, "axes.facecolor": FUNDO})

def novo_grafico(titulo):
    fig, eixos = plt.subplots(1, 2, figsize=(11, 4.6), sharey=True)
    fig.suptitle(titulo, x=0.06, ha="left", fontsize=14, fontweight="bold")
    for ax, tipo in zip(eixos, TIPOS):
        ax.set_title(f"Grafo {tipo}", loc="left", fontsize=12, color=SUAVE)
        ax.grid(axis="y", color=GRADE, linewidth=1)
        ax.set_axisbelow(True)
        for lado in ("top", "right", "left"):
            ax.spines[lado].set_visible(False)
        ax.tick_params(length=0)
        ax.set_xlabel("Threads")
    return fig, eixos

def linha(ax, x, y, n, erro=None):
    ax.errorbar(x, y, yerr=erro, color=CORES[n], linewidth=2, marker="o", markersize=8,
                markeredgecolor=FUNDO, markeredgewidth=2, capsize=3, label=f"{n}M de nós")

def salva(fig, eixos, nome):
    # Legenda unica, sem repetir entradas dos dois paineis.
    h, l = eixos[0].get_legend_handles_labels()
    fig.legend(h, l, loc="upper right", ncol=len(l), frameon=False, bbox_to_anchor=(0.985, 1.0))
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.savefig(f"resultados/{nome}.png", dpi=140)
    plt.close(fig)

POSICAO = list(range(len(THREADS)))          # 1, 2, 4, 8 e 12 igualmente espacados

# 1) tempo em escala linear
fig, eixos = novo_grafico("Tempo de cálculo")
for ax, tipo in zip(eixos, TIPOS):
    for n in CORES:
        medidas = [tempos(tipo, n)] + [tempos(tipo, n, t) for t in THREADS]
        medias = [mean(m) for m in medidas]
        linha(ax, range(6), medias, n, [stdev(m) for m in medidas])
    ax.set_xticks(range(6), ["sequencial"] + THREADS)
    ax.set_ylim(bottom=0)
eixos[0].set_ylabel("segundos")
salva(fig, eixos, "grafico_tempo")

# 2) speedup com limite baseado no maior dado observado
fig, eixos = novo_grafico("Speedup em relação ao sequencial")
max_speedup = 0
for ax, tipo in zip(eixos, TIPOS):
    ax.plot(POSICAO, [int(t) for t in THREADS], color=SUAVE, linewidth=1.2, linestyle=(0, (4, 4)), label="ideal")
    for n in CORES:
        valores = [speedup(tipo, n, t) for t in THREADS]
        max_speedup = max(max_speedup, *valores)
        linha(ax, POSICAO, valores, n)
    ax.set_xticks(POSICAO, THREADS)
limite = max(6, int(max_speedup) + 1)
for ax in eixos:
    ax.set_ylim(0, limite)
    ax.set_yticks(range(0, limite + 1))
eixos[0].set_ylabel("vezes mais rápido")
salva(fig, eixos, "grafico_speedup")

# 3) tempo sequencial uniforme x desigual
cores_tipo = {"uniforme": "#2a78d6", "desigual": "#eb6834"}
fig, ax = plt.subplots(figsize=(8, 5), facecolor=FUNDO)
ax.set_facecolor(FUNDO)
ax.set_title("Tempo sequencial por tamanho do grafo", loc="left", fontsize=14, fontweight="bold")
ax.grid(axis="y", color=GRADE, linewidth=1)
ax.set_axisbelow(True)
for lado in ("top", "right", "left"):
    ax.spines[lado].set_visible(False)
ax.tick_params(length=0)

for tipo in TIPOS:
    medias = [mean(tempos(tipo, n)) for n in TAMANHOS]
    ax.plot(list(TAMANHOS), medias, color=cores_tipo[tipo], linewidth=2.2,
            marker="o", markersize=7, markeredgecolor=FUNDO,
            markeredgewidth=1.5, label=f"Grafo {tipo}")

ax.set_xlabel("Nós (milhões)")
ax.set_ylabel("Tempo de cálculo (segundos)")
ax.set_xticks(list(TAMANHOS))
ax.set_ylim(bottom=0)
ax.legend(frameon=False)
fig.tight_layout()
fig.savefig("resultados/grafico_sequencial.png", dpi=140, bbox_inches="tight")
plt.close(fig)

print("graficos em resultados/")
