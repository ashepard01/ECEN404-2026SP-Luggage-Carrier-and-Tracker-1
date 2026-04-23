#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "rplidar.h"

#include <string.h>
#include <math.h>
#include <stdbool.h>

/*
    forward declaration
*/
static void lidar_task(void* arg);

/*
    Global constants defined here
*/

#define LIDAR_UART      UART_NUM_2
#define LIDAR_TX        GPIO_NUM_23
#define LIDAR_RX        GPIO_NUM_22
#define LIDAR_EN_IO     -1
#define LIDAR_BAUD      115200

#define LIDAR_PWM_GPIO  GPIO_NUM_33
#define LIDAR_PWM_DUTY  650

#define RB_SIZE         2048
#define Q_MIN           1
#define MM_MIN          1
#define MM_MAX          8000

/*
    Structs defined here
*/

typedef struct FrameBins {
    uint16_t range_mm[360];
    uint8_t quality[360];
} FrameBins;

typedef struct ByteRB {
    uint8_t buf[RB_SIZE];
    size_t head;
    size_t tail;
} ByteRB;

typedef struct Node {
    bool start;
    float angle_deg;
    float dist_mm;
    uint8_t quality;
} Node;

static ScanFrame g_frames[2];
static volatile int g_front = 0;
static uint32_t g_seq = 0;

/* debug */
lidar_dbg_t dbg = {0};

/*
    Helpers
*/

static int count_valid_bins(const FrameBins* f) {
    int count = 0;
    for (int i = 0; i < 360; ++i) {
        if (f->range_mm[i] > 0) count++;
    }
    return count;
}

static uint16_t min_near_bin(const ScanFrame* f, int center, int radius) {
    uint16_t best = 0;
    for (int k = -radius; k <= radius; ++k) {
        int idx = (center + k + 360) % 360;
        uint16_t d = f->range_mm[idx];
        if (d == 0) continue;
        if (best == 0 || d < best) best = d;
    }
    return best;
}

static inline size_t rb_count(const ByteRB* rb) {
    return (rb->head + RB_SIZE - rb->tail) % RB_SIZE;
}

static inline size_t rb_space(const ByteRB* rb) {
    return RB_SIZE - 1 - rb_count(rb);
}

static void rb_push(ByteRB* rb, const uint8_t* src, size_t n) {
    size_t space = rb_space(rb);

    if (n > space) {
        size_t need = n - space;
        rb->tail = (rb->tail + need) % RB_SIZE;
    }

    size_t first = RB_SIZE - rb->head;
    if (first > n) first = n;
    memcpy(&rb->buf[rb->head], src, first);

    size_t rem = n - first;
    if (rem) memcpy(&rb->buf[0], src + first, rem);

    rb->head = (rb->head + n) % RB_SIZE;
}

static bool rb_peek(const ByteRB* rb, size_t i, uint8_t* out) {
    if (i >= rb_count(rb)) return false;

    size_t idx = (rb->tail + i) % RB_SIZE;
    *out = rb->buf[idx];
    return true;
}

static void rb_drop(ByteRB* rb, size_t n) {
    size_t count = rb_count(rb);
    if (n > count) n = count;
    rb->tail = (rb->tail + n) % RB_SIZE;
}

static void frame_clear(FrameBins* f) {
    if (!f) return;
    memset(f->range_mm, 0, sizeof(f->range_mm));
    memset(f->quality, 0, sizeof(f->quality));
}

/*
    OA expects:
    bin 180 = forward
    bin 90..270 = front hemisphere
    so map lidar 0 deg -> bin 180
*/
static inline int bin_from_angle(float angle_deg) {
    int bin = (int)lroundf(angle_deg) % 360;
    if (bin < 0) bin += 360;
    return bin;
}

