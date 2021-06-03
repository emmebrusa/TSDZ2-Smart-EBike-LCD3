/*
 * LCD3 firmware
 *
 * Copyright (C) Casainho and Leon, 2019.
 *
 * Released under the GPL License, Version 3
 */

#ifndef _MAIN_H_
#define _MAIN_H_


#define EXTI_PORTA_IRQHANDLER                     3
#define EXTI_PORTC_IRQHANDLER                     5
#define EXTI_PORTD_IRQHANDLER                     6
#define EXTI_PORTE_IRQHANDLER                     7
#define TIM1_CAP_COM_IRQHANDLER                   12
#define TIM2_UPD_OVF_TRG_BRK_IRQHANDLER           13
#define TIM3_UPD_OVF_BRK_IRQHANDLER               15
#define UART2_IRQHANDLER                          21
#define ADC1_IRQHANDLER                           22



// default values for assist levels
#define DEFAULT_VALUE_ASSIST_LEVEL                                  1
#define DEFAULT_VALUE_NUMBER_OF_ASSIST_LEVELS                       5



// default values for bike wheel parameters
#define DEFAULT_VALUE_WHEEL_PERIMETER_0                             12  // 26 inch wheel: 2060 mm perimeter (12 + (8 << 8))
#define DEFAULT_VALUE_WHEEL_PERIMETER_1                             8
#define DEFAULT_VALUE_WHEEL_MAX_SPEED                               56  // 56 kph
#define DEFAULT_VALUE_MAX_WHEEL_SPEED_IMPERIAL                      35  // 35 mph



// default value for system units
#define DEFAULT_VALUE_UNITS_TYPE                                    0   // 0 = km/h and kilometer, 1 mph and miles



// default values for battery capacity variables
#define DEFAULT_VALUE_WH_OFFSET_0                                   0
#define DEFAULT_VALUE_WH_OFFSET_1                                   0
#define DEFAULT_VALUE_WH_OFFSET_2                                   0
#define DEFAULT_VALUE_WH_OFFSET_3                                   0
#define DEFAULT_VALUE_HW_X10_100_PERCENT_0                          252 // 870.0 Wh -> 33*256+252
#define DEFAULT_VALUE_HW_X10_100_PERCENT_1                          33
#define DEFAULT_VALUE_HW_X10_100_PERCENT_2                          0
#define DEFAULT_VALUE_HW_X10_100_PERCENT_3                          0
#define DEFAULT_VALUE_BATTERY_SOC_FUNCTION_ENABLED                  1



// default values for battery parameters
#define DEFAULT_VALUE_BATTERY_MAX_CURRENT                           15  // 15 amps
#define DEFAULT_VALUE_TARGET_MAX_BATTERY_POWER                      20  // 20 -> 18 * 25 = 500 watts
#define DEFAULT_VALUE_BATTERY_CELLS_NUMBER                          14  // 52 V battery
#define BATTERY_CELL_OVERVOLTAGE_X100								435 // 4.35 Volt

// default values for battery cut-off voltage
#define DEFAULT_VALUE_BATTERY_LOW_VOLTAGE_CUT_OFF_X10_0             164 // 52 V battery, LVC = 42.0 (3.0 * 14): (164 + (1 << 8))
#define DEFAULT_VALUE_BATTERY_LOW_VOLTAGE_CUT_OFF_X10_1             1



// default value motor type
#define DEFAULT_VALUE_MOTOR_TYPE                                    0 // 0 = 48, 1 = 36 volt motor



// default value optional ADC function
#define DEFAULT_VALUE_OPTIONAL_ADC_FUNCTION                         0



// default value temperature field state
#define DEFAULT_VALUE_TEMPERATURE_FIELD_STATE                       2

