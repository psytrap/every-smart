
SET_LOOP_TASK_STACK_SIZE(32 * 1024);  // 32KB


//   _____                      ____                       _     _ _ _
//  | ____|_   _____ _ __ _   _/ ___| _ __ ___   __ _ _ __| |_  | (_) |__
//  |  _| \ \ / / _ \ '__| | | \___ \| '_ ` _ \ / _` | '__| __| | | | '_ \  
//  | |___ \ V /  __/ |  | |_| |___) | | | | | | (_| | |  | |_  | | | |_) |
//  |_____| \_/ \___|_|   \__, |____/|_| |_| |_|\__,_|_|   \__| |_|_|_.__/
//

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <freertos/queue.h>
#include <ArduinoJson.h>

constexpr char *SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";  // UART service UUID
constexpr char *CHARACTERISTIC_UUID_RX = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr char *CHARACTERISTIC_UUID_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";



BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool prevDeviceConnected = false;
uint8_t txValue = 0;

constexpr uint32_t PAD_SIZE = 1000u;
constexpr uint32_t QUEUE_LEN = 5u;
constexpr uint32_t CHUNK_SIZE = 200u;
QueueHandle_t ringBuffer;
char receivePad[PAD_SIZE];

constexpr char *EVENT = "event";
constexpr char *EVENT_READY = "ready";
constexpr char *EVENT_CLICK = "click";
constexpr char *EVENT_INPUT = "input";
constexpr char *PAYLOAD = "payload";
constexpr char *ID = "id";
constexpr char *VALUE = "value";


void (*event_callback_ptr)(JsonDocument &event_doc) = nullptr;
const char *page = nullptr;


class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer){
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0) {
      if (rxValue[0] == 'X') {
        return;  // TODO Handshake
      }
      strncat(receivePad, rxValue.c_str() + 1, PAD_SIZE);
      if (rxValue[0] == '#') {
        xQueueSendFromISR(ringBuffer, (void *)receivePad, NULL);
        Serial.println(receivePad);
        receivePad[0] = '\0';  // reset pad
      }
    }
  }
};



void everySmartSetup() {
  // Create the BLE Device
  BLEDevice::init("Every Smart UART Service");
  ringBuffer = xQueueCreate(QUEUE_LEN, sizeof(char[PAD_SIZE]));
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("Waiting for a client connection to notify...");
}



bool everySmartLoop() {
  // Serial.println(deviceConnected ? "connected" : "disconnected");

  if (deviceConnected) {
    if (uxQueueMessagesWaiting(ringBuffer) != 0) {
      char rx[PAD_SIZE];
      if (xQueueReceive(ringBuffer, (void *)rx, 0) == pdTRUE) {

        Serial.println(rx);

        JsonDocument event_doc;
        DeserializationError error = deserializeJson(event_doc, rx);
        if (error) {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.f_str());
          return deviceConnected;
        }

        const char *event = event_doc[EVENT];
        Serial.println(event);
        if (strncmp(event, EVENT_READY, strlen(EVENT_READY)) == 0) {
          txChunked(page, strlen(page));
        }
        if (event_callback_ptr != nullptr) {
          event_callback_ptr(event_doc);
        }
      }
      // TODO Check pending instead of sleep
      delay(30);  // bluetooth stack will go into congestion, if too many packets are sent
    }
  }
  if (deviceConnected && !prevDeviceConnected) {  // positive edge
    Serial.println("Device connected");
    //pServer->getAdvertising()->stop();
  }
  if (!deviceConnected && prevDeviceConnected) {  // negative edge
    Serial.println("Device disconnected");
    delay(500);                                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();                  // restart advertising
    Serial.println("start advertising");
  }
  prevDeviceConnected = deviceConnected;
  return deviceConnected;
}


void txChunked(const char *data, size_t size) {
  size_t offset = 0;
  char chunk[CHUNK_SIZE];
  while (offset < strlen(data)) {
    chunk[0] = '.';
    chunk[1] = '\0';
    chunk[CHUNK_SIZE - 1] = '\0';
    if (strlen(&data[offset]) < CHUNK_SIZE) {
      chunk[0] = '#';
    }
    strncpy(&chunk[1], &data[offset], CHUNK_SIZE - 1 - 1);
    String c = chunk;  // no mem alloc
    pTxCharacteristic->setValue(c);
    pTxCharacteristic->notify();
    offset += strlen(chunk) - 1;
    delay(100);  // avoid congestions // TODO handshake "X"
  }
}


