#include "sensor_input.h"
#include <math.h>

#ifdef ENABLE_PHYSICAL_SENSORS
#include "hardware/adc.h"
#include "hardware/gpio.h"
#define PIN_POTENTIOMETER  26 // ADC0
#define PIN_THERMISTOR     27 // ADC1
#define PIN_VIBRATION      28 // ADC2
#define PIN_WHEEL_SPEED    15 // Digital Pulse
#endif

static double s_sim_time = 0.0;
static double s_current_speed = 55.0;
static double s_target_speed = 75.0;

void sensor_subsystem_init(void) {
#ifdef ENABLE_PHYSICAL_SENSORS
    adc_init();
    adc_gpio_init(PIN_POTENTIOMETER);
    adc_gpio_init(PIN_THERMISTOR);
    adc_gpio_init(PIN_VIBRATION);
    gpio_init(PIN_WHEEL_SPEED);
    gpio_set_dir(PIN_WHEEL_SPEED, GPIO_IN);
#endif
    s_sim_time = 0.0;
    s_current_speed = 55.0;
    s_target_speed = 75.0;
}

void sensor_subsystem_read(double dt_seconds, vehicle_telemetry_t *out_telemetry) {
#ifdef ENABLE_PHYSICAL_SENSORS
    // 1. Read Potentiometer (ADC0) -> Throttle & Speed
    adc_select_input(0);
    uint16_t pot_raw = adc_read();
    double throttle = ((double)pot_raw / 4095.0) * 100.0;
    double speed = ((double)pot_raw / 4095.0) * 160.0;

    // 2. Read NTC Thermistor (ADC1) -> Engine Coolant Temp
    adc_select_input(1);
    uint16_t ntc_raw = adc_read();
    double temp_c = 25.0 + (((double)ntc_raw / 4095.0) * 80.0);

    // 3. Read SW-420 Vibration (ADC2) -> Chassis Shock
    adc_select_input(2);
    uint16_t vib_raw = adc_read();
    double vib_g = ((double)vib_raw / 4095.0) * 2.5;

    out_telemetry->speed_kmh = speed;
    out_telemetry->rpm = 1000.0 + (speed * 30.0);
    out_telemetry->throttle_pct = throttle;
    out_telemetry->temperature_c = temp_c;
    out_telemetry->steering_deg = 0.0;
    out_telemetry->vibration_g = vib_g;
    out_telemetry->wheel_speed_kmh = speed;
#else
    // Deterministic generated vehicle kinematics (Stage 1 Hardware Baseline)
    s_sim_time += dt_seconds;

    // Dynamic cruising speed profile between 50 and 95 km/h
    double speed_cycle = sin(s_sim_time * 0.15); // ~40s period
    double speed = 68.0 + (speed_cycle * 22.0); // 46 to 90 km/h

    double throttle = 25.0 + (cos(s_sim_time * 0.15) * 15.0);
    if (throttle < 5.0) throttle = 5.0;
    if (throttle > 85.0) throttle = 85.0;

    // RPM correlates with gear ratios and vehicle speed
    double rpm = 1600.0 + (speed * 22.5) + (sin(s_sim_time * 1.5) * 50.0);

    // Engine temperature reaches normal operating plateau (85 - 90 °C)
    double temp = 86.0 + (sin(s_sim_time * 0.05) * 3.5);

    // Steering oscillates gently within road lane (-8 to +8 degrees)
    double steer = sin(s_sim_time * 0.4) * 8.0;

    // Vibration reflects normal road roughness (0.08 to 0.16 G)
    double vib = 0.10 + (fabs(sin(s_sim_time * 3.0)) * 0.06);

    // Wheel speed matches vehicle speed with realistic tire micro-slip
    double wheel_speed = speed + (sin(s_sim_time * 0.8) * 0.4);

    out_telemetry->speed_kmh = speed;
    out_telemetry->rpm = rpm;
    out_telemetry->throttle_pct = throttle;
    out_telemetry->temperature_c = temp;
    out_telemetry->steering_deg = steer;
    out_telemetry->vibration_g = vib;
    out_telemetry->wheel_speed_kmh = wheel_speed;
#endif
}
