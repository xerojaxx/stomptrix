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


#include "em_gpio.h"
#include "ext_lion_battery.h"
#include "bq24160.h"
#include "led_strip_controller.h"
#include "sk6812.h"

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
  .sleep.flags=SLEEP_FLAGS_DEEP_SLEEP_ENABLE,
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
 * @brief Function for taking a single temperature measurement with the WSTK Relative Humidity and Temperature (RHT) sensor.
 */
void temperatureMeasure()
{

  uint8_t htmTempBuffer[5]; /* Stores the temperature data in the Health Thermometer (HTM) format. */
  uint8_t flags = 0x00;   /* HTM flags set as 0 for Celsius, no time stamp and no temperature type. */
  int32_t tempData;     /* Stores the Temperature data read from the RHT sensor. */
  uint32_t rhData = 0;    /* Dummy needed for storing Relative Humidity data. */
  uint32_t temperature;   /* Stores the temperature data read from the sensor in the correct format */
  uint8_t *p = htmTempBuffer; /* Pointer to HTM temperature buffer needed for converting values to bitstream. */

  /* Convert flags to bitstream and append them in the HTM temperature data buffer (htmTempBuffer) */
  UINT8_TO_BITSTREAM(p, flags);

  /* Sensor relative humidity and temperature measurement returns 0 on success, nonzero otherwise */
  if (Si7013_MeasureRHAndTemp(I2C0, SI7021_ADDR, &rhData, &tempData) == 0) {
    /* Convert sensor data to correct temperature format */
    temperature = FLT_TO_UINT32(tempData, -3);
    /* Convert temperature to bitstream and place it in the HTM temperature data buffer (htmTempBuffer) */
    UINT32_TO_BITSTREAM(p, temperature);
    /* Send indication of the temperature in htmTempBuffer to all "listening" clients.
     * This enables the Health Thermometer in the Blue Gecko app to display the temperature.
     *  0xFF as connection ID will send indications to all connections. */
    gecko_cmd_gatt_server_send_characteristic_notification(
      0xFF, gattdb_battery, 5, htmTempBuffer);
  }
}

/**
 * @brief  Main function
 */
int main(void)