// default values for power assist
//#define DEFAULT_VALUE_POWER_ASSIST_FUNCTION_ENABLED                 1
// default set startup boost enabled
#define DEFAULT_STARTUP_BOOST_ENABLED								1
// default values for power assist
//#define DEFAULT_VALUE_POWER_ASSIST_LEVEL_0                          0   // 0
#define DEFAULT_VALUE_POWER_ASSIST_LEVEL_1                          20  // MAX 254
#define DEFAULT_VALUE_POWER_ASSIST_LEVEL_2                          40
#define DEFAULT_VALUE_POWER_ASSIST_LEVEL_3                          60
#define DEFAULT_VALUE_POWER_ASSIST_LEVEL_4                          80
#define DEFAULT_VALUE_POWER_ASSIST_LEVEL_5                          100
#define DEFAULT_VALUE_POWER_ASSIST_LEVEL_6                          130
#define DEFAULT_VALUE_POWER_ASSIST_LEVEL_7                          160
#define DEFAULT_VALUE_POWER_ASSIST_LEVEL_8                          200
#define DEFAULT_VALUE_POWER_ASSIST_LEVEL_9                          250



// default values for motor temperature limit function
#define DEFAULT_VALUE_MOTOR_TEMPERATURE_MIN_VALUE_LIMIT             65  // degrees celsius
#define DEFAULT_VALUE_MOTOR_TEMPERATURE_MAX_VALUE_LIMIT             80  // degrees celsius



// default values for battery voltage 
#define DEFAULT_VALUE_BATTERY_VOLTAGE_RESET_WH_COUNTER_X10_0        18  // 48/52 V battery, 53.0 volts fully charged = 530: (18 + (2 << 8))
#define DEFAULT_VALUE_BATTERY_VOLTAGE_RESET_WH_COUNTER_X10_1        2



// default values for screen parameters
#define DEFAULT_VALUE_LCD_POWER_OFF_TIME                            25  // 25 -> 25 minutes
#define DEFAULT_VALUE_LCD_BACKLIGHT_ON_BRIGHTNESS                   7  // 7 = 35 %
#define DEFAULT_VALUE_LCD_BACKLIGHT_OFF_BRIGHTNESS                  2  // 2 = 10%



// default values for internal resistance of battery
#define DEFAULT_VALUE_BATTERY_PACK_RESISTANCE_0                     200
#define DEFAULT_VALUE_BATTERY_PACK_RESISTANCE_1                     0



// default values for street mode function
#define DEFAULT_VALUE_STREET_MODE_FUNCTION_ENABLED                  0
#define DEFAULT_VALUE_STREET_MODE_SPEED_LIMIT                       25
#define DEFAULT_VALUE_STREET_MODE_POWER_LIMIT_ENABLED               0
#define DEFAULT_VALUE_STREET_MODE_POWER_LIMIT_DIV25                 10  // 10 * 25 = 250 W
#define DEFAULT_VALUE_STREET_MODE_THROTTLE_ENABLED                  0   // throttle is disabled in street mode by default
#define DEFAULT_VALUE_STREET_MODE_CRUISE_ENABLED                    0   // cruise is disabled in street mode by default



// default values for distance measurement
#define DEFAULT_VALUE_ODOMETER_X10                                  0
#define DEFAULT_VALUE_TRIP_X10                                      0



// default values for the odometer field and sub field states
#define DEFAULT_VALUE_ODOMETER_FIELD_STATE                          0
#define DEFAULT_VALUE_ODOMETER_SUB_FIELD_STATE_0                    0
#define DEFAULT_VALUE_ODOMETER_SUB_FIELD_STATE_1                    0
#define DEFAULT_VALUE_ODOMETER_SUB_FIELD_STATE_2                    0
#define DEFAULT_VALUE_ODOMETER_SUB_FIELD_STATE_3                    0
#define DEFAULT_VALUE_ODOMETER_SUB_FIELD_STATE_4                    0
#define DEFAULT_VALUE_ODOMETER_SUB_FIELD_STATE_5                    0
#define DEFAULT_VALUE_ODOMETER_SUB_FIELD_STATE_6                    0



// default values for time measurement
#define DEFAULT_VALUE_TIME_MEASUREMENT_FIELD_STATE                  1   // 1 = display time measurement since power on (TM)
#define DEFAULT_VALUE_TOTAL_SECOND_TTM                              0
#define DEFAULT_VALUE_TOTAL_MINUTE_TTM                              0
#define DEFAULT_VALUE_TOTAL_HOUR_TTM_0                              0
#define DEFAULT_VALUE_TOTAL_HOUR_TTM_1                              0


