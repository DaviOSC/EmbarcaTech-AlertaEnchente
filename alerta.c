#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "pio_matrix.pio.h"
#include "config.h"
#include "queue.h"
#include <stdio.h>
#include "pico/bootrom.h"

// Estrutura para armazenar os dados do sensor
typedef struct {
    uint16_t water_level;
    uint16_t rain_volume;
    bool alert;
} sensor_data_t;

// Fila para compartilhar dados entre tarefas
QueueHandle_t xQueueSensorData;

void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}
void vSensorTask(void *params)
{
    // Inicializa o ADC
    adc_gpio_init(ADC_JOYSTICK_Y);
    adc_gpio_init(ADC_JOYSTICK_X);
    adc_init();
    
    sensor_data_t sensor_data;

    while (true)
    {
        adc_select_input(0); // ADC0
        uint16_t raw_agua = adc_read();
        adc_select_input(1); // ADC1
        uint16_t raw_chuva = adc_read();

        // Converte os valores lidos para porcentagem
        sensor_data.water_level = (raw_agua * 100) / 4095;
        sensor_data.rain_volume = (raw_chuva * 100) / 4095;
        // Define o alerta baseado nos valores de água e chuva
        sensor_data.alert = (sensor_data.water_level >= 70) || (sensor_data.rain_volume >= 80);
        
        printf("Water Level: %d, Rain Volume: %d, Alert: %s\n", sensor_data.water_level, sensor_data.rain_volume, sensor_data.alert ? "ON" : "OFF");
        xQueueSend(xQueueSensorData, &sensor_data,portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void vDisplayTask()
{
    // Inicializa o display OLED
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_t ssd;
    ssd1306_init(&ssd, 128, 64, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);

    sensor_data_t sensor_data;
    char line1[32], line2[32], line3[32];

    while (true)
    {
        // Recebe os dados mais recentes da fila
        if (xQueueReceive(xQueueSensorData, &sensor_data, portMAX_DELAY) == pdTRUE) {
            ssd1306_fill(&ssd, false); // Limpa o display

            ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);      // Retângulo de borda
            ssd1306_line(&ssd, 3, 23, 123, 23, true);            // Linha horizontal 1 (primeira linha após o topo)
            ssd1306_line(&ssd, 3, 43, 123, 43, true);            // Linha horizontal 2 (segunda linha, espaçamento igual)

            // Mostra os dados em tempo real
            sprintf(line1, "ALERTA: %s", sensor_data.alert ? "SIM" : "NAO");
            sprintf(line3, "Agua: %3d%%", sensor_data.water_level);
            sprintf(line2, "Chuva: %3d%%", sensor_data.rain_volume);

            ssd1306_draw_string(&ssd, line1, 8, 10);
            ssd1306_draw_string(&ssd, line2, 8, 29);
            ssd1306_draw_string(&ssd, line3, 8, 48);
            ssd1306_send_data(&ssd);
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Atualiza a cada 200ms
    }
}
void vBuzzerTask(void *params)
{
    pwm_init_buzzer(BUZZER_PIN);

    sensor_data_t sensor_data;
    while (true)
    {
        // Recebe os dados mais recentes da fila
        if (xQueueReceive(xQueueSensorData, &sensor_data, portMAX_DELAY) == pdTRUE)
        {
            if (sensor_data.alert)
            {
                // Alerta sonoro: 3 bipes curtos usando beep()
                for (int i = 0; i < 3; i++) {
                    beep(BUZZER_PIN, 150);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                vTaskDelay(pdMS_TO_TICKS(500)); // Pausa entre sequências
            }
            else
            {
                // Modo normal: buzzer desligado
                pwm_set_gpio_level(BUZZER_PIN, 0);
            }
        }
    }
}
void vLedRedTask(void *params)
{
    gpio_init(LED_PIN_RED);
    gpio_set_dir(LED_PIN_RED, GPIO_OUT);

    sensor_data_t sensor_data;
    while (true)
    {
        if (xQueueReceive(xQueueSensorData, &sensor_data, portMAX_DELAY) == pdTRUE)
        {
            if (sensor_data.alert)
            {
                // Pisca o LED vermelho no modo alerta
                gpio_put(LED_PIN_RED, 1);
                vTaskDelay(pdMS_TO_TICKS(250));
                gpio_put(LED_PIN_RED, 0);
                vTaskDelay(pdMS_TO_TICKS(250));
            }
            else
            {
                // LED vermelho desligado no modo normal
                gpio_put(LED_PIN_RED, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
    }
}
void vLedGBTask(void *params)
{
    gpio_init(LED_PIN_GREEN);
    gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);
    gpio_init(LED_PIN_BLUE);
    gpio_set_dir(LED_PIN_BLUE, GPIO_OUT);

    sensor_data_t sensor_data;
    bool toggle = false;

    while (true)
    {
        if (xQueueReceive(xQueueSensorData, &sensor_data, portMAX_DELAY) == pdTRUE)
        {
            if (!sensor_data.alert)
            {
                // Alterna entre verde e azul no modo normal
                if (toggle) {
                    gpio_put(LED_PIN_GREEN, 1);
                    gpio_put(LED_PIN_BLUE, 0);
                } else {
                    gpio_put(LED_PIN_GREEN, 0);
                    gpio_put(LED_PIN_BLUE, 1);
                }
                toggle = !toggle;
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            else
            {
                // Desliga ambos no modo alerta
                gpio_put(LED_PIN_GREEN, 0);
                gpio_put(LED_PIN_BLUE, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
    }
}
void vMatrixTask() {
    // Inicializa o PIO e carrega o programa
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &pio_matrix_program);
    uint sm = pio_claim_unused_sm(pio, true);
    pio_matrix_program_init(pio, sm, offset, OUT_PIN);

    sensor_data_t sensor_data;
    while (true) {
        if (xQueueReceive(xQueueSensorData, &sensor_data, portMAX_DELAY) == pdTRUE) {
            if (sensor_data.alert) {
                // Exibe símbolo de perigo (vermelho) na matriz em modo alerta
                pio_drawn(ALERT_PATTERN, 0, pio, sm);
            } else {
                // Exibe padrão normal (verde)
                pio_drawn(NORMAL_PATTERN, 0, pio, sm);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Atualiza o padrão a cada 100ms
    }
}
int main()
{
    // Ativa BOOTSEL via botão
    gpio_init(BUTTON_PIN_B);
    gpio_set_dir(BUTTON_PIN_B, GPIO_IN);
    gpio_pull_up(BUTTON_PIN_B);
    gpio_set_irq_enabled_with_callback(BUTTON_PIN_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    stdio_init_all();

    // Cria a fila para compartilhamento de valor do joystick
    xQueueSensorData = xQueueCreate(5, sizeof(sensor_data_t));

    // Criação das tasks principais do sistema
    xTaskCreate(vSensorTask, "Sensor Task", 256, NULL, 2, NULL);
    xTaskCreate(vDisplayTask, "Display Task", 256, NULL, 1, NULL);
    xTaskCreate(vBuzzerTask, "Buzzer Task", 256, NULL, 1, NULL);
    xTaskCreate(vLedRedTask, "LED Red Task", 256, NULL, 1, NULL);
    xTaskCreate(vLedGBTask, "LED GB Task", 256, NULL, 1, NULL);
    xTaskCreate(vMatrixTask, "Matrix Task", 256, NULL, 1, NULL);

    // Inicia o agendador
    vTaskStartScheduler();
    panic_unsupported();
}

