/*
 * LCD3 firmware
 *
 * Copyright (C) Casainho and Leon, 2019.
 *
 * Released under the GPL License, Version 3
 */

#include <stdint.h>
#include <stdio.h>

#include "stm8s.h"
#include "stm8s_uart2.h"
#include "main.h"
#include "lcd.h"
#include "common.h"

#define UART_NUMBER_DATA_BYTES_TO_RECEIVE   26  // change this value depending on how many data bytes there are to receive ( Package = one start byte + data bytes + two bytes 16 bit CRC )
#define UART_NUMBER_DATA_BYTES_TO_SEND      7   // change this value depending on how many data bytes there are to send ( Package = one start byte + data bytes + two bytes 16 bit CRC )
#define UART_MAX_NUMBER_MESSAGE_ID          7   // change this value depending on how many different packages there are to send

volatile uint8_t  ui8_received_package_flag = 0;
volatile uint8_t  ui8_rx_buffer[UART_NUMBER_DATA_BYTES_TO_RECEIVE + 3];
volatile uint8_t  ui8_rx_counter = 0;
volatile uint8_t  ui8_tx_buffer[UART_NUMBER_DATA_BYTES_TO_SEND + 3];
volatile uint8_t  ui8_i;
volatile uint8_t  ui8_byte_received;
volatile uint8_t  ui8_state_machine = 0;
static uint16_t   ui16_crc_rx;
static uint16_t   ui16_crc_tx;
static uint8_t    ui8_message_ID = 0;

volatile uint8_t  ui8_received_first_package = 0;


void uart2_init (void)
{
  UART2_DeInit();
  UART2_Init((uint32_t) 9600,
             UART2_WORDLENGTH_8D,
             UART2_STOPBITS_1,
             UART2_PARITY_NO,
             UART2_SYNCMODE_CLOCK_DISABLE,
             UART2_MODE_TXRX_ENABLE);

  UART2_ITConfig(UART2_IT_RXNE_OR, ENABLE);
}

