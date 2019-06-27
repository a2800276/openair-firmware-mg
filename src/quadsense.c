#include "quadsense.h"
#include "ltc2497.h"
#include "bme280.h"
#include "openairboard.h"

#include "stdio.h"

static bool alpha_initialized = false;
static bool bme0_initialized = false;
static bool bme1_initialized = false;

static BME280_Struct bme0;
static BME280_Struct bme1;

static struct mgos_i2c *i2c;
static uint8_t adc_addr = 0;
static bool id1 = false;

static int adc_values[8] = {0,0,0,0,0,0,0,0};
static alphasense_cb alpha_cb;
static bme280_cb bme_cb;


//void quadsense_init() {
void quadsense_init( alphasense_cb a_cb, bme280_cb b_cb ) {

  alpha_cb = a_cb;
  bme_cb = b_cb;

  LOG(LL_INFO, ("alpha_cb %p", alpha_cb));
  LOG(LL_INFO, ("bme_cb %p", bme_cb));

  i2c = mgos_i2c_get_global();
  int modIdx = mgos_sys_config_get_openair_quadsense_idx();

  switch (modIdx) {
    case 1: adc_addr = 0x16; break;
    case 2: adc_addr = 0x14; id1 = true; break;
    default:
      LOG(LL_ERROR, ("Quadsense init: Invalid module index %i", modIdx));
      break;
  }
  openair_enable_module(modIdx, true);
  alpha_initialized = true;
  LOG(LL_DEBUG, ("quadsense + alpha initialized"));
  
  bme0_initialized = bme280_init(&bme0, i2c, 0);
  LOG(LL_INFO, ("quadsense + bme0 initialized"));

  bme1_initialized = bme280_init(&bme1, i2c, 1);
  LOG(LL_INFO, ("quadsense + bme1 initialized"));
}

#define VREF 4.11

bool quadsense_tick() {
  bool ok = false;
  if (alpha_initialized) {
    uint8_t chan;
    int val;
    ok = ltc2497_read(i2c, adc_addr, &chan, &val);

    if (ok) {
      adc_values[chan] = val;
      if (chan==7) {
        for (int i=0; i<8; i++) {
          LOG(LL_DEBUG, ("Channel %i value %i - %fV", i, adc_values[i], 0.5*VREF*adc_values[i]/65536.0));
        }
        alpha_cb(
            adc_values[0],
            adc_values[1],
            adc_values[2],
            adc_values[3],
            adc_values[4],
            adc_values[5],
            adc_values[6],
            adc_values[7]
            );
      }
    } else {
      LOG(LL_ERROR, ("ltc2497 read failed: %d",ok));
    }
  }
  if (bme0_initialized) { 
    uint32_t temp, press, hum;
    ok = bme280_read_data(&bme0, &temp, &press, &hum); //TODO: ID must be set for new HW
    if (ok && bme_cb) {
      float realTemp, realPress, realHum;
      bme280_compensate(&bme0, temp, press, hum, &realTemp, &realPress, &realHum);
//      LOG(LL_INFO, ("bme280 0: %f %f %f",realTemp, realPress, realHum));
      bme_cb(0, press, realPress, temp, realTemp, hum, realHum);
    }
    if (!ok) {
      bme_cb(0,0,0,0,0,0,0);
      LOG(LL_ERROR, ("bme280 read failed: %d",ok));
    }
  } else {
    bme0_initialized = bme280_init(&bme0, i2c, 0);
  }

  if (bme1_initialized) { 
    uint32_t temp, press, hum;
    ok = bme280_read_data(&bme1, &temp, &press, &hum); //TODO: ID must be set for new HW
    if (ok && bme_cb) {
      float realTemp, realPress, realHum;
      bme280_compensate(&bme1, temp, press, hum, &realTemp, &realPress, &realHum);
//      LOG(LL_INFO, ("bme280 1: %f %f %f",realTemp, realPress, realHum));
      bme_cb(1, press, realPress, temp, realTemp, hum, realHum);
    }
    if (!ok) {
      bme_cb(1,0,0,0,0,0,0);
      LOG(LL_ERROR, ("bme280 read failed: %d",ok));
    }
  } else {
    bme1_initialized = bme280_init(&bme1, i2c, 1);
  }

  return ok;
}

// vim: et:sw=2:ts=2
