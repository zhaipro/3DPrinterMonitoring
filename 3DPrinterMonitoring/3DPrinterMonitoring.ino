
// C:\Users\Administrator\AppData\Local\Temp\arduino\sketches\E0A428C3B51C3B88F2AD5E747F18025B\
// C:\Users\Administrator\AppData\Local\Arduino15\libraries
// C:\Users\Administrator\AppData\Local\Arduino15\packages
// C:\Users\Administrator\Documents\Arduino\libraries
// https://diandeng.tech/doc/arduino-support
// https://randomnerdtutorials.com/esp32-servo-motor-web-server-arduino-ide/
// https://gh.api.99988866.xyz/
// https://help.aliyun.com/zh/oss/developer-reference/api-reference
// 
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_camera.h>
#include <ESPDateTime.h>
#include <ESP32Servo.h>
#define BLINKER_PRINT Serial
#define BLINKER_WIFI      // 使用wifi连接点灯科技
#define BLINKER_ESP_TASK  // 将blinker放入单独任务中
#include <Blinker.h>

#include "Base64.h"
#include "sha1.h"
#include "hmac.h"
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid = "";              // wifi
const char* password = "";
const char *AccessKeySecret = "";   // 阿里云OSS
const char auth[] = "";             // 点灯科技

// 舵机
Servo myservo;
Servo cam_angle_servo;  // 用于控制摄像头方向
// 负责按开关的舵机角度
static int s_servo_angle = 45;
static int s_servo_duration = 302;
// 是否开启闪光灯
static int s_flashlisht_status = 0;
// 是否有拍摄任务
static bool s_cam = false;

// 新建组件对象
BlinkerButton ButtonCam("btn-cam");
BlinkerButton ButtonFlashlight("btn-flashlight");
BlinkerButton ButtonFrameSize("btn-frame_size");
BlinkerButton ButtonSwitch("btn-switch");    // 打印机电源开关
BlinkerSlider SliderServoDuration("ran-servo-duration");    // 点击按钮的时长
BlinkerSlider SliderServoAngle("ran-servo-angle");
BlinkerSlider SliderCamAngle("ran-cam-angle");

// 按下按键即会执行该函数
void button_cam_callback(const String & state)
{
  s_cam = true;
}

void button_flashlight_callback(const String & state)
{
  if (state == "on") {
    s_flashlisht_status = 1;
    ButtonFlashlight.print("on");
  } else {
    s_flashlisht_status = 0;
    ButtonFlashlight.print("off");
  }
}

void button_frame_size_callback(const String &state) {
  sensor_t * s = esp_camera_sensor_get();
  if (state == "on") {
    // drop down frame size for higher initial frame rate
    s->set_framesize(s, FRAMESIZE_UXGA);
    ButtonFrameSize.print("on");
  } else {
    s->set_framesize(s, FRAMESIZE_240X240);
    ButtonFrameSize.print("off");
  }
}

void switch_callback(const String &state) {
  myservo.write(s_servo_angle);
  Blinker.delay(s_servo_duration);
  myservo.write(180);
  Blinker.print("switch ok!");
}

void _switch_callback() {
  myservo.write(s_servo_angle);
  delay(s_servo_duration);
  myservo.write(180);
}

void slider_cam_angle_callback(int32_t value) {
  cam_angle_servo.write(value);
  Blinker.print("cam_angle_servo ok!");
}

// 设置舵机力度值
void slider_servo_angle_callback(int32_t value) {
  s_servo_angle = value;
}

// 设置舵机抬起的延迟时间
void slider_servo_duration_callback(int32_t value) {
  s_servo_duration = value;
}

// 如果未绑定的组件被触发，则会执行其中内容
void dataRead(const String & data)
{
  BLINKER_LOG("Blinker readString: ", data);
}

void heartbeat()
{
  if (s_flashlisht_status) {
    ButtonFlashlight.print("on");
  } else {
    ButtonFlashlight.print("off");
  }
  SliderServoAngle.print(s_servo_angle);
  SliderServoDuration.print(s_servo_duration);
}

void setup() {
  Serial.begin(115200);
  // 相机
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

    // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(config.pixel_format == PIXFORMAT_JPEG){
    //config.frame_size = FRAMESIZE_240X240;
    if(psramFound()){
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
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

  pinMode(4, OUTPUT);   // 闪光灯
  myservo.attach(12);
  cam_angle_servo.attach(13);
  pinMode(2, INPUT_PULLUP);   // 接收打印机任务完成的通知

  // 初始化blinker
  BLINKER_DEBUG.stream(Serial);
  // BLINKER_DEBUG.debugAll();
  Blinker.begin(auth, ssid, password);
  BLINKER_TAST_INIT();
  Blinker.attachData(dataRead);
  ButtonCam.attach(button_cam_callback);
  ButtonFlashlight.attach(button_flashlight_callback);
  ButtonFrameSize.attach(button_frame_size_callback);
  SliderServoAngle.attach(slider_servo_angle_callback);
  ButtonSwitch.attach(switch_callback);
  SliderServoDuration.attach(slider_servo_duration_callback);
  Blinker.attachHeartbeat(heartbeat);
  SliderCamAngle.attach(slider_cam_angle_callback);
}

void put_image_jpeg() {
  if (s_flashlisht_status) {
    digitalWrite(4, 1);
    delay(1000);
  }
  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  HTTPClient http;
  http.setTimeout(14 * 1000);
  http.begin("http://yifuda.oss-cn-beijing.aliyuncs.com/cam.jpg");
  // Sun, 22 Oct 2023 07:58:19 GMT
  String now = DateTime.toUTCString();
  http.addHeader("date", now.c_str());
  http.addHeader("Cache-control", "no-store");
  char string_to_sign[128];
  sprintf(string_to_sign, "PUT\n\nimage/jpeg\n%s\n/yifuda/cam.jpg", now.c_str());

  uint8_t authorization[44 + 22];
  sprintf((char*)authorization, "OSS i4vFIA76wGSWlKhE:");
  uint8_t Message_Digest[SHA1HashSize];
  hmac_sha1((const uint8_t*)AccessKeySecret, strlen(AccessKeySecret), (uint8_t*)string_to_sign, strlen(string_to_sign), Message_Digest);

  int encodedLength = Base64.encodedLength(SHA1HashSize);
  Base64.encode((char*)authorization + 21, (char*)Message_Digest, SHA1HashSize);
  authorization[encodedLength + 22] = 0;
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("authorization", (const char*)authorization);
  
  // http.addHeader(name,value)
  int httpCode = http.PUT(fb->buf, fb->len);
  //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  //如果服务器不响应OK则将服务器响应状态码通过串口输出
  if (httpCode == HTTP_CODE_OK) {
    String responsePayload = http.getString();
    Blinker.print("cam ok!");
  } else {
    Blinker.print("cam error!");
  }
  http.end();
  esp_camera_fb_return(fb);

  digitalWrite(4, 0);
}

static s_timed_shutdown = -2;

void loop() {
  delay(1000);
  if (s_cam) {
    put_image_jpeg();
    s_cam = false;
  }
  if (s_timed_shutdown > 0) {           // 等待执行命令
    s_timed_shutdown --;
  } else if (s_timed_shutdown == 0) {   // 执行
    _switch_callback();
    s_timed_shutdown = -1;
  } else if (s_timed_shutdown == -2) {  // 等待命令
    if (digitalRead(2) == LOW) {
      s_timed_shutdown = 90;            // 等待90秒
    }
  }
}
