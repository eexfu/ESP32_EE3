#include "rc522.h"

static void rc522_handler(void* arg, esp_event_base_t base, int32_t event_id, void* event_data);
esp_err_t rc522_power_down();
esp_err_t buzzer_init();
void open_buzzer();
void close_buzzer();
esp_err_t rc522_init();
esp_err_t myRC522_start();
esp_err_t detectRestartTag();