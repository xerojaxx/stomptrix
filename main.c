/***********************************************************************************************//**
 * \file   main.c
 * \brief  Silicon Labs Thermometer Example Application
 *
 * This Thermometer and OTA example allows the user to measure temperature
 * using the temperature sensor on the WSTK. The values can be read with the
 * Health Thermometer reader on the Blue Gecko smartphone app.
 ***************************************************************************************************
 * <b> (C) Copyright 2016 Silicon Labs, http://www.silabs.com</b>
 ***************************************************************************************************
 * This file is licensed under the Silicon Labs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 **************************************************************************************************/

#ifndef GENERATION_DONE
#error You must run generate first!
#endif

/* Board Headers */
#include "boards.h"
#include "ble-configuration.h"
#include "board_features.h"

/* Bluetooth stack headers */
#include "bg_types.h"
#include "native_gecko.h"
#include "aat.h"
#include "infrastructure.h"

/* GATT database */
#include "gatt_db.h"

/* EM library (EMlib) */
#include "em_system.h"

/* Libraries containing default Gecko configuration values */
#include "em_emu.h"
#include "em_cmu.h"
#ifdef FEATURE_BOARD_DETECTED
#include "bspconfig.h"
#include "pti.h"
#else
#error This sample app only works with a Silicon Labs Board
#endif

#ifdef FEATURE_IOEXPANDER
#include "bsp.h"
#include "bsp_stk_ioexp.h"
#endif /* FEATURE_IOEXPANDER */ 



/* Device initialization header */
#include "InitDevice.h"

/* Temperature sensor and I2c*/
#include "i2cspmconfig.h"
#include "i2cspm.h"
#include "si7013.h"
#include "tempsens.h"

#ifdef FEATURE_SPI_FLASH
#include "em_usart.h"
#include "mx25flash_spi.h"
#endif /* FEATURE_SPI_FLASH */

#include "dmadrv.h"
#include "em_gpio.h"
#include "ext_lion_battery.h"
#include "bq24160.h"
#include "led_strip_controller.h"
#include "sk6812.h"
#include "timer.h"
#include "stomptrix_msgs.h"

/***********************************************************************************************//**
 * @addtogroup Application
 * @{
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup app
 * @{
 **************************************************************************************************/

/* Gecko configuration parameters (see gecko_configuration.h) */
#ifndef MAX_CONNECTIONS
#define MAX_CONNECTIONS 4
#endif
uint8_t bluetooth_stack_heap[DEFAULT_BLUETOOTH_HEAP(MAX_CONNECTIONS)];

#ifdef FEATURE_PTI_SUPPORT
static const RADIO_PTIInit_t ptiInit = RADIO_PTI_INIT;
#endif

static const gecko_configuration_t config = {
  .config_flags=0,
  .sleep.flags=0,
  .bluetooth.max_connections=MAX_CONNECTIONS,
  .bluetooth.heap=bluetooth_stack_heap,
  .bluetooth.heap_size=sizeof(bluetooth_stack_heap),
  .bluetooth.sleep_clock_accuracy = 100, // ppm
  .gattdb=&bg_gattdb_data,
  .ota.flags=0,
  .ota.device_name_len=3,
  .ota.device_name_ptr="OTA",
  #ifdef FEATURE_PTI_SUPPORT
  .pti = &ptiInit,
  #endif
};

/* Flag for indicating DFU Reset must be performed */
uint8_t boot_to_dfu = 0;


/**
 * @brief  Main function
 */
