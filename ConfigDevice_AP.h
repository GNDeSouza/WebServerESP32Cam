/*
 *  Copyright (C) 2019 
 *  Created by G. N. DeSouza <Gui@DeSouzas.name>
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
 *  Boston, MA 02110-1335 USA or at
 *  https://www.gnu.org/licenses/gpl-3.0.html
 * *
 *
 */

#define     EE_STR     30

httpd_handle_t AP_httpd = NULL;

char saved_ssid[EE_STR] = "";
char saved_pswd[EE_STR] = "";
char saved_dpwd[EE_STR] = "";
char saved_ip[EE_STR] = "";
char saved_gwy[EE_STR] = "";
char saved_msk[EE_STR] = "";
char saved_mqtt[EE_STR] = "";

const char* ssid     = "GND-AP";  // or any other SSID when in AP mode
const char* password = "123456789";  // not actually used

char ssid_ap[EE_STR] = "";
char pswd_ap[EE_STR] = "";
char dpwd_ap[EE_STR] = "";
char ip_ap[EE_STR] = "";
char gwy_ap[EE_STR] = "";
char msk_ap[EE_STR] = "";
char mqtt_ap[EE_STR] = "";

// HTML web page to handle multiple input fields
// Another option: <form method="post" action="http://a/s.html" style="display:none">

const char index_html_ap[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
   <title>ESP Input Form (GND)</title>
   <meta name="viewport" content="width=device-width, initial-scale=2">
   </head><body>
   <h2>Setup ViGIR Device<h2>
   <h4>Configure your device</h4>
   <form action="/action_page" method="GET">
   SSID_Name:<br>
   <input type="text" name="ssid_name" value="ViGIR">
   <br>
   SSID_Passwd:<br>
   <input type="text" name="ssid_pswd" value="Password">
   <br>
   Device_Passwd:<br>
   <input type="text" name="devc_pswd" value="Test">
   <br>
   Device_IP:<br>
   <input type="text" name="devc_ip" value="10.14.1.207">
   <br>
   Device_GWY:<br>
   <input type="text" name="devc_gwy" value="10.14.1.1">
   <br>
   Device_Mask:<br>
   <input type="text" name="devc_msk" value="255.255.255.0">
   <br>
   MQTT_IP:<br>
   <input type="text" name="mqtt_ip" value="N/A">
   <br><br>
   <input type="submit" value="Submit">
   </form>
</body></html>)rawliteral";


IPAddress local_IP_AP(192,168,1,1);
IPAddress gateway_AP(192,168,1,1);
IPAddress subnet_AP(255,255,255,0);

IPAddress str2ip (char *str) {
   unsigned char i, j=0;
   IPAddress ip = (0,0,0,0);

   for (i=0; i<16; i++) {
      while ((str[i] != '\0') && (str[i] != '.')) {
         ip[j] = ip[j]*10 + (str[i] - '0');
         i++; }
      if (str[i] == '\0') break;
      j++; } 
   return ip; }

//===============================================================
// This routine is executed when you open the AP's IP in browser
//===============================================================
// Send web page with input fields to client

static esp_err_t root_handler_ap(httpd_req_t *req){
   httpd_resp_send(req, index_html_ap, strlen(index_html_ap));
   return ESP_OK; }

//===============================================================
// This routine is executed when you press submit
//===============================================================
// form action is handled here ("/action_page")
static esp_err_t action_handler(httpd_req_t *req){
   int buf_len; char buf[2000];

   buf_len = httpd_req_get_url_query_len(req) + 1;
   if (buf_len > 1) {
      if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
         if (httpd_query_key_value(buf, "ssid_name", ssid_ap, EE_STR) == ESP_OK) {
            Serial.printf("Found URL query parameter => ssid_name=%s\n", ssid_ap); }
         if (httpd_query_key_value(buf, "ssid_pswd", pswd_ap, EE_STR) == ESP_OK) {
            Serial.printf("Found URL query parameter => ssid_pswd=%s\n", pswd_ap); }
         if (httpd_query_key_value(buf, "devc_pswd", dpwd_ap, EE_STR) == ESP_OK) {
            Serial.printf("Found URL query parameter => devc_pswd=%s\n", dpwd_ap); }
         if (httpd_query_key_value(buf, "devc_ip", ip_ap, EE_STR) == ESP_OK) {
            Serial.printf("Found URL query parameter => devc_ip=%s\n", ip_ap); }
         if (httpd_query_key_value(buf, "devc_gwy", gwy_ap, EE_STR) == ESP_OK) {
            Serial.printf("Found URL query parameter => devc_gwy=%s\n", gwy_ap); }
         if (httpd_query_key_value(buf, "devc_msk", msk_ap, EE_STR) == ESP_OK) {
            Serial.printf("Found URL query parameter => devc_msk=%s\n", msk_ap); }
         if (httpd_query_key_value(buf, "mqtt_ip", mqtt_ap, EE_STR) == ESP_OK) {
            Serial.printf("Found URL query parameter => mqtt_ip=%s\n", mqtt_ap); } } }

   String s = "<h2> Configured: ssid_name " + String(ssid_ap) + "<br>" +
                  "            ssid_pswd " + String(pswd_ap) + "<br>" +
                  "            devc_pswd " + String(dpwd_ap) + "<br>" +
                  "            devc_ip   " + String(ip_ap) + "<br>" +
                  "            devc_gwy  " + String(gwy_ap) + "<br>" +
                  "            devc_msk  " + String(msk_ap) + "<br>" +
                  "            mqtt_ip   " + String(mqtt_ap) + "<br>" +
                  "<a href='/'> Go Back </a> </h2>"; //Send web page

   httpd_resp_send(req, s.c_str(), s.length()+1);
   return ESP_OK; }



