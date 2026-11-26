// PageRank paralelo com OpenMP.
//
// Mesma leitura, mesma escrita e mesma fórmula de pagerank_sequencial.cpp.
// O que muda está marcado com [PARALELO].
//
// Ideia central: o sequencial EMPURRA votos (o nó i escreve em acc dos vizinhos,
// e duas threads poderiam escrever na mesma posição). Aqui cada nó PUXA os votos
// de quem aponta para ele e escreve só em acc[i]. Cada posição tem um único dono,
// então não há corrida e não é preciso nenhuma trava.
//
// Uso: OMP_NUM_THREADS=8 ./pagerank_paralelo ENTRADA SAIDA

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
// TIPOS E CONSTANTES (iguais ao sequencial)
// ---------------------------------------------------------------------------

typedef unsigned int uint;
typedef std::vector<uint> outlinks_t;
typedef std::vector<outlinks_t> nodes_outlinks_t;

const float PAGERANK_SEED_VALUE = 1.0;
const float PAGERANK_RESIDUAL_LIMIT = 0.001;
const float PAGERANK_D = 0.85;


// ---------------------------------------------------------------------------
// MEDIÇÃO DE TEMPO
// ---------------------------------------------------------------------------

static double agora()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec + t.tv_usec / 1000000.0;
}

static double tempo_inversao = 0;
static double tempo_votacao = 0, tempo_damping = 0, tempo_residual = 0, tempo_copia = 0;
static int voltas = 0;


// ---------------------------------------------------------------------------
// LEITURA E ESCRITA 
// ---------------------------------------------------------------------------

template <class _STREAM> void TurnExceptionsOn(_STREAM& stream)
{
    stream.exceptions(std::ios_base::badbit |
              std::ios_base::failbit |
              std::ios_base::eofbit);
}

void ReadOutlinkLists(std::ifstream& input, nodes_outlinks_t& nodes)
{
    uint n_nodes;
    uint n_outlinks;

    TurnExceptionsOn(input);

    input >> n_nodes;
    nodes_outlinks_t read_nodes(n_nodes);
    for (uint i = 0; i < n_nodes; ++i) {
        input >> n_outlinks;
        outlinks_t& outlinks = read_nodes[i];
        for (uint j = 0; j < n_outlinks; ++j) {
            uint link;
            input >> link;
            outlinks.push_back(link);
        }
    }
    nodes.swap(read_nodes);
}

template<class _OUT>void OutputPageRanks(_OUT& output, std::valarray<float> ranks)
{
    output << ranks.size() << std::endl;
    for (uint i = 0; i < ranks.size(); ++i) {
        output << ranks[i] << std::endl;
    }
}


// ---------------------------------------------------------------------------
// INVERSÃO DO GRAFO
// ---------------------------------------------------------------------------

// [PARALELO] O arquivo diz para quem cada nó aponta. Para puxar votos precisamos
// do contrário: quem aponta para cada nó.
//
//   origem[]  as origens de todos os links, agrupadas pelo nó de destino
//   inicio[]  as origens do nó i estão em origem[ inicio[i] ... inicio[i+1]-1 ]
//
// Exemplo (0->1, 0->2, 1->2, 2->0, 3->2):
//   origem = [ 2 | 0 | 0 1 3 | ]      inicio = [ 0, 1, 2, 5, 5 ]
//
//
// Esta função continua sequencial e roda uma vez só.
void InverteGrafo(const nodes_outlinks_t& nodes,
        std::vector<uint>& inicio, std::vector<uint>& origem)
{
    const uint n = nodes.size();

    // 1) Conta quantos links chegam em cada nó v, guardando em inicio[v+1].
    inicio.assign(n + 1, 0);
    for (uint i = 0; i < n; ++i) {
        for (uint u = 0; u < nodes[i].size(); ++u) {
            inicio[nodes[i][u] + 1]++;
        }
    }
    // 2) Soma acumulada: a quantidade vira a posição onde a lista começa.
    for (uint i = 0; i < n; ++i) {
        inicio[i + 1] += inicio[i];
    }
    // 3) Escreve cada origem na próxima vaga da lista do seu destino.
    origem.resize(inicio[n]);
    std::vector<uint> vaga(inicio);
    for (uint i = 0; i < n; ++i) {
        for (uint u = 0; u < nodes[i].size(); ++u) {
            origem[vaga[nodes[i][u]]++] = i;
        }
    }
}


