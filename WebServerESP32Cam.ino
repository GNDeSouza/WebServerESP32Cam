/* 
 *  Copyright (C) 2019
 *  Created by G. N. DeSouza <Gui@DeSouzas.name> based on Rui Santos'
 *       original code (https://RandomNerdTutorials.com/) to fix some 
 *       WiFi connection issues and to add SSID/PSWD/IP configurations
 *       thru WiFi-AP
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation. Meaning:
 *          keep this copyright notice,
 *          do  not try to make money out of it,
 *          it's distributed WITHOUT ANY WARRANTY,
 *          yada yada yada...
 *
 *  You can get a copy of the GNU General Public License by writing to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1335 USA or at https://www.gnu.org/licenses/gpl-3.0.html
 *
 *  IMPORTANT!!! 
   - Select Board "ESP32 Wrover Module"
   - Select the Partion Scheme "Huge APP (3MB No OTA)
   - GPIO 0 must be connected to GND to upload a sketch
   - After connecting GPIO 0 to GND, press the ESP32-CAM on-board  
           RESET button to put your board in flashing mode
 */

#include "esp_camera.h"
#include <EEPROM.h> 
#include <WiFi.h>
#include <AsyncTCP.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h" //disable brownout problems
#include "soc/rtc_cntl_reg.h"  //disable brownout problems
#include "esp_http_server.h"

#include "ConfigDevice_AP.h"

long now = millis();
long button_count=0;
boolean correct_password = false;

#define PART_BOUNDARY "123456789000000000000987654321"

// This project was tested with the AI Thinker Model, M5STACK PSRAM Model and M5STACK WITHOUT PSRAM
#define CAMERA_MODEL_AI_THINKER
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WITHOUT_PSRAM

// Not tested with this model
//#define CAMERA_MODEL_WROVER_KIT

#include "camera_pins.h"

#define FORCE_AP                        false
#define SERIAL_BAUDRATE                 115200
#define BUTTON                          2
#define LED                             4

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
   <title>Enter ViGIR-Cam Password (GND)</title>
   <meta name="viewport" content="width=device-width, initial-scale=2">
   </head><body>
   <h2> ViGIR-Cam (by GND)<h2>
   <h4>Enter your password</h4>
   <form action="/action_page" method="GET">
   Device_Passwd:<br>
   <input type="text" name="devc_pswd" value="Password">
   <br><br>
   <input type="submit" value="Submit">
   </form>
</body></html>)rawliteral";


//===============================================================
// This routine is executed when you open the ViGIR-Cam in browser
//===============================================================
// Send web page with password prompt to client

static esp_err_t root_handler(httpd_req_t *req){
   correct_password = false;
   httpd_resp_send(req, index_html, strlen(index_html));
   return ESP_OK; }


//===============================================================
// This routine is executed when you submit password
//===============================================================
// form action is handled here ("/action_page")

static esp_err_t password_handler(httpd_req_t *req){
   esp_err_t res = ESP_OK;
   int buf_len; char buf[2000]; char dpwd_srv[200] = "";

   buf_len = httpd_req_get_url_query_len(req) + 1;
   if (buf_len > 1) {
      //Serial.println("Got here 1!");
      if ((res = httpd_req_get_url_query_str(req, buf, buf_len)) == ESP_OK) {
         //Serial.println("Got here 2!");
         if ((res = httpd_query_key_value(buf, "devc_pswd", dpwd_srv, EE_STR)) == ESP_OK) {
            //Serial.printf("Got here 3! buf = %s\n", buf);
            Serial.printf("Found URL query parameter => devc_pswd = %s saved_dpwd = %s  \n", dpwd_srv, saved_dpwd);
            //Serial.println("Got here 4!");
            //delay(100);
            if (String(dpwd_srv) != String(saved_dpwd)) {
               String s = "<h2> Wrong Password!!! <br><br> <a href='/'> Go Back </a> </h2>"; //Send web page response
               Serial.println("Wrong password!!");
               httpd_resp_send(req, s.c_str(), s.length()+1);
               correct_password = false ;
               return ESP_OK; } } } }
   else {
         String s = "<h2> Something wrong/missing Password!!! <br><br> <a href='/'> Go Back </a> </h2>"; //Send web page response
         httpd_resp_send(req, s.c_str(), s.length()+1);
         correct_password = false ;
         return ESP_FAIL; }

   if (res != ESP_OK) {
         String s = "<h2> Something wrong/missing Password!!! <br><br> <a href='/'> Go Back </a> </h2>"; //Send web page response
         httpd_resp_send(req, s.c_str(), s.length()+1);
         correct_password = false; 
         return res; }

   Serial.println("Correct password!!");
   correct_password = true; 
   String s = "<h2> All good!!! <br><br> <a href='/stream'> Ready to stream </a> </h2>"; //Send web page response
   httpd_resp_send(req, s.c_str(), s.length()+1);
   return res; }