/*****************************************
 ****************   SETUP  ***************
 *****************************************/

void setupAP(){
   // Serial port for debugging purposes
   Serial.print("Setting AP (Access Point)… ");

   // Remove the password parameter, if you want the AP (Access Point) to be open
   // WiFi.softAP(ssid, password);
   WiFi.softAP(ssid);
   delay(5000);
   // The use of config after softAP and delays seem to resolve an exception/abort
   // problem with ESP32s-Cam in AP mode
   WiFi.softAPConfig(local_IP_AP, gateway_AP, subnet_AP);
   delay(5000);

   IPAddress IP = WiFi.softAPIP();
   Serial.print("AP IP address: ");
   Serial.println(IP);

   // Start server
   httpd_config_t config = HTTPD_DEFAULT_CONFIG();
   config.server_port = 80;

   httpd_uri_t index_uri = {
       .uri       = "/",
       .method    = HTTP_GET,
       .handler   = root_handler_ap,
       .user_ctx  = NULL };
  
   httpd_uri_t action_uri = {
       .uri       = "/action_page",
       .method    = HTTP_GET,
       .handler   = action_handler,
       .user_ctx  = NULL };

   //Serial.printf("Starting web server on port: '%d'\n", config.server_port);
   if (httpd_start(&AP_httpd, &config) == ESP_OK) {
      httpd_register_uri_handler(AP_httpd, &index_uri);
      httpd_register_uri_handler(AP_httpd, &action_uri); } }



/*****************************************
 ****************   LOOP  ****************
 *****************************************/


void loopAP() {
   IPAddress ip = (0,0,0,0);

   if ((ssid_ap[0] == '\0') || (ssid_ap[0] < '0') || (ssid_ap[0] > 'z')) {
      Serial.println("Waiting...");
      delay(2000); }
   else {
      Serial.println("Configured...");
      Serial.printf("\nWiFi SSID %s\n", ssid_ap);
      Serial.printf("\nWiFi PSWD %s\n", pswd_ap);
      Serial.printf("\nDevice PSWD %s\n", dpwd_ap);
      ip = str2ip(ip_ap);
      Serial.print("\nDevice IP:");
      Serial.println(ip_ap);
      Serial.printf("  %d, %d, %d, %d\n", ip[0], ip[1], ip[2], ip[3]);
      ip = str2ip(gwy_ap);
      Serial.print("\nDevice GWY:");
      Serial.println(gwy_ap);
      Serial.printf("  %d, %d, %d, %d\n", ip[0], ip[1], ip[2], ip[3]);
      ip = str2ip(msk_ap);
      Serial.print("\nDevice MSK:");
      Serial.println(msk_ap);
      Serial.printf("  %d, %d, %d, %d\n", ip[0], ip[1], ip[2], ip[3]);
      ip = str2ip(mqtt_ap);
      Serial.print("\nDevice MQTT:");
      Serial.println(mqtt_ap);
      Serial.printf("  %d, %d, %d, %d\n", ip[0], ip[1], ip[2], ip[3]);

      EEPROM.put(0, ssid_ap);
      EEPROM.put(1*EE_STR, pswd_ap);
      EEPROM.put(2*EE_STR, dpwd_ap);
      EEPROM.put(3*EE_STR, ip_ap);
      EEPROM.put(4*EE_STR, gwy_ap);
      EEPROM.put(5*EE_STR, msk_ap);
      EEPROM.put(6*EE_STR, mqtt_ap);
      EEPROM.commit();

      // server.handleClient();          //Handle client requests
      delay(2000);
      Serial.println("Preparing to reboot in Device mode");
      ESP.restart(); } }
      