// default value motor acceleration
#define DEFAULT_VALUE_MOTOR_ACCELERATION                            25

// default value motor deceleration
#define DEFAULT_VALUE_MOTOR_DECELERATION                            0


// default values for walk assist function
#define DEFAULT_VALUE_WALK_ASSIST_FUNCTION_ENABLED                  0   // disabled by default
#define DEFAULT_VALUE_WALK_ASSIST_BUTTON_BOUNCE_TIME                0   // 0 milliseconds
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_1                           20
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_2                           25
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_3                           30
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_4                           35
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_5                           40
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_6                           42
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_7                           44
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_8                           46
#define DEFAULT_VALUE_WALK_ASSIST_LEVEL_9                           48



// default values for cruise function
#define DEFAULT_VALUE_CRUISE_FUNCTION_ENABLED                       0   // disabled by default
#define DEFAULT_VALUE_CRUISE_FUNCTION_SET_TARGET_SPEED_ENABLED      0   // disabled by default
#define DEFAULT_VALUE_CRUISE_FUNCTION_TARGET_SPEED_KPH              25  // 25 kph
#define DEFAULT_VALUE_CRUISE_FUNCTION_TARGET_SPEED_MPH              15  // 15 mph
#define DEFAULT_VALUE_SHOW_CRUISE_FUNCTION_SET_TARGET_SPEED         0   // disabled by default



// default values wheel speed field state
#define DEFAULT_VALUE_WHEEL_SPEED_FIELD_STATE                       0   // 0 = display wheel speed, 1 = display average wheel speed, 2 = display max measured wheel speed



// default values for showing odometer variables
#define DEFAULT_VALUE_SHOW_DISTANCE_DATA_ODOMETER_FIELD             1
#define DEFAULT_VALUE_SHOW_BATTERY_STATE_ODOMETER_FIELD             1
#define DEFAULT_VALUE_SHOW_PEDAL_DATA_ODOMETER_FIELD                1
#define DEFAULT_VALUE_SHOW_TIME_MEASUREMENT_ODOMETER_FIELD          0
#define DEFAULT_VALUE_SHOW_WHEEL_SPEED_ODOMETER_FIELD               0
#define DEFAULT_VALUE_SHOW_ENERGY_DATA_ODOMETER_FIELD               0
#define DEFAULT_VALUE_SHOW_MOTOR_TEMPERATURE_ODOMETER_FIELD         0
#define DEFAULT_VALUE_SHOW_BATTERY_SOC_ODOMETER_FIELD               0

// default value for the main screen power menu
#define DEFAULT_VALUE_MAIN_SCREEN_POWER_MENU_ENABLED                1

// default value pedal torque conversion
#define DEFAULT_VALUE_PEDAL_TORQUE_PER_10_BIT_ADC_STEP_X100         67


// default value pedal torque ADC offset (weight=0)
#define DEFAULT_VALUE_PEDAL_TORQUE_ADC_OFFSET		       			150
// default value pedal torque ADC range (weight=max)
#define DEFAULT_VALUE_PEDAL_TORQUE_ADC_RANGE_0		       			44
#define DEFAULT_VALUE_PEDAL_TORQUE_ADC_RANGE_1		       			1
// default value startup boost torque factor
#define DEFAULT_VALUE_STARTUP_BOOST_TORQUE_FACTOR_0					250
#define DEFAULT_VALUE_STARTUP_BOOST_TORQUE_FACTOR_1					0
// default value startup boost cadence step
#define DEFAULT_VALUE_STARTUP_BOOST_CADENCE_STEP					25
// default value riding mode on startup
#define DEFAULT_VALUE_RIDING_MODE_ON_STARTUP						1
// default value coaster brake torque threshold
#define DEFAULT_VALUE_COASTER_BRAKE_TORQUE_THRESHOLD				30


// default value cadence sensor mode
//#define DEFAULT_VALUE_CADENCE_SENSOR_MODE                           0
//#define DEFAULT_VALUE_CADENCE_SENSOR_PULSE_HIGH_PERCENTAGE_X10_0    244
//#define DEFAULT_VALUE_CADENCE_SENSOR_PULSE_HIGH_PERCENTAGE_X10_1    1


