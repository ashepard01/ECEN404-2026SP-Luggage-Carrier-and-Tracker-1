#include "buzzer.h"

#include "driver/gpio.h"
#include "esp_log.h"

namespace
{
    constexpr const char* TAG = "BUZZER";
    constexpr gpio_num_t BUZZER_GPIO = GPIO_NUM_19;
    constexpr bool BUZZER_ACTIVE_HIGH = true;

    constexpr uint32_t TRIPLET_MS   = 120;
    constexpr uint32_t NOTE_ON_MS   = 120;
    constexpr uint32_t NOTE_GAP_MS  = 40;

    struct BuzzerStep
    {
        bool on;
        uint32_t duration_ms;
    };

    constexpr BuzzerStep kPattern[] = {
        // 4 triplet notes
        {true,  NOTE_ON_MS}, {false, NOTE_GAP_MS},
        {true,  NOTE_ON_MS}, {false, NOTE_GAP_MS},
        {true,  NOTE_ON_MS}, {false, NOTE_GAP_MS},
        {true,  NOTE_ON_MS}, {false, NOTE_GAP_MS},

        // 1 triplet rest
        {false, TRIPLET_MS},

        // 2 triplet notes
        {true,  NOTE_ON_MS}, {false, NOTE_GAP_MS},
        {true,  NOTE_ON_MS}, {false, NOTE_GAP_MS},

        // 1 triplet rest
        {false, TRIPLET_MS},

        // 2 triplet notes
        {true,  NOTE_ON_MS}, {false, NOTE_GAP_MS},
        {true,  NOTE_ON_MS}, {false, NOTE_GAP_MS},

        // 2 triplet rests
        {false, TRIPLET_MS},
        {false, TRIPLET_MS},
    };

    constexpr size_t kPatternLength = sizeof(kPattern) / sizeof(kPattern[0]);

    bool g_buzzer_on = false;
    bool g_beeping_enabled = false;
    uint32_t g_last_step_ms = 0;
    size_t g_pattern_index = 0;

    void buzzer_apply_output(bool on)
    {
        g_buzzer_on = on;

        int level = 0;
        if (BUZZER_ACTIVE_HIGH)
        {
            level = on ? 1 : 0;
        }
        else
        {
            level = on ? 0 : 1;
        }

        gpio_set_level(BUZZER_GPIO, level);
    }

    void buzzer_apply_current_pattern_step()
    {
        buzzer_apply_output(kPattern[g_pattern_index].on);
    }
}

void buzzer_init()
{
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << BUZZER_GPIO);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    buzzer_off();

    ESP_LOGI(TAG, "Buzzer initialized on GPIO %d", static_cast<int>(BUZZER_GPIO));
}

void buzzer_on()
{
    g_beeping_enabled = false;
    buzzer_apply_output(true);
}

void buzzer_off()
{
    g_beeping_enabled = false;
    buzzer_apply_output(false);
}

bool buzzer_is_on()
{
    return g_buzzer_on;
}

void buzzer_start_beep()
{
    g_beeping_enabled = true;
    g_pattern_index = 0;
    g_last_step_ms = 0;
    buzzer_apply_current_pattern_step();
}

void buzzer_stop_beep()
{
    g_beeping_enabled = false;
    buzzer_apply_output(false);
}

bool buzzer_is_beeping()
{
    return g_beeping_enabled;
}

void buzzer_update(uint32_t now_ms)
{
    if (!g_beeping_enabled)
    {
        return;
    }

    if (g_last_step_ms == 0)
    {
        g_last_step_ms = now_ms;
        return;
    }

    const uint32_t step_duration = kPattern[g_pattern_index].duration_ms;

    if ((now_ms - g_last_step_ms) >= step_duration)
    {
        g_pattern_index = (g_pattern_index + 1) % kPatternLength;
        buzzer_apply_current_pattern_step();
        g_last_step_ms = now_ms;
    }
}