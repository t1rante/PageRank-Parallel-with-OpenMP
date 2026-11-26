// PageRank sequencial, comentado e cronometrado.
//
// O cálculo é exatamente o mesmo de pagerank.cpp (o original).
// O que muda aqui: os comentários e os trechos marcados com [MEDICAO],
// que medem o tempo e não alteram o resultado.
//
// Uso: ./pagerank_sequencial ENTRADA SAIDA

#include <iostream>
#include <fstream>
#include <vector>
#include <valarray>
#include <limits>
#include <cmath>
#include <stdlib.h>
#include <sys/time.h>   // [MEDICAO] gettimeofday
#include <cstdio>       // [MEDICAO]


// ---------------------------------------------------------------------------
// TIPOS
// ---------------------------------------------------------------------------

typedef unsigned int uint;

// Lista dos nós para onde UM nó aponta.
typedef std::vector<uint> outlinks_t;

// O grafo inteiro: nodes[i] é a lista de links que saem do nó i.
typedef std::vector<outlinks_t> nodes_outlinks_t;

// Não é usado em lugar nenhum (já era assim no original).
typedef std::valarray<float> nodes_pagerank_t;


// ---------------------------------------------------------------------------
// CONSTANTES
// ---------------------------------------------------------------------------

// Todo nó começa com PageRank 1.
const float PAGERANK_SEED_VALUE = 1.0;

// O laço para quando a mudança total entre duas voltas fica abaixo disto.
const float PAGERANK_RESIDUAL_LIMIT = 0.001;

// Damping: chance de o internauta seguir um link. Os outros 15% são o salto
// para uma página qualquer.
const float PAGERANK_D = 0.85;


// ---------------------------------------------------------------------------
// MEDIÇÃO DE TEMPO
// ---------------------------------------------------------------------------

// [MEDICAO] Tempo real (de relógio), em segundos, com precisão de microssegundos.
static double agora()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec + t.tv_usec / 1000000.0;
}

// [MEDICAO] Tempo acumulado de cada fase do cálculo e número de voltas.
static double tempo_votacao = 0, tempo_damping = 0, tempo_residual = 0, tempo_copia = 0;
static int voltas = 0;


// ---------------------------------------------------------------------------
// LEITURA E ESCRITA
// ---------------------------------------------------------------------------

// Faz qualquer erro de leitura ou escrita interromper o programa, em vez de
// passar despercebido. Efeito colateral: se o arquivo de entrada não terminar
// com uma quebra de linha, o último número dispara eofbit e o programa aborta.
template <class _STREAM> void TurnExceptionsOn(_STREAM& stream)
{
    stream.exceptions(std::ios_base::badbit |
              std::ios_base::failbit |
              std::ios_base::eofbit);
}

// Lê o grafo. Formato do arquivo:
//   primeira linha:  número de nós
//   linha do nó i:   quantidade de links, seguida dos destinos
void ReadOutlinkLists(std::ifstream& input, nodes_outlinks_t& nodes)
{
    uint n_nodes;
    uint n_outlinks;

    TurnExceptionsOn(input);

    input >> n_nodes;
    nodes_outlinks_t read_nodes(n_nodes);          // n listas vazias
    for (uint i = 0; i < n_nodes; ++i) {
        input >> n_outlinks;
        outlinks_t& outlinks = read_nodes[i];      // apelido para a lista do nó i
        for (uint j = 0; j < n_outlinks; ++j) {
            uint link;
            input >> link;
            outlinks.push_back(link);
        }
    }
    nodes.swap(read_nodes);                        // entrega o grafo sem copiar
}

// Grava o número de nós e depois um PageRank por linha.
// O endl força uma escrita no arquivo a cada linha: é lento.
template<class _OUT>void OutputPageRanks(_OUT& output, std::valarray<float> ranks)
{
    output << ranks.size() << std::endl;
    for (uint i = 0; i < ranks.size(); ++i) {
        output << ranks[i] << std::endl;
    }
}


// ---------------------------------------------------------------------------
// CÁLCULO
// ---------------------------------------------------------------------------

