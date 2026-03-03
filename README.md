# OASIS Climate - ESP32 Reference Firmware

## 🚀 Prototyping Phase: The Physical Link to Cloud Intelligence

**OASIS Climate ESP32** is the reference firmware implementation designed to bridge the physical world with the **OASIS Cloud AI**.

While the cloud engine builds a **Digital Twin** of your home to calculate the optimal thermal strategy, this firmware ensures that telemetry is gathered with precision and that actuation commands are executed reliably. It is built using **PlatformIO** and **C++** to provide a robust, low-latency connection to our API without the overhead of intermediate layers.

### ⚠️ Status: Active Prototyping

**This repository is currently in the PROTOTYPE phase.**
The code provided here is a proof-of-concept intended for developers and early hardware testers. It focuses on:

* **Secure Telemetry:** Direct serialization of sensor data (Temperature, Humidity) to OASIS JSON schemas.
* **Command Execution:** Polling the OASIS Cloud for the latest AI-driven modulation strategies.
* **Fail-safe Logic:** Implementing local hysteresis fallbacks in case of network loss.

### The Vision

We are building a "neural link" for your heating system. Unlike standard smart thermostats, this firmware is designed to be an extension of our Reinforcement Learning models, capable of receiving complex modulation instructions rather than simple ON/OFF signals.

**Key Features (In Development):**

* Native HTTPS communication with OASIS API.
* Efficient JSON parsing using ArduinoJson.
* Over-The-Air (OTA) updates managed by the cloud.

---

**Visit our official website for more information: [oasis-climate.com](https://oasis-climate.com)**

(Powered by Reinforcement Learning and Symbolic Regression)
