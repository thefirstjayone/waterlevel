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


Calibration:
How it works
	•	All pins use analogReadMilliVolts (so marginal levels are handled well).
	•	You’ll run six short calibrations (one per state): 100, 75, 50, 25, 5, 4.
	•	For each calibration, the board samples A=D0, B=D1, C=D2 for ~30s and stores their averages to NVS (flash).
	•	After the five states are captured, it computes adaptive thresholds per pin:
	•	For each pin, it averages that pin’s readings in the states where it should be ON and OFF, then sets threshold_pin = (avgON + avgOFF)/2.
	•	Then normal operation uses those thresholds to classify states (same mapping you wanted).

Serial commands (type in Serial Monitor)
	•	cal 100 — (Green) capture voltages for the 100% (A) state
	•	cal 75  — (Blue) capture 75% (C)
	•	cal 50  — (Yellow) capture 50% (A+B)
	•	cal 25  — (Red) capture 25% (B)
	•	cal 5   — (Violet) capture 5% (B+C)
 	•	cal 4   — (Green/Blue Flicker) capture 4% (B+C)
	•	compute — compute thresholds from the captured states and save
	•	show    — print all saved calibration values and thresholds
	•	reset   — clear calibration & thresholds from flash
	•	run     — exit calibration mode and start normal reporting
	•	help    — list commands

Tip: Do them in order: cal 4, 100, cal 75, cal 50, cal 25, cal 5, then compute, then run.
4%     Flicker    Error – unstable

Level  Pin State  Notes

100%   A HIGH     Normal full tank

75%    C HIGH

50%    A+B HIGH

25%    B HIGH

5%     B+C HIGH   Error – connecting

