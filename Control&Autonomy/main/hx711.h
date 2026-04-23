#ifndef HX711_H
#define HX711_H

#include <stdint.h>
#include <stdbool.h>

// Initialize GPIOs for the HX711 interface.
void hx711_init();

// Read one raw 24-bit conversion from the HX711.
// Returns signed 24-bit data sign-extended to int32_t.
int32_t hx711_read_raw();

// Read multiple samples and return the average raw value.
// If samples <= 0, this falls back to 1 sample.
int32_t hx711_read_average(int samples);

// Tare helper. Call this when the platform is empty.
void hx711_tare(int samples);

// Returns raw reading relative to tare offset.
int32_t hx711_read_tared_average(int samples);

// Presence detection based on raw threshold from tared reading.
// This is for simple object-present / object-removed logic.
bool hx711_is_present();

// Optional helpers for tuning / debugging.
int32_t hx711_get_tare_offset();
void hx711_set_tare_offset(int32_t offset);

#endif // HX711_H