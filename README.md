# ESP2Cloud

Send data from esp device to cloud, like IFTTT or Ubidots.

The device is configured after first boot. Device will act like an access point (AP). Connect Phone or computer to AP (Named ESP_{IP_number}), browse to {IP_number} and configure wifi info,cloud URL and sampling rate.

The code has implementation for DTH21 or AM2320 (I2C), temp and humidity sensor.

## Reflash or reconfigure
Connect GPIO0 to ground (with a button or wire) immediately after boot to enter OTA or to reset config.
- Short press: OTA mode, led blinking
- Long press: Reset config, led stedy shining. Reboot device to connect to AP and config.

## Electrical
Connecting ESP-12/7 with deep sleep:
- Connect EN to VCC.
- Pull-up on GPIO0 (4-10 Kohm). Connect to ground when flashing.
  Button between GPIO and GND would be convenient
- Connect GPIO15 and GND
- Small (220 ohm) resistor betw GPIO16 and RESET (Only for timer wake up after deep sleep)
