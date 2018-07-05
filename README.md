# orwell
Innenraumsensor für IoT / Smart Home

# Hardware

## ESP
SDA=4 => D2
SCL=5 => D1

## PIR
Connected pin SD3 aka GPIO10 aka SDD3 (4th from top left)

## BME680
I2C
https://github.com/adafruit/Adafruit_BME680

## BH1750
I2C
https://github.com/claws/BH1750

# Software

pio run -t upload
