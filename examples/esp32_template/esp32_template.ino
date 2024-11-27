
SET_LOOP_TASK_STACK_SIZE(16 * 1024);  // 16KB


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


constexpr char *SERVICE_UUID = "6E400001-B5A3-F393-E0A9-111C0FFEE111";  // UART service UUID
constexpr char *CHARACTERISTIC_UUID_RX = "6E400002-B5A3-F393-E0A9-111C0FFEE111";
constexpr char *CHARACTERISTIC_UUID_TX = "6E400003-B5A3-F393-E0A9-111C0FFEE111";



BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool prevDeviceConnected = false;

constexpr uint32_t PAD_SIZE = 1000u;
constexpr uint32_t QUEUE_LEN = 5u;
constexpr uint32_t CHUNK_SIZE = 200u;
QueueHandle_t ringBuffer;
char receivePad[PAD_SIZE];

constexpr char *EVENT = "event";
constexpr char *EVENT_READY = "ready";
constexpr char *EVENT_CLICK = "click"; // Legacy / TODO Pointer down/up events
constexpr char *EVENT_INPUT = "input";
constexpr char *PAYLOAD = "payload";
constexpr char *ID = "id";
constexpr char *VALUE = "value";
constexpr char *CLICKED = "clicked";
constexpr char *PRESSED = "pressed";
constexpr char *RELEASED = "released";

constexpr auto TEXT_MAX_LENGTH = 400u;


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
        JsonDocument event_doc;
        DeserializationError error = deserializeJson(event_doc, rx);
        if (error) {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.f_str());
          return deviceConnected;
        }

        const char *event = event_doc[EVENT];
        if (strncmp(event, EVENT_READY, strlen(EVENT_READY)) == 0) {
          txChunked(page, strlen(page));
        }
        if (event_callback_ptr != nullptr) {
          event_callback_ptr(event_doc);
        }
      }
      // TODO Check pending instead of sleep
      delay(30);
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
    delay(100);
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




//   __  __       _         _ _ _
//  |  \/  | __ _(_)_ __   | (_) |__
//  | |\/| |/ _` | | '_ \  | | | '_ \ 
//  | |  | | (_| | | | | | | | | |_) |
//  |_|  |_|\__,_|_|_| |_| |_|_|_.__/
//

#include <base64.h>
#include <time.h>


const char *app_page = R"RAW_STRING({ "es-version" : "0.0",
  "command" : "page",
  "payload" : {
    "title" : "App Template",
    "type" : "layout",
    "properties" : {
      "id" : "layout_rows",
      "flow" : "row"
    },
    "layout" : [
      {
        "type" : "button",
        "properties" : {
          "label" : "Toggle LED",
          "id" : "button_clicked"
        }
      },
      {
        "type" : "text",
        "properties" : {
          "label": "LED status",
          "id" : "status",
          "value" : "undefined"
        }
      }
    ]
  }
})RAW_STRING";

constexpr auto BUTTON_CLICKED = "button_clicked";
constexpr auto LED_ON = "LED on";
constexpr auto LED_OFF = "LED off";
bool led_on = false;

void eventCallback(JsonDocument& event_doc);

void eventCallback(JsonDocument& event_doc) {
  const char *event = event_doc[EVENT];
  Serial.println(event);
  if (strncmp(event, EVENT_INPUT, strlen(EVENT_INPUT)) == 0) {
    const char* id = event_doc[PAYLOAD][ID];
    if (strncmp(id, BUTTON_CLICKED, strlen(BUTTON_CLICKED)) == 0) {
      const char* value = event_doc[PAYLOAD][VALUE];
      const char SET_VALUE[] = R"RAW(#{ "command": "set_value", "payload": { "id" : "status", "value" : "%s" } })RAW";
      char set_value[sizeof(SET_VALUE) + strlen(LED_OFF)];
      if(led_on != true) {
        led_on = true;
        snprintf(set_value, sizeof(set_value), SET_VALUE, LED_ON);
      } else {
        led_on = false;
        snprintf(set_value, sizeof(set_value), SET_VALUE, LED_OFF);
      }
      Serial.println(set_value);
      pTxCharacteristic->setValue(String(set_value));
      pTxCharacteristic->notify();
      delay(50);
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


constexpr unsigned long updateInterval = 10 * 1000;
unsigned long previousUpdateMillis = 0;

void setup() {
  Serial.begin(115200);
  Serial.printf("Arduino Stack was set to %d bytes\n", getArduinoLoopTaskStackSize());
  Serial.print("ESP Arduino Core Version: ");
  Serial.println(ESP.getSdkVersion());
  Serial.printf(" ESP32 Arduino core version: %s\n", ESP_ARDUINO_VERSION_STR);

  event_callback_ptr = eventCallback;
  page = app_page;
  everySmartSetup();

}

void loop() {
  bool update = false;

  unsigned long currentMillis = millis();
  if (currentMillis - previousUpdateMillis >= updateInterval) {  // Check if the logging interval has passed
    previousUpdateMillis = currentMillis;
    update = true;
  }


  auto connected = everySmartLoop();

  if (connected && update) {
    Serial.println(led_on ? LED_ON : LED_OFF);
  }
}
