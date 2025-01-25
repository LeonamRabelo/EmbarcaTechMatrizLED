#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "pico/bootrom.h"
#include "matriz_led.pio.h"

#define NUM_PIXELS 25 //número de leds na matriz
#define LED_PIN 7 //pino de saída do led

// Definição de pixel GRB
struct pixel_t {
  uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[NUM_PIXELS];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

/**
 * Inicializa a máquina PIO para controle da matriz de LEDs.
 */
void npInit(uint pin) {

  // Cria programa PIO.
  uint offset = pio_add_program(pio0, &pio_matrix_program);
  np_pio = pio0;

  // Toma posse de uma máquina PIO.
  sm = pio_claim_unused_sm(np_pio, false);
  if (sm < 0) {
    np_pio = pio1;
    sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
  }

  // Inicia programa na máquina PIO obtida.
  pio_matrix_program_init(np_pio, sm, offset, pin);

  // Limpa buffer de pixels.
  for (uint i = 0; i < NUM_PIXELS; ++i) {
    leds[i].R = 0;
    leds[i].G = 0;
    leds[i].B = 0;
  }
}

/**
 * Atribui uma cor RGB a um LED.
 */
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
  leds[index].R = r;
  leds[index].G = g;
  leds[index].B = b;
}

/**
 * Limpa o buffer de pixels.
 */
void npClear() {
  for (uint i = 0; i < NUM_PIXELS; ++i)
    npSetLED(i, 0, 0, 0);
}

//Função para habilitar o modo Bootsel
void bootsel(){
  reset_usb_boot(0,0);
}

//TECLADO
// definição das colunas e das linhas e mapeamento do teclado
uint8_t coluna[4] = {18,19,20,21};
uint8_t linha[4] = {22,26,27,28};

char teclas[4][4] = {
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '*', '0', '#', 'D'};

//função para inicializar o teclado
void inicializar_teclado() {
  for (int i = 0; i < 4; i++) {
    gpio_init(linha[i]);
    gpio_set_dir(linha[i], GPIO_OUT);
    gpio_put(linha[i], 1); // Inicialmente em HIGH

    gpio_init(coluna[i]);
    gpio_set_dir(coluna[i], GPIO_IN);
    gpio_pull_up(coluna[i]); // Habilita pull-up nas colunas
  }
}

//função para ler o teclado
char ler_teclado(uint8_t *colunas, uint8_t *linhas) {
  for (int i = 0; i < 4; i++) {
    gpio_put(linhas[i], 0);
    for (int j = 0; j < 4; j++) {
            if (!gpio_get(colunas[j])) { // Verifica se a coluna está LOW
                gpio_put(linhas[i], 1);  // Restaura a linha
                return teclas[i][j];    // Retorna a tecla correspondente
            }
        }
    gpio_put(linhas[i], 1);
  }
  return 0;
}

//MATRIZ DE LEDS
//rotina para definição da intensidade de cores do led
uint matrix_rgb(float r, float g, float b)
{
  unsigned char R, G, B;
  R = r * 255;
  G = g * 255;
  B = b * 255;
  return (G << 24) | (R << 16) | (B << 8);
}

