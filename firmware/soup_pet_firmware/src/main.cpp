#include <Arduino.h>

#include <AsyncTCP.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#include <TaskScheduler.h>

#include <ESP32Servo.h>

#include "esp_camera.h"
#include "camera_pins.h"


#define TOUCH_PIN 43
#define SERVO_X_PIN 6
// D10 GPIO9 broooooooooooooooooooooo
#define SERVO_Y_PIN 5
// D9 GPIO8


Scheduler runner;

Servo servoX;
Servo servoY;

int xPos = 0;
int yPos = 0;

int movementStep = 5;

bool isTouchingHead = false;
bool isTouchingHeadOld = isTouchingHead;

bool selfDestructActive = false;
bool selfDestructMoveLeft = true;
int selfDestructMovesRemaining = 0;
unsigned long selfDestructNextMoveMs = 0;

// idk find a better way to laod the html file
// https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring

extern const uint8_t html_start[] asm("_binary_src_index_html_start");
extern const uint8_t html_end[] asm("_binary_src_index_html_end");

//static const size_t htmlContentLength = strlen_P(htmlContent);

static AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

int testNum = 0;
void test() {
  Serial.print(testNum);
  Serial.print(") go. http://");
  Serial.println(WiFi.softAPIP());
  testNum = testNum + 1;
}
Task taskTest(10000, TASK_FOREVER, &test);

void moveX(int xunf) {
  int x = xunf;
  if (xunf > 120) {
    x = 120;
  } else if (xunf < 55) {
    x = 55;
  }

  servoX.write(x);
  String str = "servoX-" + String(x);
  xPos = x;
  ws.textAll(str);
  Serial.println(str);
}

void moveY(int yunf) {
  int y = yunf;
  if (yunf > 115) {
    y = 115;
  } else if (yunf < 55) {
    y = 55;
  }

  servoY.write(y);
  String str = "servoY-" + String(y);
  yPos = y;
  ws.textAll(str);
  Serial.println(str);
}

void startSelfDestruct() {
  selfDestructActive = true;
  selfDestructMoveLeft = true;
  selfDestructMovesRemaining = 30;
  selfDestructNextMoveMs = 0;
}

void runSelfDestruct() {
  if (!selfDestructActive || selfDestructMovesRemaining <= 0) {
    selfDestructActive = false;
    return;
  }

  unsigned long now = millis();
  if (selfDestructNextMoveMs != 0 && (long)(now - selfDestructNextMoveMs) < 0) {
    return;
  }

  moveX(selfDestructMoveLeft ? 0 : 180);
  selfDestructMoveLeft = !selfDestructMoveLeft;
  selfDestructMovesRemaining--;
  selfDestructNextMoveMs = now + 50;
}

void setup() {

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  setCpuFrequencyMhz(160);

  Serial.begin(115200);

  // setup pinout
  pinMode(TOUCH_PIN, INPUT);

  servoX.setPeriodHertz(50);
  servoX.attach(SERVO_X_PIN, 500, 2400);

  servoY.setPeriodHertz(50);
  servoY.attach(SERVO_Y_PIN, 500, 2400);

  // task scheduler
  runner.addTask(taskTest);
  taskTest.enable();

  camera_init();

  // setup wifi connection for local webpanel
  WiFi.mode(WIFI_AP);
  WiFi.softAP("souppp");

  WiFi.setTxPower(WIFI_POWER_11dBm);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    size_t length = html_end - html_start;
    request->send(200, "text/html", (uint8_t *)html_start, length);
  });

  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {

    if (type == WS_EVT_CONNECT) {
      Serial.println("ws connect");
    } else if (type == WS_EVT_DISCONNECT) {
      Serial.println("ws disconnect");
    } else if (type == WS_EVT_ERROR) {
      Serial.println("ws error");
    } else if (type == WS_EVT_DATA) {
      AwsFrameInfo *info = (AwsFrameInfo *)arg;
      //Serial.printf(
      //  "index: %" PRIu64 ", len: %" PRIu64 ", final: %" PRIu8 ", opcode: %" PRIu8 ", framelen: %d\n", info->index, info->len, info->final,
      //  info->message_opcode, len
      //);

      if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) {
        return;
      }

      String message;
      message.reserve(len);
      for (size_t i = 0; i < len; i++) {
        message += (char)data[i];
      }
      
      Serial.print("Received Message: ");
      Serial.println(message);

      if (message == "sdestruct") {
        startSelfDestruct();
      }
      if (message == "stepL") {
        moveX(xPos + movementStep);
      }
      if (message == "stepR") {
        moveX(xPos - movementStep);
      }
      if (message == "stepU") {
        moveY(yPos - movementStep);
      }
      if (message == "stepD") {
        moveY(yPos + movementStep);
      }
      if (message == "normal") {
        moveX(90);
        moveY(90);
      }
      if (message == "capture") {
        Serial.println(camera_capture());
      }
      if (message.startsWith("voice:")) {
        ws.textAll(message);
      }

      //todo handling messages
    }
  });


  server.addHandler(&ws);
  server.begin();
}

void loop() {
  runner.execute();
  ws.cleanupClients();
  runSelfDestruct();

  // sensor readings
  if (digitalRead(TOUCH_PIN) == HIGH) {
    isTouchingHead = true;
    moveX(75);
    delay(150);
    moveX(105);
    delay(150);
  } else {
    isTouchingHead = false;
  }
  
  if (isTouchingHead != isTouchingHeadOld) {
    //Serial.println("touch head change detected");
    if (isTouchingHead) {
      ws.textAll("hTouch-true");
      Serial.println("hTouch-true");
    } else {
      ws.textAll("hTouch-false");
      Serial.println("hTouch-false");
    }
    isTouchingHeadOld = isTouchingHead;
  }


}