// This is the interrupt that happens when UART2 receives data. We need it to be the fastest possible and so
// we do: receive every byte and assembly as a package, finally, signal that we have a package to process (on main slow loop)
// and disable the interrupt. The interrupt should be enable again on main loop, after the package being processed
void UART2_IRQHandler(void) __interrupt(UART2_IRQHANDLER)
{
  if(UART2_GetFlagStatus(UART2_FLAG_RXNE) == SET)
  {
    UART2->SR &= (uint8_t)~(UART2_FLAG_RXNE); // this may be redundant

    ui8_byte_received = UART2_ReceiveData8 ();

    switch (ui8_state_machine)
    {
      case 0:
      if (ui8_byte_received == 67) // see if we get start package byte
      {
        ui8_rx_buffer[ui8_rx_counter] = ui8_byte_received;
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
      ui8_rx_buffer[ui8_rx_counter] = ui8_byte_received;
      
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


void uart_data_clock (void)
{
  struct_motor_controller_data *p_motor_controller_data;
  struct_configuration_variables *p_configuration_variables;

  uint8_t ui8_startup_assist_status = 0;
  uint8_t ui8_temp;
  
  if (ui8_received_package_flag)
  {
	uint8_t ui8_startup_boost_enable;
	uint8_t ui8_startup_boost_at_zero;
	
    // validation of the package data
    ui16_crc_rx = 0xffff;
    
    for (ui8_i = 0; ui8_i <= UART_NUMBER_DATA_BYTES_TO_RECEIVE; ui8_i++)
    {
      crc16 (ui8_rx_buffer[ui8_i], &ui16_crc_rx);
    }
    
    // if CRC is ok read the package
    if (((((uint16_t) ui8_rx_buffer [UART_NUMBER_DATA_BYTES_TO_RECEIVE + 2]) << 8) + ((uint16_t) ui8_rx_buffer [UART_NUMBER_DATA_BYTES_TO_RECEIVE + 1])) == ui16_crc_rx)
    {
      p_motor_controller_data = lcd_get_motor_controller_data();
      p_configuration_variables = get_configuration_variables();
      
      // battery voltage x1000
      p_motor_controller_data->ui16_battery_voltage_x1000 = (((uint16_t) ui8_rx_buffer [2]) << 8) + ((uint16_t) ui8_rx_buffer [1]);
      
      // battery current x10
      p_motor_controller_data->ui8_battery_current_x10 = ui8_rx_buffer[3];
      
      // wheel speed
      p_motor_controller_data->ui16_wheel_speed_x10 = (((uint16_t) ui8_rx_buffer [5]) << 8) + ((uint16_t) ui8_rx_buffer [4]);
      
      // brake state (bit 0)
      p_motor_controller_data->ui8_braking = ui8_rx_buffer[6] & 1;
      // Bit free for future use
	  
      // value from optional ADC channel
      p_motor_controller_data->ui8_adc_throttle = ui8_rx_buffer[7];
      
      // throttle or temperature control
      switch (p_configuration_variables->ui8_optional_ADC_function)
      {
        case THROTTLE_CONTROL:
			// throttle value with offset applied and mapped from 0 to 255
			p_motor_controller_data->ui8_throttle = ui8_rx_buffer[8];
			break;
        
        case TEMPERATURE_CONTROL:
			// current limiting mapped from 0 to 255
			p_motor_controller_data->ui8_temperature_current_limiting_value = ui8_rx_buffer[8];
			break;
      }
      
      // ADC pedal torque
      p_motor_controller_data->ui16_adc_pedal_torque_sensor = (((uint16_t) ui8_rx_buffer [10]) << 8) + ((uint16_t) ui8_rx_buffer [9]);
      
      // pedal cadence
      p_motor_controller_data->ui8_pedal_cadence_RPM = ui8_rx_buffer[11];
      
      // PWM duty_cycle
      p_motor_controller_data->ui8_duty_cycle = ui8_rx_buffer[12];
      
      // motor speed in ERPS
      p_motor_controller_data->ui16_motor_speed_erps = (((uint16_t) ui8_rx_buffer [14]) << 8) + ((uint16_t) ui8_rx_buffer [13]);
      
      // FOC angle
      p_motor_controller_data->ui8_foc_angle = ui8_rx_buffer[15];
      
      // controller system state
      p_motor_controller_data->ui8_controller_system_state = ui8_rx_buffer[16];
      
      // motor temperature
      p_motor_controller_data->ui8_motor_temperature = ui8_rx_buffer[17];
      
      // wheel_speed_sensor_tick_counter
      p_motor_controller_data->ui32_wheel_speed_sensor_tick_counter = (((uint32_t) ui8_rx_buffer[20]) << 16) + (((uint32_t) ui8_rx_buffer[19]) << 8) + ((uint32_t) ui8_rx_buffer[18]);

      // pedal torque x100
      p_motor_controller_data->ui16_pedal_torque_x100 = (((uint16_t) ui8_rx_buffer [22]) << 8) + ((uint16_t) ui8_rx_buffer [21]);
      
      // pedal torque delta no boost
      p_motor_controller_data->ui16_adc_pedal_torque_delta = (((uint16_t) ui8_rx_buffer [24]) << 8) + ((uint16_t) ui8_rx_buffer [23]);
	  
	  // pedal torque delta boost
      p_motor_controller_data->ui16_adc_pedal_torque_delta_boost = (((uint16_t) ui8_rx_buffer [26]) << 8) + ((uint16_t) ui8_rx_buffer [25]);        

      // flag that the first communication package is received from the motor controller
      ui8_received_first_package = 1;


      // ----------------- now send the data to the motor controller ----------------- //

      
      // start up byte
      ui8_tx_buffer[0] = 0x59;
      
      // message ID
      ui8_tx_buffer[1] = ui8_message_ID;
      
      // lights state & riding mode
      ui8_tx_buffer[2] = p_configuration_variables->ui8_lights_state |
						(p_motor_controller_data->ui8_riding_mode << 4);
      
      // riding mode parameter
      switch (p_motor_controller_data->ui8_riding_mode)
      {
        case POWER_ASSIST_MODE:
            ui8_tx_buffer[3] = p_configuration_variables->ui8_power_assist_level[p_configuration_variables->ui8_assist_level - 1];
          break;

        case TORQUE_ASSIST_MODE:
            ui8_tx_buffer[3] = p_configuration_variables->ui8_torque_assist_level[p_configuration_variables->ui8_assist_level - 1];
          break;
        
        case CADENCE_ASSIST_MODE:
            ui8_tx_buffer[3] = p_configuration_variables->ui8_cadence_assist_level[p_configuration_variables->ui8_assist_level - 1];
          break;
        
        case eMTB_ASSIST_MODE:
			ui8_tx_buffer[3] = p_configuration_variables->ui8_eMTB_assist_level[p_configuration_variables->ui8_assist_level - 1];
		  break;
		  
        case HYBRID_ASSIST_MODE:
            ui8_tx_buffer[3] = p_configuration_variables->ui8_power_assist_level[p_configuration_variables->ui8_assist_level - 1];
			ui8_tx_buffer[4] = p_configuration_variables->ui8_torque_assist_level[p_configuration_variables->ui8_assist_level - 1];
          break;
		  
        case WALK_ASSIST_MODE:
            ui8_tx_buffer[3] = p_configuration_variables->ui8_walk_assist_level[p_configuration_variables->ui8_assist_level - 1];
          break;
        
        case CRUISE_MODE:
          if (p_configuration_variables->ui8_cruise_function_set_target_speed_enabled)
          {
            ui8_tx_buffer[3] = p_configuration_variables->ui8_cruise_function_target_speed_kph;
          }
          else
          {
            ui8_tx_buffer[3] = 0;
          }
		  break;
        
        default:
          ui8_tx_buffer[3] = 0;
		  ui8_tx_buffer[4] = 0;
		  break;
      }
      
	  if (p_configuration_variables->ui8_assist_level == 0)
	  {
        ui8_tx_buffer[3] = 0;
		ui8_tx_buffer[4] = 0;
	  }
	  
      switch (ui8_message_ID)
      {
        case 0:
          // battery low voltage cut off x10
          ui8_tx_buffer[5] = (uint8_t) (p_configuration_variables->ui16_battery_low_voltage_cut_off_x10 & 0xff);
          ui8_tx_buffer[6] = (uint8_t) (p_configuration_variables->ui16_battery_low_voltage_cut_off_x10 >> 8);
          
          // wheel max speed
          if (p_configuration_variables->ui8_street_mode_enabled)
		  {
			ui8_tx_buffer[7] = p_configuration_variables->ui8_street_mode_speed_limit;
          }
          else
          {
            ui8_tx_buffer[7] = p_configuration_variables->ui8_wheel_max_speed;
          }
          break;
        
        case 1:
          // wheel perimeter
          ui8_tx_buffer[5] = (uint8_t) (p_configuration_variables->ui16_wheel_perimeter & 0xff);
          ui8_tx_buffer[6] = (uint8_t) (p_configuration_variables->ui16_wheel_perimeter >> 8);
          
          // optional ADC function, disable throttle if set to be disabled in Street Mode
		  ui8_temp = p_configuration_variables->ui8_optional_ADC_function;
          if ((p_configuration_variables->ui8_optional_ADC_function == THROTTLE_CONTROL) &&
		     (p_configuration_variables->ui8_street_mode_enabled) &&
		     (!p_configuration_variables->ui8_street_mode_throttle_enabled))
					ui8_temp = 0;
          
		  // optional ADC function & coaster brake torque threshold
		  ui8_tx_buffer[7] = ui8_temp |
							(p_configuration_variables->ui8_coaster_brake_torque_threshold << 2);
          break;

        case 2:
		  // startup boost enabled and cadence or speed mode
		  if(p_configuration_variables->ui8_startup_boost_enabled == 1) {
			  ui8_startup_boost_enable = 1;
			  ui8_startup_boost_at_zero = 0; // cadence
		  }
		  else if(p_configuration_variables->ui8_startup_boost_enabled == 2) {
			  ui8_startup_boost_enable = 1;
			  ui8_startup_boost_at_zero = 1; // speed
		  }
		  else {
			  ui8_startup_boost_enable = 0;
			  ui8_startup_boost_at_zero = 0;
		  }
		  
		  // get startup assist state
		  if ((p_configuration_variables->ui8_assist_level)
			 &&(p_configuration_variables->ui8_startup_assist_function_enabled))
				ui8_startup_assist_status = ui8_startup_assist; 
		  else
				ui8_startup_assist_status = 0;
		
          // set motor type & function setting
          ui8_tx_buffer[5] = p_configuration_variables->ui8_motor_type |
							(ui8_startup_boost_enable << 1) |
							(ui8_torque_sensor_calibration_status << 2) |
							(p_configuration_variables->ui8_assist_whit_error_enabled << 3) |
							(ui8_startup_boost_at_zero << 4) |
							(p_configuration_variables->ui8_field_weakening_enabled << 5) |
							(!p_configuration_variables->ui8_assist_level << 6) |
							(ui8_startup_assist_status << 7);
							
          // motor over temperature min value limit
          ui8_tx_buffer[6] = p_configuration_variables->ui8_motor_temperature_min_value_to_limit;
          
          // motor over temperature max value limit
          ui8_tx_buffer[7] = p_configuration_variables->ui8_motor_temperature_max_value_to_limit;
          break;

        case 3:
		  // adc torque calibration offset & middle offset adj
          ui8_tx_buffer[5] = (ui8_adc_torque_calibration_offset & 7);
          ui8_tx_buffer[5] |= ((ui8_adc_torque_middle_offset_adj & 31) << 3);
		  
		  // pedal torque ADC range (pedal torque ADC max - pedal torque ADC offset)
          ui8_tx_buffer[6] = (uint8_t) (ui16_adc_pedal_torque_range & 0xff);
          ui8_tx_buffer[7] = (uint8_t) (ui16_adc_pedal_torque_range >> 8);
          break;

        case 4:
          // lights configuration
          ui8_tx_buffer[5] = p_configuration_variables->ui8_lights_configuration;
          
          // assist without pedal rotation threshold
          ui8_tx_buffer[6] = p_configuration_variables->ui8_assist_without_pedal_rotation_threshold;
          
          // motor acceleration adjustment
          ui8_tx_buffer[7] = p_configuration_variables->ui8_motor_acceleration;
          break;

        case 5:
          // pedal torque conversion
		  if(ui8_torque_sensor_calibration_status == 1) {
			//ui8_tx_buffer[5] = PEDAL_TORQUE_PER_10_BIT_ADC_STEP_BASE_X100;
			ui8_tx_buffer[5] = p_configuration_variables->ui8_pedal_torque_per_10_bit_ADC_step_adv_x100;
		  }
		  else {
			ui8_tx_buffer[5] = p_configuration_variables->ui8_pedal_torque_per_10_bit_ADC_step_x100;
		  }
		  
          // max battery current in amps
          ui8_tx_buffer[6] = p_configuration_variables->ui8_battery_max_current;
          
          // battery power limit
          if (p_configuration_variables->ui8_street_mode_enabled && p_configuration_variables->ui8_street_mode_power_limit_enabled)
          {
            ui8_tx_buffer[7] = p_configuration_variables->ui8_street_mode_power_limit_div25;
          }
          else
          {
            ui8_tx_buffer[7] = p_configuration_variables->ui8_target_max_battery_power_div25;
          }
          break;
        
        case 6:
          // startup boost torque factor
          ui8_tx_buffer[5] = (uint8_t) (p_configuration_variables->ui16_startup_boost_torque_factor >> 1);
          
          // startup boost cadence step
          ui8_tx_buffer[6] = p_configuration_variables->ui8_startup_boost_cadence_step;
		  
		  // motor deceleration adjustment
          ui8_tx_buffer[7] = p_configuration_variables->ui8_motor_deceleration;
          break;

        case 7:
		  // Torque ADC offset adjustment (0 / 34)
          ui8_tx_buffer[5] = ui8_adc_pedal_torque_offset_adj;
          
          // Torque ADC range adjustment (0 / 40)
          ui8_tx_buffer[6] = p_configuration_variables->ui8_adc_pedal_torque_range_adj;
		  
		  // Torque ADC angle adjustment (0 / 40)
          ui8_tx_buffer[7] = ui8_adc_pedal_torque_angle_adj;
			break;
			
        default:
          ui8_message_ID = 0;
          break;
      }

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
      
      // increment message ID for next package
      if (++ui8_message_ID > UART_MAX_NUMBER_MESSAGE_ID) { ui8_message_ID = 0; }
    }
    
    // flag that we processed the full package
    ui8_received_package_flag = 0;

    // enable UART2 receive interrupt as we are now ready to receive a new package
    UART2->CR2 |= (1 << 5);
  }
}



#if __SDCC_REVISION < 9624
void putchar(char c)
{
  //Write a character to the UART2
  UART2_SendData8(c);

  //Loop until the end of transmission
  while (UART2_GetFlagStatus(UART2_FLAG_TXE) == RESET) ;
}
#else
int putchar(int c)
{
  //Write a character to the UART2
  UART2_SendData8(c);

  //Loop until the end of transmission
  while (UART2_GetFlagStatus(UART2_FLAG_TXE) == RESET);

  return((unsigned char)c);
}
#endif

#if __SDCC_REVISION < 9989
char getchar(void)
#else
int getchar(void)
#endif
{
  uint8_t c = 0;

  /* Loop until the Read data register flag is SET */
  while (UART2_GetFlagStatus(UART2_FLAG_RXNE) == RESET) ;

  c = UART2_ReceiveData8();

  return (c);
}