{

	//uint8_t connection_handle = 0;
#ifdef FEATURE_SPI_FLASH
  /* Put the SPI flash into Deep Power Down mode for those radio boards where it is available */
  MX25_init();
  MX25_DP();
  /* We must disable SPI communication */
  USART_Reset(USART1);

#endif /* FEATURE_SPI_FLASH */



  /* Initialize peripherals */
  enter_DefaultMode_from_RESET();


  /* Initialize stack */
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


#ifdef FEATURE_IOEXPANDER
if ( BSP_IOEXP_DEVICE_ID == BSP_IOExpGetDeviceId()) {
  BSP_PeripheralAccess(BSP_IOEXP_VCOM,   0); // Disable VCOM
  BSP_PeripheralAccess(BSP_IOEXP_DISPLAY,   0); // Disables the display by pulling DISP_ENABLE high.
  BSP_PeripheralAccess(BSP_IOEXP_SENSORS, 1); // Enables the Si7021 sensor on the Wireless STK by pulling SENSOR_ENABLE high
  BSP_PeripheralAccess(BSP_IOEXP_LEDS,    0); // The LEDs follow the bits LED0 and LED1 when this bit is set
}
#endif /* FEATURE_IOEXPANDER */


	lion_battery_init();

	led_strip_handle_t led_strip = {
		.num_of_leds = 3u,
		.set_colours = sk6812_set_colours
	};

	sk6812_init();
	colour_rgb_t colour = 0x00002222;
	led_strip_set_all(&led_strip, colour);

	//(void)led_strip_init(&led_strip);

  /* Initialize the Battery Management IC.*/
	bq24160_init();
	bq24160_read_voltage();


  while (1) {
    /* Event pointer for handling events */
    struct gecko_cmd_packet* evt;

    /* Check for stack event. */
    evt = gecko_wait_event();
    
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




        //gecko_cmd_hardware_set_soft_timer(32768/2,1,0);
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

      /* This event is generated when the software timer has ticked. In this example the temperature
       * is read after every 1 second and then the indication of that is sent to the listening client. */
      case gecko_evt_hardware_soft_timer_id:
        /* Measure the temperature as defined in the function temperatureMeasure() */
        //temperatureMeasure();


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


      /* Events related to OTA upgrading
      ----------------------------------------------------------------------------- */

      /* Checks if the user-type OTA Control Characteristic was written.
       * If written, boots the device into Device Firmware Upgrade (DFU) mode. */
//      case gecko_evt_gatt_server_user_write_request_id:
//        if(evt->data.evt_gatt_server_user_write_request.characteristic==gattdb_ota_control) {
//          /* Set flag to enter to OTA mode */
//          boot_to_dfu = 1;
//          /* Send response to Write Request */
//          gecko_cmd_gatt_server_send_user_write_response(
//            evt->data.evt_gatt_server_user_write_request.connection,
//            gattdb_ota_control,
//            bg_err_success);
//
//          /* Close connection to enter to DFU OTA mode */
//          gecko_cmd_endpoint_close(evt->data.evt_gatt_server_user_write_request.connection);
//        }
//        break;

      case gecko_evt_le_connection_opened_id:
    	  //connection_handle = evt->data.evt_le_connection_opened.connection;
    	  gecko_cmd_le_gap_set_mode(le_gap_non_discoverable, le_gap_non_connectable);
    	  break;

      case gecko_evt_gatt_server_user_write_request_id:
    	  if (evt->data.evt_gatt_server_user_write_request.characteristic == gattdb_colour)
		  {
    		  //uint16_t new_colour = evt->data.evt_gatt_server_user_write_request.characteristic.value.data;

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
    			  uint16_t batt_mv = battery_get_millvolts();

				  gecko_cmd_gatt_server_send_user_read_response(
									evt->data.evt_gatt_server_user_read_request.connection,
									evt->data.evt_gatt_server_user_read_request.characteristic,
									0x00, /* SUCCESS */
									0x02, /* length */
									(uint8_t*)&batt_mv);
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
//      case gecko_evt_gatt_characteristic_id:
//		  {
//			  uint8_t data[] = {11, 12, 13, 14, 15};
////			  gecko_cmd_gatt_server_send_characteristic_notification(
////					0xFF, gattdb_battery, 1, data);
//			  gecko_cmd_gatt_server_send_user_read_response(connection_handle,
//					  	  	  	  	  	  	  	  	  	    gattdb_battery,
//															0,
//															2,
//															data);
//		  }
//    	  break;
//      case gecko_evt_gatt_characteristic_value_id:
//		  {
//			  uint8_t data[] = {16, 17, 18, 19, 20};
//			  //			  gecko_cmd_gatt_server_send_characteristic_notification(
//			  //					0xFF, gattdb_battery, 1, data);
//			  			  gecko_cmd_gatt_server_send_user_read_response(connection_handle,
//			  					  	  	  	  	  	  	  	  	  	    gattdb_battery,
//			  															0,
//			  															3,
//			  															data);
//		  }
//    	  break;
//      case gecko_cmd_gatt_server_read_attribute_value_id:
//		  {
//			  uint8_t data[] = {16, 17, 18, 19};
//			  //			  gecko_cmd_gatt_server_send_characteristic_notification(
//			  //					0xFF, gattdb_battery, 1, data);
//			  			  gecko_cmd_gatt_server_send_user_read_response(connection_handle,
//			  					  	  	  	  	  	  	  	  	  	    gattdb_battery,
//			  															0,
//			  															4,
//			  															data);
//		  }
//    	  break;
      default:

        break;
    }
  }
}


/** @} (end addtogroup app) */
/** @} (end addtogroup Application) */