void SystemStatus() {
  /*
  TaskStatus_t *pxTaskStatusArray;
  volatile UBaseType_t uxArraySize, x;
  uint32_t ulTotalRunTime;

  // Get the number of tasks
  uxArraySize = uxTaskGetNumberOfTasks();

  // Allocate memory for the task status array
  pxTaskStatusArray = (TaskStatus_t *)pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

  if (pxTaskStatusArray != NULL) {
    // Get the current task state
    uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

    // Print the task information
    Serial.println("----------------------------------------------");
    Serial.println("Name\t\tState\tPrio\tStack\t\tNum");
    Serial.println("----------------------------------------------");
    for (x = 0; x < uxArraySize; x++) {
      Serial.print(pxTaskStatusArray[x].pcTaskName);
      Serial.print("\t\t");
      Serial.print(pxTaskStatusArray[x].eCurrentState);
      Serial.print("\t");
      Serial.print(pxTaskStatusArray[x].uxBasePriority);
      Serial.print("\t");
      Serial.print(pxTaskStatusArray[x].usStackHighWaterMark);
      Serial.print("\t\t");
      Serial.println(pxTaskStatusArray[x].xTaskNumber);
    }
    Serial.println("----------------------------------------------");

    // Free the allocated memory
    vPortFree(pxTaskStatusArray);
  }
  */
  Serial.println("\nMemory map:");
  heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);
  Serial.println("Arduino Loop Stack:");
  Serial.println(uxTaskGetStackHighWaterMark(xTaskGetCurrentTaskHandle()));
}


//   _____                                   _                    _ _ _
//  |_   _|__ _ __ ___  _ __   ___ _ __ __ _| |_ _   _ _ __ ___  | (_) |__
//    | |/ _ \ '_ ` _ \| '_ \ / _ \ '__/ _` | __| | | | '__/ _ \ | | | '_ \ 
//    | |  __/ | | | | | |_) |  __/ | | (_| | |_| |_| | | |  __/ | | | |_) |
//    |_|\___|_| |_| |_| .__/ \___|_|  \__,_|\__|\__,_|_|  \___| |_|_|_.__/
//                     |_|

const char *twenty_four = R"RAW_STRING(
    <svg version="1.1" 
        xmlns="http://www.w3.org/2000/svg" 
        width="700" height="400">
      <style>
          .line {
              stroke: black;
              fill: none;
              stroke-width: 2;
          }
          .axis {
              stroke: gray;
              fill: none;            
              stroke-width: 1;
          }
      </style>
      <rect width="700" height="400" style="fill:white;"/>
      <!-- Axis -->
      <polygon points="50,50 50,350 650,350 650,50" class="axis" />
      <line x1="50" y1="125" x2="650" y2="125" class="axis" />
      <line x1="50" y1="200" x2="650" y2="200" class="axis" />
      <line x1="50" y1="275" x2="650" y2="275" class="axis" />

      <line x1="200" y1="50" x2="200" y2="350" class="axis" />
      <line x1="350" y1="50" x2="350" y2="350" class="axis" />
      <line x1="500" y1="50" x2="500" y2="350" class="axis" />
      
      <!-- Data Line 1400 bytes-->
      <path id="dataLine24hours_1" d=
"LINE_TAG__X1________X2________X3________X4________X5________X6________X7________X8________X9________
X100________________________________________________________________________________________________
X200________________________________________________________________________________________________
X300________________________________________________________________________________________________
X400________________________________________________________________________________________________
X500________________________________________________________________________________________________
X600________________________________________________________________________________________________
X700________________________________________________________________________________________________
X800________________________________________________________________________________________________
X900________________________________________________________________________________________________
X000________________________________________________________________________________________________
X100________________________________________________________________________________________________
X200________________________________________________________________________________________________
X300____________________________________________________________________________________LINE_TAG_END" class="line" />
      <!-- Annotation -->
      <text text-anchor="end" x="40" y="355">-20</text>
      <text text-anchor="end" x="40" y="280">0</text>
      <text text-anchor="end" x="40" y="205">20</text>
      <text text-anchor="end" x="40" y="130">40</text>
      <text text-anchor="end" x="40" y="55">60</text>                            
      <text x="45" y="375">0</text>
      <text x="195" y="375">6</text>
      <text x="345" y="375">12</text>
      <text x="495" y="375">18</text>
      <text x="655" y="375">0</text>
    </svg>)RAW_STRING";


