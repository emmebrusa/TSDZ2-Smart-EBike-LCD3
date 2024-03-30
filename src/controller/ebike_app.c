/*
 * TongSheng TSDZ2 motor controller firmware
 *
 * Copyright (C) Casainho, Leon, MSpider65 2020.
 *
 * Released under the GPL License, Version 3
 */

#include "ebike_app.h"
#include <stdint.h>
#include <stdio.h>
#include "stm8s.h"
#include "stm8s_gpio.h"
#include "main.h"
#include "interrupts.h"
#include "adc.h"
#include "motor.h"
#include "pwm.h"
#include "uart.h"
#include "brake.h"
#include "lights.h"
#include "common.h"

// Initial configuration values
volatile struct_configuration_variables m_configuration_variables = {
        .ui16_battery_low_voltage_cut_off_x10 = 300, // 36 V battery, 30.0V (3.0 * 10)
        .ui16_wheel_perimeter = 2050,                // 26'' wheel: 2050 mm perimeter
        .ui8_wheel_speed_max = 25,                   // 25 Km/h
        .ui8_motor_inductance_x1048576 = 80,         // 36V motor 76 uH
        .ui8_pedal_torque_per_10_bit_ADC_step_x100 = 67,
        .ui8_target_battery_max_power_div25 = 20,    // 500W (500/25 = 20)
        .ui8_optional_ADC_function = 0               // 0 = no function
        };

// system
uint8_t ui8_assist_level_flag = 0;
static uint8_t ui8_riding_mode = OFF_MODE;
static uint8_t ui8_riding_mode_parameter = 0;
static uint8_t ui8_system_state = NO_ERROR;
volatile uint8_t ui8_motor_enabled = 1;
static uint8_t ui8_assist_without_pedal_rotation_threshold = 0;
static uint8_t ui8_assist_without_pedal_rotation_enabled = 0;
static uint8_t ui8_lights_configuration = 0;
static uint8_t ui8_assist_with_error_enabled = 0;
uint8_t ui8_lights_state = 0;
uint8_t ui8_field_weakening_enabled = 0;
static uint8_t ui8_battery_overcurrent = 0;

// power control
static uint8_t ui8_battery_current_max = DEFAULT_VALUE_BATTERY_CURRENT_MAX;
static uint8_t ui8_duty_cycle_ramp_up_inverse_step = PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_DEFAULT;
static uint8_t ui8_duty_cycle_ramp_up_inverse_step_default = PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_DEFAULT;
static uint8_t ui8_duty_cycle_ramp_down_inverse_step = PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_DEFAULT;
static uint8_t ui8_duty_cycle_ramp_down_inverse_step_default = PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_DEFAULT;
static uint16_t ui16_battery_voltage_filtered_x1000 = 0;
static uint8_t ui8_battery_current_filtered_x10 = 0;
static uint8_t ui8_adc_battery_current_max = ADC_10_BIT_BATTERY_CURRENT_MAX;
static uint8_t ui8_adc_battery_current_target = 0;
static uint8_t ui8_duty_cycle_target = 0;
volatile uint8_t ui8_adc_motor_phase_current_max = ADC_10_BIT_MOTOR_PHASE_CURRENT_MAX;

// cadence sensor
uint16_t ui16_cadence_ticks_count_min_speed_adj = CADENCE_SENSOR_CALC_COUNTER_MIN;
static uint8_t ui8_pedal_cadence_RPM = 0;
uint8_t ui8_pedal_cadence_fast_stop = 0;

// torque sensor
static uint16_t ui16_adc_pedal_torque_offset = ADC_TORQUE_SENSOR_OFFSET_DEFAULT;
static uint16_t ui16_adc_pedal_torque_offset_init = ADC_TORQUE_SENSOR_OFFSET_DEFAULT;
static uint16_t ui16_adc_pedal_torque_offset_cal = ADC_TORQUE_SENSOR_OFFSET_DEFAULT;
//static uint16_t ui16_adc_pedal_torque_offset_temp = ADC_TORQUE_SENSOR_OFFSET_DEFAULT;
static uint8_t ui8_adc_torque_calibration_offset = 0;
static uint8_t ui8_adc_torque_middle_offset_adj = 0;
static uint8_t ui8_adc_pedal_torque_offset_adj = 0;
static uint8_t ui8_adc_pedal_torque_delta_adj = 0;
static uint8_t ui8_adc_pedal_torque_range_adj = 0;
static uint16_t ui16_adc_pedal_torque_range = 0;
static uint16_t ui16_adc_pedal_torque_range_ingrease_x100 = 0;
static uint8_t ui8_adc_pedal_torque_angle_adj = 0;
static uint16_t ui16_adc_pedal_torque_range_target_max = 0;
static uint16_t ui16_adc_pedal_torque_with_weight = 0;
static uint8_t ui8_pedal_torque_ADC_step_calc_x100 = 0;
uint16_t ui16_adc_coaster_brake_threshold = 0;
static uint8_t ui8_coaster_brake_enabled = 0;
static uint8_t ui8_coaster_brake_torque_threshold = 0;
static uint16_t ui16_adc_pedal_torque = 0;
static uint16_t ui16_adc_pedal_torque_delta = 0;
static uint16_t ui16_adc_pedal_torque_delta_temp = 0;
static uint16_t ui16_adc_pedal_torque_delta_no_boost = 0;
static uint16_t ui16_pedal_torque_x100 = 0;
//static uint16_t ui16_human_power_x10 = 0;
static uint8_t ui8_torque_sensor_calibration_enabled = 0;
static uint8_t ui8_hybrid_torque_parameter = 0;

// wheel speed sensor
static uint16_t ui16_wheel_speed_x10 = 0;

// throttle control
volatile uint8_t ui8_throttle_adc = 0;

// motor temperature control
static uint16_t ui16_adc_motor_temperature_filtered = 0;
static uint16_t ui16_motor_temperature_filtered_x10 = 0;
static uint8_t ui8_motor_temperature_max_value_to_limit = 0;
static uint8_t ui8_motor_temperature_min_value_to_limit = 0;
static uint8_t ui8_temperature_current_limiting_value = 0;

// eMTB assist
#define eMTB_POWER_FUNCTION_ARRAY_SIZE      241

static const uint8_t ui8_eMTB_power_function_160[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5,
        5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 11, 11, 11, 11, 12,
        12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18,
        19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 22, 22, 22, 22, 23, 23, 23, 24, 24, 24, 24, 25, 25, 25, 26, 26, 26, 27,
        27, 27, 27, 28, 28, 28, 29, 29, 29, 30, 30, 30, 31, 31, 31, 32, 32, 32, 33, 33, 33, 34, 34, 34, 35, 35, 35, 36,
        36, 36, 37, 37, 37, 38, 38, 38, 39, 39, 40, 40, 40, 41, 41, 41, 42, 42, 42, 43, 43, 44, 44, 44, 45, 45, 45, 46,
        46, 47, 47, 47, 48, 48, 48, 49, 49, 50, 50, 50, 51, 51, 52, 52, 52, 53, 53, 54, 54, 54, 55, 55, 56, 56, 56, 57,
        57, 58, 58, 58, 59, 59, 60, 60, 61, 61, 61, 62, 62, 63, 63, 63, 64, 64 };
static const uint8_t ui8_eMTB_power_function_165[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6,
        6, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 14, 14,
        14, 14, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 20, 20, 20, 21, 21, 21, 22, 22, 22, 23,
        23, 23, 24, 24, 24, 25, 25, 25, 26, 26, 27, 27, 27, 28, 28, 28, 29, 29, 30, 30, 30, 31, 31, 32, 32, 32, 33, 33,
        34, 34, 34, 35, 35, 36, 36, 36, 37, 37, 38, 38, 39, 39, 39, 40, 40, 41, 41, 42, 42, 42, 43, 43, 44, 44, 45, 45,
        46, 46, 47, 47, 47, 48, 48, 49, 49, 50, 50, 51, 51, 52, 52, 53, 53, 54, 54, 55, 55, 56, 56, 57, 57, 58, 58, 59,
        59, 60, 60, 61, 61, 62, 62, 63, 63, 64, 64, 65, 65, 66, 66, 67, 67, 68, 68, 69, 69, 70, 71, 71, 72, 72, 73, 73,
        74, 74, 75, 75, 76, 77, 77, 78, 78, 79, 79, 80, 81, 81, 82, 82, 83, 83, 84, 85 };
static const uint8_t ui8_eMTB_power_function_170[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7,
        7, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 16, 16, 16,
        17, 17, 18, 18, 18, 19, 19, 19, 20, 20, 21, 21, 21, 22, 22, 23, 23, 23, 24, 24, 25, 25, 26, 26, 26, 27, 27, 28,
        28, 29, 29, 30, 30, 30, 31, 31, 32, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 38, 39, 39, 40, 40, 41, 41,
        42, 42, 43, 43, 44, 45, 45, 46, 46, 47, 47, 48, 48, 49, 49, 50, 51, 51, 52, 52, 53, 53, 54, 55, 55, 56, 56, 57,
        58, 58, 59, 59, 60, 61, 61, 62, 63, 63, 64, 64, 65, 66, 66, 67, 68, 68, 69, 70, 70, 71, 71, 72, 73, 73, 74, 75,
        75, 76, 77, 77, 78, 79, 80, 80, 81, 82, 82, 83, 84, 84, 85, 86, 87, 87, 88, 89, 89, 90, 91, 92, 92, 93, 94, 94,
        95, 96, 97, 97, 98, 99, 100, 100, 101, 102, 103, 103, 104, 105, 106, 107, 107, 108, 109, 110, 110, 111 };
static const uint8_t ui8_eMTB_power_function_175[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
        1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 8, 8, 8, 8, 9,
        9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 16, 16, 17, 17, 17, 18, 18, 19, 19, 20,
        20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 31, 31, 32, 32, 33, 33, 34,
        34, 35, 36, 36, 37, 37, 38, 39, 39, 40, 40, 41, 42, 42, 43, 44, 44, 45, 45, 46, 47, 47, 48, 49, 49, 50, 51, 51,
        52, 53, 53, 54, 55, 56, 56, 57, 58, 58, 59, 60, 61, 61, 62, 63, 64, 64, 65, 66, 67, 67, 68, 69, 70, 70, 71, 72,
        73, 74, 74, 75, 76, 77, 78, 78, 79, 80, 81, 82, 83, 83, 84, 85, 86, 87, 88, 88, 89, 90, 91, 92, 93, 94, 95, 95,
        96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
        118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139,
        140, 141, 142, 143, 144, 145, 146 };
static const uint8_t ui8_eMTB_power_function_180[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
        1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10,
        11, 11, 11, 12, 12, 13, 13, 14, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 23, 23, 24,
        24, 25, 25, 26, 27, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33, 34, 34, 35, 36, 36, 37, 38, 38, 39, 40, 41, 41, 42,
        43, 43, 44, 45, 46, 46, 47, 48, 49, 50, 50, 51, 52, 53, 54, 54, 55, 56, 57, 58, 59, 59, 60, 61, 62, 63, 64, 65,
        66, 67, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92,
        93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 105, 106, 107, 108, 109, 110, 111, 112, 114, 115, 116, 117, 118,
        119, 120, 122, 123, 124, 125, 126, 128, 129, 130, 131, 132, 134, 135, 136, 137, 139, 140, 141, 142, 144, 145,
        146, 147, 149, 150, 151, 153, 154, 155, 157, 158, 159, 161, 162, 163, 165, 166, 167, 169, 170, 171, 173, 174,
        176, 177, 178, 180, 181, 182, 184, 185, 187, 188, 190, 191, 192 };
static const uint8_t ui8_eMTB_power_function_185[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
        1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7, 7, 8, 8, 8, 9, 9, 10, 10, 11, 11, 11, 12,
        12, 13, 13, 14, 14, 15, 15, 16, 17, 17, 18, 18, 19, 19, 20, 21, 21, 22, 23, 23, 24, 25, 25, 26, 27, 27, 28, 29,
        29, 30, 31, 32, 32, 33, 34, 35, 36, 36, 37, 38, 39, 40, 40, 41, 42, 43, 44, 45, 46, 46, 47, 48, 49, 50, 51, 52,
        53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 74, 75, 76, 77, 78, 79, 80, 81,
        83, 84, 85, 86, 87, 89, 90, 91, 92, 93, 95, 96, 97, 98, 100, 101, 102, 104, 105, 106, 107, 109, 110, 111, 113,
        114, 115, 117, 118, 120, 121, 122, 124, 125, 127, 128, 129, 131, 132, 134, 135, 137, 138, 140, 141, 143, 144,
        146, 147, 149, 150, 152, 153, 155, 156, 158, 160, 161, 163, 164, 166, 168, 169, 171, 172, 174, 176, 177, 179,
        181, 182, 184, 186, 187, 189, 191, 193, 194, 196, 198, 199, 201, 203, 205, 207, 208, 210, 212, 214, 216, 217,
        219, 221, 223, 225, 227, 228, 230, 232, 234, 236, 238, 240, 240, 240, 240, 240, 240, 240, 240 };
static const uint8_t ui8_eMTB_power_function_190[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14,
        14, 15, 16, 16, 17, 18, 18, 19, 20, 20, 21, 22, 22, 23, 24, 25, 25, 26, 27, 28, 29, 29, 30, 31, 32, 33, 34, 35,
        36, 37, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 51, 52, 53, 54, 55, 56, 57, 58, 60, 61, 62, 63, 64,
        66, 67, 68, 69, 70, 72, 73, 74, 76, 77, 78, 80, 81, 82, 84, 85, 86, 88, 89, 91, 92, 94, 95, 96, 98, 99, 101,
        102, 104, 105, 107, 108, 110, 112, 113, 115, 116, 118, 120, 121, 123, 124, 126, 128, 130, 131, 133, 135, 136,
        138, 140, 142, 143, 145, 147, 149, 150, 152, 154, 156, 158, 160, 162, 163, 165, 167, 169, 171, 173, 175, 177,
        179, 181, 183, 185, 187, 189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 209, 211, 214, 216, 218, 220, 222,
        224, 227, 229, 231, 233, 235, 238, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240 };