// default value for torque assist
//#define DEFAULT_VALUE_TORQUE_ASSIST_FUNCTION_ENABLED                0
// default set torque sensor calibration enabled
#define DEFAULT_TORQUE_SENSOR_CALIBRATION_ENABLED					0
// default value for torque assist
#define DEFAULT_VALUE_TORQUE_ASSIST_LEVEL_1                         40	// MAX 254
#define DEFAULT_VALUE_TORQUE_ASSIST_LEVEL_2                         60
#define DEFAULT_VALUE_TORQUE_ASSIST_LEVEL_3                         80
#define DEFAULT_VALUE_TORQUE_ASSIST_LEVEL_4                         100
#define DEFAULT_VALUE_TORQUE_ASSIST_LEVEL_5                         120
#define DEFAULT_VALUE_TORQUE_ASSIST_LEVEL_6                         140
#define DEFAULT_VALUE_TORQUE_ASSIST_LEVEL_7                         180
#define DEFAULT_VALUE_TORQUE_ASSIST_LEVEL_8                         210
#define DEFAULT_VALUE_TORQUE_ASSIST_LEVEL_9                         250



// default value for cadence assist
//#define DEFAULT_VALUE_CADENCE_ASSIST_FUNCTION_ENABLED               0
// default set assistance with error enabled
#define DEFAULT_ASSISTANCE_WITH_ERROR_ENABLED						0
// default value for cadence assist
#define DEFAULT_VALUE_CADENCE_ASSIST_LEVEL_1                        100	// MAX 254
#define DEFAULT_VALUE_CADENCE_ASSIST_LEVEL_2                        120
#define DEFAULT_VALUE_CADENCE_ASSIST_LEVEL_3                        130
#define DEFAULT_VALUE_CADENCE_ASSIST_LEVEL_4                        140
#define DEFAULT_VALUE_CADENCE_ASSIST_LEVEL_5                        160
#define DEFAULT_VALUE_CADENCE_ASSIST_LEVEL_6                        180
#define DEFAULT_VALUE_CADENCE_ASSIST_LEVEL_7                        200
#define DEFAULT_VALUE_CADENCE_ASSIST_LEVEL_8                        220
#define DEFAULT_VALUE_CADENCE_ASSIST_LEVEL_9                        250



// default value for eMTB assist
#define DEFAULT_VALUE_EMTB_ASSIST_FUNCTION_ENABLED                  1
// default value for eMTB sensitivity
//#define DEFAULT_VALUE_EMTB_ASSIST_SENSITIVITY                       20
#define DEFAULT_VALUE_EMTB_ASSIST_LEVEL_1                           4	// MAX 20
#define DEFAULT_VALUE_EMTB_ASSIST_LEVEL_2                           6
#define DEFAULT_VALUE_EMTB_ASSIST_LEVEL_3                           8
#define DEFAULT_VALUE_EMTB_ASSIST_LEVEL_4                           10
#define DEFAULT_VALUE_EMTB_ASSIST_LEVEL_5                           12
#define DEFAULT_VALUE_EMTB_ASSIST_LEVEL_6                           14
#define DEFAULT_VALUE_EMTB_ASSIST_LEVEL_7                           16
#define DEFAULT_VALUE_EMTB_ASSIST_LEVEL_8                           18
#define DEFAULT_VALUE_EMTB_ASSIST_LEVEL_9                           20
#define DEFAULT_VALUE_EMTB_ASSIST_LEVEL_10                          10


// default value assist without pedal rotation threshold
#define DEFAULT_VALUE_ASSIST_WITHOUT_PEDAL_ROTATION_THRESHOLD       30

// default value field weakening
#define DEFAULT_VALUE_FIELD_WEAKENING_ENABLED						0

// default value lights
#define DEFAULT_VALUE_LIGHTS_MODE                                   0
#define DEFAULT_VALUE_LIGHTS_STATE                                  0
#define DEFAULT_VALUE_LIGHTS_CONFIGURATION                          0



#endif // _MAIN_H_