// Autor: Levi Silva Freitas
// Data: 18/02/2025

// Inclusão de bibliotecas necessárias
#include <stdio.h> // Inclusão da biblioteca padrão de entrada e saída
#include <stdlib.h> // Inclusão da biblioteca padrão do C
#include "pico/stdlib.h" // Inclusão da biblioteca de funções padrão do Pico
#include "hardware/adc.h" // Inclusão da biblioteca de funções do ADC
#include "hardware/i2c.h" // Inclusão da biblioteca de funções do I2C
#include "hardware/pwm.h" // Inclusão da biblioteca de funções do PWM
#include "hardware/gpio.h" // Inclusão da biblioteca de funções do GPIO
#include "inc/font.h" // Inclusão da biblioteca de fontes
#include "inc/ssd1306.h" // Inclusão da biblioteca do display OLED

// Definições da comunicação I2C
#define I2C_ENDERECO 0x3C // Endereço do display OLED
#define I2C_PORT i2c1 // Barramento I2C utilizado
#define I2C_SDA 14 // Pino SDA do barramento I2C
#define I2C_SCL 15 // Pino SCL do barramento I2C

// Definições de pinos e periféricos
#define LED_VERMELHO 13 // Pino do LED vermelho
#define LED_AZUL 12 // Pino do LED azul
#define LED_VERDE 11 // Pino do LED verde
#define JOYSTICK_Y 27 // Pino do eixo Y do joystick
#define JOYSTICK_X 26 // Pino do eixo X do joystick
#define BOTAO_JOY 22 // Pino do botão do joystick
#define BOTAO_A 5 // Pino do botão A

// Variáveis globais
static bool leds_pwm_ativados = false; // Estado dos LEDs PWM
static bool estado_led_verde = false; // Estado do LED verde
static volatile uint64_t antes = 0; // Tempo da última interrupção
static uint8_t estilo_borda = 0; // Estilo da borda do display
static ssd1306_t display; // Display OLED

// Prototipação de funções
/**
 * @brief Inicializa os componentes do sistema.
 * 
 * @return true se a inicialização for bem-sucedida, false caso contrário.
 */
bool inicializar_componentes();

/**
 * @brief Manipulador de interrupção para os botões.
 * 
 * @param gpio Pino GPIO que acionou a interrupção.
 * @param eventos Eventos ocorridos na interrupção.
 */
static void manipulador_irq_gpio(uint gpio, uint32_t eventos);

/**
 * @brief Define o padrão dos LEDs RGB com base nos valores do joystick.
 * 
 * @param x_val Valor lido do eixo X do joystick.
 * @param y_val Valor lido do eixo Y do joystick.
 */
void definir_padrao_led(uint16_t x_val, uint16_t y_val);

/**
 * @brief Desenha a borda do display de acordo com o estilo atual.
 */
void desenhar_borda();

/**
 * @brief Configura o PWM para controle de intensidade dos LEDs.
 * 
 * @param gpio Pino GPIO associado ao LED a ser configurado.
 */
void configurar_pwm(uint gpio);

int main() {
    stdio_init_all(); // Inicializa a comunicação serial
    if (!inicializar_componentes()) { // Inicializa os componentes do sistema
        printf("Erro ao inicializar os componentes.\n"); // Exibe mensagem de erro caso a inicialização falhe
        return 1; // Encerra o programa
    }

    while (true) {
        ssd1306_fill(&display, false); // Limpa o display
        desenhar_borda(); // Desenha a borda do display

        adc_select_input(0); // Seleciona o canal 0 do ADC
        uint16_t x_val = adc_read(); // Lê os valores do eixo X do joystick
        adc_select_input(1); // Seleciona o canal 1 do ADC
        uint16_t y_val = adc_read(); // Lê os valores do eixo Y do joystick
        
        uint8_t x_pos = (y_val * 112) / 4095; // Ajusta a posição do quadrado no eixo X
        uint8_t y_pos = (x_val * 56) / 4095; // Ajusta a posição do quadrado no eixo Y
        ssd1306_rect(&display, y_pos, x_pos, 10, 10, true, true); // Desenha o quadrado no display
        ssd1306_send_data(&display); // Atualiza o display
        
        definir_padrao_led(x_val, y_val); // Define o padrão dos LEDs RGB

        sleep_ms(50); // Evita que o display pisque
    }
    return 0;
}