static const uint8_t ui8_eMTB_power_function_195[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 1, 2, 2, 2, 3, 3, 3, 3, 4, 4, 5, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 13, 13, 14, 15, 15, 16,
        17, 17, 18, 19, 20, 21, 21, 22, 23, 24, 25, 26, 27, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39, 40, 41, 42,
        43, 44, 45, 47, 48, 49, 50, 51, 53, 54, 55, 57, 58, 59, 61, 62, 63, 65, 66, 68, 69, 70, 72, 73, 75, 76, 78, 79,
        81, 83, 84, 86, 87, 89, 91, 92, 94, 96, 97, 99, 101, 103, 104, 106, 108, 110, 112, 113, 115, 117, 119, 121, 123,
        125, 127, 129, 131, 132, 134, 136, 139, 141, 143, 145, 147, 149, 151, 153, 155, 157, 160, 162, 164, 166, 168,
        171, 173, 175, 177, 180, 182, 184, 187, 189, 191, 194, 196, 199, 201, 203, 206, 208, 211, 213, 216, 218, 221,
        224, 226, 229, 231, 234, 237, 239, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240 };
static const uint8_t ui8_eMTB_power_function_200[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 10, 10, 11, 12, 12, 13, 14, 14, 15, 16, 17, 18, 18, 19,
        20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 34, 35, 36, 37, 38, 40, 41, 42, 44, 45, 46, 48, 49, 50, 52,
        53, 55, 56, 58, 59, 61, 62, 64, 66, 67, 69, 71, 72, 74, 76, 77, 79, 81, 83, 85, 86, 88, 90, 92, 94, 96, 98, 100,
        102, 104, 106, 108, 110, 112, 114, 117, 119, 121, 123, 125, 128, 130, 132, 135, 137, 139, 142, 144, 146, 149,
        151, 154, 156, 159, 161, 164, 166, 169, 172, 174, 177, 180, 182, 185, 188, 190, 193, 196, 199, 202, 204, 207,
        210, 213, 216, 219, 222, 225, 228, 231, 234, 237, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240 };
static const uint8_t ui8_eMTB_power_function_205[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
        2, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9, 9, 10, 11, 11, 12, 13, 14, 15, 16, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 26, 27, 28, 29, 30, 32, 33, 34, 36, 37, 38, 40, 41, 43, 44, 46, 47, 49, 50, 52, 54, 55, 57, 59, 61, 62,
        64, 66, 68, 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, 90, 92, 95, 97, 99, 101, 104, 106, 108, 111, 113, 116, 118,
        121, 123, 126, 128, 131, 134, 136, 139, 142, 145, 147, 150, 153, 156, 159, 162, 165, 168, 171, 174, 177, 180,
        183, 186, 189, 192, 196, 199, 202, 205, 209, 212, 216, 219, 222, 226, 229, 233, 236, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240 };
static const uint8_t ui8_eMTB_power_function_210[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2,
        2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 7, 7, 8, 9, 9, 10, 11, 12, 13, 14, 14, 15, 16, 17, 19, 20, 21, 22, 23, 24, 26, 27,
        28, 30, 31, 32, 34, 35, 37, 39, 40, 42, 43, 45, 47, 49, 50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 71, 73, 75, 77,
        80, 82, 84, 87, 89, 92, 94, 97, 99, 102, 104, 107, 110, 113, 115, 118, 121, 124, 127, 130, 133, 136, 139, 142,
        145, 149, 152, 155, 158, 162, 165, 169, 172, 176, 179, 183, 186, 190, 194, 197, 201, 205, 209, 213, 216, 220,
        224, 228, 232, 237, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240 };
static const uint8_t ui8_eMTB_power_function_215[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2,
        2, 2, 3, 3, 4, 4, 5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 21, 22, 24, 25, 26, 28, 29, 31,
        33, 34, 36, 38, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 60, 62, 64, 67, 69, 71, 74, 76, 79, 82, 84, 87, 90, 93,
        96, 98, 101, 104, 107, 111, 114, 117, 120, 123, 127, 130, 134, 137, 141, 144, 148, 152, 155, 159, 163, 167, 171,
        175, 179, 183, 187, 191, 195, 200, 204, 208, 213, 217, 222, 226, 231, 235, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240 };
static const uint8_t ui8_eMTB_power_function_220[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2,
        2, 3, 3, 4, 4, 5, 6, 7, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 19, 20, 22, 23, 25, 27, 28, 30, 32, 33, 35, 37,
        39, 41, 43, 46, 48, 50, 52, 55, 57, 60, 62, 65, 67, 70, 73, 76, 79, 82, 85, 88, 91, 94, 97, 101, 104, 108, 111,
        115, 118, 122, 126, 130, 133, 137, 141, 145, 150, 154, 158, 162, 167, 171, 176, 180, 185, 190, 194, 199, 204,
        209, 214, 219, 224, 230, 235, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240 };
static const uint8_t ui8_eMTB_power_function_225[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2,
        3, 3, 4, 4, 5, 6, 7, 8, 8, 9, 10, 12, 13, 14, 15, 17, 18, 20, 21, 23, 24, 26, 28, 30, 32, 34, 36, 38, 40, 43,
        45, 47, 50, 52, 55, 58, 61, 64, 66, 70, 73, 76, 79, 82, 86, 89, 93, 96, 100, 104, 108, 112, 116, 120, 124, 128,
        133, 137, 142, 146, 151, 156, 161, 166, 171, 176, 181, 186, 191, 197, 202, 208, 214, 219, 225, 231, 237, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240 };
static const uint8_t ui8_eMTB_power_function_230[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2,
        3, 4, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 18, 20, 21, 23, 25, 27, 29, 31, 33, 36, 38, 40, 43, 46, 48, 51,
        54, 57, 60, 63, 67, 70, 74, 77, 81, 85, 88, 92, 96, 101, 105, 109, 114, 118, 123, 128, 133, 138, 143, 148, 153,
        158, 164, 170, 175, 181, 187, 193, 199, 205, 212, 218, 225, 231, 238, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240 };
static const uint8_t ui8_eMTB_power_function_235[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 3,
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 37, 40, 43, 45, 48, 52, 55, 58, 62,
        65, 69, 73, 77, 81, 85, 89, 94, 98, 103, 108, 113, 118, 123, 128, 134, 139, 145, 151, 157, 163, 169, 176, 182,
        189, 196, 202, 210, 217, 224, 232, 239, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240 };
static const uint8_t ui8_eMTB_power_function_240[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 3, 3,
        4, 5, 6, 7, 8, 9, 10, 12, 13, 15, 17, 19, 21, 23, 25, 27, 30, 32, 35, 38, 41, 44, 47, 51, 54, 58, 62, 66, 70,
        74, 79, 83, 88, 93, 98, 103, 108, 114, 120, 125, 131, 137, 144, 150, 157, 164, 171, 178, 185, 193, 200, 208,
        216, 224, 233, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240 };
static const uint8_t ui8_eMTB_power_function_245[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 4,
        4, 5, 6, 8, 9, 10, 12, 14, 15, 17, 19, 22, 24, 27, 29, 32, 35, 38, 42, 45, 49, 53, 57, 61, 65, 70, 74, 79, 84,
        89, 95, 100, 106, 112, 119, 125, 132, 138, 145, 153, 160, 168, 176, 184, 192, 200, 209, 218, 227, 237, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240 };
static const uint8_t ui8_eMTB_power_function_250[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 4,
        5, 6, 7, 9, 10, 12, 14, 16, 18, 20, 23, 25, 28, 31, 34, 38, 41, 45, 49, 54, 58, 63, 67, 72, 78, 83, 89, 95, 101,
        108, 114, 121, 128, 136, 144, 151, 160, 168, 177, 186, 195, 204, 214, 224, 235, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240 };
static const uint8_t ui8_eMTB_power_function_255[eMTB_POWER_FUNCTION_ARRAY_SIZE] = { 0, 0, 0, 0, 0, 1, 1, 1, 2, 3, 4, 5,
        6, 7, 8, 10, 12, 14, 16, 18, 21, 24, 26, 30, 33, 37, 41, 45, 49, 54, 58, 64, 69, 75, 80, 87, 93, 100, 107, 114,
        122, 130, 138, 146, 155, 164, 174, 184, 194, 204, 215, 226, 238, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
        240, 240, 240 };

// walk assist
static uint8_t ui8_walk_assist_speed_target_x10 = 0;
static uint8_t ui8_walk_assist_duty_cycle_counter = 0;
static uint8_t ui8_walk_assist_duty_cycle_target = WALK_ASSIST_DUTY_CYCLE_MIN;
static uint8_t ui8_walk_assist_duty_cycle_max = WALK_ASSIST_DUTY_CYCLE_MIN;
static uint8_t ui8_walk_assist_adj_delay = WALK_ASSIST_ADJ_DELAY_MIN;
static uint16_t ui16_walk_assist_wheel_speed_counter = 0;
static uint16_t ui16_walk_assist_erps_target = 0;
static uint16_t ui16_walk_assist_erps_min = 0;
static uint16_t ui16_walk_assist_erps_max = 0;
static uint8_t ui8_walk_assist_speed_flag = 0;

// cruise
static int16_t i16_cruise_pid_kp = 0;
static int16_t i16_cruise_pid_ki = 0;
static uint8_t ui8_cruise_PID_initialize = 1;
static uint16_t ui16_wheel_speed_target_received_x10 = 0;

// startup boost
static uint8_t ui8_startup_boost_enabled = 0;
static uint8_t ui8_startup_boost_at_zero = 0;
static uint8_t ui8_startup_boost_flag = 0;
static uint16_t ui16_startup_boost_factor_array[120];
static uint16_t ui16_startup_boost_factor_temp = 0;
static uint8_t ui8_startup_boost_cadence_step = 0;

// startup assist
static uint8_t ui8_startup_assist = 0;
static uint8_t ui8_startup_assist_adc_battery_current_target = 0;

// UART
#define UART_NUMBER_DATA_BYTES_TO_RECEIVE   7  // change this value depending on how many data bytes there are to receive ( Package = one start byte + data bytes + two bytes 16 bit CRC )
#define UART_NUMBER_DATA_BYTES_TO_SEND      26  // change this value depending on how many data bytes there are to send ( Package = one start byte + data bytes + two bytes 16 bit CRC )

volatile uint8_t ui8_received_package_flag = 0;
volatile uint8_t ui8_rx_buffer[UART_NUMBER_DATA_BYTES_TO_RECEIVE + 3];
volatile uint8_t ui8_rx_counter = 0;
volatile uint8_t ui8_tx_buffer[UART_NUMBER_DATA_BYTES_TO_SEND + 3];
volatile uint8_t ui8_i;
// initialize the ui8_tx_counter like at the end of send operation to enable the first send.
volatile uint8_t ui8_tx_counter = UART_NUMBER_DATA_BYTES_TO_SEND + 1;
volatile uint8_t ui8_byte_received;
volatile uint8_t ui8_state_machine = 0;
static uint16_t ui16_crc_rx;
static uint16_t ui16_crc_tx;
volatile uint8_t ui8_message_ID = 0;
static uint8_t no_rx_counter = 0;

static void uart_receive_package(void);
static void uart_send_package(void);

// system functions
static void get_pedal_torque(void);
static void calc_wheel_speed(void);
static void calc_cadence(void);

static void ebike_control_lights(void);
static void ebike_control_motor(void);
static void check_system(void);

static void apply_power_assist();
static void apply_torque_assist();
static void apply_cadence_assist();
static void apply_emtb_assist();
static void apply_hybrid_assist();
static void apply_cruise();
static void apply_walk_assist();
static void apply_calibration_assist();
static void apply_throttle();
static void apply_temperature_limiting();
static void apply_speed_limit();


void ebike_app_init(void)
{
	// Phase Hall angle references default
	ui8_hall_ref_angles[0] = PHASE_ROTOR_ANGLE_30;
	ui8_hall_ref_angles[1] = PHASE_ROTOR_ANGLE_90;
	ui8_hall_ref_angles[2] = PHASE_ROTOR_ANGLE_150;
	ui8_hall_ref_angles[3] = PHASE_ROTOR_ANGLE_210;
	ui8_hall_ref_angles[4] = PHASE_ROTOR_ANGLE_270;
	ui8_hall_ref_angles[5] = PHASE_ROTOR_ANGLE_330;
	
	// Hall counter offset for angle interpolation default
    ui8_hall_counter_offsets[0] = HALL_COUNTER_OFFSET_UP;
    ui8_hall_counter_offsets[1] = HALL_COUNTER_OFFSET_DOWN;
    ui8_hall_counter_offsets[2] = HALL_COUNTER_OFFSET_UP;
    ui8_hall_counter_offsets[3] = HALL_COUNTER_OFFSET_DOWN;
    ui8_hall_counter_offsets[4] = HALL_COUNTER_OFFSET_UP;
    ui8_hall_counter_offsets[5] = HALL_COUNTER_OFFSET_DOWN;
}

	
void ebike_app_controller(void)
{
	// calculate the wheel speed
    calc_wheel_speed();
	
	// calculate the cadence and set limits from wheel speed
    calc_cadence();

	// Calculate filtered Battery Voltage (mV)
    ui16_battery_voltage_filtered_x1000 = ui16_adc_battery_voltage_filtered * BATTERY_VOLTAGE_PER_10_BIT_ADC_STEP_X1000;

    // Calculate filtered Battery Current (Ampx10)
    ui8_battery_current_filtered_x10 = (uint8_t)(((uint16_t) ui8_adc_battery_current_filtered * BATTERY_CURRENT_PER_10_BIT_ADC_STEP_X100) / 10);

	// get pedal torque
    get_pedal_torque();

	// check if there are any errors for motor control
    //check_system();

    // send/receive data every 4 cycles (25ms * 4)
	static uint8_t ui8_counter;
	
	switch (ui8_counter++ & 0x03) {
		case 0: 
			uart_receive_package();
			break;
		case 1:
			uart_send_package();
			break;
		case 2:
			ebike_control_lights();
			break;
		case 3:
			check_system();
			break;
	}
	
	// use received data and sensor input to control motor
    ebike_control_motor();

    /*------------------------------------------------------------------------

     NOTE: regarding function call order

     Do not change order of functions if not absolutely sure it will
     not cause any undesirable consequences.

     ------------------------------------------------------------------------*/
}


