#include "sync_measure.h"
#include "eprobe.h"

#include <iostream>

#include "AdafruitIO_WiFi.h"

#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#include <SPI.h>
#include "FS.h"
#include "SD.h"

#include "file.h"
#include "gxepd_display.h"
#include "system_time.h"

#ifdef GxGDEP015OC1_ACTIVE
#include "welcome_screen_200x200.h"
#endif
#ifdef GxGDE0213B1_ACTIVE
#include "welcome_screen_128x250.h"
#endif

// Pin definitions
#define BME680_PIN_CS 33

#define DISPLAY_PIN_CS 32
#define DISPLAY_PIN_DC 17
#define DISPLAY_PIN_RST 16
#define DISPLAY_PIN_BSY 4

#define PIN_LED GPIO_NUM_5

// Macros
#define isAdafruitIoConnected() (io.status() >= AIO_CONNECTED)

// Function Prototypes
void bme680_setup();
void datalog_setup();
void aio_setup();
void display_setup();
void wifi_setup();
void sd_setup();
void wifi_EventCallback(WiFiEvent_t event);
void wifi_connect();

void aio_connectIfDisconnected();
void display_showSensorData(bme680_sensor_data_t sensorData);
void display_showMainScreen();
void display_showStartupStatus(const char *message);
void display_showStartupScreen();
void display_updateBufferForData(const char *temp_buf, const char *humidity_buf,
                                 const char *pressure_buf,
                                 const char *airquality_buf,
                                 long refreshCounter);

void aio_sendSensorData(bme680_sensor_data_t sensorData);
void aio_checkIoEventsIfConnected();
void gpio_signalMeasureCycleSuccess();
bme680_sensor_data_t bme680_readSensorData();

// BME680
static Adafruit_BME680 bme(BME680_PIN_CS);

// Display
static GxIO_Class displayIo(SPI, DISPLAY_PIN_CS, DISPLAY_PIN_DC,
                            DISPLAY_PIN_RST);  // (interface, CS, DC, RST, BL)
static GxEPD_Class display(displayIo, DISPLAY_PIN_RST,
                           DISPLAY_PIN_BSY);  // (RST, BSY)

// Adafruit IO
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);
AdafruitIO_Feed *temperatureFeed = io.feed("temperature");
AdafruitIO_Feed *humidityFeed = io.feed("humidity");
AdafruitIO_Feed *pressureFeed = io.feed("pressure");
AdafruitIO_Feed *airqualityFeed = io.feed("airquality");

// Global constants
#define STR_DATE_TIME_LEN 64
static const char *LOG_TAG = "SyncMeasure";

static uint16_t cycleCounter;

void setupSyncMeasure() {
  cycleCounter = 0;

  display_setup();
  display_showStartupScreen();

  display_showStartupStatus("SD");
  sd_setup();

  display_showStartupStatus("WiFi");
  wifi_setup();
  // aio_setup();

  display_showStartupStatus("BME680");
  bme680_setup();

  display_showStartupStatus("Datalog");
  datalog_setup();

  display_showMainScreen();
}

void wifi_EventCallback(WiFiEvent_t event) {
  ESP_LOGD(LOG_TAG, "[WiFi-event] event: %d", event);

  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      ESP_LOGD(LOG_TAG, "WiFi connected\nIP address: %s",
               WiFi.localIP().toString().c_str());
      systime_setup();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      ESP_LOGD(LOG_TAG, "WiFi lost connection");
      break;
  }
}

void wifi_setup() {
  ESP_LOGI(LOG_TAG, "Setup WiFi");

  WiFi.onEvent(wifi_EventCallback);

  wifi_connect();
}

void aio_setup() {
  ESP_LOGI(LOG_TAG, "Setup Adafruit IO");

  // io.connect();

  ESP_LOGI(LOG_TAG, "Adafruit IO connection status: %s", io.statusText());
}

