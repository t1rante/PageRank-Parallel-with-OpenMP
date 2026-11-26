// Gera um grafo aleatorio no formato que o pagerank le.
//
//   gera_grafo uniforme|desigual N GRAU SEMENTE > arquivo
//
// uniforme: todos os nos tem, em media, GRAU links, para destinos quaisquer.
// desigual: os primeiros 10% dos nos concentram 55% dos links, tanto os que
//           saem quanto os que chegam. Serve para testar o balanceamento de
//           carga. O grau medio do grafo continua sendo GRAU.
//
// Mesma semente = mesmo arquivo.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

int main(int argc, char* argv[])
{
    if (argc != 5) {
        fprintf(stderr, "Uso: %s uniforme|desigual N GRAU SEMENTE > arquivo\n", argv[0]);
        return 1;
    }
    bool desigual    = strcmp(argv[1], "desigual") == 0;
    unsigned n       = atoi(argv[2]);
    unsigned grau    = atoi(argv[3]);
    std::mt19937 sorteio(atoi(argv[4]));
    unsigned famosos = n / 10;          // os 10% de nos mais ligados
    if (famosos == 0) famosos = 1;

    printf("%u\n", n);
    for (unsigned i = 0; i < n; ++i) {
        // 1) quantos links saem deste no?
        unsigned media = grau;
        if (desigual) media = i < famosos ? grau * 11 / 2 : grau / 2;
        unsigned links = sorteio() % (2 * media + 1);   // de 0 a 2*media

        // 2) para onde eles vao?
        printf("%u", links);
        for (unsigned j = 0; j < links; ++j) {
            bool vai_para_famoso = desigual && sorteio() % 2 == 0;
            unsigned destino = sorteio() % (vai_para_famoso ? famosos : n);
            printf(" %u", destino);
        }
        printf("\n");
    }
    return 0;
}