void npWrite() {
  // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
  for (uint i = 0; i < NUM_PIXELS; ++i) {
    pio_sm_put_blocking(np_pio, sm, leds[i].G);
    pio_sm_put_blocking(np_pio, sm, leds[i].R);
    pio_sm_put_blocking(np_pio, sm, leds[i].B);
  }
  sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

// Função para converter a posição do matriz para uma posição do vetor.
int getIndex(int x, int y) {
    // Se a linha for par (0, 2, 4), percorremos da esquerda para a direita.
    // Se a linha for ímpar (1, 3), percorremos da direita para a esquerda.
    if (y % 2 == 0) {
        return 24-(y * 5 + x); // Linha par (esquerda para direita).
    } else {
        return 24-(y * 5 + (4 - x)); // Linha ímpar (direita para esquerda).
    }
}

//Funcao para desenhar a matriz
void desenhaMatriz(int matriz[5][5][3], int tempo_ms, float intensidade){
    for (int linha = 0; linha < 5; linha++){
        for (int coluna = 0; coluna < 5; coluna++){
            int posicao = getIndex(linha, coluna);
            npSetLED(posicao, (matriz[coluna][linha][0]*intensidade), (matriz[coluna][linha][1]*intensidade), (matriz[coluna][linha][2]*intensidade));
        }
    }
    npWrite();
    sleep_ms(tempo_ms);
    npClear();
}

//função principal
int main()
{
    PIO pio = pio0; 
    bool frequenciaClock;
    uint16_t i;
    uint valor_led;
    float r = 0.0, b = 0.0 , g = 0.0;

    frequenciaClock = set_sys_clock_khz(128000, false); //frequência de clock
    stdio_init_all();
    inicializar_teclado();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    printf("iniciando a transmissão PIO");
    if (frequenciaClock) printf("clock set to %ld\n", clock_get_hz(clk_sys));

    //configurações da PIO
    uint offset = pio_add_program(pio, &pio_matrix_program);
    uint sm = pio_claim_unused_sm(pio, true);
    pio_matrix_program_init(pio, sm, offset, LED_PIN);

    while (true) {
    
    char tecla = ler_teclado(coluna, linha);

    if (tecla)
    {
      printf("Tecla pressionada: %c\n", tecla);

    switch (tecla)
    {
    case 1:                             // Verifica se a tecla 1 foi pressionada


        break;
    
    case 2:                                 // Verifica se a tecla 2 foi pressionada



        break;

    case 3:                                 // Verifica se a tecla 3 foi pressionada



        break;

    case 4:                                 // Verifica se a tecla 4 foi pressionada



        break;

    case 5:                                 // Verifica se a tecla 5 foi pressionada



        break;
    
    case 6:                                 // Verifica se a tecla 6 foi pressionada



        break;

    case 7:                                 // Verifica se a tecla 7 foi pressionada



        break;
    
    case 8:                                 // Verifica se a tecla 8 foi pressionada
    //Letreiro "C E P E D I + (CARINHA_FELIZ)"
    //Gerar a letra C na matriz leds, na cor azul
    int matrizC[5][5][3]= {
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 0}}
    };
    //Chama a funcao para desenhar a matriz, passando a matriz e o tempo em milisegundos
    desenhaMatriz(matrizC, 2000, 0.8);

    //Gerar a letra E na matriz leds, na cor azul
    int matrizE[5][5][3]= {
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 0}}
    };
    //Chama a funcao para desenhar a matriz, passando a matriz e o tempo em milisegundos
    desenhaMatriz(matrizE, 2000, 0.8);

    //Gerar a letra P na matriz leds, na cor azul
    int matrizP[5][5][3]= {
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 255}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
    };
    //Chama a funcao para desenhar a matriz, passando a matriz e o tempo em milisegundos
    desenhaMatriz(matrizP, 2000, 0.8);

    //Gerar a letra E na matriz leds, na cor azul
    int matrizE2[5][5][3]= {
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 0}}
    };
    //Chama a funcao para desenhar a matriz, passando a matriz e o tempo em milisegundos
    desenhaMatriz(matrizE2, 2000, 0.8);  

    //Gerar a letra D na matriz leds, na cor azul
    int matrizD[5][5][3]= {
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 255}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 255}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 255}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 0}}
    };
    //Chama a funcao para desenhar a matriz, passando a matriz e o tempo em milisegundos
    desenhaMatriz(matrizD, 2000, 0.8);

    //Gerar a letra I na matriz leds, na cor azul
    int matrizI[5][5][3]= {
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}}
    };
    //Chama a funcao para desenhar a matriz, passando a matriz e o tempo em milisegundos
    desenhaMatriz(matrizI, 2000, 0.8);

    //Gerar um emoji de rosto sorrindo na matriz leds, na cor azul
    int matrizCarinha[5][5][3]= {
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 255}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 255}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 0}}
    };
    //Chama a funcao para desenhar a matriz, passando a matriz e o tempo em milisegundos
    desenhaMatriz(matrizCarinha, 2000, 0.8);

    break;

    case 9:                                 // Verifica se a tecla 9 foi pressionada



        break;

    case 'A':                               // Verifica se a tecla A foi pressionada
         
        npClear();
        break;

    case 'B':                             // Verifica se a tecla B foi pressionada
        r = 0;
        g = 0;
        b = 1;
        break;

    case 'C':                             // Verifica se  a tecla C foi pressionada
        r = 0.8;
        g = 0;
        b = 0;
        break;

    case 'D':                             // Verifica se a tecla D foi pressionada
        r = 0;
        g = 0.5;
        b = 0;
        break;

    case '#':                             // Verifica se a tecla # foi pressionada
        r = 0.2;
        g = 0.2;
        b = 0.2;
        break;

    case '*':                             // Verifica se a tecla * foi pressionada
        bootsel();
        break;

      default:
        printf("Tecla não configurada\n");
      }
      sleep_ms(100); // Delay para evitar leitura repetida
    }
    sleep_ms(100);
    }
}
