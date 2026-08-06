# TrackIt — BLE GNSS Receiver (ESP32-S3 + u-blox M10)

A battery-oriented GNSS receiver built on an **ESP32-S3** and a **u-blox M10** module, exposing position, speed, altitude and fix quality over **Bluetooth Low Energy** using the standard SIG **Location and Navigation** profile — so any generic BLE client can consume it without a proprietary app.

The interesting part isn't the wiring. It's that the entire stack below the application — the UBX protocol layer, the M10 driver, the BLE abstraction, the filesystem, the logger, the scheduler, the shared-value primitive — was written from scratch as **nine independently reusable, platform-agnostic components**, each in its own repository and pulled in as a git submodule.

```
Application  ──►  GNSS pipeline ──► BLE GATT server
                       │                  │
                  M10 driver          BLE driver (NimBLE)
                       │
                  UBX protocol driver  ──►  UART (injected callbacks)
```

---

## Table of contents

- [What it does](#what-it-does)
- [Hardware](#hardware)
- [Architecture](#architecture)
- [Engineering highlights](#engineering-highlights)
- [BLE interface](#ble-interface)
- [Component library](#component-library)
- [Building](#building)
- [Repository layout](#repository-layout)
- [Known limitations & roadmap](#known-limitations--roadmap)

---

## What it does

- Configures a u-blox M10 receiver over UBX (constellations, nav model, update rate, DOP limits, time pulse, output message set) and streams **NAV-PVT** + **NAV-DOP** at 1 Hz.
- Publishes live telemetry over BLE as a standard **Location and Navigation** service (`0x1819`) with notifications, gated behind an encrypted, bonded connection.
- Accepts the current UTC time *from* the phone over the **Current Time** service (`0x1805`) and injects it into the receiver as an AssistNow time hint — cutting cold-start time when no fix is available.
- Persists the receiver's **navigation database (AssistNow Offline / MGA-DBD)** and last known position to on-board LittleFS every 3 hours, and re-injects both on boot when no fix is acquired quickly — a self-maintaining warm-start cache with no network dependency.
- Runs with dynamic frequency scaling (10–80 MHz) and automatic light sleep, held off only while the system is still initializing.
- Reports state through an RGB status LED (configuring / GNSS error / BLE error / advertising / connected).

---

## Hardware

> **No custom PCB.** The firmware is developed and validated on an off-the-shelf ESP32-S3 development board with an M10 breakout wired to it. A dedicated board has not been fabricated, so there is no schematic or layout to show.

| Function | Pin | Notes |
|---|---|---|
| GNSS UART TX (MCU → M10) | `GPIO15` | UART1, 115200 8N1, no flow control |
| GNSS UART RX (M10 → MCU) | `GPIO16` | 6 KB RX ring buffer, 2 KB TX |
| Status LED | `GPIO48` | WS2812 addressable, driven over RMT, 10 % brightness |

The M10's baud rate is negotiated at startup and then **written to the module's own flash** (`M10_SetBaudRate(..., M10_CONFIG_LAYER_FLASH)`), so subsequent boots connect at 115200 immediately instead of re-scanning.

**Flash map** (8 MB, `partitions.csv`):

| Partition | Type | Offset | Size |
|---|---|---|---|
| `nvs` | data | `0x9000` | 12 KB |
| `pht_init` | phy | `0xF000` | 4 KB |
| `factory` | app | `0x10000` | 1 MB |
| `storage` | littlefs | `0x110000` | ~3 MB |


## Engineering highlights

These are the parts worth reading the source for.

### Streaming frame parser for a mixed NMEA/UBX byte stream

The M10 emits ASCII NMEA sentences and binary UBX frames interleaved on the same UART. `main/gnss.c` implements a resynchronizing framer that:

- scans for both sync patterns (`$` and `0xB5 0x62`) and processes whichever appears first;
- distinguishes **incomplete** messages (keep buffering) from **corrupt** ones (drop one byte, resync) — returning a tri-state result so the outer loop knows whether to read more or retry immediately;
- bounds the damage: an NMEA sentence with no terminator is abandoned past 100 bytes or as soon as a second `$` appears; a UBX frame that never validates is abandoned once the buffer hits 75 % full — so a single garbled byte can't stall the pipeline;
- compacts the buffer in place with `memmove` and never allocates per message.

### Zero-allocation UBX transport

The UBX component (`components/ubx`) uses a **fixed-size payload pool** — 10 slots × 1 KB, statically allocated — with explicit acquire/release. There is no `malloc` on the message path, so memory behaviour is deterministic under sustained 1 Hz traffic and cannot fragment over a long run.

It also implements a **config mode**: while a `CFG-VALSET` sequence is in flight, every incoming message that isn't an ACK/NACK is discarded, so unsolicited periodic navigation output can't be mistaken for a configuration response. The M10 driver toggles this automatically around each config write.

### AssistNow Offline without a network

`M10_ExportNavData` walks the receiver's `MGA-DBD` database out over UBX in chunks; the application accumulates it into a 12 KB buffer and writes it to LittleFS alongside the last known fix, every three hours. On boot, a 5-second one-shot timer checks for a valid fix — if there isn't one and cached data exists, the firmware stops the GNSS engine, re-injects the position hint and the full nav database, and restarts it. Combined with the UTC hint written over BLE, this gives a warm start with no assistance server and no internet connection.

### Shared values: a typed pub/sub primitive over FreeRTOS

Rather than scattering global structs behind ad-hoc mutexes, `components/shared_values` provides a small handle type combining a **mutex** (for the value) with an **event group** (for subscriber wakeups). Producers call `SHVAL_PointerSetValue()`; each consumer blocks in `SHVAL_PointerWaitForValue()` on its own event bit and receives a private copy. Adding a new BLE characteristic notification means allocating one bit and one task — no changes to the producer.

### Platform-agnostic drivers by construction

The UBX and M10 drivers call **no ESP-IDF API at all**. Every hardware touchpoint — `UartInit`, `UartSend`, `UartSetBaudRate`, `UartFlush`, `WaitForMsg`, `SignalNewMsg`, plus weakly-linked `UBX_GetTickMsCB` / `UBX_WaitForMsCB` — is a function pointer supplied by the application. Porting the GNSS stack to an STM32 with bare-metal DMA means writing those eight functions and nothing else. The same discipline applies to the logger, which routes through a callback table (`on_log`, `on_fatal_err`, `optional_on_format`) that this project binds to `ESP_LOGx`.

### Self-deleting configuration tasks

Startup work that runs exactly once — GNSS configuration, BLE configuration — is a real task with a real stack, which calls `SCHEDULER_Remove()` on itself when finished. The stack is reclaimed rather than being permanently reserved, and the `Name == NULL` transition doubles as a completion signal that dependent tasks poll before starting.

### Security that is actually enforced

Encryption isn't just advertised. Every telemetry characteristic carries `BLE_GATT_CHR_F_READ_ENC`, and the subscribe handler independently re-checks the link:

```c
uint8_t on_gatt_subscribe_event(struct ble_gap_event *event) {
    if (event->subscribe.attr_handle == gBleAttributes.LocationAndSpeedChrHandle) {
        uint8_t is_encrypted;
        if (BLE_CheckConnEncrypted(event->subscribe.conn_handle, &is_encrypted) != BLE_ERROR_OK || !is_encrypted) {
            return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
        }
    }
    return 0;
}
```

Bonding uses passkey pairing with LE Secure Connections, and bonds persist in NVS across reboots.

---

## BLE interface

Advertised as **`GNSS Receiver`**, GAP appearance `0x1444` (*Location and Navigation Pod*), connection interval 45–70 ms, 4 s supervision timeout.

### Location and Navigation — `0x1819`

| Characteristic | UUID | Properties | Payload |
|---|---|---|---|
| LN Feature | `0x2A6A` | Read | Feature bitmask: location, elevation, heading, HDOP |
| Location and Speed | `0x2A67` | Read (enc), Notify | 24 B: flags, speed, lat, lon, altitude, heading, UTC date/time |
| Elevation | `0x2A6C` | Read (enc), Notify | 3 B signed, centimetres |
| Position Quality | `0x2A69` | Read (enc), Notify | 5 B: flags, satellites in solution, HDOP, VDOP |

### Current Time — `0x1805`

| Characteristic | UUID | Properties | Payload |
|---|---|---|---|
| Date Time | `0x2A08` | Read / **Write** (enc) | 7 B `year(2) month day hour min sec` |

Writing this characteristic hands the receiver a UTC hint via `MGA-INI-TIME`. The firmware deliberately **discards** the hint if it already has a valid 3D fix, so a phone with a bad clock can't degrade a good solution.

### Debug service (compile-time)

With `DEBUG_MODE_ENABLED`, a custom 128-bit service exposes the current fix as a UTF-8 string — readable straight from nRF Connect without decoding any binary layouts:

```
Longitude: 23.3218750; Latitude: 42.6977080; Velocity: 1.34 m/s (0.5 km/h); Altitude: 562.40 m
```

Every characteristic also carries a `0x2901` User Description descriptor, so a generic client shows meaningful names instead of raw handles.

---

## Component library

Nine components, each a standalone repository consumed here as a submodule. All are reusable outside this project.

| Component | Repository | Role |
|---|---|---|
| `ubx` | [ublox-ubx-generic-driver](https://github.com/ikok07/ublox-ubx-generic-driver) | UBX framing, Fletcher checksums, payload pool, config mode, poll/ACK |
| `m10` | [ublox-m10-generic-driver](https://github.com/ikok07/ublox-m10-generic-driver) | M10 configuration, power modes, resets, status, full MGA/AssistNow |
| `ble` | [esp32_ble_driver](https://github.com/ikok07/esp32_ble_driver.git) | NimBLE abstraction: GAP, GATT server, bonding, multi-connection notify |
| `shared_values` | [esp32_shared_values](https://github.com/ikok07/esp32_shared_values.git) | Mutex + event-group pub/sub for scalar and pointer values |
| `tasks_scheduler` | [esp32_tasks_scheduler](https://github.com/ikok07/esp32_tasks_scheduler.git) | Declarative task descriptors with core pinning and lifecycle |
| `logger` | [esp32_logger](https://github.com/ikok07/esp32_logger.git) | Levelled logging over an injectable backend, with fatal handling |
| `fs` | [esp32_littlefs_driver](https://github.com/ikok07/esp32_littlefs_driver.git) | LittleFS mount/format and typed read/write/append/exists API |
| `power` | [esp32_power_driver](https://github.com/ikok07/esp32_power_driver.git) | DFS configuration and PM locks for frequency and light sleep |
| `timer` | [esp32_timer](https://github.com/ikok07/esp32_timer.git) | Timer helpers |

Both u-blox drivers ship with their own full API documentation — see [`components/ubx/README.md`](components/ubx/README.md) and [`components/m10/README.md`](components/m10/README.md).

---

## Building

Built against **ESP-IDF v6.0** with a C23-capable toolchain (`main/idf_component.yml` declares a floor of v4.1; the managed dependencies require ≥ 5.0).

```bash
git clone --recursive https://github.com/ikok07/esp32_ble_gps.git
cd esp32_ble_gps

idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

If the repository was cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

Managed dependencies (`espressif/led_strip`, `joltwallet/littlefs`) are resolved automatically by the IDF component manager.

Configuration that matters lives in a few well-marked places rather than in `menuconfig`:

| Setting | Location |
|---|---|
| GNSS UART pins, buffer sizes, storage paths | `main/gnss.c` (top of file) |
| Receiver config: constellations, nav model, rate, DOP, time pulse | `main/gnss.c` → `gnss_config_task()` |
| Device name, appearance, connection params, security | `main/bt.c` |
| GATT table, passkey | `main/bt-config.c` |
| Task priorities, stack depths, core assignment | `main/include/tasks_common.h` |
| CPU frequency range | `components/power/include/power.h` |
| Status LED pin and brightness | `main/include/status_led.h` |

Reference material used during development (u-blox M10 integration manual, BLE GATT specification supplement, assigned numbers) is kept in [`docs/`](docs/).

---

## Repository layout

```
main/
  main.c                  Bring-up sequence and task orchestration
  gnss.c                  UART framer, M10 configuration, MGA persistence
  telemetry-parser.c      NAV-PVT / NAV-DOP decoding into shared values
  bt.c                    BLE handle configuration and startup
  bt-config.c             GATT service/characteristic table, GAP callbacks
  bt-access-cb.c          Read/write handlers and SIG payload packing
  bt-notifications.c      Per-characteristic notification producers
  status_led.c            WS2812 state indicator
  log-config.c            Logger backend bound to ESP_LOGx
components/               Nine submodule drivers + app_state
docs/                     Datasheets and specification references
partitions.csv            Custom partition table (1 MB app / 3 MB LittleFS)
```

---

## Known limitations & roadmap

Stated plainly, because they're the honest state of the project:

- **Heading is always zero.** The Location and Speed payload sets the heading flag and reserves the field, but no magnetometer is fitted yet. A QMC5883L is the intended part (datasheet already in `docs/`); until then the field is a placeholder.
- **The BLE passkey is a compile-time constant** (`BLE_DEVICE_PASSWORD` in `main/bt-config.c`). Fine for bring-up on a device with no display; a shipping unit would derive it per-device or expose it out-of-band.
- **Navigation Data characteristic (`0x2A68`) is stubbed out**, pending a decision on what route/waypoint data is worth exposing.
- **No custom PCB.** Everything runs on a development board with the M10 wired in; a dedicated board is the natural next step.
- The nav-database export buffer is a fixed 12 KB — large enough in practice for the current constellation set, but it rejects rather than streams if the database ever exceeds it.

---

**Author:** Kaloyan Stefanov · Firmware in C23 on ESP-IDF / FreeRTOS
