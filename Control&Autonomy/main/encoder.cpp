
#include "encoder.h"
#include "esp_timer.h"


// use pulse_cnt.h instead of pcnt.h
// https://github.com/espressif/esp-idf/tree/v6.0/examples/peripherals/pcnt/rotary_encoder
// this is an example doing exactly what you want
// This either handles overflow using pcnt
// or idk
// have chat look at that example u might have to download the files and feed it to it
// or just copy what the example does manually
// Glitch filter depends on the datasheet on the link u sent me i think its like the rise n fall time spec



// ----------------------------
// Pin Definitions (adjust)
// ----------------------------
static constexpr gpio_num_t PIN_L_A = GPIO_NUM_16;
static constexpr gpio_num_t PIN_L_B = GPIO_NUM_32;
static constexpr gpio_num_t PIN_R_A = GPIO_NUM_27;
static constexpr gpio_num_t PIN_R_B = GPIO_NUM_18;

// ----------------------------
// Encoder Parameters
// ----------------------------
static constexpr float PPR = 2048.0f;
static constexpr float CPR = PPR * 4.0f;          // 4x quadrature = 8192 counts/rev
static constexpr float WHEEL_RADIUS = 0.1283f;   // meters

// Large limits are recommended for overflow compensation
static constexpr int PCNT_HIGH_LIMIT = 30000;
static constexpr int PCNT_LOW_LIMIT  = -30000;

// Glitch filter in nanoseconds. Keep this well below the shortest real pulse width.
static constexpr uint32_t GLITCH_FILTER_NS = 100;



//encoder update
static constexpr float VEL_WINDOW_S = 0.10f;   // 100 ms
static constexpr float VEL_ALPHA = 0.35f;
static constexpr float VEL_DEADBAND_MPS = 0.015f;

static float s_left_accum_dt = 0.0f;
static float s_right_accum_dt = 0.0f;
static int32_t s_left_accum_counts = 0;
static int32_t s_right_accum_counts = 0;
static int64_t s_last_update_us = 0;



// ----------------------------
// Optional no-op watch callback
// ----------------------------
// Registering a callback guarantees the driver's interrupt service is installed
// when the unit is enabled. We do not use events directly in the control loop.
static bool IRAM_ATTR pcnt_on_reach(pcnt_unit_handle_t,
                                    const pcnt_watch_event_data_t *,
                                    void *)
{
    return false;
}

// ----------------------------
// Configure one PCNT unit for 4x quadrature decode
// ----------------------------
static esp_err_t setup_encoder_unit(EncoderState &enc,
                                    gpio_num_t pin_a,
                                    gpio_num_t pin_b)
{
    pcnt_unit_config_t unit_config = {
        .low_limit = PCNT_LOW_LIMIT,
        .high_limit = PCNT_HIGH_LIMIT,        .intr_priority = 0,
        .flags = {
            .accum_count = 1,
        },
    };
    ESP_RETURN_ON_ERROR(pcnt_new_unit(&unit_config, &enc.unit), "ENC", "pcnt_new_unit failed");

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = GLITCH_FILTER_NS,
    };
    ESP_RETURN_ON_ERROR(pcnt_unit_set_glitch_filter(enc.unit, &filter_config), "ENC", "pcnt_unit_set_glitch_filter failed");

    // Channel A: edge=A, level=B
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = pin_a,
        .level_gpio_num = pin_b,
        .flags = {
            .invert_edge_input = 0,
            .invert_level_input = 0,
        },
    };
    ESP_RETURN_ON_ERROR(pcnt_new_channel(enc.unit, &chan_a_config, &enc.chan_a), "ENC", "pcnt_new_channel A failed");

    // Channel B: edge=B, level=A
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = pin_b,
        .level_gpio_num = pin_a,
        .flags = {
            .invert_edge_input = 0,
            .invert_level_input = 0,
        },
    };
    ESP_RETURN_ON_ERROR(pcnt_new_channel(enc.unit, &chan_b_config, &enc.chan_b), "ENC", "pcnt_new_channel B failed");

    // Same 4x decode logic as your old implementation
    ESP_RETURN_ON_ERROR(
        pcnt_channel_set_edge_action(
            enc.chan_a,
            PCNT_CHANNEL_EDGE_ACTION_INCREASE,
            PCNT_CHANNEL_EDGE_ACTION_DECREASE),
        "ENC", "pcnt_channel_set_edge_action A failed");

    ESP_RETURN_ON_ERROR(
        pcnt_channel_set_level_action(
            enc.chan_a,
            PCNT_CHANNEL_LEVEL_ACTION_KEEP,
            PCNT_CHANNEL_LEVEL_ACTION_INVERSE),
        "ENC", "pcnt_channel_set_level_action A failed");

    ESP_RETURN_ON_ERROR(
        pcnt_channel_set_edge_action(
            enc.chan_b,
            PCNT_CHANNEL_EDGE_ACTION_INCREASE,
            PCNT_CHANNEL_EDGE_ACTION_DECREASE),
        "ENC", "pcnt_channel_set_edge_action B failed");

    ESP_RETURN_ON_ERROR(
        pcnt_channel_set_level_action(
            enc.chan_b,
            PCNT_CHANNEL_LEVEL_ACTION_INVERSE,
            PCNT_CHANNEL_LEVEL_ACTION_KEEP),
        "ENC", "pcnt_channel_set_level_action B failed");

    // Required for overflow compensation
    ESP_RETURN_ON_ERROR(pcnt_unit_add_watch_point(enc.unit, PCNT_HIGH_LIMIT), "ENC", "add high watch point failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_add_watch_point(enc.unit, PCNT_LOW_LIMIT), "ENC", "add low watch point failed");

    // Install a no-op callback so the ISR service is definitely enabled
    pcnt_event_callbacks_t cbs = {
        .on_reach = pcnt_on_reach,
    };
    ESP_RETURN_ON_ERROR(pcnt_unit_register_event_callbacks(enc.unit, &cbs, nullptr), "ENC", "register callbacks failed");

    // Recommended after adding watch points
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(enc.unit), "ENC", "clear count failed");

    ESP_RETURN_ON_ERROR(pcnt_unit_enable(enc.unit), "ENC", "enable unit failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_start(enc.unit), "ENC", "start unit failed");

    enc.count = 0;
    enc.last_count = 0;
    enc.delta_count = 0;
    enc.velocity_mps = 0.0f;

    return ESP_OK;
}