//===============================================================
// This routine is handles the video streamming
//===============================================================

static esp_err_t stream_handler(httpd_req_t *req){
   camera_fb_t * fb = NULL;
   esp_err_t res = ESP_OK;
   size_t _jpg_buf_len = 0;
   uint8_t * _jpg_buf = NULL;
   char * part_buf[64];

   if (correct_password != true) {
      return ESP_FAIL; }

   res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
   if(res != ESP_OK) {
      return res; }

   while(true) {
      fb = esp_camera_fb_get();
      if (!fb) {
         Serial.println("Camera capture failed");
         res = ESP_FAIL; }
      else {
         if(fb->width > 400) {
            if(fb->format != PIXFORMAT_JPEG){
               bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
               esp_camera_fb_return(fb);
               fb = NULL;
               if(!jpeg_converted) {
                  Serial.println("JPEG compression failed");
                  res = ESP_FAIL; } }
               else {
                  _jpg_buf_len = fb->len;
                  _jpg_buf = fb->buf; } } }
      if(res == ESP_OK) {
         size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
         res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen); }
      if(res == ESP_OK) {
         res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len); }
      if(res == ESP_OK) {
         res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY)); }
      if(fb) {
         esp_camera_fb_return(fb);
         fb = NULL;
         _jpg_buf = NULL; }
      else if(_jpg_buf) {
         free(_jpg_buf);
         _jpg_buf = NULL; }
      if(res != ESP_OK){
         correct_password = false; 
         break; }
         //Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));
      }
   return res; }



void startCameraServer(){
   httpd_config_t config = HTTPD_DEFAULT_CONFIG();
   config.server_port = 80;

   httpd_uri_t index_uri = {
       .uri       = "/",
       .method    = HTTP_GET,
       .handler   = root_handler,
       .user_ctx  = NULL };

   httpd_uri_t password_uri = {
       .uri       = "/action_page",
       .method    = HTTP_GET,
       .handler   = password_handler,
       .user_ctx  = NULL };
  
   httpd_uri_t stream_uri = {
       .uri       = "/stream",
       .method    = HTTP_GET,
       .handler   = stream_handler,
       .user_ctx  = NULL };
  
   //Serial.printf("Starting web server on port: '%d'\n", config.server_port);
   if (httpd_start(&stream_httpd, &config) == ESP_OK) {
      httpd_register_uri_handler(stream_httpd, &index_uri);
      httpd_register_uri_handler(stream_httpd, &password_uri);
      httpd_register_uri_handler(stream_httpd, &stream_uri); } }


// -----------------------------------------------------------------------------
// Wifi
// -----------------------------------------------------------------------------
IPAddress local_IP(10,14,1,207);
IPAddress gateway(10,14,1,1);
IPAddress subnet(255,255,255,0);

void wifiSetup() {
    // Set WIFI module to STA mode
    WiFi.mode(WIFI_STA);
    if ((saved_ip[0] == '\0') || (saved_ip[0] < '0') || (saved_ip[0] > 'z')) {
       local_IP = str2ip(saved_ip);
       gateway = str2ip(saved_gwy);
       subnet = str2ip(saved_msk); }

    WiFi.config(local_IP, gateway, subnet);

    // Connect
    Serial.printf("[WIFI] Connecting to %s ", saved_ssid);
    WiFi.begin(saved_ssid, saved_pswd);

    // save SSID/password for next reconnection
    // "false" = don't write to flash unless SSID/passord has changed
    //WiFi.persistent(false);

    // Wait
    int count=0;
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print("+");
        delay(100);
        if (count++ > 200) {
          Serial.println("\n\nRebooting...");
          ESP.restart();} }
        
    Serial.println();

    WiFi.setAutoReconnect(true);
    // Connected!
    Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str()); }