constexpr int HISTORY_TIME_STEP = 600;  // 10 minutes
constexpr int TEMPERATURE_BUFFER_LEN = (3600 * 24 / HISTORY_TIME_STEP);

constexpr float X_OFFSET = 50;
constexpr float Y_OFFSET = 275;
constexpr int X_MAX = 600;
constexpr float X_SCALE = (float)X_MAX / (24. * 3600);  // px / s
constexpr float Y_SCALE = 225. / 60.;                   // px / °C

int round_int(float v) {
  return v + 0.5;
}

/**
Generate SVG path based on X_OFFSET, Y_OFFSET, X_SCALE and Y_SCALE
*/
void generateHistoryLine(const std::array<int16_t, TEMPERATURE_BUFFER_LEN> &history_pad, const int16_t now, size_t pad_length, char *path_string, size_t length, int since_midnight) {
  // start
  int steps = (since_midnight / HISTORY_TIME_STEP) + 1;  // ceil()
  float x = X_OFFSET + X_SCALE * steps * HISTORY_TIME_STEP;
  float y = Y_OFFSET - Y_SCALE * now / 10;
  int offset = 0;
  offset += snprintf(path_string, length, "M%d %d", round_int(x), round_int(y));  // start
  for (int i = 0; i < pad_length; i++) {
    if (steps - i >= 0) {  // till midnight
      x = X_OFFSET + X_SCALE * (steps - i) * HISTORY_TIME_STEP;
    } else {
      x = X_OFFSET + X_MAX + X_SCALE * (steps - i) * HISTORY_TIME_STEP;
    }
    y = Y_OFFSET - Y_SCALE * history_pad.at(i) / 10;
    offset += snprintf(&path_string[offset], length - offset, " L%d %d", round_int(x), round_int(y));

    if (steps == i) {  // roll over
      x = X_OFFSET + X_MAX;
      offset += snprintf(&path_string[offset], length - offset, " M%d %d", round_int(x), round_int(y));
    }
  }
}

//template<>
//void generateHistoryLine24Hours<HISTORY_TIME_STEP>(const std::array<int16_t, TEMPERATURE_BUFFER_LEN> &history_pad, const int16_t now, size_t pad_length, char *path_string, size_t length, int since_midnight);

class HistoryBuffer {
public:
  HistoryBuffer()
    : buffer({}), full(false), offset(0) {}
  void push(int16_t value) {
    buffer.at(offset) = value;
    offset++;
    if (offset >= TEMPERATURE_BUFFER_LEN) {
      offset = 0;
      full = true;  // first roll over == full
    }
  }
  int peekPad(std::array<int16_t, TEMPERATURE_BUFFER_LEN> &history_pad) {
    for (int i = 0; i < TEMPERATURE_BUFFER_LEN; i++) {
      int read_offset = offset - 1 - i;
      if (read_offset < 0 && full == true) {  // rollover
        read_offset = TEMPERATURE_BUFFER_LEN + read_offset;
      } else {
        if (read_offset < 0 && full == false) {  // no rollover when not full
          return i;                              // -1 + 1 // doubled off-by-one issue
        }
      }
      history_pad.at(i) = buffer.at(read_offset);
    }
    return TEMPERATURE_BUFFER_LEN;
  }
private:
  bool full;
  unsigned int offset;
  std::array<int16_t, TEMPERATURE_BUFFER_LEN> buffer;
};



//   __  __       _         _ _ _
//  |  \/  | __ _(_)_ __   | (_) |__
//  | |\/| |/ _` | | '_ \  | | | '_ \ 
//  | |  | | (_| | | | | | | | | |_) |
//  |_|  |_|\__,_|_|_| |_| |_|_|_.__/
//

#include <base64.h>
#include <time.h>


HistoryBuffer temperatureHistory;


