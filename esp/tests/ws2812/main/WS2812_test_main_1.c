#include "driver/rmt.h"
#include "driver/gpio.h"
#define LED_NUM         16       // Количество светодиодов
#define RMT_TX_CHANNEL  RMT_CHANNEL_0
#define WS2812_GPIO     8       // Пин данных
// Временные параметры (в тиках RMT, при тактовой частоте 80MHz 1 тик = 12.5 нс)
#define T0H     30      // Длительность высокого уровня для бита 0 (30 * 12.5ns = 375ns)
#define T0L     70      // Длительность низкого уровня для бита 0 (875ns)
#define T1H     70      // Длительность высокого уровня для бита 1 (875ns)
#define T1L     30      // Длительность низкого уровня для бита 1 (375ns)
#define RESET   4000    // Время сброса (50 мкс)
void setup_rmt() {
    rmt_config_t config = {
        .rmt_mode = RMT_MODE_TX,
        .channel = RMT_TX_CHANNEL,
        .gpio_num = WS2812_GPIO,
        .clk_div = 1,           // Тактовая частота 80MHz
        .mem_block_num = 1,
        .flags = 0,
        .tx_config = {
            .carrier_freq_hz = 0,
            .carrier_level = RMT_CARRIER_LEVEL_LOW,
            .idle_level = RMT_IDLE_LEVEL_LOW,
            .carrier_duty_percent = 0,
            .carrier_en = false,
            .loop_en = false,
            .idle_output_en = true,
        }
    };
    rmt_config(&config);
    rmt_driver_install(config.channel, 0, 0);
}
//```
//### 2. Преобразование цветовых данных в сигнал RMT
//Преобразуем цвет RGB в формат GRB (порядок WS2812) и сгенерируем соответствующие элементы RMT.
//```c
// Конвертация RGB в GRB (порядок WS2812)
uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}
// Заполнение массива элементов RMT
void fill_rmt_items(rmt_item32_t* item, uint32_t grb) {
    for (int bit = 23; bit >= 0; bit--) {
        uint32_t bit_val = (grb >> bit) & 1;
        item->level0 = 1;
        item->level1 = 0;
        if (bit_val) {
            item->duration0 = T1H;
            item->duration1 = T1L;
        } else {
            item->duration0 = T0H;
            item->duration1 = T0L;
        }
        item++;
    }
}
//```
//### 3. Отправка данных на светодиоды
//Построим полную последовательность сигналов RMT (включая данные всех светодиодов и сигнал сброса).
//```c
void send_led_data(uint32_t* colors, size_t num_leds) {
    size_t num_items = (num_leds * 24) + 1; // 24 бита на каждый светодиод + сигнал сброса
    rmt_item32_t* items = (rmt_item32_t*)malloc(num_items * sizeof(rmt_item32_t));
    memset(items, 0, num_items * sizeof(rmt_item32_t));
    // Заполняем данные светодиодов
    for (int i = 0; i < num_leds; i++) {
        fill_rmt_items(&items[i * 24], colors[i]);
    }
    // Добавляем сигнал сброса (длительный низкий уровень)
    items[num_leds * 24].level0 = 0;
    items[num_leds * 24].duration0 = RESET;
    items[num_leds * 24].flags = RMT_ITEM32_LAST; // Флаг окончания
    // Отправляем данные
    rmt_write_items(RMT_TX_CHANNEL, items, num_items, true);
    free(items);
}
//```
//### 4. Пример основной программы
//Управление светодиодами для отображения разных цветов.
//```c
void app_main() {
    setup_rmt();
    
    uint32_t colors[LED_NUM] = {0};
    
    // Пример: установка цветов светодиодов
    colors[0] = rgb_to_grb(255, 0, 0);    // Красный
    colors[1] = rgb_to_grb(0, 255, 0);    // Зеленый
    colors[2] = rgb_to_grb(0, 0, 255);    // Синий
    
    // Отправляем данные на светодиоды
    send_led_data(colors, LED_NUM);
    
    // Задержка и изменение цвета (пример)
    vTaskDelay(pdMS_TO_TICKS(1000));
    colors[3] = rgb_to_grb(255, 255, 0);  // Желтый
    send_led_data(colors, LED_NUM);
}
