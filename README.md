If you prefer to use ESPHome instead of MQTT then check this - https://github.com/e6on/ESP32_MiFlora_ESPHome

## ESP32_MiFlora_MQTT

Arduino sketch - ESP32 BLE client for XIaomi Mi Flora Plant sensors. ESP32 can be powered with LiPo battery and battery level is read from ADC pin. ESP32 battery level and Mi Flora sensor measurements are pushed to the MQTT server.

## Hardware & Software used

- Adafruit HUZZAH32 – ESP32 Feather Board - https://www.adafruit.com/product/3405
- Adafruit Lithium Ion Battery 3.7v 2000mAh - https://www.adafruit.com/product/2011
- Xiaomi Mi Plant Sensor
- MQTT server: Home Assistant Mosquitto broker add-on - https://home-assistant.io

## Setup

1. Install the Espressif Arduino-ESP32 support - https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html
2. Edit settings in config.h:
- FLORA_DEVICES - MAC address(es) of your Xiaomi Mi Plant sensor(s)
- SLEEP_DURATION - sleep duration between sensor reads
- EMERGENCY_HIBERNATE - emergency hibernate countdown when something gets stuck
- BATTERY_INTERVAL - Mi Flora battery status read interval
- RETRY - device retry count when something fails
- WIFI_SSID - WLAN SSID
- WIFI_PASSWORD - WLAN password
- ADC_Pin - ESP32 ADC pin number where the Battery is connected to
- MQTT Server settings

2. Open sketch in Arduino, compile & upload.

3. esp_miflora.yaml - Example MQTT sensor config for Home Assistant (replace MAC addresses in the file for your MiFlora & ESP32)

## Credits

- Original arduino sketch - https://github.com/sidddy/flora
- More ideas for the sketch - https://github.com/Pi-And-More/PAM-ESP32-Multi-MiFlora
- Battery Monitoring on the Adafruit Huzzah32 ESP32 - http://cuddletech.com/?p=1030
- Calculating battery percentage using an Arduino - https://electronics.stackexchange.com/questions/110104/calculating-battery-percentage-using-an-arduino
- Adafruit HUZZAH32 – ESP32 Feather Board Power Management - https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/power-management
