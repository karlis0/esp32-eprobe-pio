#include <Arduino.h>
#include "eprobe.h"

#include "system_time.h"
#include "sync_measure.h"

#define MEASURE_CYCLE_TIME (30 * 1000)

void setupTime();
void obtainTime();
void initializeSntp();

static const char *LOG_TAG = "EProbeMain";

void loop() {
  while (1) {
    measureLoop();

    delay(MEASURE_CYCLE_TIME);
  }
}

void setup() {
  Serial.begin(112500);
  while (!Serial)
    ;
  ESP_LOGI(LOG_TAG, "Environment Probe");

  setupSyncMeasure();
}