int main(void)
{

	//uint8_t connection_handle = 0;

  /* Initialize peripherals */
  enter_DefaultMode_from_RESET();

  /* Initialize stack */

  DMADRV_Init();
  gecko_init(&config);

  CMU_ClockEnable(cmuClock_GPIO, true);
	GPIO_PinModeSet(gpioPortB, 12, gpioModePushPull, 0);
	GPIO_PinOutSet(gpioPortB, 12);
	GPIO_PinOutClear(gpioPortB, 12);

	GPIO_PinModeSet(gpioPortC, 10, gpioModePushPull, 0);
	GPIO_PinOutSet(gpioPortC, 10);
	GPIO_PinOutClear(gpioPortC, 10);

	GPIO_PinModeSet(gpioPortF, 2, gpioModePushPull, 0); // LED signal low.
	GPIO_PinModeSet(gpioPortD, 14, gpioModePushPull, 1); // LED power on

	lion_battery_init();

	led_strip_handle_t led_strip = {
		.num_of_leds = 60u,
		.set_colours = sk6812_set_colours
	};

	sk6812_init();
	(void)led_strip_init(&led_strip);

  /* Initialize the Battery Management IC.*/
	bq24160_init();
	bq24160_read_voltage();


	//led_strip_set_all(&led_strip, 0x000000FF);//0x000000ff);
	led_strip_slide_animation(&led_strip, 0x00ff0000, true);

  while (1) {
    /* Event pointer for handling events */
    struct gecko_cmd_packet* evt = NULL;

    /* Check for stack event. */
    //evt = gecko_wait_event();
    evt = gecko_peek_event();

    /* Handle events */
    switch (BGLIB_MSG_ID(evt->header)) {

      /* This boot event is generated when the system boots up after reset.
       * Here the system is set to start advertising immediately after boot procedure. */
      case gecko_evt_system_boot_id:
        /* Set advertising parameters. 100ms advertisement interval. All channels used.
         * The first two parameters are minimum and maximum advertising interval, both in
         * units of (milliseconds * 1.6). The third parameter '7' sets advertising on all channels. */
        gecko_cmd_le_gap_set_adv_parameters(160,160,7);

		//const uint8_t adv_data[] = {0x00, 0x01, 0x02, 0x03};
		//gecko_cmd_le_gap_set_adv_data(0u, sizeof(adv_data), adv_data);


        /* Start general advertising and enable connections. */
        gecko_cmd_le_gap_set_mode(le_gap_general_discoverable, le_gap_undirected_connectable);


#define TIMER_10MS_HANDLE 1
#define TICKS_FOR_10_MS (32768/1000)
#define TICKS_FOR_2S (32768/2)
        gecko_cmd_hardware_set_soft_timer(TICKS_FOR_10_MS, TIMER_10MS_HANDLE, 0);


        break;

      /* This event is generated when a connected client has either
       * 1) changed a Characteristic Client Configuration, meaning that they have enabled
       * or disabled Notifications or Indications, or
       * 2) sent a confirmation upon a successful reception of the indication. */
      case gecko_evt_gatt_server_characteristic_status_id:
        /* Check that the characteristic in question is temperature - its ID is defined
         * in gatt.xml as "temp_measurement". Also check that status_flags = 1, meaning that
         * the characteristic client configuration was changed (notifications or indications
         * enabled or disabled). */
//        if ((evt->data.evt_gatt_server_characteristic_status.characteristic == gattdb_battery)
//          && (evt->data.evt_gatt_server_characteristic_status.status_flags == 0x01)) {
//          if (evt->data.evt_gatt_server_characteristic_status.client_config_flags == 0x02) {
//            /* Indications have been turned ON - start the repeating timer. The 1st parameter '32768'
//             * tells the timer to run for 1 second (32.768 kHz oscillator), the 2nd parameter is
//             * the timer handle and the 3rd parameter '0' tells the timer to repeat continuously until
//             * stopped manually.*/
//            gecko_cmd_hardware_set_soft_timer(32768,0,0);
//          } else if (evt->data.evt_gatt_server_characteristic_status.client_config_flags == 0x00) {
//            /* Indications have been turned OFF - stop the timer. */
//            gecko_cmd_hardware_set_soft_timer(0,0,0);
//          }
//        }
        break;

      case gecko_evt_hardware_soft_timer_id:
		timer_10ms_tick();
//		static uint8_t blue_colour = 0x00u;
//		led_strip_set_all(&led_strip, (blue_colour++));
        break;

      case gecko_evt_le_connection_closed_id:
        /* Check if need to boot to dfu mode */
        if (boot_to_dfu) {
          /* Enter to DFU OTA mode */
          gecko_cmd_system_reset(2);
        }
        else {
          /* Stop timer in case client disconnected before indications were turned off */
          gecko_cmd_hardware_set_soft_timer(0, 0, 0);
          /* Restart advertising after client has disconnected */
          gecko_cmd_le_gap_set_mode(le_gap_general_discoverable, le_gap_undirected_connectable);
        }
        break;

      case gecko_evt_le_connection_opened_id:
    	  //connection_handle = evt->data.evt_le_connection_opened.connection;
    	  gecko_cmd_le_gap_set_mode(le_gap_non_discoverable, le_gap_non_connectable);
    	  break;

      case gecko_evt_gatt_server_user_write_request_id:
    	  if (evt->data.evt_gatt_server_user_write_request.characteristic == gattdb_colour)
		  {
    		  if(evt->data.evt_gatt_server_user_write_request.value.len < sizeof(stomptrix_ctrl_msg_t)){break;}
    		  stomptrix_ctrl_msg_t* ctrl_msg = (stomptrix_ctrl_msg_t*)&evt->data.evt_gatt_server_user_write_request.value.data;

    		  switch(ctrl_msg->msg_id)
    		  {
    		  case LED_SET_STATIC_COLOUR:
    		  {
    			  static bool toggle_led_strip = true;
    			  if(evt->data.evt_gatt_server_user_write_request.value.len < sizeof(led_set_static_colour_t)){break;}

    			  led_set_static_colour_t* static_colour_msg = (led_set_static_colour_t*)&ctrl_msg->data;
				  if( toggle_led_strip )
				  {
					  toggle_led_strip = false;
					  led_strip_set_all(&led_strip, static_colour_msg->colour);
				  }
				  else
				  {
					  toggle_led_strip = true;
					  led_strip_set_all(&led_strip, 0x00000000);
				  }
    			  break;
    		  }
    		  case LED_SET_SLIDE_ANIMATION:
    		  {
    			  led_slide_animation_t* slide_anim_msg = (led_slide_animation_t*)&ctrl_msg->data;
    			  if(evt->data.evt_gatt_server_user_write_request.value.len < sizeof(led_slide_animation_t)){break;}
    			  led_strip_slide_animation(&led_strip, slide_anim_msg->colour, slide_anim_msg->single_or_cont);
    		  }
    		  default:
    			  break;
    		  }
    		  gecko_cmd_gatt_server_send_user_write_response(
    		              evt->data.evt_gatt_server_user_write_request.connection,
						  gattdb_colour,
    		              bg_err_success);
		  }
    	  break;

      case gecko_evt_gatt_server_user_read_request_id:
    	  {
    		  if (evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_battery)
    		  {
    			  static uint8_t batt_percentage;
    			  batt_percentage = battery_get_percentage();

				  gecko_cmd_gatt_server_send_user_read_response(
									evt->data.evt_gatt_server_user_read_request.connection,
									evt->data.evt_gatt_server_user_read_request.characteristic,
									0x00, /* SUCCESS */
									0x01, /* length */
									(uint8_t*)&batt_percentage);
    		  }
    		  if (evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_colour)
			  {
				  uint32_t colour = 0x01234567;
				  gecko_cmd_gatt_server_send_user_read_response(
									evt->data.evt_gatt_server_user_read_request.connection,
									evt->data.evt_gatt_server_user_read_request.characteristic,
									0x00, /* SUCCESS */
									0x04, /* length */
									(uint8_t*)&colour);
			  }
			  break;
    	  }
      default:

        break;
    }
  }
}

