#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double speed_kmh;
    double rpm;
    double temperature_c;
    double throttle_pct;
    double steering_deg;
    double vibration_g;
    double wheel_speed_kmh;
} vehicle_telemetry_t;

/**
 * @brief Initialize sensor subsystem (ADC or deterministic simulation generator).
 */
void sensor_subsystem_init(void);

/**
 * @brief Advance sensor state and read current telemetry snapshot.
 * @param dt_seconds Time delta since last update
 * @param out_telemetry Output telemetry structure
 */
void sensor_subsystem_read(double dt_seconds, vehicle_telemetry_t *out_telemetry);

// Optional future hardware sensor hook flags
// #define ENABLE_PHYSICAL_POTENTIOMETER_GP26
// #define ENABLE_PHYSICAL_THERMISTOR_GP27
// #define ENABLE_PHYSICAL_VIBRATION_GP28
// #define ENABLE_PHYSICAL_SPEED_PULSE_GP15

#ifdef __cplusplus
}
#endif
