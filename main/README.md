# ZigBee Sensor Node for ESP32-H2 with BME280

## Features:
- Measures Temperature, Humidity, Pressure via BME280
- Sends data over ZigBee (compatible with Zigbee2MQTT)
- Battery level monitoring
- Deep sleep + wake by button (GPIO9)
- Reset ZigBee settings on long press

## Components:
- ESP32-H2
- BME280 (I2C)
- Button on GPIO9

## Requirements:
- ESP-IDF v5.5+
- Python dependencies: kconfiglib, pyserial