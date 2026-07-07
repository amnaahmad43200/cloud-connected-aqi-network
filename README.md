# Cloud Connected AQI Network for Pakistan

A final-year engineering project (Department of Electrical Engineering, Institute of Space Technology, Islamabad — July 2024) that builds a low-cost, battery-powered Air Quality Index (AQI) sensor network for Islamabad, streams the data to the cloud, and uses machine learning to predict future AQI trends.

**Authors:** Amna Ahmad, Zujaja Ibrahim
**Supervisor:** Faraan Mahmood

## Overview

Islamabad, like most rapidly urbanizing cities, lacks affordable real-time AQI monitoring infrastructure. This project addresses that gap with a network of ESP32-based sensor nodes that:

- Measure key pollutants (PM2.5, PM1, CO, SO₂) using calibrated low-cost sensors
- Compute a time-weighted, standards-based Final AQI on-device
- Push readings to the cloud over MQTT (Blynk IoT) every hour
- Fall back to hourly SMS alerts (via SIM800L/GSM) when Wi-Fi is unavailable
- Feed three months of collected data into machine learning models to forecast future AQI

## Repository Structure

```
├── analytics/src/
│   ├── AQI_Predictive_Analysis.ipynb   # ML pipeline: EDA, correlation, model training/evaluation
│   └── readme.md
├── data/
│   ├── Node1.csv                        # Raw/processed sensor data collected from Node 1
│   └── readme.md
├── docs/
│   └── THESIS_FYP_REPORT[Final].pdf     # Full thesis report (design, implementation, results)
├── esp32_node/src/
│   ├── node_aqi_code_esp32.ino          # Firmware: sensing, AQI calc, cloud upload, deep sleep, SMS
│   └── readme.md
└── README.md
```

## Hardware / Node Design

Each sensor node is built around an **ESP32** microcontroller and includes:

| Component | Purpose |
|---|---|
| ESP32 | Dual-core Wi-Fi MCU — sensing, AQI computation, cloud transmission, deep sleep |
| MQ7 | Carbon monoxide (CO) detection |
| MQ135 | Sulfur dioxide (SO₂) detection |
| DSM501A | Particulate matter (PM2.5 / PM1) detection via light scattering |
| OLED (SSD1306 / TFT 128x160) | On-device display of live AQI |
| SIM800L | GSM module for hourly SMS alerts when Wi-Fi is unavailable |
| 5V/2A power bank | Portable power supply |

**Key design choices:**
- Sensors are individually calibrated (MQ7, MQ135, DSM501A) before deployment
- Final AQI is computed on-device as a **rolling/time-weighted average of 24 hourly readings**
- ESP32 enters **deep sleep between hourly readings**, storing data in RTC memory — this conserves an estimated **~97.5% of battery life**, enabling long-term unattended field deployment
- A custom **PCB** and **3D-printed CAD casing** were designed for the final node enclosure
- Data is stored on **AWS** and visualized in real time through **Blynk IoT** dashboards/templates per node

## Data Collection

- **~3 months** of continuous hourly data collected from deployed node(s)
- Final dataset: **2,718 entries × 7 columns** — `Datetime`, `PM_2.5`, `PM_1`, `SO2`, `CO`, `Total_AQI`, `Category` (no missing values)
- AQI trend observed: a general **decrease in AQI from March to July**, consistent with known seasonal AQI patterns in Pakistan

## Predictive Analysis (Machine Learning)

Using the collected dataset, four regression models were trained and compared to forecast AQI:

- **Linear Regression**
- **Random Forest Regressor**
- **Gradient Boost Regressor**
- **CatBoost Regressor**

**Correlation findings:**
- PM2.5 and PM1 are strongly correlated (**0.82**)
- Total AQI correlates well with PM2.5 (**0.76**) and PM1 (**0.77**)
- CO shows only weak correlation with other pollutants (**0.046–0.27**)

**Model comparison (evaluated on training time, R², RMSE, MAPE):**

| Model | Strengths |
|---|---|
| Linear Regression | Fastest to train; moderate accuracy — best suited for simple linear relationships |
| Random Forest | Good balance of speed and accuracy with low error — handles complex, non-linear data well |
| Gradient Boost Regressor | High predictive accuracy; longer training time |
| CatBoost Regressor | High performance with efficient categorical feature handling; longer training time |

Data was split **80% training / 20% testing**, and models were benchmarked on Training Time, Training Score, Test Score, R² Score, RMSE, and MAPE (see `analytics/src/AQI_Predictive_Analysis.ipynb` and Chapter 5 of the thesis for full results and plots).

## System Validation

- Serial monitor logs confirm correct sensor sampling, AQI calculation, Wi-Fi connection, cloud upload ("Sending data to Blynk Cloud"), and deep-sleep cycling
- Cloud dashboards (Blynk templates) display live, per-node pollutant concentrations and overall AQI, viewable remotely

## Alignment with UN Sustainable Development Goals

This project maps to:
- **SDG 3** – Good Health and Well-being
- **SDG 11** – Sustainable Cities and Communities
- **SDG 13** – Climate Action
- **SDG 17** – Partnerships for the Goals

## Future Work

- Scale from single/few nodes to a distributed multi-node network covering broader geographic areas of Islamabad and beyond
- Aggregate multi-location data in the cloud to enable region-wide AQI comparisons across time and season
- Use the expanded dataset to improve predictive model accuracy and support broader environmental policy and public-health decision-making in Pakistan

## Repository Contents

- `docs/THESIS_FYP_REPORT[Final].pdf` — full write-up: literature survey, hardware design, calibration, PCB/CAD design, firmware explanation, ML pipeline, and detailed results
- `esp32_node/src/node_aqi_code_esp32.ino` — ESP32 firmware
- `analytics/src/AQI_Predictive_Analysis.ipynb` — data visualization, correlation analysis, and model training/evaluation notebook
- `data/Node1.csv` — collected sensor dataset
