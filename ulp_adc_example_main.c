/*
   Technicka univerzita v Kosiciach
   Diplomová práca: Nizko-prikonovy senzorovy uzol na baze ESP32
   Meno: Tomas Balog
   Datum: Februar 16, 2021

   ULP - Experimentalna verzia meranie ADC "Originalna verzia"
  ------------------------------------------------------------------------------
   Tento kod sluzi hlavne na otestovanie funkcnosti ULP
   V tomto kode je meranie ADC menej ako 1,5 V alebo viac
   Opisane su v tomto kode hlavne funkcie kodu
   Tento kod je experimentalnym prikladom v Diplomovej praci
  ------------------------------------------------------------------------------


   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
  ------------------------------------------------------------------------------
*/

#include <stdio.h>
#include <string.h>
#include "esp_sleep.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "esp32/ulp.h"
#include "ulp_main.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

											/*  Táto funkcia sa vyvola hned po obnoveni napajania, aby sa do nej nacital program ULP
  											* Pamat RTC a konfiguracia ADC. */

static void init_ulp_program(void);

											/* Táto funkcia sa vola vzdy pred prechodom do hlbokého spánku.
 											  Spusti program ULP a vynuluje pocitadlo merania "counter" */
static void start_ulp_program(void);
											/* App_main hlavna funkcia kde modul ESP32 ide najprv do hlbokeho spanku
											potom sa prebudi zmera hodnotu ADC vypise ju a vyhodnoti menej 1,5 V alebo viac ako 1,5 V */
void app_main(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_ULP) {
        printf("Not ULP wakeup\n");
        init_ulp_program();
    } else {
        printf("Deep sleep wakeup\n");
        printf("ULP did %d measurements since last reset\n", ulp_sample_counter & UINT16_MAX);
        printf("Thresholds:  low=%d  high=%d\n", ulp_low_thr, ulp_high_thr);
        ulp_last_result &= UINT16_MAX;
        printf("Value=%d was %s threshold\n", ulp_last_result,
                ulp_last_result < ulp_low_thr ? "below" : "above");
    }											/* Znovu modul ESP32 ide spat a startuje program ULP*/
    printf("Entering deep sleep\n\n");
    start_ulp_program();
    ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup() );
    esp_deep_sleep_start();
}


											/* nacitava sa binarny kod o velkosti 32 bit */

static void init_ulp_program(void)
{
    esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,
            (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
    ESP_ERROR_CHECK(err);

     											/* Konfigurácia ADC kanalov */

    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
#if CONFIG_IDF_TARGET_ESP32
    adc1_config_width(ADC_WIDTH_BIT_12);
#elif CONFIG_IDF_TARGET_ESP32S2
    adc1_config_width(ADC_WIDTH_BIT_13);
#endif
    adc1_ulp_enable();

    											/* Nastavenie nizkej a vysokej prahovej hodnoty 1,35 V - 1,75 V */

    ulp_low_thr = 1500;
    ulp_high_thr = 2000;

    /* Nastavenie doby prebudenia ULP na 1000ms */
    ulp_set_wakeup_period(0, 1000000);

    											/* Odpojte GPIO12 a GPIO15, aby ste odstranili prudovy tok
      											pullup / pulldown. */


    rtc_gpio_isolate(GPIO_NUM_12);
    rtc_gpio_isolate(GPIO_NUM_15);
                                                /* Potlacenie bootovacich sprav */
#if CONFIG_IDF_TARGET_ESP32

    esp_deep_sleep_disable_rom_logging();
#endif
}

static void start_ulp_program(void)
{

    ulp_sample_counter = 0;    								/* Resetovanie counteru*/

    											/* Startovanie programu pomocou RTC slow memory*/
    esp_err_t err = ulp_run(&ulp_entry - RTC_SLOW_MEM);
    ESP_ERROR_CHECK(err);
}
