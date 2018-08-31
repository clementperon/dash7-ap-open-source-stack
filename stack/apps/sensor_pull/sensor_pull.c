/* * OSS-7 - An opensource implementation of the DASH7 Alliance Protocol for ultra
 * lowpower wireless sensor communication
 *
 * Copyright 2015 University of Antwerp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


// This examples does not push sensor data to gateway(s) continuously, but instead writes the sensor value to a local file,
// which can then be fetched on request.
// Temperature data is used as a sensor value, when a HTS221 is available, otherwise value 0 is used.
// Contrary to the sensor_push example we are now defining an Access Profile which has a periodic scan automation enabled.
// The sensor will sniff the channel every second for background adhoc synchronization frames, to be able to receive requests from other nodes.

#include <stdio.h>
#include <stdlib.h>

#include "hwleds.h"
#include "hwsystem.h"
#include "hwlcd.h"

#include "scheduler.h"
#include "timer.h"
#include "assert.h"
#include "fs.h"
#include "log.h"
#include "compress.h"

#include "d7ap.h"
#include "alp_layer.h""
#include "dae.h"

#ifdef USE_HTS221
  #include "HTS221_Driver.h"
  #include "hwi2c.h"
#endif

#if !defined(USE_SX127X) && !defined(USE_NETDEV_SX127X)
  #error "background frames are only supported by the sx127x driver for now"
#endif


#define SENSOR_FILE_ID           0x40
#define SENSOR_FILE_SIZE         2
#define SENSOR_INTERVAL_SEC	TIMER_TICKS_PER_SEC * 10

#ifdef USE_HTS221
  static i2c_handle_t* hts221_handle;
#endif

void execute_sensor_measurement()
{
  int16_t temperature = 0; // in decicelsius. When there is no sensor, we just transmit 0 degrees

#if defined USE_HTS221
  HTS221_Get_Temperature(hts221_handle, &temperature);
#endif

  temperature = __builtin_bswap16(temperature); // need to store in big endian in fs
  fs_write_file(SENSOR_FILE_ID, 0, (uint8_t*)&temperature, SENSOR_FILE_SIZE);

  timer_post_task_delay(&execute_sensor_measurement, SENSOR_INTERVAL_SEC);
}

void init_user_files()
{
  // file 0x40: contains our sensor data
  fs_file_header_t sensor_file_header = (fs_file_header_t){
      .file_properties.action_protocol_enabled = 0,
      .length = SENSOR_FILE_SIZE,
  };

  fs_init_file(SENSOR_FILE_ID, &sensor_file_header, NULL);

  // file 0x41: reserved file (for example action file)
  // TODO this can be removed when we support creating files post init
  fs_file_header_t file_header = (fs_file_header_t){
      .file_properties.action_protocol_enabled = 0,
      .length = 11,
  };

  fs_init_file(0x41, &file_header, NULL);

  // file 0x42: reserved file for interface configuration
  // TODO this can be removed when we support creating files post init
  d7ap_session_config_t session_config;
  fs_init_file_with_d7asp_interface_config(0x42, &session_config);
}

void bootstrap()
{
    log_print_string("Device booted\n");

    fs_init_args_t fs_init_args = (fs_init_args_t){
        .fs_d7aactp_cb = &alp_layer_process_d7aactp,
        .access_profiles_count = DEFAULT_ACCESS_PROFILES_COUNT,
        .access_profiles = default_access_profiles,
        .access_class = 0x11, // use scanning AC
        .fs_user_files_init_cb = &init_user_files
    };

    fs_init(&fs_init_args);
    d7ap_init();
    alp_layer_init(NULL, false);

#if defined USE_HTS221
    hts221_handle = i2c_init(0, 0, 100000);
    HTS221_DeActivate(hts221_handle);
    HTS221_Set_BduMode(hts221_handle, HTS221_ENABLE);
    HTS221_Set_Odr(hts221_handle, HTS221_ODR_7HZ);
    HTS221_Activate(hts221_handle);
#endif

    sched_register_task(&execute_sensor_measurement);
    timer_post_task_delay(&execute_sensor_measurement, SENSOR_INTERVAL_SEC);
}
