# Final Assignment – Temperature & Ambient Light Monitoring System

## Overview
This project implements a sensor monitoring system on the **Raspberry Pi Pico** using:
- **I2C** for sensor communication
- **SPI** for LED control via an I/O expander

The system reads:
- Temperature using **LM75B** (11-bit resolution, 0.125°C precision)
- Ambient light using **APDS-9306** (up to 20-bit resolution)

Sensor data is displayed on LEDs and printed to the serial monitor. Interrupts are used for threshold-based alerts.

---

## Hardware Setup

### I2C Pins
- SDA -> GPIO 4  
- SCL -> GPIO 5  

### Interrupt Pins
- OS (Temperature interrupt) -> GPIO 6 (pull-up)  
- INT (Light interrupt) -> GPIO 7 (pull-up)  

### SPI Pins (MCP23S08 I/O Expander)
- SCK -> GPIO 18  
- MOSI -> GPIO 19  
- MISO -> GPIO 16  
- CS -> GPIO 17  

### Button
- Mode switch -> GPIO 15  

---

## Sensors

### LM75B – Temperature Sensor
- I2C Address: `0x49`  
- Resolution: **0.125°C (11-bit ADC)**  

#### How it works
1. Write register `0x00` (temperature register)  
2. Read 2 bytes  
3. Combine into 16-bit value  
4. Shift right by 5 bits -> 11-bit data  
5. Convert to Celsius:  Temp = raw * 0.125

---

#### Threshold Interrupt
- OS pin goes LOW when temperature > threshold  
- Configured via:
  - `TOS` (upper limit)
  - `THYST` (hysteresis)

---

### APDS-9306 – Ambient Light Sensor
- I2C Address: `0x52`  
- Resolution: **up to 20-bit**  

#### Initialization
- Enable ALS  
- Set measurement rate (high resolution mode)  
- Set gain (3x)  

#### Reading Data
1. Write `ALS_DATA_0`  
2. Read 3 bytes  
3. Combine into 20-bit value  

#### Interrupt
- Triggered when light is outside threshold range  
- Requires:
  - Enable interrupt bit  
  - Set upper/lower thresholds  
  - Clear interrupt via `MAIN_STATUS`  

---

## SPI – MCP23S08 (LED Control)

### Protocol
SPI sends 3 bytes: [Control Byte][Register][Data]

### Example
[0x40][0x09][0xFF] -> All LEDs ON
[0x40][0x09][0x00] -> All LEDs OFF

---

## Features

### Mode Switching
- Button toggles between:
  - Temperature display  
  - Light display  

### LED Output
- Displays sensor values (LSB)

### Interrupt Alerts
- Temperature too high -> LEDs flash  
- Light threshold exceeded -> LEDs flash  

---

## Interrupt Handling

All interrupts are handled in a single callback:

- **Button** -> Switch mode  
- **OS pin** -> Temperature warning  
- **INT pin** -> Light warning  

---

## Test Plan

### Temperature Test
- Touch sensor to increase temperature  
- Verify:
  - OS pin triggers  
  - LEDs flash  
  - Warning printed  

### Light Test
- Shine bright light  
- Verify:
  - INT pin triggers  
  - LEDs flash  
  - Warning printed  

---

## Implementation Notes

- I2C uses `i2c_write_blocking` and `i2c_read_blocking`  
- SPI uses `spi_write_blocking`  
- Interrupts use GPIO IRQ with callback  
- Debouncing implemented for button (200 ms)  

---

## Precision Comparison

| Sensor | Resolution |
|--------|-----------|
| LM75B (Temperature) | 0.125°C (11-bit) |
| APDS-9306 (Light)   | Up to 20-bit |

**Conclusion:** The **APDS-9306 light sensor has the highest precision**.

---

## How to Run

1. Flash code to Raspberry Pi Pico  
2. Open serial monitor  
3. Press button to switch modes  
4. Trigger interrupts using heat or light  

---

## Author
**Son Cao – 570135**