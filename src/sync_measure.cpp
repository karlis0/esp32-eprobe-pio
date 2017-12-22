#include "sync_measure.h"
#include "eprobe.h"

#include <iostream>

#include "AdafruitIO_WiFi.h"

#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#include <SPI.h>
#include "FS.h"
#include "SD.h"

#include "gxepd_display.h"
#include "system_time.h"
#include "file.h"

#define PIN_LED GPIO_NUM_5
#define STR_DATE_TIME_LEN 64

void setupBme680();
void setupDataLogging();
void setupAdafruitIo();
void setupDisplay();
void setupSD();
void displaySensorData(bme680_sensor_data_t sensorData);
void displayInitialScreen();
void updateDisplayBufferForData(const char *temp_buf, const char *humidity_buf,
                                const char *pressure_buf,
                                const char *airquality_buf,
                                long refreshCounter);

void sendSensorData(bme680_sensor_data_t sensorData);
void signalMeasureCycleSuccess();
bme680_sensor_data_t readBme680Data();

static Adafruit_BME680 bme;

static const char *LOG_TAG = "SyncMeasure";

static GxIO_Class displayIo(SPI, 32, 17, 16);  // (interface, CS, DC, RST, BL)
static GxEPD_Class display(displayIo, 16, 4);  // (RST, BSY)

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);
AdafruitIO_Feed *temperatureFeed = io.feed("temperature");
AdafruitIO_Feed *humidityFeed = io.feed("humidity");
AdafruitIO_Feed *pressureFeed = io.feed("pressure");
AdafruitIO_Feed *airqualityFeed = io.feed("airquality");

static uint16_t cycleCounter;

void setupSyncMeasure() {
  cycleCounter = 0;

  setupSD();

  setupAdafruitIo();
  setupTime();
  setupBme680();
  setupDataLogging();
  setupDisplay();

  displayInitialScreen();
}

void setupAdafruitIo() {
  ESP_LOGI(LOG_TAG, "Setup Adafruit IO");

  io.connect();

  while (io.status() < AIO_CONNECTED) {
    ESP_LOGI(LOG_TAG, "Waiting for Adafruit IO connection");
    delay(500);
  }
  ESP_LOGI(LOG_TAG, "Adafruit IO connection status: %s", io.statusText());
}

void setupBme680() {
  ESP_LOGD(LOG_TAG, "Setup BME680 sensor");
  if (!bme.begin()) {
    ESP_LOGE(LOG_TAG, "Could not find a valid BME680 sensor, check wiring!");
    while (1)
      ;
  }

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);  // 320*C for 150 ms
}

void setupDataLogging() {
  ESP_LOGD(LOG_TAG, "Setup data logging");

  ::gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);
}

void setupDisplay() { display.init(); }

void measureLoop() {
  cycleCounter++;
  ESP_LOGD(LOG_TAG, "Entering messuring loop (Cycle: %d)", cycleCounter);

  io.run();

  bme680_sensor_data_t sensorData = readBme680Data();
  displaySensorData(sensorData);
  sendSensorData(sensorData);
  signalMeasureCycleSuccess();
}

bme680_sensor_data_t readBme680Data() {
  ESP_LOGD(LOG_TAG, "Read BME680 sensor data");
  bme680_sensor_data_t sensorData{};

  if (!bme.performReading()) {
    Serial.println("Failed to perform BME680 reading");
    return sensorData;
  }
  sensorData.temperature = bme.temperature;
  sensorData.pressure = bme.pressure;
  sensorData.humidity = bme.humidity;
  sensorData.airquality = bme.gas_resistance;

  return sensorData;
}

