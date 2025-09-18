Water Tank Level Sensor (ESP32-C3)

This project is an ESP32-C3–based water tank monitoring system that uses voltage sensing on three input pins to determine water level. The system maps pin combinations into discrete fill states:
	•	100%
	•	75%
	•	50%
	•	25%
	•	5% (Error: transmitter connection issue)
	•	4% (Error: flicker detected while connecting to transmitter)

Features
	•	Wi-Fi enabled: Publishes water level to ThingSpeak every 15 seconds.
	•	Error detection: Special states (4% and 5%) indicate communication issues between the transmitter and receiver.
	•	Calibration mode: Type cal over Serial to enter calibration. Each state can be sampled for 30 seconds to capture voltage values, which are then stored and used as defaults.
	•	Debug output: Serial monitor logs raw voltages, detected state, and classification results for testing and tuning.
	•	Startup sequence: Optionally simulates fill levels (25%, 50%, 75%, 100%) to confirm end-to-end reporting.

Hardware
	•	MCU: ESP32-C3 (tested with Seeed Studio XIAO ESP32-C3)
	•	Inputs: Voltage sensors on pins D0, D1, D2
	•	Outputs: Wi-Fi (ThingSpeak), optional integration with HomeKit / Google Home via Homebridge or IFTTT

Error Codes
	•	4% – Detected flicker between 75% and 100%. This means the receiver is attempting to connect with the transmitter but cannot lock on reliably.
	•	5% – Receiver in transmitter connection mode. Used as a dedicated state to indicate communication error.

Usage
	1.	Flash the firmware onto an ESP32-C3 board.
	2.	Power the receiver circuit and connect sensors to pins D0, D1, and D2.
	3.	Open the Serial Monitor (115200 baud) for debug logs and calibration.
	4.	View live water level data on ThingSpeak or integrate with a smart home platform.

Level  Pin State  Notes
100%   A HIGH     Normal full tank
75%    C HIGH
50%    A+B HIGH
25%    B HIGH
5%     B+C HIGH   Error – connecting
4%     Flicker    Error – unstable