static void ebike_control_motor(void)
{
	// reset ebike loop check counter
	ui16_ebike_check_counter = 0;
	
	// assist level = OFF if the motor goes alone and with current or duty cycle target = 0 (safety)
	if((ui16_motor_speed_erps)&&(!ui8_adc_battery_current_target || !ui8_duty_cycle_target))
		ui8_assist_level_flag = 0;
	
    // reset control variables (safety)
    ui8_duty_cycle_ramp_up_inverse_step = PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_DEFAULT;
    ui8_duty_cycle_ramp_down_inverse_step = PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_DEFAULT;
    ui8_adc_battery_current_target = 0;
    ui8_duty_cycle_target = 0;

    // reset initialization of Cruise PID controller
    if (ui8_riding_mode != CRUISE_MODE) {
		ui8_cruise_PID_initialize = 1;
	}

    // select riding mode
    switch (ui8_riding_mode) {
		case POWER_ASSIST_MODE: apply_power_assist(); break;
		case TORQUE_ASSIST_MODE: apply_torque_assist(); break;
		case CADENCE_ASSIST_MODE: apply_cadence_assist(); break;
		case eMTB_ASSIST_MODE: apply_emtb_assist(); break;
		case HYBRID_ASSIST_MODE: apply_hybrid_assist(); break;
		case CRUISE_MODE: apply_cruise(); break;
		case WALK_ASSIST_MODE: apply_walk_assist(); break;
		case MOTOR_CALIBRATION_MODE: apply_calibration_assist(); break;
    }

    // select optional ADC function
    switch (m_configuration_variables.ui8_optional_ADC_function) {
		case THROTTLE_CONTROL: apply_throttle(); break;
		case TEMPERATURE_CONTROL: apply_temperature_limiting(); break;
    }

    // speed limit
    apply_speed_limit();


	// Check battery Over-current (read current here in case PWM interrupt for some error was disabled)
	// Read in assembler to ensure data consistency (conversion overrun)
	#ifndef __CDT_PARSER__ // avoid Eclipse syntax check
	__asm
		ldw x, 0x53ea // ADC1->DB5RH
		cpw x, 0x53ea // ADC1->DB5RH
		jreq 00010$
		ldw x, 0x53ea // ADC1->DB5RH
	00010$:
		cpw x, #ADC_10_BIT_BATTERY_OVERCURRENT
		jrc 00011$
		mov _ui8_battery_overcurrent+0, #ERROR_BATTERY_OVERCURRENT
	00011$:
	__endasm;
	#endif
	
	ui8_system_state = ui8_battery_overcurrent;
	
    // reset control parameters if... (safety)
    if((ui8_brake_state) ||
	  (ui8_system_state == ERROR_MOTOR_BLOCKED) ||
	  (ui8_system_state == ERROR_BATTERY_OVERCURRENT) ||				  
	  (!ui8_motor_enabled) ||
	  (ui8_riding_mode == OFF_MODE) ||
	  (ui8_riding_mode_parameter == 0) ||
	  ((ui8_system_state != NO_ERROR)&&(!ui8_assist_with_error_enabled))) {
        ui8_controller_duty_cycle_ramp_up_inverse_step = PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_DEFAULT;
        ui8_controller_duty_cycle_ramp_down_inverse_step = PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN;
        ui8_controller_adc_battery_current_target = 0;
        ui8_controller_duty_cycle_target = 0;
    } else {
        // limit max current if higher than configured hardware limit (safety)
        if (ui8_adc_battery_current_max > ADC_10_BIT_BATTERY_CURRENT_MAX) {
            ui8_adc_battery_current_max = ADC_10_BIT_BATTERY_CURRENT_MAX;
        }

        // limit target current if higher than max value (safety)
        if (ui8_adc_battery_current_target > ui8_adc_battery_current_max) {
            ui8_adc_battery_current_target = ui8_adc_battery_current_max;
        }

        // limit target duty cycle if higher than max value
        if (ui8_duty_cycle_target > PWM_DUTY_CYCLE_MAX) {
            ui8_duty_cycle_target = PWM_DUTY_CYCLE_MAX;
        }
		
        // limit target duty cycle ramp up inverse step if lower than min value (safety)
        if (ui8_duty_cycle_ramp_up_inverse_step < PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN) {
            ui8_duty_cycle_ramp_up_inverse_step = PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN;
        }

        // limit target duty cycle ramp down inverse step if lower than min value (safety)
        if (ui8_duty_cycle_ramp_down_inverse_step < PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN) {
            ui8_duty_cycle_ramp_down_inverse_step = PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN;
        }
		
        // set duty cycle ramp up in controller
        ui8_controller_duty_cycle_ramp_up_inverse_step = ui8_duty_cycle_ramp_up_inverse_step;

        // set duty cycle ramp down in controller
        ui8_controller_duty_cycle_ramp_down_inverse_step = ui8_duty_cycle_ramp_down_inverse_step;

        // set target battery current in controller
        ui8_controller_adc_battery_current_target = ui8_adc_battery_current_target;

        // set target duty cycle in controller
        ui8_controller_duty_cycle_target = ui8_duty_cycle_target;
	}

    // check if the motor should be enabled or disabled
    if (ui8_motor_enabled
		&& ((ui8_system_state == ERROR_BATTERY_OVERCURRENT)
            || ((ui16_motor_speed_erps == 0)
            && (!ui8_adc_battery_current_target)
            && (!ui8_g_duty_cycle)))) {
        ui8_motor_enabled = 0;
        motor_disable_pwm();
    } else if (!ui8_motor_enabled
            && (ui16_motor_speed_erps < 50) // enable the motor only if it rotates slowly or is stopped
            && (ui8_adc_battery_current_target)
            && (!ui8_brake_state)) {
        ui8_motor_enabled = 1;
		ui8_g_duty_cycle = PWM_DUTY_CYCLE_STARTUP;
        ui8_fw_hall_counter_offset = 0;
        motor_enable_pwm();
	}
}


static void apply_power_assist()
{
	uint8_t ui8_tmp;
    uint8_t ui8_power_assist_multiplier_x50 = ui8_riding_mode_parameter;
	
	// check for assist without pedal rotation when there is no pedal rotation
	if(ui8_assist_without_pedal_rotation_enabled) {
		if((!ui8_pedal_cadence_RPM) &&
		   (ui16_adc_pedal_torque_delta > (120 - ui8_assist_without_pedal_rotation_threshold))) {
				ui8_pedal_cadence_RPM = 1;
		}
	}
	
  if((ui8_pedal_cadence_RPM)||(ui8_startup_assist_adc_battery_current_target)) {
	// calculate torque on pedals + torque startup boost
    uint32_t ui32_pedal_torque_x100 = (uint32_t)(ui16_adc_pedal_torque_delta * m_configuration_variables.ui8_pedal_torque_per_10_bit_ADC_step_x100);
	
    // calculate power assist by multiplying human power with the power assist multiplier
    uint32_t ui32_power_assist_x100 = (((uint32_t)(ui8_pedal_cadence_RPM * ui8_power_assist_multiplier_x50))
            * ui32_pedal_torque_x100) / 480U; // see note below

    /*------------------------------------------------------------------------

     NOTE: regarding the human power calculation

     (1) Formula: pedal power = torque * rotations per second * 2 * pi
     (2) Formula: pedal power = torque * rotations per minute * 2 * pi / 60
     (3) Formula: pedal power = torque * rotations per minute * 0.1047
     (4) Formula: pedal power = torque * 100 * rotations per minute * 0.001047
     (5) Formula: pedal power = torque * 100 * rotations per minute / 955
     (6) Formula: pedal power * 100  =  torque * 100 * rotations per minute * (100 / 955)
     (7) Formula: assist power * 100  =  torque * 100 * rotations per minute * (100 / 955) * (ui8_power_assist_multiplier_x50 / 50)
     (8) Formula: assist power * 100  =  torque * 100 * rotations per minute * (2 / 955) * ui8_power_assist_multiplier_x50
     (9) Formula: assist power * 100  =  torque * 100 * rotations per minute * ui8_power_assist_multiplier_x50 / 480

     ------------------------------------------------------------------------*/

    // calculate target current
    uint32_t ui32_battery_current_target_x100 = (ui32_power_assist_x100 * 1000) / ui16_battery_voltage_filtered_x1000;
	
    // set battery current target in ADC steps
    uint16_t ui16_adc_battery_current_target = (uint16_t)ui32_battery_current_target_x100 / BATTERY_CURRENT_PER_10_BIT_ADC_STEP_X100;
	
    // set motor acceleration
    if (ui16_wheel_speed_x10 >= 200) {
        ui8_duty_cycle_ramp_up_inverse_step = PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN;
        ui8_duty_cycle_ramp_down_inverse_step = PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN;
    } else {
        ui8_duty_cycle_ramp_up_inverse_step = map_ui8((uint8_t)(ui16_wheel_speed_x10>>2),
                (uint8_t)10, // 10*4 = 40 -> 4 kph
                (uint8_t)50, // 50*4 = 200 -> 20 kph
                (uint8_t)ui8_duty_cycle_ramp_up_inverse_step_default,
                (uint8_t)PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN);
        ui8_tmp = map_ui8(ui8_pedal_cadence_RPM,
                (uint8_t)20, // 20 rpm
                (uint8_t)70, // 70 rpm
                (uint8_t)ui8_duty_cycle_ramp_up_inverse_step_default,
                (uint8_t)PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN);
        if (ui8_tmp < ui8_duty_cycle_ramp_up_inverse_step)
            ui8_duty_cycle_ramp_up_inverse_step = ui8_tmp;

        ui8_duty_cycle_ramp_down_inverse_step = map_ui8((uint8_t)(ui16_wheel_speed_x10>>2),
                (uint8_t)10, // 10*4 = 40 -> 4 kph
                (uint8_t)50, // 50*4 = 200 -> 20 kph
                (uint8_t)ui8_duty_cycle_ramp_down_inverse_step_default,
                (uint8_t)PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN);
        ui8_tmp = map_ui8(ui8_pedal_cadence_RPM,
                (uint8_t)20, // 20 rpm
                (uint8_t)70, // 70 rpm
                (uint8_t)ui8_duty_cycle_ramp_down_inverse_step_default,
                (uint8_t)PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN);
        if (ui8_tmp < ui8_duty_cycle_ramp_down_inverse_step)
            ui8_duty_cycle_ramp_down_inverse_step = ui8_tmp;
    }

    // set battery current target
    if (ui16_adc_battery_current_target > ui8_adc_battery_current_max) {
        ui8_adc_battery_current_target = ui8_adc_battery_current_max;
    } else {
        ui8_adc_battery_current_target = ui16_adc_battery_current_target;
    }
	
	// set startup assist battery current target
	if (ui8_startup_assist) {
		if (ui8_adc_battery_current_target > ui8_startup_assist_adc_battery_current_target)
			ui8_startup_assist_adc_battery_current_target = ui8_adc_battery_current_target;
		
		ui8_adc_battery_current_target = ui8_startup_assist_adc_battery_current_target;
	} else {
		ui8_startup_assist_adc_battery_current_target = 0;
    }
	
    // set duty cycle target
    if (ui8_adc_battery_current_target) {
        ui8_duty_cycle_target = PWM_DUTY_CYCLE_MAX;
    } else {
        ui8_duty_cycle_target = 0;
    }
  }
}


