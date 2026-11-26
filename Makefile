# Projeto de testes do PageRank
#
# make          compila
# make dados     gera os grafos
# make teste     executa toda a rodada e gera os resultados
# make clean     apaga programas e arquivos gerados

CXX := g++
CXXFLAGS := -O2 -Wall -Wextra

PROGRAMAS := pagerank_sequencial pagerank_paralelo check_output gera_grafo
GRAFOS := $(foreach n,1 2 3 4 5 6 7 8 9 10,uniforme_$(n)M.txt desigual_$(n)M.txt)

all: $(PROGRAMAS)

pagerank_sequencial: pagerank_sequencial.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

pagerank_paralelo: pagerank_paralelo.cpp
	$(CXX) $(CXXFLAGS) -fopenmp -o $@ $<

check_output: Codigos_auxiliares/check_output.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

gera_grafo: Codigos_auxiliares/gera_grafo.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

dados: $(GRAFOS)

uniforme_%M.txt: gera_grafo
	./gera_grafo uniforme $*000000 8 42 > $@

desigual_%M.txt: gera_grafo
	./gera_grafo desigual $*000000 8 42 > $@

# Um único comando executa a rodada completa e produz os resultados finais.
teste: all dados
	python3 Codigos_auxiliares/testa_sequencial.py
	python3 Codigos_auxiliares/testa_paralelo.py
	python3 Codigos_auxiliares/analisa.py

clean:
	@if pgrep pagerank_ >/dev/null 2>&1; then echo "Há um teste em execução."; exit 1; fi
	rm -f $(PROGRAMAS) uniforme_*M.txt desigual_*M.txt
	rm -rf __pycache__ Codigos_auxiliares/__pycache__

.PHONY: all dados teste clean