void displaySensorData(bme680_sensor_data_t sensorData) {
  ESP_LOGD(LOG_TAG, "Displaying Sensor Data");
  char temp_buf[10];
  char humidity_buf[10];
  char pressure_buf[13];
  char airquality_buf[13];
  char strftime_buf[STR_DATE_TIME_LEN];
  time_t now;

  time(&now);

  createCurrentTimeOutput(now, strftime_buf, (STR_DATE_TIME_LEN - 1), "%c");

  Serial.println(
      "------------------------------------------------------------");
  Serial.print("Measured at ");
  Serial.println(strftime_buf);
  Serial.println(
      "------------------------------------------------------------");

  snprintf(temp_buf, 10, "%.2f*C", (double)sensorData.temperature);
  snprintf(humidity_buf, 10, "%.2f%%", (double)sensorData.humidity);
  snprintf(pressure_buf, 12, "%.2fmb", (double)(sensorData.pressure / 100.0));
  snprintf(airquality_buf, 12, "%.2fkR",
           (double)(sensorData.airquality / 1000.0));

  Serial.println(temp_buf);
  Serial.println(humidity_buf);
  Serial.println(pressure_buf);
  Serial.println(airquality_buf);

  Serial.println(
      "------------------------------------------------------------");

  if (cycleCounter % 40 == 0) {
    displayInitialScreen();
  }

  createCurrentTimeOutput(now, strftime_buf, (STR_DATE_TIME_LEN - 1), "%c");
  char dataLogLine[129];
  snprintf(dataLogLine,
   128,
    "'%s',%.2f,%.2f,%.2f,%.4f\n",
    strftime_buf,
     (double)sensorData.temperature,
     (double)sensorData.humidity,
     (double)sensorData.pressure / 100,0,
     (double)sensorData.airquality / 1000.0
     );

  appendFile(SD, "/datalog.csv", dataLogLine);

  updateDisplayBufferForData(temp_buf, humidity_buf, pressure_buf,
                             airquality_buf, cycleCounter);
}

void updateDisplayBufferForData(const char *temp_buf, const char *humidity_buf,
                                const char *pressure_buf,
                                const char *airquality_buf,
                                long refreshCounter) {
  char strftime_buf[STR_DATE_TIME_LEN];
  time_t now;

  time(&now);

  createCurrentTimeOutput(now, strftime_buf, (STR_DATE_TIME_LEN - 1), "%T");

  display.setTextColor(GxEPD_BLACK);
  display.fillScreen(GxEPD_WHITE);
  display.setFont(fsmall);
  display.setCursor(value_column, line_height);
  display.print(temp_buf);
  display.setCursor(value_column, 2 * line_height);
  display.print(humidity_buf);
  display.setCursor(value_column, 3 * line_height);
  display.print(pressure_buf);
  display.setCursor(value_column, 4 * line_height);
  display.print(airquality_buf);
  display.updateWindow(value_column, 0, display.width() - value_column,
                       (4 * line_height) + (line_height / 2));

  display.setTextColor(GxEPD_WHITE);
  display.fillScreen(GxEPD_BLACK);
  display.setFont(fsmall7pt);
  display.setCursor(0, 5 * line_height);
  display.print(strftime_buf);
  display.print(" (");
  display.print(refreshCounter);
  display.print(")");
  display.updateWindow(0, (5 * line_height) - (line_height / 2),
                       display.width(), 34);
}

void displayInitialScreen() {
  ESP_LOGD(LOG_TAG, "Display initial screen");

  display.setTextColor(GxEPD_BLACK);
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(0, line_height);
  display.setFont(fthermometer);
  display.print(" ");
  display.setCursor(0, 2 * line_height);
  display.setFont(fpercent);
  display.print(" ");
  display.setCursor(0, 3 * line_height);
  display.setFont(ftachometer);
  display.print(" ");
  display.update();
  //    vTaskDelay(2000 / portTICK_PERIOD_MS);

  // partial update to full screen to preset for partial update of box window
  // (this avoids strange background effects)
  display.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
}