static bool try_pop_node(ByteRB* rb, Node* out) {
    if (rb_count(rb) < 5) return false;

    uint8_t b0, b1, b2, b3, b4;
    rb_peek(rb, 0, &b0);
    rb_peek(rb, 1, &b1);
    rb_peek(rb, 2, &b2);
    rb_peek(rb, 3, &b3);
    rb_peek(rb, 4, &b4);

    bool start = (b0 & 0x01) != 0;
    bool check = ((b0 >> 1) & 0x01) != 0;
    if (start == check) {
        rb_drop(rb, 1);
        return false;
    }

    uint16_t angle_q6 = ((uint16_t)b2 << 8) | b1;
    if ((angle_q6 & 0x1) == 0) {
        rb_drop(rb, 1);
        return false;
    }

    float angle_deg = (angle_q6 >> 1) / 64.0f;
    uint16_t dist_q2 = ((uint16_t)b4 << 8) | b3;
    float dist_mm = dist_q2 / 4.0f;
    uint8_t quality = b0 >> 2;

    if (dist_q2 == 0) {
        rb_drop(rb, 5);
        return false;
    }

    if (angle_deg < 0.0f || angle_deg >= 360.0f) {
        dbg.nodes_angle_bad++;
        rb_drop(rb, 5);
        return false;
    }

    out->start = start;
    out->angle_deg = angle_deg;
    out->dist_mm = dist_mm;
    out->quality = quality;

    rb_drop(rb, 5);
    dbg.nodes_total++;
    return true;
}

static void frame_bin_point(FrameBins* f, const Node* n) {
    if (!f || !n) return;

    if (n->quality < Q_MIN) {
        dbg.nodes_quality_bad++;
        return;
    }

    if (n->dist_mm < MM_MIN || n->dist_mm > MM_MAX) {
        dbg.nodes_range_bad++;
        return;
    }

    int bin = bin_from_angle(n->angle_deg);
    uint16_t curr = f->range_mm[bin];

    if (curr == 0 || n->dist_mm < curr) {
        f->range_mm[bin] = (uint16_t)lroundf(n->dist_mm);
        f->quality[bin] = n->quality;
    }

    dbg.nodes_pushed++;
}

static void publish_frame(const FrameBins* bins) {
    if (!bins) return;

    int back = 1 - g_front;
    ScanFrame* out = &g_frames[back];

    memcpy(out->range_mm, bins->range_mm, sizeof(out->range_mm));
    memcpy(out->quality, bins->quality, sizeof(out->quality));

    out->t_us = esp_timer_get_time();
    out->seq  = ++g_seq;

    g_front = back;
    dbg.frames_published++;
}