bool inicializar_componentes() {
    adc_init(); // Inicializa o ADC
    adc_gpio_init(JOYSTICK_X); // Inicializa o pino do eixo X do joystick
    adc_gpio_init(JOYSTICK_Y); // Inicializa o pino do eixo Y do joystick

    gpio_init(BOTAO_JOY); // Inicializa o pino do botão do joystick
    gpio_set_dir(BOTAO_JOY, GPIO_IN); // Configura o pino como entrada
    gpio_pull_up(BOTAO_JOY); // Habilita o resistor de pull-up interno
    gpio_init(BOTAO_A); // Inicializa o pino do botão A
    gpio_set_dir(BOTAO_A, GPIO_IN); // Configura o pino como entrada
    gpio_pull_up(BOTAO_A); // Habilita o resistor de pull-up interno 
    
    gpio_set_irq_enabled_with_callback(BOTAO_JOY, GPIO_IRQ_EDGE_FALL, true, &manipulador_irq_gpio); // Habilita a interrupção no botão do joystick
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &manipulador_irq_gpio); // Habilita a interrupção no botão A
    
    i2c_init(I2C_PORT, 400 * 1000); // Inicializa o barramento I2C
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Configura o pino SDA como pino I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Configura o pino SCL como pino I2C
    gpio_pull_up(I2C_SDA); // Habilita o resistor de pull-up no pino SDA
    gpio_pull_up(I2C_SCL); // Habilita o resistor de pull-up no pino SCL
    
    ssd1306_init(&display, 128, 64, false, I2C_ENDERECO, I2C_PORT); // Inicializa o display OLED
    ssd1306_config(&display); // Configura o display OLED
    ssd1306_fill(&display, false); // Limpa o display
    ssd1306_send_data(&display); // Atualiza o display
    
    configurar_pwm(LED_AZUL); // Configura o PWM para o LED azul
    configurar_pwm(LED_VERMELHO); // Configura o PWM para o LED vermelho
    configurar_pwm(LED_VERDE); // Configura o PWM para o LED verde
    return true; // Retorna verdadeiro para indicar que a inicialização foi bem-sucedida
}

static void manipulador_irq_gpio(uint gpio, uint32_t eventos) {
    uint64_t agora = to_us_since_boot(get_absolute_time()); // Obtém o tempo atual em microssegundos
    if ((agora - antes) < 200000) // Debounce de 200 ms
        return;
    antes = agora; // Atualiza o tempo da última interrupção
    
    if (gpio == BOTAO_JOY) { // Verifica se a interrupção foi no botão do joystick
        printf("Botão do joystick pressionado.\n");
        estado_led_verde = !estado_led_verde; // Alterna o estado do LED verde
        printf("Estado do LED verde: %s\n", estado_led_verde ? "Ligado" : "Desligado"); // Exibe o estado do LED verde
        pwm_set_gpio_level(LED_VERDE, estado_led_verde ? 4095 : 0); // Atualiza o estado do LED verde
        estilo_borda = (estilo_borda + 1) % 2; // Alterna o estilo da borda
        printf("Estilo da borda: %s\n", estilo_borda == 0 ? "Simples" : "Dupla"); // Exibe o estilo da borda
    } else if (gpio == BOTAO_A) { // Verifica se a interrupção foi no botão A
        printf("Botão A pressionado.\n"); 
        leds_pwm_ativados = !leds_pwm_ativados; // Alterna o estado dos LEDs PWM
        printf("PWM dos LEDs: %s\n", leds_pwm_ativados ? "Ativado" : "Desativado"); // Exibe o estado dos LEDs PWM
    }
}

void definir_padrao_led(uint16_t x_val, uint16_t y_val) { 
    if (leds_pwm_ativados) { // Verifica se os LEDs PWM estão ativados
        pwm_set_gpio_level(LED_AZUL, abs((int)x_val - 2048) * 2); // Define a intensidade do LED azul
        pwm_set_gpio_level(LED_VERMELHO, abs((int)y_val - 2048) * 2); // Define a intensidade do LED vermelho
    } else {
        pwm_set_gpio_level(LED_AZUL, 0); // Desliga o LED azul
        pwm_set_gpio_level(LED_VERMELHO, 0); // Desliga o LED vermelho
    }
}

void desenhar_borda() {
    switch (estilo_borda) { // Verifica o estilo da borda
        case 0:
            ssd1306_rect(&display, 0, 0, 127, 63, true, false); // Desenha a borda simples
            break;
        case 1:
            ssd1306_rect(&display, 0, 0, 127, 63, true, false); // Desenha a borda dupla
            ssd1306_rect(&display, 1, 1, 125, 61, true, false); // Desenha a borda dupla
            break;
    }
}

void configurar_pwm(uint gpio) {
    gpio_set_function(gpio, GPIO_FUNC_PWM); // Configura o pino como saída PWM
    uint slice = pwm_gpio_to_slice_num(gpio); // Obtém o número do slice associado ao pino
    pwm_set_wrap(slice, 4095); // Define o valor máximo do contador
    pwm_set_chan_level(slice, pwm_gpio_to_channel(gpio), 0); // Define o valor inicial do contador
    pwm_set_enabled(slice, true); // Habilita o PWM
}