// Residual: soma das mudanças |novo - antigo| de todos os nós.
float CalculateL1Norm(const std::valarray<float>& old_pageranks,
              const std::valarray<float>& new_pageranks)
{
    std::valarray<float> diff = abs(new_pageranks - old_pageranks);
    return diff.sum();
}

// Fórmula:  PR(i) = (1-d) + d * soma de PR(v)/N(v), para todo v que aponta para i
//           N(v) = quantos links saem de v
void CalculatePageRank(const nodes_outlinks_t& nodes,
        std::valarray<float>& pagerank)
{
    float residual = std::numeric_limits<float>::max();   // garante a 1a volta
    float vote_value = 0.0;
    const float d = PAGERANK_D;

    // pagerank = valores da volta ANTERIOR (durante a votação, só é lido)
    if (pagerank.size() != nodes.size()) {
        pagerank.resize(nodes.size());
    }
    pagerank = PAGERANK_SEED_VALUE;

    // acc = valores da PRÓXIMA volta, em construção (só é escrito)
    std::valarray<float> acc(0.0, nodes.size());

    // Cada volta usa o resultado da anterior: as voltas não podem ser
    // executadas ao mesmo tempo.
    while (residual > PAGERANK_RESIDUAL_LIMIT) {
        double t0 = agora();   // [MEDICAO]

        // FASE 1 - VOTAÇÃO: cada nó divide seu PageRank entre os nós que aponta.
        // É a parte mais cara: passa por todos os links do grafo.
        for (uint i = 0; i < pagerank.size(); ++i) {
            const outlinks_t& outlinks = nodes[i];
            if (!outlinks.empty()) {
                vote_value = pagerank[i]/outlinks.size();
                for (uint u = 0; u < outlinks.size(); ++u) {
                    // Vários nós podem somar na MESMA posição de acc.
                    acc[outlinks[u]] += vote_value;
                }
            }
        }
        double t1 = agora();   // [MEDICAO]

        // FASE 2 - DAMPING: aplica a fórmula em cada nó.
        acc *= d;
        acc += (1.0 - d);
        double t2 = agora();   // [MEDICAO]

        // FASE 3 - RESIDUAL: quanto os valores mudaram nesta volta?
        residual = CalculateL1Norm(pagerank, acc);
        double t3 = agora();   // [MEDICAO]

        // FASE 4 - CÓPIA: o novo vira o antigo, e acc volta a zero.
        pagerank = acc;
        acc = 0.0;
        double t4 = agora();   // [MEDICAO]

        tempo_votacao  += t1 - t0;   // [MEDICAO]
        tempo_damping  += t2 - t1;   // [MEDICAO]
        tempo_residual += t3 - t2;   // [MEDICAO]
        tempo_copia    += t4 - t3;   // [MEDICAO]
        ++voltas;                    // [MEDICAO]
    }
}


int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "ERROR: Wrong number of arguments.\n" <<
                 "Usage: ./pr INPUT OUTPUT\n" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::ifstream input;
    std::ofstream output;
    nodes_outlinks_t nodes;

    TurnExceptionsOn(input);
    TurnExceptionsOn(output);

    input.open(argv[1]);
    output.open(argv[2]);

    double inicio = agora();        // [MEDICAO]
    ReadOutlinkLists(input, nodes);
    double leu = agora();           // [MEDICAO]
    std::valarray<float> ranks(nodes.size());
    CalculatePageRank(nodes, ranks);
    double calculou = agora();      // [MEDICAO]
    OutputPageRanks(output, ranks);
    double escreveu = agora();      // [MEDICAO]

    // [MEDICAO] Uma linha no stderr; o arquivo de saída não muda.
    // leitura,calculo,escrita,total,voltas,votacao,damping,residual,copia
    fprintf(stderr, "%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%.6f\n",
            leu - inicio, calculou - leu, escreveu - calculou, escreveu - inicio,
            voltas, tempo_votacao, tempo_damping, tempo_residual, tempo_copia);
}
