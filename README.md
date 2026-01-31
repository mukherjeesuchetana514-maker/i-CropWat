# i-CropWat: Smart Weather-Aware Irrigation System 🌱💧

![Status](https://img.shields.io/badge/Status-Prototype-green) ![Platform](https://img.shields.io/badge/Platform-ESP32-blue) ![IoT](https://img.shields.io/badge/IoT-Blynk-orange)

> Remolding plant watering...

**i-CropWat** is an intelligent, IoT-enabled irrigation system designed to optimize water usage for agriculture and smart gardening. Unlike traditional timer-based systems, i-CropWat combines real-time **soil moisture data** with **live weather forecasting** (via WeatherAPI) to make smart watering decisions—preventing water waste when rain is predicted.

---

## 🌟 Key Features

* **🧠 Weather Intelligence:** Automatically fetches local weather forecasts. If rain is predicted (high probability), the system skips scheduled watering to conserve resources.
* **📱 IoT Monitoring:** Real-time tracking of Soil Moisture, Air Temperature, and Humidity via the **Blynk IoT App**.
* **🖥️ On-Device Menu System:** A 20x4 LCD interface allows users to select specific crops or flowers without reprogramming the device.
* **🌾 Customized Plant Profiles:** Pre-configured moisture thresholds for diverse crops (e.g., Wheat, Rice) and flowers (e.g., Rose, Sunflower) to support precision agriculture.
* **🚨 Emergency Protocol:**
    * **Auto-Water:** Triggers immediate watering if moisture drops to critical levels.
    * **Auto-Cutoff:** Stops pumping immediately if moisture exceeds safety limits to prevent root rot.
* **🔋 Efficient & Low Cost:** Designed for small and marginal farmers with an estimated build cost of approx.

---

## 🛠️ Hardware Architecture

### Components Required 
| Component | Budget (Est.) | Function |
| :--- | :--- | :--- |
| **NodeMCU – ESP 32** | ₹250 | The brain of the system (Wi-Fi + Processing) |
| **Soil Moisture Sensor** | ₹35 | Measures volumetric water content in soil |
| **Mini Micro Pump DC (3-6V)** | ₹60 | Delivers water to the plant roots |
| **Solid State Relay (SSR)** | ₹45 | Controls the high-power water pump |
| **DHT11 Sensor** | ₹50 | Measures ambient Temperature and Humidity |
| **LCD Display (I2C)** | ₹120 | Displays status, menu, and weather data |
| **Power Supply/Battery** | ₹100 | Powers the microcontroller and pump |
| **Connecting Wires** | ₹65 | System integration |
| **Others** | ₹70 | Enclosure and misc. hardware |
| **Total** | **~₹795** | |

### Snapshots
* `i-CropWat.jpg` *

---

## ⚙️ Software & Libraries

The project is built using **C++** on the Arduino framework.

**Required Libraries:**
* `WiFi.h` & `HTTPClient.h` (For API calls)
* `ArduinoJson.h` (Parsing WeatherAPI data)
* `LiquidCrystal_I2C.h` (LCD Control)
* `DHT.h` (Temperature/Humidity)
* `BlynkSimpleEsp32.h` (IoT Dashboard)

---

## 🚀 Installation & Setup

1.  **Clone the Repository:**
    ```bash
    git clone [https://github.com/mukherjeesuchetana514-maker/i-CropWat.git]
2.  **Hardware Setup:**
    * Connect the **Soil Sensor** to Pin `34`.
    * Connect the **DHT11** to Pin `4`.
    * Connect the **Relay** to Pin `5`.
    * Connect Menu Buttons to Pins `12` (Menu), `14` (Up), `26` (Down), `25` (Select).
3.  **Configure Credentials:**
    * Open `i-CropWat.ino` in Arduino IDE.
    * **IMPORTANT:** Update the following lines with your own keys:
        ```cpp
        char BLYNK_AUTH[] = "YOUR_BLYNK_AUTH_TOKEN";
        const char* WIFI_SSID = "YOUR_WIFI_NAME";
        const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
        const char* OWM_APIKEY = "YOUR_WEATHERAPI_KEY";
        ```
4.  **Upload:**
    * Select Board: `DOIT ESP32 DEVKIT V1`.
    * Upload the code.

---

## 📱 How It Works 

1.  **Startup:** The system connects to Wi-Fi and fetches the location automatically via IP.
2.  **Selection Mode:** The user selects the **Category** (Crop/Flower) and **Plant Name** via the LCD Menu.
3.  **Monitoring Loop:**
    * **Sensor Check:** Periodically reads sensor data.
    * **Weather Check:** Polls WeatherAPI for rain predictions.
4.  **Decision Engine:**
    * *Is soil dry?* -> Check Weather.
    * *Is rain coming?* -> **SKIP** watering (Save Water).
    * *Is weather clear?* -> **START** Pump via Relay.
5.  **Feedback:** Status is updated on the LCD and sent to the phone (Blynk).

---

## 🔮 Future Scope 

* **Hardware Integration:** Transition from breadboard prototypes to a custom PCB.
* **Product Optimization:** Switching to retail-version components to lower costs for mass production.
* **Urban Adaptability:** Refining the system for balcony gardens and community vertical farming.
* **Patenting:** Filing for a patent for the unique logic integration.

---

## 👥 Team Starlet

**Developed by students of MCKV Institute of Engineering:**
* **Suchetana Mukherjee** 
* **Sukanya Rana** 

**Supervisor:**
* **Mr. [cite_start]Mojammel Rahaman** (Assistant Professor, Basic Science & Humanitics Dept, MCKVIE) 

---

## 🙏 Acknowledgements

Special thanks to our college faculty and the open-source community for the libraries used in this project.

*References:*
* *Smart Irrigation System Literature*
* *ESP32 IoT Implementations*

---
*© 2026 i-CropWat. All Rights Reserved.*