static void lidar_uart_setup(void) {
    uart_config_t uc = {
        .baud_rate = LIDAR_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB
    };

    uart_param_config(LIDAR_UART, &uc);
    uart_set_pin(LIDAR_UART, LIDAR_TX, LIDAR_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(LIDAR_UART, 8192, 0, 0, NULL, 0);
}

static void lidar_pwm_setup(void) {
    ledc_timer_config(&(ledc_timer_config_t){
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 20000,
        .clk_cfg = LEDC_AUTO_CLK
    });

    ledc_channel_config(&(ledc_channel_config_t){
        .gpio_num = LIDAR_PWM_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = LIDAR_PWM_DUTY,
        .hpoint = 0,
        .flags.output_invert = 0
    });

    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static bool lidar_start_scan_and_get_descriptor(void) {
    const uint8_t stop_cmd[2] = {0xA5, 0x25};
    const uint8_t scan_cmd[2] = {0xA5, 0x20};

    uart_flush_input(LIDAR_UART);

    uart_write_bytes(LIDAR_UART, (const char*)stop_cmd, 2);
    uart_wait_tx_done(LIDAR_UART, pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(100));
    uart_flush_input(LIDAR_UART);

    uart_write_bytes(LIDAR_UART, (const char*)scan_cmd, 2);
    uart_wait_tx_done(LIDAR_UART, pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t hdr[7] = {0};
    int got = 0;
    TickType_t t0 = xTaskGetTickCount();

    while ((xTaskGetTickCount() - t0) < pdMS_TO_TICKS(1000)) {
        uint8_t b;
        int n = uart_read_bytes(LIDAR_UART, &b, 1, pdMS_TO_TICKS(20));
        if (n != 1) continue;

        if (got == 0) {
            if (b == 0xA5) {
                hdr[got++] = b;
            }
        } else if (got == 1) {
            if (b == 0x5A) {
                hdr[got++] = b;
            } else if (b == 0xA5) {
                hdr[0] = 0xA5;
                got = 1;
            } else {
                got = 0;
            }
        } else {
            hdr[got++] = b;
            if (got == 7) break;
        }
    }

    // printf("LIDAR descriptor got=%d: ", got);
    // for (int i = 0; i < got; i++) {
    //     printf("%02X ", hdr[i]);
    // }
    // printf("\n");

    return (got >= 7 && hdr[0] == 0xA5 && hdr[1] == 0x5A);
}

/*
    Public functions
*/

void lidar_start_task(void) {
    memset(g_frames, 0, sizeof(g_frames));
    g_front = 0;
    g_seq = 0;
    xTaskCreatePinnedToCore(lidar_task, "lidar_task", 4096, NULL, 3, NULL, 0);
}

bool lidar_get_latest(ScanFrame* out) {
    if (!out) return false;

    int idx = g_front;
    memcpy(out, &g_frames[idx], sizeof(ScanFrame));

    if (idx != g_front) {
        memcpy(out, &g_frames[g_front], sizeof(ScanFrame));
    }

    return out->seq != 0;
}

bool lidar_get_latest_ranges_mm(float out_ranges[360]) {
    if (!out_ranges) return false;

    ScanFrame frame;
    if (!lidar_get_latest(&frame)) return false;

    for (int i = 0; i < 360; i++) {
        out_ranges[i] = (float)frame.range_mm[i];
    }

    return true;
}

/*
    Main task
*/

static void lidar_task(void* arg)
{
    (void)arg;

#if (LIDAR_EN_IO >= 0)
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << (uint64_t)LIDAR_EN_IO),
        .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&io);
    gpio_set_level(LIDAR_EN_IO, 1);
#endif

    lidar_pwm_setup();
    vTaskDelay(pdMS_TO_TICKS(1000));

    lidar_uart_setup();   // <-- move here, only once

    static ByteRB rb;
    static FrameBins curr;
    static uint8_t buf[64];

    for (;;) {

        memset(&rb, 0, sizeof(rb));
        frame_clear(&curr);

        bool have_started = false;
        float prev_angle_deg = -1.0f;
        bool  have_prev_angle = false;

        // retry until descriptor received
        while (!lidar_start_scan_and_get_descriptor()) {
            printf("LIDAR: bad descriptor, retrying\n");
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        

        for (;;) {
            int n = uart_read_bytes(LIDAR_UART, buf, sizeof(buf), pdMS_TO_TICKS(20));
            if (n > 0) {
                rb_push(&rb, buf, (size_t)n);
            }
            

            while (1) {
                size_t before = rb_count(&rb);
                Node node;

                if (try_pop_node(&rb, &node)) {
                    bool new_scan = false;
                
                    // if (node.start) {
                    //     dbg.start_flags++;
                    //     new_scan = true;
                    // }
                
                    if (have_prev_angle) {
                        if (prev_angle_deg > 300.0f && node.angle_deg < 60.0f) {
                            new_scan = true;
                        }
                    }
                
                    prev_angle_deg = node.angle_deg;
                    have_prev_angle = true;
                
                    if (new_scan) {
                        if (have_started) {
                            int valid = count_valid_bins(&curr);
                            if (valid > 150) {
                                publish_frame(&curr);
                            }
                        }
                        frame_clear(&curr);
                        have_started = true;
                    }
                
                    if (have_started) {
                        frame_bin_point(&curr, &node);
                    }
                
                    continue;
                }

                if (rb_count(&rb) == before) break;
            }

            static int64_t last_print = 0;
            int64_t now = esp_timer_get_time();
            if (now - last_print > 1000000) {
                last_print = now;

                // printf("FPS:%lu nodes:%lu pushed:%lu start:%lu angle_bad:%lu q_bad:%lu range_bad:%lu rb:%u\n",
                //         dbg.frames_published,
                //         dbg.nodes_total,
                //         dbg.nodes_pushed,
                //         dbg.start_flags,
                //         dbg.nodes_angle_bad,
                //         dbg.nodes_quality_bad,
                //         dbg.nodes_range_bad,
                //         (unsigned)rb_count(&rb));

                // printf("bins: 0=%u 90=%u 180=%u 270=%u\n",
                //     min_near_bin(&g_frames[g_front], 0, 2),
                //     min_near_bin(&g_frames[g_front], 90, 2),
                //     min_near_bin(&g_frames[g_front], 180, 2),
                //     min_near_bin(&g_frames[g_front], 270, 2));

                dbg.frames_published = 0;
                dbg.nodes_total = 0;
                dbg.nodes_pushed = 0;
                dbg.start_flags = 0;
                dbg.nodes_angle_bad = 0;
                dbg.nodes_quality_bad = 0;
                dbg.nodes_range_bad = 0;
            }

            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
}