// ---------------------------------------------------------------------------
// CÁLCULO
// ---------------------------------------------------------------------------

void CalculatePageRank(const nodes_outlinks_t& nodes,
        std::valarray<float>& pagerank)
{
    float residual = std::numeric_limits<float>::max();
    const float d = PAGERANK_D;
    const float salto = 1.0 - d;
    const uint n = nodes.size();

    if (pagerank.size() != n) {
        pagerank.resize(n);
    }
    pagerank = PAGERANK_SEED_VALUE;
    std::valarray<float> acc(0.0, n);

    //Voto que cada nó dá a cada vizinho: pagerank / n de links.
    std::valarray<float> voto(0.0, n);

    double antes = agora();   
    std::vector<uint> inicio, origem;
    InverteGrafo(nodes, inicio, origem);
    tempo_inversao = agora() - antes;   

    // As voltas continuam uma depois da outra: cada uma usa o resultado da
    // anterior. O que paralelizei é o trabalho DENTRO de cada volta.
    while (residual > PAGERANK_RESIDUAL_LIMIT) {
        double t0 = agora();   

        // FASE 1 - VOTAÇÃO
        // Cada nó calcula o valor do seu voto.
        //     static: divide os nós em fatias iguais, porque o custo é igual.
        #pragma omp parallel for schedule(static)
        for (uint i = 0; i < n; ++i) {
            voto[i] = nodes[i].empty() ? 0 : pagerank[i] / nodes[i].size();
        }
        // Barreira, ninguém passa daqui até todos os votos existirem.

        // Cada nó soma os votos que recebe.
        //     Várias threads leem voto[], mas só uma escreve em acc[i].
        //     dynamic: cada thread pega um bloco de 4096 nós. Quando acaba,
        //     busca outro. Quem tem nós "leves" pega mais blocos.
        #pragma omp parallel for schedule(dynamic, 4096)
        for (uint i = 0; i < n; ++i) {
            float soma = 0;        // declarada aqui dentro: cada thread tem a sua (Local)
            for (uint k = inicio[i]; k < inicio[i + 1]; ++k) {
                soma += voto[origem[k]];
            }
            acc[i] = soma;
        }
        double t1 = agora();   

        // FASE 2 - DAMPING: cada posição só depende dela mesma.
        #pragma omp parallel for schedule(static)
        for (uint i = 0; i < n; ++i) {
            acc[i] = acc[i] * d + salto;
        }
        double t2 = agora();  

        // FASE 3 - RESIDUAL: cada thread soma a sua parte numa cópia privada,
        // e o reduction junta as cópias no fim.
        residual = 0;
        #pragma omp parallel for schedule(static) reduction(+:residual)
        for (uint i = 0; i < n; ++i) {
            residual += std::fabs(acc[i] - pagerank[i]);
        }
        double t3 = agora();   // [MEDICAO]

        // FASE 4 - CÓPIA. Não é preciso zerar acc: a fase 1b escreve por cima.
        #pragma omp parallel for schedule(static)
        for (uint i = 0; i < n; ++i) {
            pagerank[i] = acc[i];
        }
        double t4 = agora(); 

        tempo_votacao  += t1 - t0;   
        tempo_damping  += t2 - t1;   
        tempo_residual += t3 - t2;   
        tempo_copia    += t4 - t3;   
        ++voltas;                    
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

    double inicio = agora();       
    ReadOutlinkLists(input, nodes);
    double leu = agora();           
    std::valarray<float> ranks(nodes.size());
    CalculatePageRank(nodes, ranks);
    double calculou = agora();      
    OutputPageRanks(output, ranks);
    double escreveu = agora();      
    
    fprintf(stderr, "%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%.6f,%.6f\n",
            leu - inicio, calculou - leu, escreveu - calculou, escreveu - inicio,
            voltas, tempo_votacao, tempo_damping, tempo_residual, tempo_copia,
            tempo_inversao);
}
