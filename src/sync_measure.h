#ifndef SYNC_MEASURE_H
#define SYNC_MEASURE_H

#include <ctime>

typedef struct {
    time_t acquiringTime;
    float temperature;
    float humidity;
    float pressure;
    float airquality;
} bme680_sensor_data_t;

void setupSyncMeasure();
void measureLoop();

#endif