const char *app_page = R"RAW_STRING({ "es-version" : "0.0",
  "command" : "page",
  "payload" : {
    "type" : "layout",
    "properties" : {
      "id" : "layout",
      "flow" : "row"
    },
    "layout" : [
      {
        "type" : "number",
        "properties" : {
          "label": "Temperature in °C",
          "id" : "temperature",
          "step" : ".1",
          "value" : "-99.9",
          "update" : "false",
          "readonly" : "true"
        }
      },
      {
        "type" : "image",
        "properties" : {
          "id" : "twenty_four",
          "src" : "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI3MDAiIGhlaWdodD0iNDAwIj48cmVjdCB3aWR0aD0iNzAwIiBoZWlnaHQ9IjQwMCIgc3R5bGU9ImZpbGw6d2hpdGU7Ii8+PC9zdmc+"
        }
      },
      {
        "type" : "time",
        "properties" : {
          "id" : "time",
          "label" : "Logger time",
          "value" : ""
        }
      }
    ]
  }
})RAW_STRING";



void updateIndicator(float temperatureC) {
  const char SET_VALUE[] = R"RAW(#{ "command": "set_value", "payload": { "id" : "temperature", "value" : "%.1f" } })RAW";
  char set_value[sizeof(SET_VALUE) + sizeof("+100.0")];
  snprintf(set_value, sizeof(set_value), SET_VALUE, temperatureC);
  Serial.println(set_value);
  pTxCharacteristic->setValue(String(set_value));
  pTxCharacteristic->notify();
  delay(50);  // avoid congestions // TODO handshake "X"
}

void updateTime() {
  struct timeval tv;
  struct tm *timeinfo;
  gettimeofday(&tv, NULL);
  //  time_t now;
  // time(&now);

  timeinfo = gmtime(&tv.tv_sec);

  char set_value[] = R"RAW(#{ "command": "set_value", "payload": { "id" : "time", "value" : "00:00" } })RAW";
  char value[] = "00:00";
  strftime(value, sizeof(value), "%H:%M", timeinfo);
  strncpy(&set_value[66], value, sizeof(value) - 1);  // get around zero termination
  pTxCharacteristic->setValue(String(set_value));
  pTxCharacteristic->notify();
  delay(50);  // avoid congestions // TODO handshake "X"
}



void setTimeFromString(const char *timeString) {
  // Parse the time string
  if (strnlen(timeString, 6) != 5) {
    return;
  }
  int hours = (timeString[0] - '0') * 10 + (timeString[1] - '0');
  int minutes = (timeString[3] - '0') * 10 + (timeString[4] - '0');

  // Set the time
  struct tm timeinfo = { 0 };
  timeinfo.tm_hour = hours;
  timeinfo.tm_min = minutes;
  timeinfo.tm_sec = 0;
  time_t updatedTime = mktime(&timeinfo) - 8 * 3600;
  struct timeval tv = { .tv_sec = updatedTime, .tv_usec = 0 };
  settimeofday(&tv, NULL);
}


void updateImage(float temperatureC) {
  constexpr size_t IMAGE_BUFFER = 5000u;
  char SET_IMAGE[IMAGE_BUFFER] = R"RAW({ "command": "set_image", "payload": { "id" : "twenty_four", "src" : "data:image/svg+xml;base64,)RAW";
  SET_IMAGE[IMAGE_BUFFER - 1] = '\0';
  const char *tail = "\" } }";
  String tf(twenty_four);
  constexpr size_t LINE_BUFFER = 1400u;
  constexpr char *LINE_TAG = "LINE_TAG";
  constexpr char *LINE_TAG_END = "LINE_TAG_END";
  auto start = tf.indexOf(LINE_TAG);
  auto end = tf.indexOf(LINE_TAG_END) + strlen(LINE_TAG_END) - 1;
  for (auto i = start; i <= end; i++) {
    tf[i] = ' ';
  }
  auto len = end - start + 1;

  std::array<int16_t, TEMPERATURE_BUFFER_LEN> history_pad = {};
  char test_pad[1400] = { '\0' };
  auto pad_len = temperatureHistory.peekPad(history_pad);

  struct timeval tv;
  struct tm *tm_now;
  gettimeofday(&tv, NULL);
  tm_now = gmtime(&tv.tv_sec);
  int seconds_since_midnight = tm_now->tm_hour * 3600 + tm_now->tm_min * 60;
  generateHistoryLine(history_pad, round_int(temperatureC * 10), pad_len, test_pad, sizeof(test_pad) - 1, seconds_since_midnight);
  for (auto i = 0; i < strlen(test_pad); i++) {
    tf[start + i] = test_pad[i];
  }


  auto base64 = base64::encode(tf);
  strncat(&SET_IMAGE[strlen(SET_IMAGE)], base64.c_str(), IMAGE_BUFFER - strlen(SET_IMAGE) - 1);
  strncat(&SET_IMAGE[strlen(SET_IMAGE)], tail, IMAGE_BUFFER - strlen(SET_IMAGE) - 1);
  SET_IMAGE[IMAGE_BUFFER - 1] = '\0';
  txChunked(SET_IMAGE, strlen(SET_IMAGE));
}


