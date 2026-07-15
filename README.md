# Firmware Telemetry Core — Multi-Sensor Environment

Non-blocking ESP32 firmware that reads three sensor layers (temperature/humidity,
ambient light, distance), filters each stream in real time, and drives a PWM
actuator off live sensor feedback. No `delay()` anywhere in `loop()`.

## Requirement → Implementation Map

| Brief asked for | What this firmware does |
|---|---|
| Asynchronous multi-tasking, no `delay()` | `millis()`-gated task scheduler; ultrasonic echo is interrupt-driven |
| GPIO processing & signal handling | DHT22 digital driver, LDR analog read, PWM LED actuator |
| PWM actuator based on data feedback | LED brightness closes the loop on light sensor reading |
| Processing signal matrices | 5-sample rolling buffer per sensor, telemetry reports average |
| Non-blocking error correction | Bad samples discarded, last good value held — no retry loop |
| Virtualization simulation | Built and tested on Wokwi (link below) |

## Simulation Link

[Click here to open Wokwi project](https://wokwi.com/projects/46933346124399905281)

## Simulation Screenshots

![Simulation Overview](docs/simulation-overview.jpg)

![Serial Monitor Output](docs/serial-monitor-output.jpg)

## Wiring

| Component | ESP32 pin |
|---|---|
| DHT22 VCC | 3V3 |
| DHT22 GND | GND |
| DHT22 DATA | GPIO 15 |
| Potentiometer VCC | 3V3 |
| Potentiometer GND | GND |
| Potentiometer SIG | GPIO 34 |
| HC-SR04 VCC | 5V |
| HC-SR04 GND | GND |
| HC-SR04 TRIG | GPIO 5 |
| HC-SR04 ECHO | GPIO 18 |
| LED anode (+) | GPIO 25 (via resistor) |
| LED cathode (−) | GND |

## Sample Output

```
{"t_ms":5000,"temp_C":26.4,"hum_pct":48.2,"light_pct":31.7,"dist_cm":42.9,"errors":0}
```

## Repo Structure

```
getintern-week1-firmware/
├── README.md
├── firmware_telemetry_core.ino
└── docs/
    ├── wokwi-link.md
    ├── simulation-overview.jpg
    └── serial-monitor-output.jpg
```