/*****************************************
 ****************   SETUP  ***************
 *****************************************/

void setup() {
   pinMode(LED, OUTPUT);
   digitalWrite(LED, LOW);
   pinMode(BUTTON, INPUT);
   delay(10000);   // wait for serial handler on Mac after power switch of device

   WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
 
   Serial.begin(SERIAL_BAUDRATE, SERIAL_8N1);//, SERIAL_TX_ONLY);
   Serial.setDebugOutput(false);
   Serial.println("Ready");
   Serial.println();

   EEPROM.begin(512);

   EEPROM.get(0, saved_ssid);
   EEPROM.get(1*EE_STR, saved_pswd);
   EEPROM.get(2*EE_STR, saved_dpwd);
   EEPROM.get(3*EE_STR, saved_ip);
   EEPROM.get(4*EE_STR, saved_gwy);
   EEPROM.get(5*EE_STR, saved_msk);
   EEPROM.get(6*EE_STR, saved_mqtt);

   if ((FORCE_AP == true) || (saved_ssid[0] == '\0') || (saved_ssid[0] < '0') || (saved_ssid[0] > 'z')) {
      Serial.println("Setup: AP Mode...");
      setupAP();
      return; }

   if ((saved_dpwd[0] == '\0') || (saved_dpwd[0] < '0') || (saved_dpwd[0] > 'z')) {
      String("2020").toCharArray(saved_dpwd, 5); }

   Serial.printf("Saved in EEPROM: %s %s %s %s %s %s %s\n",
          saved_ssid, saved_pswd, saved_dpwd, saved_ip, saved_gwy, saved_msk, saved_mqtt); 

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

   if(psramFound()){
      config.frame_size = FRAMESIZE_UXGA;
      config.jpeg_quality = 10;
      config.fb_count = 2; }
   else {
      config.frame_size = FRAMESIZE_SVGA;
      config.jpeg_quality = 12;
      config.fb_count = 1; }

   //  Force a low res image
   config.frame_size = FRAMESIZE_SVGA;
   config.jpeg_quality = 12;
   config.fb_count = 1;

   // Camera init
   esp_err_t err = esp_camera_init(&config);
   if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x", err);
      return; }

   // Wifi
   wifiSetup();
   Serial.println("");
   Serial.println("WiFi connected");
  
   // Start streaming web server
   startCameraServer();

   Serial.print("Camera Stream Ready! Go to: http://");
   Serial.print(WiFi.localIP());
   Serial.println(""); }



/*****************************************
 ****************   LOOP  ****************
 *****************************************/


void loop() {
    if ((saved_ssid[0] == '\0') || (saved_ssid[0] < '0') || (saved_ssid[0] > 'z') || (FORCE_AP == true)) {
       Serial.println("Loop: AP Mode...");
       loopAP();
       return; }

    if (digitalRead(BUTTON) == 0) {
       Serial.printf("Button Pressed: %d\n", button_count);
       button_count++;
       if (button_count > 600) digitalWrite(LED,(button_count/300) % 2); }
    else {
       //Serial.printf("Button Released: %d\n", button_count);
        button_count = 0; }

    if (button_count >= 5000) {
       for (int i=0; i<10; i++) {
          Serial.println("Preparing to reboot in AP mode");
          //digitalWrite(LED,0);
          delay(500);
          digitalWrite(LED,1);
          delay(500); }
       saved_ssid[0] = '\0';
       EEPROM.put(0, saved_ssid);
       EEPROM.commit();
       delay(500); 
       ESP.restart(); }

    now = millis();

    if (now >= 86400000) ESP.restart();   //  daily restart
    if (now >= 7200000) ESP.restart();   //  restart every 2h

    if (!WiFi.isConnected()) {
       if (WiFi.status() != WL_CONNECTED) {
          WiFi.reconnect();
          Serial.printf("[WIFI] Connecting to %s ", saved_ssid);
          Serial.print("."); }
       if (WiFi.isConnected()) {
          // Start streaming web server
          startCameraServer(); }
       else Serial.printf("WIFI is down"); }
}