static void apply_torque_assist()
{
	uint8_t ui8_tmp;
	
	// check for assist without pedal rotation when there is no pedal rotation
	if(ui8_assist_without_pedal_rotation_enabled) {
		if((!ui8_pedal_cadence_RPM)&&
			(ui16_adc_pedal_torque_delta > (120 - ui8_assist_without_pedal_rotation_threshold))) {
				ui8_pedal_cadence_RPM = 1;
		}
	}
	
	if(ui8_assist_with_error_enabled)
		ui8_pedal_cadence_RPM = 1;
	
    // calculate torque assistance
    if (((ui16_adc_pedal_torque_delta)&&(ui8_pedal_cadence_RPM))
	  ||(ui8_startup_assist_adc_battery_current_target)) {
        // get the torque assist factor
        uint8_t ui8_torque_assist_factor = ui8_riding_mode_parameter;

        // calculate torque assist target current
        uint16_t ui16_adc_battery_current_target_torque_assist = ((uint16_t) ui16_adc_pedal_torque_delta
                * ui8_torque_assist_factor) / TORQUE_ASSIST_FACTOR_DENOMINATOR;

        // set motor acceleration
        if (ui16_wheel_speed_x10 >= 200) {
            ui8_duty_cycle_ramp_up_inverse_step = PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN;
            ui8_duty_cycle_ramp_down_inverse_step = PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN;
        } else {
            ui8_duty_cycle_ramp_up_inverse_step = map_ui8((uint8_t)(ui16_wheel_speed_x10>>2),
                    (uint8_t)10, // 10*4 = 40 -> 4 kph
                    (uint8_t)50, // 50*4 = 200 -> 20 kph
                    (uint8_t)ui8_duty_cycle_ramp_up_inverse_step_default,
                    (uint8_t)PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN);
            ui8_tmp = map_ui8(ui8_pedal_cadence_RPM,
                    (uint8_t)20, // 20 rpm
                    (uint8_t)70, // 70 rpm
                    (uint8_t)ui8_duty_cycle_ramp_up_inverse_step_default,
                    (uint8_t)PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN);
            if (ui8_tmp < ui8_duty_cycle_ramp_up_inverse_step)
                ui8_duty_cycle_ramp_up_inverse_step = ui8_tmp;

            ui8_duty_cycle_ramp_down_inverse_step = map_ui8((uint8_t)(ui16_wheel_speed_x10>>2),
                    (uint8_t)10, // 10*4 = 40 -> 4 kph
                    (uint8_t)50, // 50*4 = 200 -> 20 kph
                    (uint8_t)ui8_duty_cycle_ramp_down_inverse_step_default,
                    (uint8_t)PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN);
            ui8_tmp = map_ui8(ui8_pedal_cadence_RPM,
                    (uint8_t)20, // 20 rpm
                    (uint8_t)70, // 70 rpm
                    (uint8_t)ui8_duty_cycle_ramp_down_inverse_step_default,
                    (uint8_t)PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN);
            if (ui8_tmp < ui8_duty_cycle_ramp_down_inverse_step)
                ui8_duty_cycle_ramp_down_inverse_step = ui8_tmp;
        }
        // set battery current target
        if (ui16_adc_battery_current_target_torque_assist > ui8_adc_battery_current_max) {
            ui8_adc_battery_current_target = ui8_adc_battery_current_max;
        } else {
            ui8_adc_battery_current_target = ui16_adc_battery_current_target_torque_assist;
        }
		
		// set startup assist battery current target
		if (ui8_startup_assist) {
			if (ui8_adc_battery_current_target > ui8_startup_assist_adc_battery_current_target)
				ui8_startup_assist_adc_battery_current_target = ui8_adc_battery_current_target;
		
			ui8_adc_battery_current_target = ui8_startup_assist_adc_battery_current_target;
		} else {
			ui8_startup_assist_adc_battery_current_target = 0;
		}
		
		// set duty cycle target
        if (ui8_adc_battery_current_target) {
            ui8_duty_cycle_target = PWM_DUTY_CYCLE_MAX;
        } else {
            ui8_duty_cycle_target = 0;
        }
    }
}


static void apply_cadence_assist()
{
	uint8_t ui8_tmp;
	
    if (ui8_pedal_cadence_RPM) {
		// get the cadence assist duty cycle increment
		uint16_t ui16_cadence_assist_duty_cycle_increment = ((PWM_DUTY_CYCLE_MAX - ui8_riding_mode_parameter) * ui8_pedal_cadence_RPM) / 120;
        // get the cadence assist duty cycle target
		uint8_t ui8_cadence_assist_duty_cycle_target = ui8_riding_mode_parameter + (uint8_t)ui16_cadence_assist_duty_cycle_increment;
		
        // limit cadence assist duty cycle target
        if (ui8_cadence_assist_duty_cycle_target > PWM_DUTY_CYCLE_MAX) {
            ui8_cadence_assist_duty_cycle_target = PWM_DUTY_CYCLE_MAX;
        }

        // set motor acceleration
        if (ui16_wheel_speed_x10 >= 200) {
            ui8_duty_cycle_ramp_up_inverse_step = PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN;
            ui8_duty_cycle_ramp_down_inverse_step = PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN;
        } else {
            ui8_duty_cycle_ramp_up_inverse_step = map_ui8((uint8_t)(ui16_wheel_speed_x10>>2),
                    (uint8_t)10, // 10*4 = 40 -> 4 kph
                    (uint8_t)50, // 50*4 = 200 -> 20 kph
                    (uint8_t)ui8_duty_cycle_ramp_up_inverse_step_default + PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_CADENCE_OFFSET,
                    (uint8_t)PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN);
            ui8_tmp = map_ui8(ui8_pedal_cadence_RPM,
                    (uint8_t)20, // 20 rpm
                    (uint8_t)70, // 70 rpm
                    (uint8_t)ui8_duty_cycle_ramp_up_inverse_step_default + PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_CADENCE_OFFSET,
                    (uint8_t)PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN);
            if (ui8_tmp < ui8_duty_cycle_ramp_up_inverse_step)
                ui8_duty_cycle_ramp_up_inverse_step = ui8_tmp;

			ui8_duty_cycle_ramp_down_inverse_step = map_ui8((uint8_t)(ui16_wheel_speed_x10>>2),
					(uint8_t)10, // 10*4 = 40 -> 4 kph
					(uint8_t)50, // 50*4 = 200 -> 20 kph
					(uint8_t)ui8_duty_cycle_ramp_down_inverse_step_default,
					(uint8_t)PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN);
			ui8_tmp = map_ui8(ui8_pedal_cadence_RPM,
					(uint8_t)20, // 20 rpm
					(uint8_t)70, // 70 rpm
					(uint8_t)ui8_duty_cycle_ramp_down_inverse_step_default,
					(uint8_t)PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN);
			if (ui8_tmp < ui8_duty_cycle_ramp_down_inverse_step)
				ui8_duty_cycle_ramp_down_inverse_step = ui8_tmp;
        }
        // set battery current target
        ui8_adc_battery_current_target = ui8_adc_battery_current_max;

        // set duty cycle target
        ui8_duty_cycle_target = ui8_cadence_assist_duty_cycle_target;
    }
}


