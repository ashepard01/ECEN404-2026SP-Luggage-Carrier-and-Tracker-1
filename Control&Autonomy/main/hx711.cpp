#include "hx711.h"

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

namespace
{
    constexpr const char* TAG = "HX711";

    // Change these later to your actual chosen GPIOs.
    constexpr gpio_num_t HX711_DOUT_GPIO = GPIO_NUM_34;
    constexpr gpio_num_t HX711_SCK_GPIO  = GPIO_NUM_13;

    // Tune these later.
    constexpr int HX711_DEFAULT_AVG_SAMPLES = 2;

    // For presence detection after tare.
    // Adjust this based on real raw readings.
    constexpr int32_t WEIGHT_PRESENT_THRESHOLD_RAW = 10000;

    // Some load cells / wiring arrangements make added weight increase raw value.
    // Others make it decrease raw value.
    // Flip this if your threshold comparison is backwards.
    constexpr bool PRESENT_WHEN_ABOVE_THRESHOLD = true;

    // Optional timeout so the system does not block forever waiting for DOUT low.
    constexpr TickType_t HX711_READY_TIMEOUT_TICKS = pdMS_TO_TICKS(100);

    int32_t g_tare_offset = 0;

    bool hx711_wait_ready()
    {
        TickType_t start = xTaskGetTickCount();

        while (gpio_get_level(HX711_DOUT_GPIO) != 0)
        {
            if ((xTaskGetTickCount() - start) > HX711_READY_TIMEOUT_TICKS)
            {
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        return true;
    }

    void hx711_clock_pulse()
    {
        gpio_set_level(HX711_SCK_GPIO, 1);
        esp_rom_delay_us(1);
        gpio_set_level(HX711_SCK_GPIO, 0);
        esp_rom_delay_us(1);
    }
}

void hx711_init()
{
    gpio_config_t dout_cfg = {};
    dout_cfg.pin_bit_mask = (1ULL << HX711_DOUT_GPIO);
    dout_cfg.mode = GPIO_MODE_INPUT;
    dout_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    dout_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    dout_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&dout_cfg);

    gpio_config_t sck_cfg = {};
    sck_cfg.pin_bit_mask = (1ULL << HX711_SCK_GPIO);
    sck_cfg.mode = GPIO_MODE_OUTPUT;
    sck_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    sck_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    sck_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&sck_cfg);

    gpio_set_level(HX711_SCK_GPIO, 0);

    ESP_LOGI(TAG, "HX711 initialized. DOUT=%d SCK=%d",
             static_cast<int>(HX711_DOUT_GPIO),
             static_cast<int>(HX711_SCK_GPIO));
}

int32_t hx711_read_raw()
{
    if (!hx711_wait_ready())
    {
        ESP_LOGW(TAG, "Timeout waiting for HX711 data ready");
        return 0;
    }

    uint32_t value = 0;

    // Read 24 bits, MSB first.
    for (int i = 0; i < 24; ++i)
    {
        gpio_set_level(HX711_SCK_GPIO, 1);
        esp_rom_delay_us(1);

        value = (value << 1) | static_cast<uint32_t>(gpio_get_level(HX711_DOUT_GPIO));

        gpio_set_level(HX711_SCK_GPIO, 0);
        esp_rom_delay_us(1);
    }

    // 25th pulse selects Channel A, Gain 128 for next conversion.
    hx711_clock_pulse();

    // Sign-extend 24-bit two's complement to 32-bit signed.
    if (value & 0x800000)
    {
        value |= 0xFF000000;
    }

    return static_cast<int32_t>(value);
}

int32_t hx711_read_average(int samples)
{
    if (samples <= 0)
    {
        samples = 1;
    }

    int64_t sum = 0;

    for (int i = 0; i < samples; ++i)
    {
        sum += static_cast<int64_t>(hx711_read_raw());
    }

    return static_cast<int32_t>(sum / samples);
}

void hx711_tare(int samples)
{
    g_tare_offset = hx711_read_average(samples > 0 ? samples : HX711_DEFAULT_AVG_SAMPLES);
    ESP_LOGI(TAG, "Tare offset set to %ld", static_cast<long>(g_tare_offset));
}

int32_t hx711_read_tared_average(int samples)
{
    return hx711_read_average(samples > 0 ? samples : HX711_DEFAULT_AVG_SAMPLES) - g_tare_offset;
}

bool hx711_is_present()
{
    int32_t reading = hx711_read_tared_average(HX711_DEFAULT_AVG_SAMPLES);

    if (PRESENT_WHEN_ABOVE_THRESHOLD)
    {
        return reading > WEIGHT_PRESENT_THRESHOLD_RAW;
    }

    return reading < -WEIGHT_PRESENT_THRESHOLD_RAW;
}

int32_t hx711_get_tare_offset()
{
    return g_tare_offset;
}

void hx711_set_tare_offset(int32_t offset)
{
    g_tare_offset = offset;
}