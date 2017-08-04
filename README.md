# ESP2Cloud

Send data from esp device to cloud, like IFTTT or Ubidots. Data will be in IFTTT/maker json format

The device must be configured after first boot. Device will act like an access point (AP). Connect phone or computer to AP (Named ESP_{IP_number}), browse to {IP_number} and configure wifi info, cloud URL and sampling rate.

The code has implementation for DTH21 (One wire) and AM2320 (I2C), temp and humidity sensor, selectable with preprocessor define.

## Reflash or reconfigure
Connect GPIO0 to ground (with a button or wire) immediately after boot to enter OTA or to reset config.
- Short press: OTA mode, led blinking
- Long press: Reset config, led stedy shining. Reboot device to connect to AP and config.

## Electrical
Connecting ESP-12/7 with deep sleep:
- Connect EN to VCC.
- Pull-up on GPIO0 (4-10 Kohm). Connect to ground when flashing.
  Button between GPIO0 and GND would be convenient
- Connect GPIO15 and GND
- Small (220 ohm) resistor betw GPIO16 and RESET (Needed for timer wake up after deep sleep)