static void apply_emtb_assist()
{
//#define eMTB_ASSIST_ADC_TORQUE_OFFSET    10
#define eMTB_ASSIST_ADC_TORQUE_OFFSET    5
	uint8_t ui8_tmp;
	
	// check for assist without pedal rotation when there is no pedal rotation
	if(ui8_assist_without_pedal_rotation_enabled) {
		if((!ui8_pedal_cadence_RPM)&&
			(ui16_adc_pedal_torque_delta > (120 - ui8_assist_without_pedal_rotation_threshold))) {
				ui8_pedal_cadence_RPM = 1;
		}
	}

	if(ui8_assist_with_error_enabled)
		ui8_pedal_cadence_RPM = 1;
	
	if(((ui16_adc_pedal_torque_delta > 0) && 
		(ui16_adc_pedal_torque_delta < (eMTB_POWER_FUNCTION_ARRAY_SIZE - eMTB_ASSIST_ADC_TORQUE_OFFSET)) &&
		(ui8_pedal_cadence_RPM))||(ui8_startup_assist_adc_battery_current_target))
	{
		// initialize eMTB assist target current
		uint8_t ui8_adc_battery_current_target_eMTB_assist = 0;
		
		// get the eMTB assist sensitivity
		uint8_t ui8_eMTB_assist_sensitivity = ui8_riding_mode_parameter;
		
		switch (ui8_eMTB_assist_sensitivity)
		{
			case 0: ui8_adc_battery_current_target_eMTB_assist = 0;
			case 1: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_160[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 2: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_165[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 3: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_170[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 4: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_175[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 5: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_180[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 6: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_185[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 7: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_190[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 8: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_195[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 9: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_200[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 10: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_205[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 11: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_210[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 12: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_215[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 13: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_220[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 14: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_225[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 15: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_230[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 16: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_235[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 17: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_240[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 18: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_245[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 19: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_250[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
			case 20: ui8_adc_battery_current_target_eMTB_assist = ui8_eMTB_power_function_255[ui16_adc_pedal_torque_delta + eMTB_ASSIST_ADC_TORQUE_OFFSET]; break;
		}

        // set motor acceleration
        if (ui16_wheel_speed_x10 >= 200) {
            ui8_duty_cycle_ramp_up_inverse_step = PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN;
            ui8_duty_cycle_ramp_down_inverse_step = PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN;
        } else {
            ui8_duty_cycle_ramp_up_inverse_step = map_ui8((uint8_t)(ui16_wheel_speed_x10>>2),
                    (uint8_t)10, // 10*4 = 40 -> 4 kph
                    (uint8_t)50, // 50*4 = 200 -> 20 kph
                    (uint8_t)ui8_duty_cycle_ramp_up_inverse_step_default,
                    (uint8_t)PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN);
            ui8_tmp = map_ui8(ui8_pedal_cadence_RPM,
                    (uint8_t)20, // 20 rpm
                    (uint8_t)70, // 70 rpm
                    (uint8_t)ui8_duty_cycle_ramp_up_inverse_step_default,
                    (uint8_t)PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN);
            if (ui8_tmp < ui8_duty_cycle_ramp_up_inverse_step)
                ui8_duty_cycle_ramp_up_inverse_step = ui8_tmp;

            ui8_duty_cycle_ramp_down_inverse_step = map_ui8((uint8_t)(ui16_wheel_speed_x10>>2),
                    (uint8_t)10, // 10*4 = 40 -> 4 kph
                    (uint8_t)50, // 50*4 = 200 -> 20 kph
                    (uint8_t)ui8_duty_cycle_ramp_down_inverse_step_default,
                    (uint8_t)PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN);
            ui8_tmp = map_ui8(ui8_pedal_cadence_RPM,
                    (uint8_t)20, // 20 rpm
                    (uint8_t)70, // 70 rpm
                    (uint8_t)ui8_duty_cycle_ramp_down_inverse_step_default,
                    (uint8_t)PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN);
            if (ui8_tmp < ui8_duty_cycle_ramp_down_inverse_step)
                ui8_duty_cycle_ramp_down_inverse_step = ui8_tmp;
        }
        // set battery current target
        if (ui8_adc_battery_current_target_eMTB_assist > ui8_adc_battery_current_max) {
            ui8_adc_battery_current_target = ui8_adc_battery_current_max;
        } else {
            ui8_adc_battery_current_target = ui8_adc_battery_current_target_eMTB_assist;
        }
		
		// set startup assist battery current target
		if (ui8_startup_assist) {
			if (ui8_adc_battery_current_target > ui8_startup_assist_adc_battery_current_target)
				ui8_startup_assist_adc_battery_current_target = ui8_adc_battery_current_target;
		
			ui8_adc_battery_current_target = ui8_startup_assist_adc_battery_current_target;
		} else {
			ui8_startup_assist_adc_battery_current_target = 0;
		}
		
        // set duty cycle target
        if (ui8_adc_battery_current_target) {
            ui8_duty_cycle_target = PWM_DUTY_CYCLE_MAX;
        } else {
            ui8_duty_cycle_target = 0;
        }
    }
}


static void apply_walk_assist()
{
	if(ui8_assist_with_error_enabled) {
		// get walk assist duty cycle target
		ui8_walk_assist_duty_cycle_target = ui8_riding_mode_parameter + 20;
	}
	else {
		// get walk assist speed target x10
		ui8_walk_assist_speed_target_x10 = ui8_riding_mode_parameter;
		
		// set walk assist duty cycle target
		if((!ui8_walk_assist_speed_flag)&&(!ui16_motor_speed_erps)) {
			ui8_walk_assist_duty_cycle_target = WALK_ASSIST_DUTY_CYCLE_STARTUP;
			ui8_walk_assist_duty_cycle_max = WALK_ASSIST_DUTY_CYCLE_STARTUP;
			ui16_walk_assist_wheel_speed_counter = 0;
			ui16_walk_assist_erps_target = 0;
		}
		else if(ui8_walk_assist_speed_flag) {
			if(ui16_motor_speed_erps < ui16_walk_assist_erps_min) {
				ui8_walk_assist_adj_delay = WALK_ASSIST_ADJ_DELAY_MIN;
				
				if(ui8_walk_assist_duty_cycle_counter++ > ui8_walk_assist_adj_delay) {
					if(ui8_walk_assist_duty_cycle_target < ui8_walk_assist_duty_cycle_max) {
						ui8_walk_assist_duty_cycle_target++;
					}
					else {
						ui8_walk_assist_duty_cycle_max++;
					}
					ui8_walk_assist_duty_cycle_counter = 0;
				}
			}
			else if(ui16_motor_speed_erps < ui16_walk_assist_erps_target) {
				ui8_walk_assist_adj_delay = (ui16_motor_speed_erps - ui16_walk_assist_erps_min) * WALK_ASSIST_ADJ_DELAY_MIN;
				
				if(ui8_walk_assist_duty_cycle_counter++ > ui8_walk_assist_adj_delay) {
					if(ui8_walk_assist_duty_cycle_target < ui8_walk_assist_duty_cycle_max) {
						ui8_walk_assist_duty_cycle_target++;
					}
					
					ui8_walk_assist_duty_cycle_counter = 0;
				}
			}
			else if(ui16_motor_speed_erps < ui16_walk_assist_erps_max) {
				ui8_walk_assist_adj_delay = (ui16_walk_assist_erps_max - ui16_motor_speed_erps) * WALK_ASSIST_ADJ_DELAY_MIN;
			
				if(ui8_walk_assist_duty_cycle_counter++ > ui8_walk_assist_adj_delay) {
					if(ui8_walk_assist_duty_cycle_target > WALK_ASSIST_DUTY_CYCLE_MIN) {
						ui8_walk_assist_duty_cycle_target--;
					}
					ui8_walk_assist_duty_cycle_counter = 0;
				}
			}
			else if(ui16_motor_speed_erps >= ui16_walk_assist_erps_max) {
				ui8_walk_assist_adj_delay = WALK_ASSIST_ADJ_DELAY_MIN;
			
				if(ui8_walk_assist_duty_cycle_counter++ > ui8_walk_assist_adj_delay) {
					if(ui8_walk_assist_duty_cycle_target > WALK_ASSIST_DUTY_CYCLE_MIN) {
						ui8_walk_assist_duty_cycle_target--;
						ui8_walk_assist_duty_cycle_max--;
					}
					ui8_walk_assist_duty_cycle_counter = 0;
				}
			}
		}
		else {
			ui8_walk_assist_adj_delay = WALK_ASSIST_ADJ_DELAY_STARTUP;
			
			if(ui8_walk_assist_duty_cycle_counter++ > ui8_walk_assist_adj_delay) {
				if(ui16_wheel_speed_x10) {
					if(ui16_wheel_speed_x10 > 42) {
						ui8_walk_assist_duty_cycle_target--;
					}
					
					if(ui16_walk_assist_wheel_speed_counter++ >= 10) {
						ui8_walk_assist_duty_cycle_max += 10;
					
						// set walk assist erps target
						ui16_walk_assist_erps_target = ((ui16_motor_speed_erps * ui8_walk_assist_speed_target_x10) / ui16_wheel_speed_x10);
						ui16_walk_assist_erps_min = ui16_walk_assist_erps_target - WALK_ASSIST_ERPS_THRESHOLD;
						ui16_walk_assist_erps_max = ui16_walk_assist_erps_target + WALK_ASSIST_ERPS_THRESHOLD;
					
						// set walk assist speed flag
						ui8_walk_assist_speed_flag = 1;
					}
				}
				else {
					if((ui8_walk_assist_duty_cycle_max + 10) < WALK_ASSIST_DUTY_CYCLE_MAX) {
						ui8_walk_assist_duty_cycle_target++;
						ui8_walk_assist_duty_cycle_max++;
					}
				}
				ui8_walk_assist_duty_cycle_counter = 0;
			}
		}
	}

	if(ui8_walk_assist_duty_cycle_target > WALK_ASSIST_DUTY_CYCLE_MAX) {
		ui8_walk_assist_duty_cycle_target = WALK_ASSIST_DUTY_CYCLE_MAX;
	}
		
	// set motor acceleration
	ui8_duty_cycle_ramp_up_inverse_step = WALK_ASSIST_DUTY_CYCLE_RAMP_UP_INVERSE_STEP;	
	ui8_duty_cycle_ramp_down_inverse_step = PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_DEFAULT;
	
	if(ui16_wheel_speed_x10 < WALK_ASSIST_THRESHOLD_SPEED_X10) {
		// set battery current target
		ui8_adc_battery_current_target = ui8_min(WALK_ASSIST_ADC_BATTERY_CURRENT_MAX, ui8_adc_battery_current_max);
		// set duty cycle target
		ui8_duty_cycle_target = ui8_walk_assist_duty_cycle_target;
	}
	else {
		ui8_adc_battery_current_target = 0;
		ui8_duty_cycle_target = 0;
	}
}


static void apply_cruise()
{
//#define CRUISE_PID_KP                             14    // 48 volt motor: 12, 36 volt motor: 14
//#define CRUISE_PID_KI                             0.7   // 48 volt motor: 1, 36 volt motor: 0.7
#define CRUISE_PID_INTEGRAL_LIMIT                 1000
#define CRUISE_PID_KD                             0

    if (ui16_wheel_speed_x10 > CRUISE_THRESHOLD_SPEED_X10) {
        static int16_t i16_error;
        static int16_t i16_last_error;
        static int16_t i16_integral;
        static int16_t i16_derivative;
        static int16_t i16_control_output;
        static uint16_t ui16_wheel_speed_target_x10;

        // initialize cruise PID controller
        if (ui8_cruise_PID_initialize) {
            ui8_cruise_PID_initialize = 0;

            // reset PID variables
            i16_error = 0;
            i16_last_error = 0;
            i16_integral = 320; // initialize integral to a value so the motor does not start from zero
            i16_derivative = 0;
            i16_control_output = 0;

            // check what target wheel speed to use (received or current)
            ui16_wheel_speed_target_received_x10 = (uint16_t)ui8_riding_mode_parameter * (uint16_t)10;

            if (ui16_wheel_speed_target_received_x10 > 0) {
                // set received target wheel speed to target wheel speed
                ui16_wheel_speed_target_x10 = ui16_wheel_speed_target_received_x10;
            } else {
                // set current wheel speed to maintain
                ui16_wheel_speed_target_x10 = ui16_wheel_speed_x10;
            }
        }

        // calculate error
        i16_error = (ui16_wheel_speed_target_x10 - ui16_wheel_speed_x10);

        // calculate integral
        i16_integral = i16_integral + i16_error;

        // limit integral
        if (i16_integral > CRUISE_PID_INTEGRAL_LIMIT) {
            i16_integral = CRUISE_PID_INTEGRAL_LIMIT;
        } else if (i16_integral < 0) {
            i16_integral = 0;
        }

        // calculate derivative
        i16_derivative = i16_error - i16_last_error;

        // save error to last error
        i16_last_error = i16_error;

        // calculate control output ( output =  P I D )
        i16_control_output = (i16_cruise_pid_kp * i16_error) + (i16_cruise_pid_ki * i16_integral) + (CRUISE_PID_KD * i16_derivative);

        // limit control output to just positive values
        if (i16_control_output < 0) {
            i16_control_output = 0;
        }

        // limit control output to the maximum value
        if (i16_control_output > 1000) {
            i16_control_output = 1000;
        }

        // set motor acceleration
        ui8_duty_cycle_ramp_up_inverse_step = CRUISE_DUTY_CYCLE_RAMP_UP_INVERSE_STEP;
        ui8_duty_cycle_ramp_down_inverse_step = PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_DEFAULT;

        // set battery current target
        ui8_adc_battery_current_target = ui8_adc_battery_current_max;

        // set duty cycle target  |  map the control output to an appropriate target PWM value
        ui8_duty_cycle_target = map_ui8((uint8_t) (i16_control_output >> 2),
                (uint8_t)0,                   // minimum control output from PID
                (uint8_t)250,                 // maximum control output from PID
                (uint8_t)0,                   // minimum duty cycle
                (uint8_t)(PWM_DUTY_CYCLE_MAX-1)); // maximum duty cycle
	}
}


static void apply_hybrid_assist()
{
	uint8_t ui8_tmp;
	uint16_t ui16_adc_battery_current_target_power_assist;
	uint16_t ui16_adc_battery_current_target_torque_assist;
	uint16_t ui16_adc_battery_current_target;
	
	// check for assist without pedal rotation when there is no pedal rotation
	if(ui8_assist_without_pedal_rotation_enabled) {
		if((!ui8_pedal_cadence_RPM)&&
			(ui16_adc_pedal_torque_delta > (120 - ui8_assist_without_pedal_rotation_threshold))) {
				ui8_pedal_cadence_RPM = 1;
		}
	}
	
  if((ui8_pedal_cadence_RPM)||(ui8_startup_assist_adc_battery_current_target)) {
	// calculate torque assistance
	if (ui16_adc_pedal_torque_delta)
	{
		// get the torque assist factor
		uint8_t ui8_torque_assist_factor = ui8_hybrid_torque_parameter;
		
		// calculate torque assist target current
		ui16_adc_battery_current_target_torque_assist = ((uint16_t) ui16_adc_pedal_torque_delta * ui8_torque_assist_factor) / TORQUE_ASSIST_FACTOR_DENOMINATOR;
	}
	else
	{
		ui16_adc_battery_current_target_torque_assist = 0;
	}
	
	// calculate power assistance
	// get the power assist multiplier
	uint8_t ui8_power_assist_multiplier_x50 = ui8_riding_mode_parameter;

	// calculate power assist by multiplying human power with the power assist multiplier
    uint32_t ui32_power_assist_x100 = (((uint32_t)(ui8_pedal_cadence_RPM * ui8_power_assist_multiplier_x50))
            * ui16_pedal_torque_x100) / 480U; // see note below
	
	// calculate power assist target current x100
	uint32_t ui32_battery_current_target_x100 = (ui32_power_assist_x100 * 1000) / ui16_battery_voltage_filtered_x1000;
	
	// calculate power assist target current
	ui16_adc_battery_current_target_power_assist = (uint16_t)ui32_battery_current_target_x100 / BATTERY_CURRENT_PER_10_BIT_ADC_STEP_X100;
	
	// set battery current target in ADC steps
	if(ui16_adc_battery_current_target_power_assist > ui16_adc_battery_current_target_torque_assist)
		ui16_adc_battery_current_target = ui16_adc_battery_current_target_power_assist;
	else
		ui16_adc_battery_current_target = ui16_adc_battery_current_target_torque_assist;
	
	// set motor acceleration
	if (ui16_wheel_speed_x10 >= 200) {
        ui8_duty_cycle_ramp_up_inverse_step = PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN;
        ui8_duty_cycle_ramp_down_inverse_step = PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN;
    } else {
        ui8_duty_cycle_ramp_up_inverse_step = map_ui8((uint8_t)(ui16_wheel_speed_x10>>2),
                (uint8_t)10, // 10*4 = 40 -> 4 kph
                (uint8_t)50, // 50*4 = 200 -> 20 kph
                (uint8_t)ui8_duty_cycle_ramp_up_inverse_step_default,
                (uint8_t)PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN);
        ui8_tmp = map_ui8(ui8_pedal_cadence_RPM,
                (uint8_t)20, // 20 rpm
                (uint8_t)70, // 70 rpm
                (uint8_t)ui8_duty_cycle_ramp_up_inverse_step_default,
                (uint8_t)PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN);
        if (ui8_tmp < ui8_duty_cycle_ramp_up_inverse_step)
            ui8_duty_cycle_ramp_up_inverse_step = ui8_tmp;

        ui8_duty_cycle_ramp_down_inverse_step = map_ui8((uint8_t)(ui16_wheel_speed_x10>>2),
                (uint8_t)10, // 10*4 = 40 -> 4 kph
                (uint8_t)50, // 50*4 = 200 -> 20 kph
                (uint8_t)ui8_duty_cycle_ramp_down_inverse_step_default,
                (uint8_t)PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN);
        ui8_tmp = map_ui8(ui8_pedal_cadence_RPM,
                (uint8_t)20, // 20 rpm
                (uint8_t)70, // 70 rpm
                (uint8_t)ui8_duty_cycle_ramp_down_inverse_step_default,
                (uint8_t)PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN);
        if (ui8_tmp < ui8_duty_cycle_ramp_down_inverse_step)
            ui8_duty_cycle_ramp_down_inverse_step = ui8_tmp;
    }
	
	// set battery current target
	if (ui16_adc_battery_current_target > ui8_adc_battery_current_max) { ui8_adc_battery_current_target = ui8_adc_battery_current_max; }
	else { ui8_adc_battery_current_target = ui16_adc_battery_current_target; }
	
	// set startup assist battery current target
	if (ui8_startup_assist) {
		if (ui8_adc_battery_current_target > ui8_startup_assist_adc_battery_current_target)
			ui8_startup_assist_adc_battery_current_target = ui8_adc_battery_current_target;
		
		ui8_adc_battery_current_target = ui8_startup_assist_adc_battery_current_target;
	} else {
		ui8_startup_assist_adc_battery_current_target = 0;
	}
	
	// set duty cycle target
	if (ui8_adc_battery_current_target) { ui8_duty_cycle_target = PWM_DUTY_CYCLE_MAX; }
	else { ui8_duty_cycle_target = 0; }
  }
}



static void apply_calibration_assist() {
    // ui8_riding_mode_parameter contains the target duty cycle
    uint8_t ui8_calibration_assist_duty_cycle_target = ui8_riding_mode_parameter;

    // limit cadence assist duty cycle target
    if (ui8_calibration_assist_duty_cycle_target >= PWM_DUTY_CYCLE_MAX) {
        ui8_calibration_assist_duty_cycle_target = (uint8_t)(PWM_DUTY_CYCLE_MAX-1);
    }

    // set motor acceleration
    ui8_duty_cycle_ramp_up_inverse_step = PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN;
    ui8_duty_cycle_ramp_down_inverse_step = PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN;

    // set battery current target
    ui8_adc_battery_current_target = ui8_adc_battery_current_max;

    // set duty cycle target
    ui8_duty_cycle_target = ui8_calibration_assist_duty_cycle_target;
}


static void apply_throttle()
{
    // map adc value from 0 to 255
    ui8_throttle_adc = map_ui8((uint8_t)(ui16_adc_throttle >> 2),
            (uint8_t) ADC_THROTTLE_MIN_VALUE,
            (uint8_t) ADC_THROTTLE_MAX_VALUE,
            (uint8_t) 0,
            (uint8_t) 255);
	
    // map ADC throttle value from 0 to max battery current
    uint8_t ui8_adc_battery_current_target_throttle = map_ui8((uint8_t) ui8_throttle_adc,
            (uint8_t) 0,
            (uint8_t) 255,
            (uint8_t) 0,
            (uint8_t) ui8_adc_battery_current_max);
			
    if (ui8_adc_battery_current_target_throttle > ui8_adc_battery_current_target) {
        // set motor acceleration
        if (ui16_wheel_speed_x10 >= 255) {
            ui8_duty_cycle_ramp_up_inverse_step = PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN;
            ui8_duty_cycle_ramp_down_inverse_step = PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN;
        } else {
            ui8_duty_cycle_ramp_up_inverse_step = map_ui8((uint8_t) ui16_wheel_speed_x10,
                    (uint8_t) 40,
                    (uint8_t) 255,
                    (uint8_t) THROTTLE_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_DEFAULT,
                    (uint8_t) THROTTLE_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN);

            ui8_duty_cycle_ramp_down_inverse_step = map_ui8((uint8_t) ui16_wheel_speed_x10,
                    (uint8_t) 40,
                    (uint8_t) 255,
                    (uint8_t) PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_DEFAULT,
                    (uint8_t) PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN);
        }
        // set battery current target
        ui8_adc_battery_current_target = ui8_adc_battery_current_target_throttle;

        // set duty cycle target
        ui8_duty_cycle_target = PWM_DUTY_CYCLE_MAX;
    }
}


static void apply_temperature_limiting()
{
    // get ADC measurement
    volatile uint16_t ui16_temp = ui16_adc_throttle;

    // filter ADC measurement to motor temperature variable
    ui16_adc_motor_temperature_filtered = filter(ui16_temp, ui16_adc_motor_temperature_filtered, 8);

    // convert ADC value
    ui16_motor_temperature_filtered_x10 = (uint16_t)(((uint32_t) ui16_adc_motor_temperature_filtered * 10000) / 2048);

    // min temperature value can not be equal or higher than max temperature value
    if (ui8_motor_temperature_min_value_to_limit >= ui8_motor_temperature_max_value_to_limit) {
        ui8_adc_battery_current_target = 0;
		ui8_temperature_current_limiting_value = 0;
    } else {
        // adjust target current if motor over temperature limit
        ui8_adc_battery_current_target = map_ui16((uint16_t) ui16_motor_temperature_filtered_x10,
				(uint16_t) ((uint8_t)ui8_motor_temperature_min_value_to_limit * (uint8_t)10U),
				(uint16_t) ((uint8_t)ui8_motor_temperature_max_value_to_limit * (uint8_t)10U),
				ui8_adc_battery_current_target,
				0);
				
		// get a value linear to the current limitation, just to show to user (to LCD3)
		ui8_temperature_current_limiting_value = map_ui16((uint16_t) ui16_motor_temperature_filtered_x10,
                (uint16_t) ui8_motor_temperature_min_value_to_limit * 10U,
                (uint16_t) ui8_motor_temperature_max_value_to_limit * 10U,
                (uint16_t) 255,
                0);

	}
}


static void apply_speed_limit()
{
    if (m_configuration_variables.ui8_wheel_speed_max > 0) {
        // set battery current target
        ui8_adc_battery_current_target = map_ui16((uint16_t) ui16_wheel_speed_x10,
                (uint16_t) (((uint8_t)(m_configuration_variables.ui8_wheel_speed_max) * (uint8_t)10U) - (uint8_t)20U),
                (uint16_t) (((uint8_t)(m_configuration_variables.ui8_wheel_speed_max) * (uint8_t)10U) + (uint8_t)20U),
                ui8_adc_battery_current_target,
                0);
    }
}


static void calc_wheel_speed(void)
{
    // calc wheel speed (km/h x10)
    if (ui16_wheel_speed_sensor_ticks) {
        uint16_t ui16_tmp = ui16_wheel_speed_sensor_ticks;
        // rps = PWM_CYCLES_SECOND / ui16_wheel_speed_sensor_ticks (rev/sec)
        // km/h*10 = rps * ui16_wheel_perimeter * ((3600 / (1000 * 1000)) * 10)
        // !!!warning if PWM_CYCLES_SECOND is not a multiple of 1000
        ui16_wheel_speed_x10 = (uint16_t)(((uint32_t) m_configuration_variables.ui16_wheel_perimeter * ((PWM_CYCLES_SECOND/1000)*36U)) / ui16_tmp);
    } else {
        ui16_wheel_speed_x10 = 0;
    }
}


static void calc_cadence(void)
{
    // get the cadence sensor ticks
    uint16_t ui16_cadence_sensor_ticks_temp = ui16_cadence_sensor_ticks;

    // adjust cadence sensor ticks counter min depending on wheel speed
    ui16_cadence_ticks_count_min_speed_adj = map_ui16(ui16_wheel_speed_x10,
            40,
            400,
            CADENCE_SENSOR_CALC_COUNTER_MIN,
            CADENCE_SENSOR_TICKS_COUNTER_MIN_AT_SPEED);

    // calculate cadence in RPM and avoid zero division
    // !!!warning if PWM_CYCLES_SECOND > 21845
    if (ui16_cadence_sensor_ticks_temp) {
        ui8_pedal_cadence_RPM = (uint8_t)((PWM_CYCLES_SECOND * 3U) / ui16_cadence_sensor_ticks_temp);
		
		if(ui8_pedal_cadence_RPM > 120) {ui8_pedal_cadence_RPM = 120;}
	}
    else {
        ui8_pedal_cadence_RPM = 0;
	}
	
	/*-------------------------------------------------------------------------------------------------

     NOTE: regarding the cadence calculation

     Cadence is calculated by counting how many ticks there are between two LOW to HIGH transitions.

     Formula for calculating the cadence in RPM:

     (1) Cadence in RPM = (60 * PWM_CYCLES_SECOND) / CADENCE_SENSOR_NUMBER_MAGNETS) / ticks

     (2) Cadence in RPM = (PWM_CYCLES_SECOND * 3) / ticks

     -------------------------------------------------------------------------------------------------*/
}


#define TOFFSET_CYCLES 120 // 3sec (25ms*120)
static uint8_t toffset_cycle_counter = 0;

static void get_pedal_torque(void)
{
	uint16_t ui16_temp = 0;
	
    if(toffset_cycle_counter < TOFFSET_CYCLES) {
        uint16_t ui16_tmp = ui16_adc_torque;
        ui16_adc_pedal_torque_offset_init = filter(ui16_tmp, ui16_adc_pedal_torque_offset_init, 2);
        toffset_cycle_counter++;
		
        ui16_adc_pedal_torque = ui16_adc_pedal_torque_offset_init;
		ui16_adc_pedal_torque_offset_cal = ui16_adc_pedal_torque_offset_init + ui8_adc_torque_calibration_offset;
	}
	else {
		// torque sensor offset adjustment
		ui16_adc_pedal_torque_offset = ui16_adc_pedal_torque_offset_cal;
		if(ui8_pedal_cadence_RPM) {
			ui16_adc_pedal_torque_offset -= ui8_adc_torque_middle_offset_adj;
			ui16_adc_pedal_torque_offset += ui8_adc_pedal_torque_offset_adj;
		}

		if((ui8_coaster_brake_enabled)&&(ui16_adc_pedal_torque_offset > ui8_coaster_brake_torque_threshold)) {
			//ui16_adc_coaster_brake_threshold = ui16_adc_pedal_torque_offset - ui8_coaster_brake_torque_threshold;
			ui16_adc_coaster_brake_threshold = ui16_adc_pedal_torque_offset_cal - ui8_coaster_brake_torque_threshold;
		}
		else {
			ui16_adc_coaster_brake_threshold = 0;
		}
		
        // get adc pedal torque
        ui16_adc_pedal_torque = ui16_adc_torque;
    }

    // calculate the delta value from calibration
    if(ui16_adc_pedal_torque > ui16_adc_pedal_torque_offset) {
		// adc pedal torque delta remapping
		if(ui8_torque_sensor_calibration_enabled) {
			// adc pedal torque delta adjustment
			ui16_temp = ui16_adc_pedal_torque - ui16_adc_pedal_torque_offset_init;
			ui16_adc_pedal_torque_delta = ui16_adc_pedal_torque - ui16_adc_pedal_torque_offset
				- ((ui8_adc_pedal_torque_delta_adj	* ui16_temp) / ADC_TORQUE_SENSOR_RANGE_TARGET);
		
			// adc pedal torque range adjusment
			ui16_temp = (ui16_adc_pedal_torque_delta * ui16_adc_pedal_torque_range_ingrease_x100) / 10;
			ui16_adc_pedal_torque_delta = (((ui16_temp
				* ((ui16_temp / ADC_TORQUE_SENSOR_ANGLE_COEFF + ADC_TORQUE_SENSOR_ANGLE_COEFF_X10) / ADC_TORQUE_SENSOR_ANGLE_COEFF)) / 50) // 100
				* (100 + ui8_adc_pedal_torque_range_adj)) / 200; // 100
			
			// adc pedal torque angle adjusment
			if(ui16_adc_pedal_torque_delta < ui16_adc_pedal_torque_range_target_max) {
				ui16_temp = (ui16_adc_pedal_torque_delta * ui16_adc_pedal_torque_range_target_max)
					/ (ui16_adc_pedal_torque_range_target_max - (((ui16_adc_pedal_torque_range_target_max - ui16_adc_pedal_torque_delta) * 10)
					/ ui8_adc_pedal_torque_angle_adj));
				
				ui16_adc_pedal_torque_delta = ui16_temp;
			}
		}
		else {
			ui16_adc_pedal_torque_delta = ui16_adc_pedal_torque - ui16_adc_pedal_torque_offset;
		}
		ui16_adc_pedal_torque_delta = (ui16_adc_pedal_torque_delta + ui16_adc_pedal_torque_delta_temp) >> 1;
	}
	else {
		ui16_adc_pedal_torque_delta = 0;
    }
	ui16_adc_pedal_torque_delta_temp = ui16_adc_pedal_torque_delta;
	
	// for startup assist without pedaling in power assist mode
	ui16_adc_pedal_torque_delta_no_boost = ui16_adc_pedal_torque_delta;
	
    // calculate torque on pedals
    ui16_pedal_torque_x100 = ui16_adc_pedal_torque_delta * m_configuration_variables.ui8_pedal_torque_per_10_bit_ADC_step_x100;
	
	// calculate human power x10
	//ui16_human_power_x10 = (ui16_pedal_torque_x100 * ui8_pedal_cadence_RPM) / 96; // see note below
	
	// startup boost mode
	switch (ui8_startup_boost_at_zero) {
		case CADENCE:
			ui8_startup_boost_flag = 1;
			break;
		case SPEED:
			if(ui16_wheel_speed_x10 == 0) {
				ui8_startup_boost_flag = 1;
			}
			else if(ui8_pedal_cadence_RPM > 45) {
				ui8_startup_boost_flag = 0;
			}
			break;
	}
	// pedal torque delta + startup boost in power assist mode
	if((ui8_startup_boost_enabled)&&(ui8_startup_boost_flag)
	  &&(ui8_riding_mode == POWER_ASSIST_MODE)) {
		// calculate startup boost torque & new pedal torque delta
		uint32_t ui32_temp = ((uint32_t)(ui16_adc_pedal_torque_delta * ui16_startup_boost_factor_array[ui8_pedal_cadence_RPM])) / 100;
		ui16_adc_pedal_torque_delta += (uint16_t) ui32_temp;
	}
	
	/*------------------------------------------------------------------------

    NOTE: regarding the human power calculation
    
    (1) Formula: power = torque * rotations per second * 2 * pi
    (2) Formula: power = torque * rotations per minute * 2 * pi / 60
    (3) Formula: power = torque * rotations per minute * 0.1047
    (4) Formula: power = torque * 100 * rotations per minute * 0.001047
    (5) Formula: power = torque * 100 * rotations per minute / 955
    (6) Formula: power * 10  =  torque * 100 * rotations per minute / 96
    
	------------------------------------------------------------------------*/
}


struct_configuration_variables* get_configuration_variables(void)
{
    return &m_configuration_variables;
}


static void check_system()
{
// E08 ERROR_SPEED_SENSOR
#define CHECK_SPEED_SENSOR_COUNTER_THRESHOLD          125 // 125 * 100ms = 12.5 seconds
#define MOTOR_ERPS_SPEED_THRESHOLD	                  180
	static uint16_t ui16_check_speed_sensor_counter;
	static uint16_t ui16_error_speed_sensor_counter;
	
	// check speed sensor
	if((ui16_motor_speed_erps > MOTOR_ERPS_SPEED_THRESHOLD)
	  &&(ui8_riding_mode != WALK_ASSIST_MODE)) {
		ui16_check_speed_sensor_counter++;
	}
	else {
		ui16_check_speed_sensor_counter = 0;
	}
	
	if(ui16_wheel_speed_x10) {
		ui16_check_speed_sensor_counter = 0;
	}
	
	if(ui16_check_speed_sensor_counter > CHECK_SPEED_SENSOR_COUNTER_THRESHOLD) {
		
		// set speed sensor error code
		ui8_system_state = ERROR_SPEED_SENSOR;
	}
	else if (ui8_system_state == ERROR_SPEED_SENSOR) {
		// increment speed sensor error reset counter
        ui16_error_speed_sensor_counter++;

        // check if the counter has counted to the set threshold for reset
        if (ui16_error_speed_sensor_counter > CHECK_SPEED_SENSOR_COUNTER_THRESHOLD) {
            // reset speed sensor error code
            if (ui8_system_state == ERROR_SPEED_SENSOR) {
				ui8_system_state = NO_ERROR;
				ui16_error_speed_sensor_counter = 0;
            }
		}
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// E03 ERROR_CADENCE_SENSOR
#define CHECK_CADENCE_SENSOR_COUNTER_THRESHOLD          250 // 250 * 100ms = 25 seconds
#define CADENCE_SENSOR_RESET_COUNTER_THRESHOLD         	80  // 800 * 100ms = 8.0 seconds
#define ADC_TORQUE_SENSOR_DELTA_THRESHOLD				(uint16_t)((ADC_TORQUE_SENSOR_RANGE_TARGET >> 1) + 20)
	static uint8_t ui8_check_cadence_sensor_counter;
	static uint8_t ui8_error_cadence_sensor_counter;
	
	// check cadence sensor
	if((ui16_adc_pedal_torque_delta_no_boost > ADC_TORQUE_SENSOR_DELTA_THRESHOLD)
	  &&(!ui8_startup_assist)&&((ui8_pedal_cadence_RPM > 130)||(!ui8_pedal_cadence_RPM))) {
		ui8_check_cadence_sensor_counter++;
	}
	else {
		ui8_check_cadence_sensor_counter = 0;
	}
	
	if(ui8_check_cadence_sensor_counter > CHECK_CADENCE_SENSOR_COUNTER_THRESHOLD) {
		// set cadence sensor error code
		ui8_system_state = ERROR_CADENCE_SENSOR;
	}
	else if (ui8_system_state == ERROR_CADENCE_SENSOR) {
		// increment cadence sensor error reset counter
        ui8_error_cadence_sensor_counter++;

        // check if the counter has counted to the set threshold for reset
        if (ui8_error_cadence_sensor_counter > CADENCE_SENSOR_RESET_COUNTER_THRESHOLD) {
            // reset cadence sensor error code
            if (ui8_system_state == ERROR_CADENCE_SENSOR) {
				ui8_system_state = NO_ERROR;
				ui8_error_cadence_sensor_counter = 0;
            }
		}
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// E02 ERROR_TORQUE_SENSOR
    // check torque sensor
    if (((ui16_adc_pedal_torque_offset > 300) ||
		(ui16_adc_pedal_torque_offset < 10) ||
		(ui16_adc_pedal_torque > 500))
        && ((ui8_riding_mode == POWER_ASSIST_MODE) ||
			(ui8_riding_mode == TORQUE_ASSIST_MODE)||
			(ui8_riding_mode == HYBRID_ASSIST_MODE)||
			(ui8_riding_mode == eMTB_ASSIST_MODE))) {
        // set error code
        ui8_system_state = ERROR_TORQUE_SENSOR;
    } else if (ui8_system_state == ERROR_TORQUE_SENSOR) {
        // reset error code
        ui8_system_state = NO_ERROR;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// E04 ERROR_MOTOR_BLOCKED
#define MOTOR_BLOCKED_COUNTER_THRESHOLD               	2  // 2 * 100ms = 0.2 seconds
#define MOTOR_BLOCKED_BATTERY_CURRENT_THRESHOLD_X10   	30 // 30 = 3.0 amps
#define MOTOR_BLOCKED_ERPS_THRESHOLD                  	20 // 20 ERPS
#define MOTOR_BLOCKED_RESET_COUNTER_THRESHOLD         	80  // 80 * 100ms = 8.0 seconds

    static uint8_t ui8_motor_blocked_counter;
    static uint8_t ui8_motor_blocked_reset_counter;

    // if the motor blocked error is enabled start resetting it
    if (ui8_system_state == ERROR_MOTOR_BLOCKED) {
        // increment motor blocked reset counter with 25 milliseconds
        ui8_motor_blocked_reset_counter++;

        // check if the counter has counted to the set threshold for reset
        if (ui8_motor_blocked_reset_counter > MOTOR_BLOCKED_RESET_COUNTER_THRESHOLD) {
            // reset motor blocked error code
            if (ui8_system_state == ERROR_MOTOR_BLOCKED) {
                ui8_system_state = NO_ERROR;
            }

            // reset the counter that clears the motor blocked error
            ui8_motor_blocked_reset_counter = 0;
        }
    } else {
        // if battery current is over the current threshold and the motor ERPS is below threshold start setting motor blocked error code
        if ((ui8_battery_current_filtered_x10 > MOTOR_BLOCKED_BATTERY_CURRENT_THRESHOLD_X10)
                && (ui16_motor_speed_erps < MOTOR_BLOCKED_ERPS_THRESHOLD)) {
            // increment motor blocked counter with 100 milliseconds
            ++ui8_motor_blocked_counter;

            // check if motor is blocked for more than some safe threshold
            if (ui8_motor_blocked_counter > MOTOR_BLOCKED_COUNTER_THRESHOLD) {
                // set error code
                ui8_system_state = ERROR_MOTOR_BLOCKED;

                // reset motor blocked counter as the error code is set
                ui8_motor_blocked_counter = 0;
            }
        } else {
            // current is below the threshold and/or motor ERPS is above the threshold so reset the counter
            ui8_motor_blocked_counter = 0;
        }
    }
}


void ebike_control_lights(void)
{
#define DEFAULT_FLASH_ON_COUNTER_MAX      3
#define DEFAULT_FLASH_OFF_COUNTER_MAX     2
#define BRAKING_FLASH_ON_COUNTER_MAX      1
#define BRAKING_FLASH_OFF_COUNTER_MAX     1

    static uint8_t ui8_default_flash_state;
    static uint8_t ui8_default_flash_state_counter; // increments every function call -> 100 ms
    static uint8_t ui8_braking_flash_state;
    static uint8_t ui8_braking_flash_state_counter; // increments every function call -> 100 ms

    /****************************************************************************/

    // increment flash counters
    ++ui8_default_flash_state_counter;
    ++ui8_braking_flash_state_counter;

    /****************************************************************************/

    // set default flash state
    if ((ui8_default_flash_state) && (ui8_default_flash_state_counter > DEFAULT_FLASH_ON_COUNTER_MAX)) {
        // reset flash state counter
        ui8_default_flash_state_counter = 0;

        // toggle flash state
        ui8_default_flash_state = 0;
    } else if ((!ui8_default_flash_state) && (ui8_default_flash_state_counter > DEFAULT_FLASH_OFF_COUNTER_MAX)) {
        // reset flash state counter
        ui8_default_flash_state_counter = 0;

        // toggle flash state
        ui8_default_flash_state = 1;
    }

    /****************************************************************************/

    // set braking flash state
    if ((ui8_braking_flash_state) && (ui8_braking_flash_state_counter > BRAKING_FLASH_ON_COUNTER_MAX)) {
        // reset flash state counter
        ui8_braking_flash_state_counter = 0;

        // toggle flash state
        ui8_braking_flash_state = 0;
    } else if ((!ui8_braking_flash_state) && (ui8_braking_flash_state_counter > BRAKING_FLASH_OFF_COUNTER_MAX)) {
        // reset flash state counter
        ui8_braking_flash_state_counter = 0;

        // toggle flash state
        ui8_braking_flash_state = 1;
    }

    /****************************************************************************/

    // select lights configuration
    switch (ui8_lights_configuration) {
    case 0:

        // set lights
        lights_set_state(ui8_lights_state);

        break;

    case 1:

        // check lights state
        if (ui8_lights_state) {
            // set lights
            lights_set_state(ui8_default_flash_state);
        } else {
            // set lights
            lights_set_state(ui8_lights_state);
        }

        break;

    case 2:

        // check light and brake state
        if (ui8_lights_state && ui8_brake_state) {
            // set lights
            lights_set_state(ui8_braking_flash_state);
        } else {
            // set lights
            lights_set_state(ui8_lights_state);
        }

        break;

    case 3:

        // check light and brake state
        if (ui8_lights_state && ui8_brake_state) {
            // set lights
            lights_set_state(ui8_brake_state);
        } else if (ui8_lights_state) {
            // set lights
            lights_set_state(ui8_default_flash_state);
        } else {
            // set lights
            lights_set_state(ui8_lights_state);
        }

        break;

    case 4:

        // check light and brake state
        if (ui8_lights_state && ui8_brake_state) {
            // set lights
            lights_set_state(ui8_braking_flash_state);
        } else if (ui8_lights_state) {
            // set lights
            lights_set_state(ui8_default_flash_state);
        } else {
            // set lights
            lights_set_state(ui8_lights_state);
        }

        break;

    case 5:

        // check brake state
        if (ui8_brake_state) {
            // set lights
            lights_set_state(ui8_brake_state);
        } else {
            // set lights
            lights_set_state(ui8_lights_state);
        }

        break;

    case 6:

        // check brake state
        if (ui8_brake_state) {
            // set lights
            lights_set_state(ui8_braking_flash_state);
        } else {
            // set lights
            lights_set_state(ui8_lights_state);
        }

        break;

    case 7:

        // check brake state
        if (ui8_brake_state) {
            // set lights
            lights_set_state(ui8_brake_state);
        } else if (ui8_lights_state) {
            // set lights
            lights_set_state(ui8_default_flash_state);
        } else {
            // set lights
            lights_set_state(ui8_lights_state);
        }

        break;

    case 8:

        // check brake state
        if (ui8_brake_state) {
            // set lights
            lights_set_state(ui8_braking_flash_state);
        } else if (ui8_lights_state) {
            // set lights
            lights_set_state(ui8_default_flash_state);
        } else {
            // set lights
            lights_set_state(ui8_lights_state);
        }

        break;

    default:

        // set lights
        lights_set_state(ui8_lights_state);

        break;
    }

    /*------------------------------------------------------------------------------------------------------------------

     NOTE: regarding the various light modes

     (0) lights ON when enabled
     (1) lights FLASHING when enabled

     (2) lights ON when enabled and BRAKE-FLASHING when braking
     (3) lights FLASHING when enabled and ON when braking
     (4) lights FLASHING when enabled and BRAKE-FLASHING when braking

     (5) lights ON when enabled, but ON when braking regardless if lights are enabled
     (6) lights ON when enabled, but BRAKE-FLASHING when braking regardless if lights are enabled

     (7) lights FLASHING when enabled, but ON when braking regardless if lights are enabled
     (8) lights FLASHING when enabled, but BRAKE-FLASHING when braking regardless if lights are enabled

     ------------------------------------------------------------------------------------------------------------------*/
}

// This is the interrupt that happens when UART2 receives data. We need it to be the fastest possible and so
// we do: receive every byte and assembly as a package, finally, signal that we have a package to process (on main slow loop)
// and disable the interrupt. The interrupt should be enable again on main loop, after the package being processed
void UART2_IRQHandler(void) __interrupt(UART2_IRQHANDLER)
{
  if (UART2_GetFlagStatus(UART2_FLAG_RXNE) == SET)
  {
    UART2->SR &= (uint8_t)~(UART2_FLAG_RXNE); // this may be redundant

    ui8_byte_received = UART2_ReceiveData8 ();

    switch (ui8_state_machine)
    {
      case 0:
      if (ui8_byte_received == 0x59) // see if we get start package byte
      {
        ui8_rx_buffer [ui8_rx_counter] = ui8_byte_received;
        ui8_rx_counter++;
        ui8_state_machine = 1;
      }
      else
      {
        ui8_rx_counter = 0;
        ui8_state_machine = 0;
      }
      break;

      case 1:
      ui8_rx_buffer [ui8_rx_counter] = ui8_byte_received;
      
      // increment index for next byte
      ui8_rx_counter++;

      // reset if it is the last byte of the package and index is out of bounds
      if (ui8_rx_counter >= UART_NUMBER_DATA_BYTES_TO_RECEIVE + 3)
      {
        ui8_rx_counter = 0;
        ui8_state_machine = 0;
        ui8_received_package_flag = 1; // signal that we have a full package to be processed
        UART2->CR2 &= ~(1 << 5); // disable UART2 receive interrupt
      }
      break;

      default:
      break;
    }
  }
}


static void uart_receive_package(void)
{
	uint8_t ui8_temp;
	uint16_t ui16_temp;
	
	// Reset to 0 when a valid message from the LCD is received
	no_rx_counter++;
	
	if (ui8_received_package_flag)
	{
		// validation of the package data
		ui16_crc_rx = 0xffff;
    
		for (ui8_i = 0; ui8_i <= UART_NUMBER_DATA_BYTES_TO_RECEIVE; ui8_i++)
		{
		crc16 (ui8_rx_buffer[ui8_i], &ui16_crc_rx);
		}

		// if CRC is correct read the package (16 bit value and therefore last two bytes)
		if (((((uint16_t) ui8_rx_buffer [UART_NUMBER_DATA_BYTES_TO_RECEIVE + 2]) << 8) + ((uint16_t) ui8_rx_buffer [UART_NUMBER_DATA_BYTES_TO_RECEIVE + 1])) == ui16_crc_rx)
		{
			// Reset the safety counter
            no_rx_counter = 0;
			
            // message ID
            ui8_message_ID = ui8_rx_buffer[1];
			
			ui8_temp = ui8_rx_buffer[2];
			// lights state
            ui8_lights_state = ui8_temp & 1;
            // riding mode
            ui8_riding_mode = (ui8_temp & 240 ) >> 4;
			
			if(ui8_riding_mode != WALK_ASSIST_MODE)
			// reset walk assist speed flag
			ui8_walk_assist_speed_flag = 0;

            // riding mode parameter
            ui8_riding_mode_parameter = ui8_rx_buffer[3];
			
			// hybrid torque parameter
			ui8_hybrid_torque_parameter = ui8_rx_buffer[4];

            switch (ui8_message_ID) {
              case 0:
                // battery low voltage cut off x10
                m_configuration_variables.ui16_battery_low_voltage_cut_off_x10 = (((uint16_t) ui8_rx_buffer[6]) << 8) + ((uint16_t) ui8_rx_buffer[5]);

                // set low voltage cutoff (10 bit)
                ui16_adc_voltage_cut_off = (m_configuration_variables.ui16_battery_low_voltage_cut_off_x10 * 100U) / BATTERY_VOLTAGE_PER_10_BIT_ADC_STEP_X1000;

				// set low voltage shutdown (10 bit)
				ui16_adc_voltage_shutdown = ui16_adc_voltage_cut_off - DIFFERENCE_CUT_OFF_SHUTDOWN_10_BIT;
		
                // wheel max speed
                m_configuration_variables.ui8_wheel_speed_max = ui8_rx_buffer[7];
                break;
				
              case 1:
                // wheel perimeter
                m_configuration_variables.ui16_wheel_perimeter = (((uint16_t) ui8_rx_buffer[6]) << 8) + ((uint16_t) ui8_rx_buffer[5]);
				
				ui8_temp = ui8_rx_buffer[7];
                // motor temperature limit function or throttle
                m_configuration_variables.ui8_optional_ADC_function = ui8_temp & 3;
				// coaster brake torque threshold
				ui8_coaster_brake_torque_threshold = (ui8_temp & 252 ) >> 2;
				// coaster brake enabled
				if(ui8_coaster_brake_torque_threshold) {
					ui8_coaster_brake_enabled = 1;
				}
				else {
					ui8_coaster_brake_enabled = 0;
				}
                break;

              case 2:
				ui8_temp = ui8_rx_buffer[5];
                // type of motor (1=36 volt, 0=48 volt)
				if(ui8_temp & 1) {
					m_configuration_variables.ui8_motor_inductance_x1048576 = 80; // 36 V motor
					i16_cruise_pid_kp = 14;
					i16_cruise_pid_ki = 0.7;
				}
				else {
					m_configuration_variables.ui8_motor_inductance_x1048576 = 142; // 48 V motor
					i16_cruise_pid_kp = 12;
					i16_cruise_pid_ki = 1;
				}
				// bit variables
				ui8_startup_boost_enabled = (ui8_temp & 2) >> 1;
				ui8_torque_sensor_calibration_enabled = (ui8_temp & 4) >> 2;
				ui8_assist_with_error_enabled = (ui8_temp & 8) >> 3;
				//ui8_pedal_cadence_fast_stop = (ui8_temp & 16) >> 4;
				ui8_startup_boost_at_zero = (ui8_temp & 16) >> 4;
				ui8_field_weakening_enabled = (ui8_temp & 32) >> 5;
				ui8_assist_level_flag = !((ui8_temp & 64) >> 6);
				ui8_startup_assist = (ui8_temp & 128) >> 7;
				
                // motor over temperature min value limit
                ui8_motor_temperature_min_value_to_limit = ui8_rx_buffer[6];
				
                // motor over temperature max value limit
                ui8_motor_temperature_max_value_to_limit = ui8_rx_buffer[7];
                break;

			  case 3:
				// adc torque calibration offset & middle offset adj
				ui8_adc_torque_calibration_offset = (ui8_rx_buffer[5] & 7);
				ui8_adc_torque_middle_offset_adj = (ui8_rx_buffer[5] & 248) >> 3;
				
				// pedal torque ADC range (weight=max)
				ui16_adc_pedal_torque_range = (((uint16_t) ui8_rx_buffer[7]) << 8) + ((uint16_t) ui8_rx_buffer[6]);
				ui16_adc_pedal_torque_range_ingrease_x100 = (ADC_TORQUE_SENSOR_RANGE_TARGET * 50) / ui16_adc_pedal_torque_range; //  / 2 * 100
				break;

              case 4:
                // lights configuration
                ui8_lights_configuration = ui8_rx_buffer[5];
								
                // assist without pedal rotation threshold
                ui8_assist_without_pedal_rotation_threshold = ui8_rx_buffer[6];
				// assist without pedal rotation enabled
				if(ui8_assist_without_pedal_rotation_threshold)
					ui8_assist_without_pedal_rotation_enabled = 1;
				else
					ui8_assist_without_pedal_rotation_enabled = 0;
				
                // check if assist without pedal rotation threshold is valid (safety)
                if (ui8_assist_without_pedal_rotation_threshold > 100)
                    ui8_assist_without_pedal_rotation_threshold = 0;

                // motor acceleration adjustment
                uint8_t ui8_motor_acceleration_adjustment = ui8_rx_buffer[7];

                // set duty cycle ramp up inverse step
                ui8_duty_cycle_ramp_up_inverse_step_default = map_ui8((uint8_t)ui8_motor_acceleration_adjustment,
                        (uint8_t) 0,
                        (uint8_t) 100,
                        (uint8_t) PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_DEFAULT,
                        (uint8_t) PWM_DUTY_CYCLE_RAMP_UP_INVERSE_STEP_MIN);
                break;

              case 5:
                // pedal torque conversion
				// torque sensor adc step (default 67) calibration disabled
				// torque sensor adc step (default 34) calibration enabled
                m_configuration_variables.ui8_pedal_torque_per_10_bit_ADC_step_x100 = ui8_rx_buffer[5];

                // max battery current
                ui8_battery_current_max = ui8_rx_buffer[6];

                // battery power limit
                m_configuration_variables.ui8_target_battery_max_power_div25 = ui8_rx_buffer[7];

                // calculate max battery current in ADC steps from the received battery current limit
                uint8_t ui8_adc_battery_current_max_temp_1 = (uint16_t)(ui8_battery_current_max * (uint8_t)100)
                        / (uint16_t)BATTERY_CURRENT_PER_10_BIT_ADC_STEP_X100;

                // calculate max battery current in ADC steps from the received power limit
                uint32_t ui32_battery_current_max_x100 = ((uint32_t) m_configuration_variables.ui8_target_battery_max_power_div25 * 2500000)
                        / ui16_battery_voltage_filtered_x1000;
                uint8_t ui8_adc_battery_current_max_temp_2 = ui32_battery_current_max_x100 / BATTERY_CURRENT_PER_10_BIT_ADC_STEP_X100;

                // set max battery current
                ui8_adc_battery_current_max = ui8_min(ui8_adc_battery_current_max_temp_1, ui8_adc_battery_current_max_temp_2);
				ui16_temp = (uint16_t)(ui8_adc_battery_current_max * ADC_10_BIT_MOTOR_PHASE_CURRENT_MAX);
				ui8_adc_motor_phase_current_max = (uint8_t)(ui16_temp / ADC_10_BIT_BATTERY_CURRENT_MAX);
                break;

              case 6:
				// startup boost torque factor
				ui16_startup_boost_factor_array[0] = (uint16_t) ui8_rx_buffer[5] << 1;
				
				// startup boost cadence step
				ui8_startup_boost_cadence_step = ui8_rx_buffer[6];
				
				if(ui16_startup_boost_factor_temp != ui16_startup_boost_factor_array[0]) {
					for (ui8_i = 1; ui8_i < 120; ui8_i++) {
						ui16_temp = (ui16_startup_boost_factor_array[ui8_i - 1] * (uint16_t)ui8_startup_boost_cadence_step) >> 8;
						ui16_startup_boost_factor_array[ui8_i] = ui16_startup_boost_factor_array[ui8_i - 1] - ui16_temp;	
					}
					ui16_startup_boost_factor_temp = ui16_startup_boost_factor_array[0];
				}

				// motor deceleration adjustment
				uint8_t ui8_motor_deceleration_adjustment = ui8_rx_buffer[7];
	  
				// set duty cycle ramp down inverse step default
				ui8_duty_cycle_ramp_down_inverse_step_default = map_ui8((uint8_t)ui8_motor_deceleration_adjustment,
					(uint8_t) 0,
					(uint8_t) 100,
					(uint8_t) PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_DEFAULT,
					(uint8_t) PWM_DUTY_CYCLE_RAMP_DOWN_INVERSE_STEP_MIN);
					
				if(ui8_motor_deceleration_adjustment == 100)
					ui8_pedal_cadence_fast_stop = 1;
				else
					ui8_pedal_cadence_fast_stop = 0;

                break;
			  
			  case 7:
				// Torque ADC offset adjustment (0 / 34)
				ui8_adc_pedal_torque_offset_adj = ui8_rx_buffer[5];
				// Torque ADC delta adjustment
				ui8_adc_pedal_torque_delta_adj = (ui8_adc_torque_middle_offset_adj * 2) - ui8_adc_torque_calibration_offset - ui8_adc_pedal_torque_offset_adj;
				
				// Torque ADC range adjustment (0 / 40)
				ui8_adc_pedal_torque_range_adj = ui8_rx_buffer[6];
				// pedal torque range target max
				ui16_adc_pedal_torque_range_target_max = (ADC_TORQUE_SENSOR_RANGE_TARGET_MIN
					* (100 + ui8_adc_pedal_torque_range_adj)) / 100;
			
				// Torque ADC angle adjustment (0 / 40)
				ui8_adc_pedal_torque_angle_adj = ui8_rx_buffer[7];
				break;
				
              default:
                // nothing, should display error code
                break;
            }
        }

        // signal that we processed the full package
        ui8_received_package_flag = 0;

        // enable UART2 receive interrupt as we are now ready to receive a new package
        UART2->CR2 |= (1 << 5);
    }
	
	// reset riding mode if connection with the LCD is lost for more than 0,3 sec (safety)
	if (no_rx_counter > 3)
		ui8_riding_mode = OFF_MODE;
}


static void uart_send_package(void) {
	uint16_t ui16_temp;
	
    // start up byte
    ui8_tx_buffer[0] = 0x43;

    // battery voltage filtered x1000
    ui8_tx_buffer[1] = (uint8_t) (ui16_battery_voltage_filtered_x1000 & 0xff);
    ui8_tx_buffer[2] = (uint8_t) (ui16_battery_voltage_filtered_x1000 >> 8);
	
    // battery current filtered x10
    ui8_tx_buffer[3] = ui8_battery_current_filtered_x10;

    // wheel speed x10
    ui8_tx_buffer[4] = (uint8_t) (ui16_wheel_speed_x10 & 0xff);
    ui8_tx_buffer[5] = (uint8_t) (ui16_wheel_speed_x10 >> 8);

	// brake state (bit 0)
	ui8_tx_buffer[6] = ui8_brake_state;
	// Bit free for future use
  
    // optional ADC channel value
    ui8_tx_buffer[7] = (uint8_t)(ui16_adc_throttle >> 2);

	// throttle or temperature control
	switch (m_configuration_variables.ui8_optional_ADC_function)
	{
		case THROTTLE_CONTROL:
			// throttle value with offset applied and mapped from 0 to 255
			ui8_tx_buffer[8] = ui8_throttle_adc;
			break;
		case TEMPERATURE_CONTROL:
			// current limiting mapped from 0 to 255
			ui8_tx_buffer[8] = ui8_temperature_current_limiting_value;
			break;
	}
  
    // ADC torque sensor
    ui8_tx_buffer[9] = (uint8_t) (ui16_adc_pedal_torque & 0xff);
    ui8_tx_buffer[10] = (uint8_t) (ui16_adc_pedal_torque >> 8);

    // pedal cadence
    ui8_tx_buffer[11] = ui8_pedal_cadence_RPM;

	// convert duty-cycle to 0 - 100 %
	ui16_temp = (uint16_t) ui8_g_duty_cycle;
	ui16_temp = (ui16_temp * 100) / PWM_DUTY_CYCLE_MAX;
	ui8_tx_buffer[12] = (uint8_t) ui16_temp;

    // motor speed in ERPS
    ui8_tx_buffer[13] = (uint8_t) (ui16_motor_speed_erps & 0xff);
    ui8_tx_buffer[14] = (uint8_t) (ui16_motor_speed_erps >> 8);

    // FOC angle
    ui8_tx_buffer[15] = ui8_g_foc_angle;

    // system state
    ui8_tx_buffer[16] = ui8_system_state;

    // motor temperature
    ui8_tx_buffer[17] = ui16_motor_temperature_filtered_x10 / 10U;

    // wheel_speed_sensor_tick_counter
    ui8_tx_buffer[18] = (uint8_t) (ui32_wheel_speed_sensor_ticks_total & 0xff);
    ui8_tx_buffer[19] = (uint8_t) ((ui32_wheel_speed_sensor_ticks_total >> 8) & 0xff);
    ui8_tx_buffer[20] = (uint8_t) ((ui32_wheel_speed_sensor_ticks_total >> 16) & 0xff);

    // pedal torque x100
    ui8_tx_buffer[21] = (uint8_t) (ui16_pedal_torque_x100 & 0xff);
    ui8_tx_buffer[22] = (uint8_t) (ui16_pedal_torque_x100 >> 8);

	// human power x10
	//ui8_tx_buffer[23] = (uint8_t) (ui16_human_power_x10 & 0xff);
	//ui8_tx_buffer[24] = (uint8_t) (ui16_human_power_x10 >> 8);
	
	// pedal torque delta no boost
	ui8_tx_buffer[23] = (uint8_t) (ui16_adc_pedal_torque_delta_no_boost & 0xff);
	ui8_tx_buffer[24] = (uint8_t) (ui16_adc_pedal_torque_delta_no_boost >> 8);
	
	// pedal torque delta boost
	ui8_tx_buffer[25] = (uint8_t) (ui16_adc_pedal_torque_delta & 0xff);
	ui8_tx_buffer[26] = (uint8_t) (ui16_adc_pedal_torque_delta >> 8);
	// pedal torque offset
	//ui8_tx_buffer[25] = (uint8_t) (ui16_adc_pedal_torque_offset & 0xff);
	//ui8_tx_buffer[26] = (uint8_t) (ui16_adc_pedal_torque_offset >> 8);

	// prepare crc of the package
	ui16_crc_tx = 0xffff;
  
	for (ui8_i = 0; ui8_i <= UART_NUMBER_DATA_BYTES_TO_SEND; ui8_i++)
	{
		crc16 (ui8_tx_buffer[ui8_i], &ui16_crc_tx);
	}
  
	ui8_tx_buffer[UART_NUMBER_DATA_BYTES_TO_SEND + 1] = (uint8_t) (ui16_crc_tx & 0xff);
	ui8_tx_buffer[UART_NUMBER_DATA_BYTES_TO_SEND + 2] = (uint8_t) (ui16_crc_tx >> 8) & 0xff;

	// send the full package to UART
	for (ui8_i = 0; ui8_i <= UART_NUMBER_DATA_BYTES_TO_SEND + 2; ui8_i++)
	{
		putchar (ui8_tx_buffer[ui8_i]);
	}
}
