===PyHouse ESP32 Application

This is the documentation for the PyHouse Esp32 Application.

# Features:
  OTA uploading of new images.
  MQTT messaging

## WiFi
  Should always use DHCP address
  Should prefer IPv6 over IPv4

## OTA - Over The Air
Server Name/Ip
Portis fixed to 8585
Filename is fixed to PyHouse_Esp32.bin
Version is 00.00.01 to start

## MQTT
Server Name/IP
Port is fixed to 1883/8883
Client ID is PyH-32-xxxxxx generated.
Topic is pyhouse/esp32/xxxx
Should use self signed certificates