void bme680_setup() {
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

void datalog_setup() {
  ESP_LOGD(LOG_TAG, "Setup data logging");

  ::gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);
}

void display_setup() { display.init(); }

void measureLoop() {
  cycleCounter++;
  ESP_LOGD(LOG_TAG, "Entering messuring loop (Cycle: %d)", cycleCounter);

  aio_connectIfDisconnected();
  aio_checkIoEventsIfConnected();

  bme680_sensor_data_t sensorData = bme680_readSensorData();
  display_showSensorData(sensorData);
  aio_sendSensorData(sensorData);
  gpio_signalMeasureCycleSuccess();
}

bme680_sensor_data_t bme680_readSensorData() {
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

void display_showSensorData(bme680_sensor_data_t sensorData) {
  ESP_LOGD(LOG_TAG, "Displaying Sensor Data");
  char temp_buf[10];
  char humidity_buf[10];
  char pressure_buf[13];
  char airquality_buf[13];
  char strftime_buf[STR_DATE_TIME_LEN];
  time_t now;

  time(&now);

  systime_createCurrentTimeOutput(now, strftime_buf, (STR_DATE_TIME_LEN - 1),
                                  "%c");

  Serial.println(
      "------------------------------------------------------------");
  Serial.printf("Measure cycle %d at %s\n", cycleCounter, strftime_buf);
  Serial.println(
      "------------------------------------------------------------");

  snprintf(temp_buf, 10, "%.2f", (double)sensorData.temperature);
  snprintf(humidity_buf, 10, "%.2f", (double)sensorData.humidity);
  snprintf(pressure_buf, 12, "%.2f", (double)(sensorData.pressure / 100.0));
  snprintf(airquality_buf, 12, "%.2fk",
           (double)(sensorData.airquality / 1000.0));

  Serial.println(temp_buf);
  Serial.println(humidity_buf);
  Serial.println(pressure_buf);
  Serial.println(airquality_buf);

  Serial.println(
      "------------------------------------------------------------");

  if (cycleCounter % 40 == 0) {
    display_showMainScreen();
  }

  systime_createCurrentTimeOutput(now, strftime_buf, (STR_DATE_TIME_LEN - 1),
                                  "%c");
  char dataLogLine[129];
  snprintf(dataLogLine, 128, "'%s',%.2f,%.2f,%.2f,%.4f\n", strftime_buf,
           (double)sensorData.temperature, (double)sensorData.humidity,
           (double)sensorData.pressure / 100, 0,
           (double)sensorData.airquality / 1000.0);

  appendFile(SD, "/datalog.csv", dataLogLine);

  display_updateBufferForData(temp_buf, humidity_buf, pressure_buf,
                              airquality_buf, cycleCounter);
}

void display_updateBufferForData(const char *temp_buf, const char *humidity_buf,
                                 const char *pressure_buf,
                                 const char *airquality_buf,
                                 long refreshCounter) {
  char strftime_buf[STR_DATE_TIME_LEN];
  time_t now;

  time(&now);

#ifdef GxGDEP015OC1_ACTIVE
  systime_createCurrentTimeOutput(now, strftime_buf, (STR_DATE_TIME_LEN - 1),
                                  "%d.%m.%y %T");
#endif
#ifdef GxGDE0213B1_ACTIVE
  systime_createCurrentTimeOutput(now, strftime_buf, (STR_DATE_TIME_LEN - 1),
                                  "%T");
#endif

  display.setTextColor(GxEPD_BLACK);
  display.fillScreen(GxEPD_WHITE);
  display.setFont(fsmall);

  display.setCursor(34, 22);
  display.print(temp_buf);
  display.updateWindow(34, 0, 100, 24);

  display.setCursor(34, 72);
  display.print(humidity_buf);
  display.updateWindow(34, 50, 100, 24);

  display.setCursor(34, 122);
  display.print(pressure_buf);
  display.updateWindow(34, 100, 100, 24);

  display.setCursor(34, 172);
  display.print(airquality_buf);
  display.updateWindow(34, 150, 100, 24);

#ifdef GxGDEP015OC1_ACTIVE
  display.setFont(fsmall7pt);
  display.setCursor(136, 11);
  display.printf("[%06d]", refreshCounter);
  display.updateWindow(136, 0, display.width(), 22);
  display.setTextColor(GxEPD_WHITE);
  display.fillScreen(GxEPD_BLACK);
  display.setCursor(2, (200-7));
  display.print(strftime_buf);
  display.updateWindow(0, 200-20, display.width(), 20);
#endif

#ifdef GxGDE0213B1_ACTIVE
  display.setTextColor(GxEPD_WHITE);
  display.fillScreen(GxEPD_BLACK);
  display.setFont(fsmall7pt);
  display.setCursor(2, 220);
  display.print(strftime_buf);
  display.print(" (");
  display.print(refreshCounter);
  display.print(")");
  display.updateWindow(0, 200, display.width(), 34);
#endif
}

void display_showMainScreen() {
  ESP_LOGD(LOG_TAG, "Show main screen");

  display.drawBitmap(welcomeScreenBitmap, sizeof(welcomeScreenBitmap));
  // fill second buffer with same background image to prevent flickering
  display.drawBitmap(welcomeScreenBitmap, sizeof(welcomeScreenBitmap));
}

void display_showStartupScreen() {
  ESP_LOGD(LOG_TAG, "Show startup screen");

  display.setTextColor(GxEPD_BLACK);
  display.fillScreen(GxEPD_WHITE);
  display.setFont(fsmall);
  display.setCursor(3, line_height);
  display.print("eProbe");
  display.setCursor(3, 2 * line_height);
  display.print(".starting.");

  display.update();
  display.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
}

void display_showStartupStatus(const char *message) {
  display.setTextColor(GxEPD_WHITE);
  display.fillScreen(GxEPD_BLACK);
  display.setFont(fsmall);
  display.setCursor(3, 3 * line_height);
  display.print(message);
  display.updateWindow(0, (3 * line_height) - (line_height / 2),
                       display.width(), 34); 
}

void aio_connectIfDisconnected() {
  ESP_LOGI(LOG_TAG, "Checking WiFi and AIO connection");

  int wifiStatus = WiFi.status();
  if (wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_CONNECTION_LOST ||
      wifiStatus == WL_DISCONNECTED) {
    wifi_connect();
  }
}

void wifi_connect() {
  int status = 0;
  int waitStates = 0;

  io.connect();
  // WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (status != WL_CONNECTED && waitStates < 10) {
    waitStates++;
    ESP_LOGI(LOG_TAG, "Attempting to connect to network (%d/10)", waitStates);
    status = WiFi.status();

    delay(1000);
  }
}

void aio_sendSensorData(bme680_sensor_data_t sensorData) {
  ESP_LOGD(LOG_TAG, "Send sensor to Adafruit IO");

  if (isAdafruitIoConnected()) {
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
    ESP_LOGD(LOG_TAG, "Adafruit IO disconnected (%d). Skip sending data",
             io.status());
  }
}

void aio_checkIoEventsIfConnected() {
  if (isAdafruitIoConnected()) {
    ESP_LOGD(LOG_TAG, "Checking incoming AdafruitIO events");
    io.run();
  } else {
    ESP_LOGD(LOG_TAG, "Skip checking incoming AdafruitIO events");
  }
}

void gpio_signalMeasureCycleSuccess() {
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
void sd_setup() {
  if (!SD.begin()) {
    ESP_LOGW(LOG_TAG, "Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    ESP_LOGW(LOG_TAG, "No SD card attached");
    return;
  }

  const char *cardTypeName;
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
  ESP_LOGD(LOG_TAG, "SD Card Size: %lluMB", cardSize);
}
