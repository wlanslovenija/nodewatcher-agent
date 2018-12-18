/*
 * nodewatcher-agent - remote monitoring daemon
 *
 * Copyright (C) 2018 Marin Stević
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <nodewatcher-agent/module.h>
#include <nodewatcher-agent/json.h>
#include <nodewatcher-agent/utils.h>

static int nw_meshpoint_start_acquire_data(struct nodewatcher_module *module,
									struct ubus_context *ubus,
									struct uci_context *uci)
{
  static int buffer;
  
  static char buffer_string[1024];
  
  json_object *object = json_object_new_object();

  json_object *ina230_POE = json_object_new_object();
  /* Current */
  FILE *ina230_POE_c = fopen("/sys/devices/platform/soc/soc:i2c-gpio/i2c-2/2-0040/hwmon/hwmon0/curr1_input", "r");
  if (ina230_POE_c != NULL) {
    if (fscanf(ina230_POE_c, "%d", &buffer) == 1)
      json_object_object_add(ina230_POE, "current", json_object_new_int(buffer));
    fclose(ina230_POE_c);
  }
  /* Voltage */
  FILE *ina230_POE_v = fopen("/sys/devices/platform/soc/soc:i2c-gpio/i2c-2/2-0040/hwmon/hwmon0/in1_input", "r");
  if (ina230_POE_v != NULL) {
    if (fscanf(ina230_POE_v, "%d", &buffer) == 1)
      json_object_object_add(ina230_POE, "voltage", json_object_new_int(buffer));
    fclose(ina230_POE_v);
  }
  /* Power */
  FILE *ina230_POE_p = fopen("/sys/devices/platform/soc/soc:i2c-gpio/i2c-2/2-0040/hwmon/hwmon0/power1_input", "r");
  if (ina230_POE_p != NULL) {
    if (fscanf(ina230_POE_p, "%d", &buffer) == 1)
      json_object_object_add(ina230_POE, "power", json_object_new_int(buffer));
    fclose(ina230_POE_p);
  }
  json_object_object_add(object, "ina230_POE", ina230_POE);
  
  json_object *ina230_input = json_object_new_object();
  /* Current */
  FILE *ina230_input_c = fopen("/sys/devices/platform/soc/soc:i2c-gpio/i2c-2/2-0044/hwmon/hwmon1/curr1_input", "r");
  if (ina230_input_c != NULL) {
    if (fscanf(ina230_input_c, "%d", &buffer) == 1)
      json_object_object_add(ina230_input, "current", json_object_new_int(buffer));
    fclose(ina230_input_c);
  }
  /* Voltage */
  FILE *ina230_input_v = fopen("/sys/devices/platform/soc/soc:i2c-gpio/i2c-2/2-0044/hwmon/hwmon1/in1_input", "r");
  if (ina230_input_v != NULL) {
    if (fscanf(ina230_input_v, "%d", &buffer) == 1)
      json_object_object_add(ina230_input, "voltage", json_object_new_int(buffer));
    fclose(ina230_input_v);
  }
  /* Power */
  FILE *ina230_input_p = fopen("/sys/devices/platform/soc/soc:i2c-gpio/i2c-2/2-0044/hwmon/hwmon1/power1_input", "r");
  if (ina230_input_p != NULL) {
    if (fscanf(ina230_input_p, "%d", &buffer) == 1)
      json_object_object_add(ina230_input, "power", json_object_new_int(buffer));
    fclose(ina230_input_p);
  }
  json_object_object_add(object, "ina230_input", ina230_input);
  
  json_object *bme280 = json_object_new_object();
  /* Temperature */
  FILE *bme280_t = fopen("/sys/devices/platform/soc/soc:i2c-gpio/i2c-2/2-0076/iio:device0/in_temp_input", "r");
  if (bme280_t != NULL) {
    if (fscanf(bme280_t, "%d", &buffer) == 1)
      json_object_object_add(bme280, "temperature", json_object_new_int(buffer));
    fclose(bme280_t);
  }
  /* Humidity */
  FILE *bme280_h = fopen("/sys/devices/platform/soc/soc:i2c-gpio/i2c-2/2-0076/iio:device0/in_humidityrelative_input", "r");
  if (bme280_h != NULL) {
    if (fscanf(bme280_h, "%d", &buffer) == 1)
      json_object_object_add(bme280, "humidity", json_object_new_int(buffer));
    fclose(bme280_h);
  }
  /* Pressure */
  FILE *bme280_p = fopen("/sys/devices/platform/soc/soc:i2c-gpio/i2c-2/2-0076/iio:device0/in_pressure_input", "r");
  if (bme280_p != NULL) {
    if (fscanf(bme280_p, "%[^\n]", buffer_string) == 1)
      json_object_object_add(bme280, "pressure", json_object_new_string(buffer_string));
    fclose(bme280_p);
  }
  json_object_object_add(object, "bme280", bme280);

  /* Store resulting JSON object */
  return nw_module_finish_acquire_data(module, object);
}

static int nw_meshpoint_init(struct nodewatcher_module *module,
                           struct ubus_context *ubus,
                           struct uci_context *uci)
{
  return 0;
}

/* Module descriptor */
struct nodewatcher_module nw_module = {
  .name = "core.meshpoint",
  .author = "Marin Stevic",
  .version = 1,
  .hooks = {
    .init               = nw_meshpoint_init,
    .start_acquire_data = nw_meshpoint_start_acquire_data,
  },
  .schedule = {
    .refresh_interval = 30,
  },
};