void eventCallback(JsonDocument &event_doc, float temperatureC) {
  const char *event = event_doc[EVENT];
  if (strncmp(event, EVENT_CLICK, strlen(EVENT_CLICK)) == 0) {
    updateTime();
    updateImage(temperatureC);
  } else if (strncmp(event, EVENT_CLICK, strlen(EVENT_CLICK)) == 0) {
  } else if (strncmp(event, EVENT_INPUT, strlen(EVENT_INPUT)) == 0) {
    const char *id = event_doc[PAYLOAD][ID];
    const char *TIME_ID = "time";
    if (strncmp(id, TIME_ID, strlen(TIME_ID)) == 0) {
      setTimeFromString(event_doc[PAYLOAD][VALUE]);
    }
  }
}


//  __  __       _            _
// |  \/  | __ _(_)_ __      / \   _ __  _ __
// | |\/| |/ _` | | '_ \    / _ \ | '_ \| '_ \ 
// | |  | | (_| | | | | |  / ___ \| |_) | |_) |
// |_|  |_|\__,_|_|_| |_| /_/   \_\ .__/| .__/
//                                |_|   |_|

// https://www.asciiart.eu/text-to-ascii-art

#include <OneWire.h>
#include <DallasTemperature.h>


constexpr unsigned long updateInterval = 10 * 1000;
unsigned long previousUpdateMillis = 0;
constexpr unsigned long historyInterval = HISTORY_TIME_STEP * 1000;
unsigned long previousHistoryMillis = 0;

constexpr uint8_t SENSOR_PIN = 25;  // ESP32 pin GPIO17 connected to DS18B20 sensor's DATA pin
OneWire oneWire(SENSOR_PIN);
DallasTemperature DS18B20(&oneWire);


float temperatureC = 0.;

void eventCallbackTemperature(JsonDocument &event_doc) {
  eventCallback(event_doc, temperatureC);
}

void setup() {
  Serial.begin(115200);
  Serial.printf("Arduino Stack was set to %d bytes\n", getArduinoLoopTaskStackSize());
  Serial.print("ESP Arduino Core Version: ");
  Serial.println(ESP.getSdkVersion());
  Serial.printf(" ESP32 Arduino core version: %s\n", ESP_ARDUINO_VERSION_STR);

  event_callback_ptr = eventCallbackTemperature;
  page = app_page;
  everySmartSetup();

  struct tm timeinfo = { 0 };
  timeinfo.tm_hour = 10;  // Hour (24-hour format)
  timeinfo.tm_min = 30;   // Minute
  timeinfo.tm_sec = 0;    // Second
  time_t updatedTime = mktime(&timeinfo);
  struct timeval tv = { .tv_sec = updatedTime, .tv_usec = 0 };
  settimeofday(&tv, NULL);

  DS18B20.begin();  // initialize the DS18B20 sensor
}

void loop() {
  DS18B20.requestTemperatures();              // send the command to get temperatures
  temperatureC = DS18B20.getTempCByIndex(0);  // read temperature in °C

  bool update = false;

  unsigned long currentMillis = millis();
  if (currentMillis - previousUpdateMillis >= updateInterval) {  // Check if the logging interval has passed
    previousUpdateMillis = currentMillis;
    update = true;
  }
  if (currentMillis - previousHistoryMillis >= historyInterval) {  // Check if the logging interval has passed
    previousHistoryMillis = currentMillis;
    temperatureHistory.push(round_int(temperatureC * 10));
    // TODO SystemStatus();
  }


  auto connected = everySmartLoop();

  if (connected && update) {
    updateTime();
    updateImage(temperatureC);
    updateIndicator(temperatureC);
  }
}