void sendSensorData(bme680_sensor_data_t sensorData) {
  ESP_LOGD(LOG_TAG, "Send sensor to Adafruit IO");
  aio_status_t ioStatus = io.status();
  if (ioStatus >= AIO_CONNECTED) {
    bool temperatureSuccess = temperatureFeed->save(sensorData.temperature);
    bool humiditySuccess = humidityFeed->save(sensorData.humidity);
    bool pressureSuccess = pressureFeed->save(sensorData.pressure);
    bool airqualitySuccess = airqualityFeed->save(sensorData.airquality);

    ESP_LOGI(LOG_TAG,
             "Adafruit IO feed response: temperature: %d, humidity: %d, "
             "pressure: %d, airquality: %d",
             temperatureSuccess, humiditySuccess, pressureSuccess,
             airqualitySuccess);
  } else {
    ESP_LOGD(LOG_TAG, "Adafruit IO disconnected (%d)", ioStatus);
  }
}

void signalMeasureCycleSuccess() {
  ::gpio_set_level(PIN_LED, false);
  delay(100);
  ::gpio_set_level(PIN_LED, true);
}

//-------- SD Card
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  ESP_LOGI(LOG_TAG, "Listing directory: %s", dirname);

  File root = fs.open(dirname);
  if (!root) {
    ESP_LOGE(LOG_TAG, "Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    ESP_LOGW(LOG_TAG, "Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char *path) {
  Serial.printf("Creating Dir: %s", path);
  if (fs.mkdir(path)) {
    ESP_LOGD(LOG_TAG, "Dir created");
  } else {
    ESP_LOGW(LOG_TAG, "mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char *path) {
  Serial.printf("Removing Dir: %s", path);
  if (fs.rmdir(path)) {
    ESP_LOGD(LOG_TAG, "Dir removed");
  } else {
    ESP_LOGW(LOG_TAG, "rmdir failed");
  }
}

void readFile(fs::FS &fs, const char *path) {
  ESP_LOGI(LOG_TAG, "Reading file: %s", path);

  File file = fs.open(path);
  if (!file) {
    ESP_LOGE(LOG_TAG, "Failed to open file for reading");
    return;
  }

  ESP_LOGI(LOG_TAG, "Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  ESP_LOGI(LOG_TAG, "Writing file: %s", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    ESP_LOGE(LOG_TAG, "Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    ESP_LOGD(LOG_TAG, "File written");
  } else {
    ESP_LOGW(LOG_TAG, "Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message) {
  ESP_LOGI(LOG_TAG, "Appending to file: %s", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    ESP_LOGE(LOG_TAG, "Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    ESP_LOGD(LOG_TAG, "Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char *path1, const char *path2) {
  ESP_LOGI(LOG_TAG, "Renaming file %s to %s", path1, path2);
  if (fs.rename(path1, path2)) {
    ESP_LOGD(LOG_TAG, "File renamed");
  } else {
    ESP_LOGW(LOG_TAG, "Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    ESP_LOGD(LOG_TAG, "File deleted");
  } else {
    ESP_LOGW(LOG_TAG, "Delete failed");
  }
}

/* void testFileIO(fs::FS &fs, const char *path) {
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file) {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len) {
      size_t toRead = len;
      if (toRead > 512) {
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }

  file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++) {
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}
 */
void setupSD() {
  if (!SD.begin()) {
    ESP_LOGW(LOG_TAG, "Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    ESP_LOGW(LOG_TAG, "No SD card attached");
    return;
  }

char *cardTypeName;
  if (cardType == CARD_MMC) {
    cardTypeName = "MMC";
  } else if (cardType == CARD_SD) {
    cardTypeName = "SDSC";
  } else if (cardType == CARD_SDHC) {
    cardTypeName = "SDHC";
  } else {
    cardTypeName = "UNKNOWN";
  }
  ESP_LOGI(LOG_TAG, "SD Card Type: %s", cardTypeName);

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  /*   listDir(SD, "/", 0);
    createDir(SD, "/mydir");
    listDir(SD, "/", 0);
    removeDir(SD, "/mydir");
    listDir(SD, "/", 2);
    writeFile(SD, "/hello.txt", "Hello ");
    appendFile(SD, "/hello.txt", "World!\n");
    readFile(SD, "/hello.txt");
    deleteFile(SD, "/foo.txt");
    renameFile(SD, "/hello.txt", "/foo.txt");
    readFile(SD, "/foo.txt");
    testFileIO(SD, "/test.txt");
    Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
   */
}
