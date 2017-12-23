#include <Arduino.h>
#include "eprobe.h"

#include "sync_measure.h"
#include "system_time.h"

void print_wakeup_reason();

#define MEASURE_CYCLE_TIME (30 * 1000)
#define uS_TO_S_FACTOR \
  1000000                /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 30 /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;

static const char *LOG_TAG = "EProbeMain";

void loop() {
  while (1) {
    measureLoop();

#ifdef SLEEP_ENABLED
    ESP_LOGD(LOG_TAG, "Going into light sleep");
    esp_light_sleep_start();
    ESP_LOGD(LOG_TAG, "Woke up from light sleep");
#else
    delay(MEASURE_CYCLE_TIME);
#endif
  }
}

void setup() {
  ++bootCount;

  Serial.begin(112500);
  while (!Serial)
    ;
  ESP_LOGI(LOG_TAG, "Environment Probe");
  ESP_LOGI(LOG_TAG, "Boot count %d", bootCount);

#ifdef SLEEP_ENABLED
  ESP_LOGI(LOG_TAG, "Boot number: %d", bootCount);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  print_wakeup_reason();
#endif

  setupSyncMeasure();
}

#ifdef SLEEP_ENABLED
/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case 1:
      Serial.println("Wakeup caused by external signal using RTC_IO");
      break;
    case 2:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case 3:
      Serial.println("Wakeup caused by timer");
      break;
    case 4:
      Serial.println("Wakeup caused by touchpad");
      break;
    case 5:
      Serial.println("Wakeup caused by ULP program");
      break;
    default:
      Serial.println("Wakeup was not caused by deep sleep");
      break;
  }
}
#endif
