#include "system_time.h"
#include "eprobe.h"

#include "apps/sntp/sntp.h"
#include "nvs_flash.h"

#define isTimeSet() (timeinfo.tm_year >= (2016 - 1900))

void systime_obtainTime();
void systime_initializeSntp();

static const char *LOG_TAG = "SystemTime";
static const char *NTP_HOST = "pool.ntp.org";

void systime_setup() {
  ESP_LOGI(LOG_TAG, "Setup time");
  time_t now;
  struct tm timeinfo {};
  time(&now);
  localtime_r(&now, &timeinfo);
  // Is time set? If not, tm_year will be (1970 - 1900).
  if (!isTimeSet()) {
    ESP_LOGI(LOG_TAG, "Time not yet set. Retrying optaining via NTP.");
    systime_obtainTime();
    // update 'now' variable with current time
    time(&now);
  }

  // Set timezone to China Standard Time
  //    putenv("TZ=cet-1cest,m3.5.0,m10.5.0");
  setenv("TZ", "cet-1cest,m3.5.0,m10.5.0", 1);
  tzset();

  // xEventGroupSetBits(m_sntpEventGroup, DATE_SET_BIT);
}

void systime_obtainTime() {
  ESP_LOGI(LOG_TAG, "Optaining current time via NTP");
  // ESP_ERROR_CHECK(nvs_flash_init());

  systime_initializeSntp();

  time_t now = 0;
  struct tm timeinfo {};
  int retry = 0;
  const int retry_count = 10;
  char displayTime[65];
  while (!isTimeSet() && ++retry < retry_count) {
    ESP_LOGI(LOG_TAG, "Waiting for system time to be set... (%d/%d)", retry,
             retry_count);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    time(&now);
    localtime_r(&now, &timeinfo);
  }

  systime_createCurrentTimeOutput(now, displayTime, sizeof(displayTime) - 1, "%c");
  ESP_LOGI(LOG_TAG, "Set time to %s", displayTime);

  sntp_stop();
}

void systime_initializeSntp() {
  ESP_LOGI(LOG_TAG, "Initializing SNTP");
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, (char *)NTP_HOST);
  sntp_init();
}

void systime_createCurrentTimeOutput(time_t timestamp, char *strftime_buf,
                             size_t buf_len, const char *pattern) {
  struct tm timeinfo {};
  localtime_r(&timestamp, &timeinfo);

  strftime(strftime_buf, buf_len, pattern, &timeinfo);
  //   strftime(strftime_buf, buf_len, "%T", &timeinfo);
}
