#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Stepper.h>

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM

#include "camera_pins.h"

#define LOCAL   0
#define REMOTE  1

const char* ssid = "**************";  //put WIFI SSID here
const char* password = "*********";   //put WIFI Password here
char auth[] = "***********";          //Auth Code sent by Blynk
String local_IP;

const int stepsPerRevolution = 2050;  // change this to fit the number of steps per revolution
// for your motor

#define STEPPER_PIN0 14
#define STEPPER_PIN1 15
#define STEPPER_PIN2 13
#define STEPPER_PIN3 12
#define LED 4
//#define ENABLE_FLASH
#define LIGHT_RELAY 2
//#define LIGHT_RELAY 4

//Blynk virtual IOS
#define LOCAL_VIDEO_STREAM    V4 
#define REMOTE_PHOTO_CAPTURE  V5
#define CAMERA                V1
#define STEPPER_MOTOR         V2
#define LIGHT_SWITCH          V3
// initialize the stepper library 
Stepper myStepper(stepsPerRevolution, STEPPER_PIN0, STEPPER_PIN1, STEPPER_PIN2, STEPPER_PIN3);

int stepCount = 0;         // number of steps the motor has taken

void startCameraServer();

void takePhoto(uint mode)
{
#ifdef ENABLE_FLASH
  digitalWrite(LED, HIGH);
  delay(300);
#endif
  if(mode == LOCAL) {
      uint32_t randomNum = random(50000);
      Serial.println("http://"+local_IP+"/capture?_cb="+ (String)randomNum);
      Blynk.setProperty(CAMERA, "urls", "http://"+local_IP+"/capture?_cb="+(String)randomNum); //ESP32 CAM 1
      delay(500);
  }
  if(mode == REMOTE) {
        WiFiClient client;
        const int Port = 8080;
        if (!client.connect("192.168.1.40", Port)) {
            Serial.println("connection failed");
            return;
        }


        // This will send the request to the server
        client.print("Connect");
        unsigned long timeout = millis();
        while (client.available() == 0) {
          if (millis() - timeout > 15000) {
              Serial.println(">>> Client Timeout !");
              client.stop();
              return;
          }
        }

        // Read all the lines of the reply from server and print them to Serial
        while(client.available()) {
            String image_url = client.readStringUntil('\r');
            Serial.print(image_url);
            Blynk.setProperty(CAMERA, "urls", image_url); //ESP32 CAM 1
        }
  }

#ifdef ENABLE_FLASH
  digitalWrite(LED, LOW);
  delay(200);
#endif
}


void setup() {
#ifdef ENABLE_FLASH
  pinMode(LED,OUTPUT);
#endif
  pinMode(LIGHT_RELAY,OUTPUT);


  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // it wil set the static IP address to 192, 168, 1, 47
  IPAddress IP(192, 168, 1, 100);
  //it wil set the gateway static IP address to 192, 168, 2,2
  IPAddress gateway(192, 168, 1 ,1);

  // Following three settings are optional
  IPAddress subnet(255, 255, 255, 0);
  IPAddress primaryDNS(8, 8, 8, 8); 
  IPAddress secondaryDNS(8, 8, 4, 4);

  // This part of code will try create static IP address
  if (!WiFi.config(IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  local_IP = WiFi.localIP().toString();
  Serial.println("' to connect");

  Blynk.begin(auth, ssid, password);
}

static uint streamVideo;

BLYNK_WRITE(LOCAL_VIDEO_STREAM) 
{
   Serial.println("VIDEO_STREM state change");
    if (param.asInt()) {
        //HIGH
        streamVideo = 1;
    } else {
       //LOW
       streamVideo = 0;
    }
}


static uint capturePhoto;
BLYNK_WRITE(REMOTE_PHOTO_CAPTURE) 
{
   Serial.println("REMOTE_PHOTO_CAPTURE state change");
    if (param.asInt()) {
        //HIGH
        capturePhoto = 1;
    } else {
       //LOW
       capturePhoto = 0;
    }
}

BLYNK_WRITE(LIGHT_SWITCH) 
{
   Serial.println("LIGHT_SWITCH state change");
    if (param.asInt()) {
        //HIGH
        digitalWrite(LIGHT_RELAY, HIGH);
    } else {
        //LOW
        digitalWrite(LIGHT_RELAY, LOW);
    }
}

void stepper_one_revolution() {
  for(int i=0; i<stepsPerRevolution; i++) {
    myStepper.step(1);
    delay(10);
  }
  digitalWrite(STEPPER_PIN0, LOW);
  digitalWrite(STEPPER_PIN1, LOW);
  digitalWrite(STEPPER_PIN2, LOW);
  digitalWrite(STEPPER_PIN3, LOW);
}

BLYNK_WRITE(STEPPER_MOTOR) 
{
  Serial.println("STEPPER_MOTOR state change");
  stepper_one_revolution();
}

void loop() {
   Blynk.run();
   if(streamVideo){
      takePhoto(LOCAL);
   }else if(capturePhoto) {
      takePhoto(REMOTE);
      capturePhoto = 0;
   }
}
