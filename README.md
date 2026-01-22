# ELRS Backpack send data via ESPNOW to ESP8266/ESP32 
 Backpack Expresslrs Example with ESP8266/ESP32 and CRSF Protokoll
 Based on https://github.com/druckgott/ELRS-Backpack-Example-ESPNOW

## Requirements

 * In the ELRS Backpack, Telemetry ESPNOW must be enabled (tested with Backpack version 1.5.1).
 * A telemetry connection between the transmitter and, for example, the drone/flight controller must be established.

## Web UI (ESP32/ESP8266)

The device starts a Wi‑Fi AP and serves a live telemetry dashboard plus a settings page:

 * AP SSID: `Backpack_ELRS_Crsf`
 * AP password: `12345678`
 * IP: `10.0.0.1`
 * Dashboard: `http://10.0.0.1/`
 * Settings (binding phrase): `http://10.0.0.1/settings`

### Upload LittleFS pages

The HTML pages are stored in `data/` and served from LittleFS. You must upload the filesystem image at least once:

```
pio run -t uploadfs -e esp32dev
```

For ESP8266:

```
pio run -t uploadfs -e nodemcuv2
```

If `pio` is not in your PATH, use the full path to the CLI (example):

```
/Users/<you>/.local/bin/pio run -t uploadfs -e esp32dev
```

After editing any file in `data/`, re-run the `uploadfs` command for your target.

### Notes

* The telemetry page connects to a WebSocket on `ws://10.0.0.1:81/`.
* After changing the binding phrase, the device reboots to apply the new UID.
