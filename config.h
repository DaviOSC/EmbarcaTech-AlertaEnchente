#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define ADC_JOYSTICK_X 26
#define ADC_JOYSTICK_Y 27

#define LED_PIN_RED 13
#define LED_PIN_GREEN 11
#define LED_PIN_BLUE 12
#define BUZZER_PIN 21
#define BUTTON_PIN_B 6
#define OUT_PIN 7

#define NUM_PIXELS 25

#define BUZZER_FREQUENCY 4000/// Frequência do buzzer em Hz
#define LIGHT_TIME 4000 // Tempo de cada estado do semáforo em milissegundos


// Definições dos padrões de cores para a Matriz de LEDs
double NORMAL_PATTERN[NUM_PIXELS][3] = {
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}
};

double ALERT_PATTERN[NUM_PIXELS][3] = {
    {0, 0, 0}, {0, 0, 0}, {1, 1, 0}, {0, 0, 0}, {0, 0, 0},
    {0, 0, 0}, {1, 1, 0}, {1, 0, 0}, {1, 1, 0}, {0, 0, 0},
    {1, 1, 0}, {1, 1, 0}, {1, 0, 0}, {1, 1, 0}, {1, 1, 0},
    {1, 1, 0}, {1, 1, 0}, {1, 1, 0}, {1, 1, 0}, {1, 1, 0},
    {1, 1, 0}, {1, 1, 0}, {1, 0, 0}, {1, 1, 0}, {1, 1, 0}
};

// Função para inicializar o buzzer
void pwm_init_buzzer(uint pin) {
    // Configurar o pino como saída de PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o PWM com frequência desejada
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096)); // Divisor de clock
    pwm_init(slice_num, &config, true);

    // Iniciar o PWM no nível baixo
    pwm_set_gpio_level(pin, 0);
}

// Função para emitir um beep com duração especificada
void beep(uint pin, uint duration_ms) {
    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o duty cycle para 50% (ativo)
    pwm_set_gpio_level(pin, 2048);

    // Temporização
    sleep_ms(duration_ms);

    // Desativar o sinal PWM (duty cycle 0)
    pwm_set_gpio_level(pin, 0);
}

uint32_t matrix_rgb(double r, double g, double b)
{
    unsigned char R, G, B;
    R = r * 255;
    G = g * 255;
    B = b * 255;
    return (G << 24) | (R << 16) | (B << 8);
}

// rotina para acionar a matrix de leds
void pio_drawn(double desenho[][3], uint32_t valor_led, PIO pio, uint sm)
{
    for (int16_t i = NUM_PIXELS - 1; i >= 0; i--)
    {
        double r = desenho[i][0] * 0.1; // Reduz a intensidade da cor
        double g = desenho[i][1] * 0.1;
        double b = desenho[i][2] * 0.1;

        valor_led = matrix_rgb(r, g, b);
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}

