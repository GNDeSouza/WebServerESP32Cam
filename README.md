# WebServerESP32Cam
Web Cam using ESP32Cam with WiFi-AP configuration mode

This is a program based on another code for a webcam server,
but with one additional functionality: the ESPCam can boot in
a WiFi-AP open mode (no password), so one can login into the ESP
for initially setting up the SSID/Password/IP to their needs. 
Once SSID/Password/IP are entered, the ESP reboots in webcam
server mode. If at any time one needs to reset the configuration,
they can simply press a button for about 10 secs and the config
is forgotten (erase from EEPROM), and booting the ESP back in
WiFi-AP mode. 

This is a very familiar/standard feature in most IoT and many
other devices nowadays (Alexa, light switches, door bells, etc)
and I am sure you can relate to its usefulness.  Also, if the
IP is omitted during the WiFi-AP configuration mode, the program
uses DHCP to choose one.

The code is NOT optimized and clean -- you may find a couple of
unused variables here and there.  But it works well and I have
setup a few cameras for myself and friends.

The code also provides password-protected access to the camera
stream, so one can safely access this webcam from anywhere --
e.g. thru Port Forwarding from their home routers to the webcam
IP address (port 80).  The password is NOT saved encripted, so
one should be careful here not to utilize passwords that are
used for other purposes. The password feature provides access
to the browser until the stream is broken, after which, the
password has to be entered again.