// ----------------------------
// Public API
// ----------------------------
extern "C" void encoder_init()
{
    // Optional. PCNT can own the GPIO routing itself, but keeping input config is fine.
    gpio_set_direction(PIN_L_A, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_L_B, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_R_A, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_R_B, GPIO_MODE_INPUT);

    // Leave pull-ups disabled unless your level shifter/output stage needs them.
    // gpio_pullup_dis(PIN_L_A);
    // gpio_pullup_dis(PIN_L_B);
    // gpio_pullup_dis(PIN_R_A);
    // gpio_pullup_dis(PIN_R_B);

    ESP_ERROR_CHECK(setup_encoder_unit(enc_left, PIN_L_A, PIN_L_B));
    ESP_ERROR_CHECK(setup_encoder_unit(enc_right, PIN_R_A, PIN_R_B));

    s_last_update_us = esp_timer_get_time();
}

extern "C" void encoder_update(float dt_unused)
{
    (void)dt_unused;

    const int64_t now_us = esp_timer_get_time();

    if (s_last_update_us == 0) {
        s_last_update_us = now_us;
        return;
    }

    const float dt = (now_us - s_last_update_us) * 1e-6f;
    s_last_update_us = now_us;

    if (dt <= 0.0f) return;

    ESP_ERROR_CHECK(pcnt_unit_get_count(enc_left.unit, &enc_left.count));
    ESP_ERROR_CHECK(pcnt_unit_get_count(enc_right.unit, &enc_right.count));

    enc_left.delta_count  = enc_left.count  - enc_left.last_count;
    enc_right.delta_count = enc_right.count - enc_right.last_count;

    enc_left.last_count  = enc_left.count;
    enc_right.last_count = enc_right.count;

    s_left_accum_counts  += enc_left.delta_count;
    s_right_accum_counts += enc_right.delta_count;
    s_left_accum_dt      += dt;
    s_right_accum_dt     += dt;

    if (s_left_accum_dt >= VEL_WINDOW_S) {
        const float rev_left = static_cast<float>(s_left_accum_counts) / CPR;
        const float raw_left_mps =
            (rev_left * 2.0f * static_cast<float>(M_PI) / s_left_accum_dt) * WHEEL_RADIUS;

        enc_left.velocity_mps =
            VEL_ALPHA * raw_left_mps + (1.0f - VEL_ALPHA) * enc_left.velocity_mps;

        if (fabsf(enc_left.velocity_mps) < VEL_DEADBAND_MPS) {
            enc_left.velocity_mps = 0.0f;
        }

        s_left_accum_counts = 0;
        s_left_accum_dt = 0.0f;
    }

    if (s_right_accum_dt >= VEL_WINDOW_S) {
        const float rev_right = static_cast<float>(s_right_accum_counts) / CPR;
        const float raw_right_mps =
            (rev_right * 2.0f * static_cast<float>(M_PI) / s_right_accum_dt) * WHEEL_RADIUS;

        enc_right.velocity_mps =
            VEL_ALPHA * raw_right_mps + (1.0f - VEL_ALPHA) * enc_right.velocity_mps;

        if (fabsf(enc_right.velocity_mps) < VEL_DEADBAND_MPS) {
            enc_right.velocity_mps = 0.0f;
        }

        s_right_accum_counts = 0;
        s_right_accum_dt = 0.0f;
    }
}

extern "C" float encoder_get_left_velocity()
{
    return -enc_left.velocity_mps;
}

extern "C" float encoder_get_right_velocity()
{
    return enc_right.velocity_mps;
}

extern "C" int32_t encoder_get_left_count()
{
    return -enc_left.count;
}

extern "C" int32_t encoder_get_right_count()
{
    return enc_right.count;
}

extern "C" void encoder_zero()
{
    ESP_ERROR_CHECK(pcnt_unit_clear_count(enc_left.unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(enc_right.unit));

    enc_left.count = 0;
    enc_right.count = 0;
    enc_left.last_count = 0;
    enc_right.last_count = 0;
    enc_left.delta_count = 0;
    enc_right.delta_count = 0;
    enc_left.velocity_mps = 0.0f;
    enc_right.velocity_mps = 0.0f;
    s_left_accum_dt = 0.0f;
    s_right_accum_dt = 0.0f;
    s_left_accum_counts = 0;
    s_right_accum_counts = 0;
    s_last_update_us = esp_timer_get_time();

    s_last_update_us = esp_timer_get_time();
}
