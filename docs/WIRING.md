# Wiring Guide — DS18B20 Sensors on WT32-ETH01

This document covers the electrical design for connecting six DS18B20
sensors to a **WT32-ETH01 (ESP32-ETH01)** board: five refrigerator sensors
on one shared OneWire bus and one outdoor sensor on a separate pin.

---

## 1. Why the WT32-ETH01 GPIO choice matters

The WT32-ETH01 dedicates many pins to the **LAN8720 Ethernet PHY** over the
RMII interface, so very few GPIOs are actually free. Pins used by Ethernet
**must not** be reused:

| Function          | GPIO |
|-------------------|------|
| RMII REF CLK (in) | 0    |
| MDC               | 23   |
| MDIO              | 18   |
| PHY power enable  | 16   |
| TXD0 / TXD1 / TX_EN | 19 / 22 / 21 |
| RXD0 / RXD1 / CRS_DV | 25 / 26 / 27 |

Of the remaining broken-out pins, several are **strapping pins** (GPIO2,
GPIO12, GPIO15) whose state at boot selects flash voltage / boot mode. A
4.7 kΩ pull-up to 3.3 V on a strapping pin can prevent the board from
booting. We therefore use **non-strapping, free pins**:

| Use                         | GPIO | Notes                              |
|-----------------------------|------|------------------------------------|
| Fridge OneWire bus (×5)     | **4**  | Free, not a strapping pin          |
| Outdoor OneWire (×1)        | **14** | Free, not a strapping pin          |

> These are defined in `src/config.h` as `PIN_ONEWIRE_FRIDGE` and
> `PIN_ONEWIRE_OUTDOOR`. If your board variant routes these differently,
> change them there.

---

## 2. How multiple DS18B20s share one data line

Each DS18B20 contains a **unique, factory-laser 64-bit ROM ID**. The 1-Wire
protocol lets the master (ESP32) address each device individually by that
ID, so any number of sensors can share **one data wire**:

1. The master issues a *Search ROM* command and enumerates every device's
   64-bit address on the bus.
2. To read a specific sensor, the master sends *Match ROM* + that device's
   address, then the temperature command. Only the addressed device replies.

The firmware enumerates all addresses at boot, **sorts them**, and maps the
sorted order to *Refrigerator 1…5*. Because the sort is deterministic, the
same physical sensor keeps the same slot across reboots. (Sorted ROM IDs are
shown per sensor on the dashboard so you can label them physically.)

This is **multi-drop** wiring — all sensors share three rails: **VDD, GND,
DATA** — with a *single* pull-up resistor for the whole bus.

---

## 3. Pull-up resistor

A single **4.7 kΩ** resistor is connected between **DATA and 3.3 V**. It
holds the open-drain bus high between transmissions. One resistor serves the
entire fridge bus (do **not** add one per sensor). The outdoor bus on GPIO14
needs its own 4.7 kΩ pull-up.

For long runs or many devices, 3.3 kΩ may improve edge rates; start with
4.7 kΩ.

---

## 4. Wiring diagram

### 4.1 Refrigerator bus (5 sensors, shared)

```
                         3.3V ──────────────┬───────────────────────────┐
                                            │                           │
                                          [4.7kΩ]                        │
                                            │                           │
   ESP32 GPIO4 ─────────────────────────────┴───── DATA ────────────────┤  (bus)
                                                                         │
   GND ──────────────────────────────────────────── GND ────────────────┤
                                                                         │
        ┌──────────────┬──────────────┬──────────────┬──────────────┬───┴──────────┐
        │ DS18B20 #1   │ DS18B20 #2   │ DS18B20 #3   │ DS18B20 #4   │ DS18B20 #5    │
        │ VDD DATA GND │ VDD DATA GND │ VDD DATA GND │ VDD DATA GND │ VDD DATA GND  │
        └──────────────┴──────────────┴──────────────┴──────────────┴───────────────┘
       (Fridge 1)     (Fridge 2)     (Fridge 3)     (Fridge 4)     (Fridge 5)
```

Each DS18B20 connects: **VDD → 3.3 V, GND → GND, DATA → GPIO4** (all in
parallel). **Use external (normal) power — connect VDD to 3.3 V. Do not use
parasite power** for a 5-sensor, long-cable installation; it is far less
reliable.

### 4.2 Outdoor sensor (separate pin)

```
   3.3V ──────────┬────────────────
                  │
                [4.7kΩ]
                  │
   GPIO14 ─────────┴──── DATA ──── DS18B20 (outdoor)
   GND  ───────────────── GND
   3.3V ───────────────── VDD
```

---

## 5. Pin assignment table (full)

| Signal              | WT32-ETH01 pin | Connects to                      |
|---------------------|----------------|----------------------------------|
| Fridge bus DATA     | GPIO4          | DATA of fridge sensors 1–5 + 4.7 kΩ to 3.3 V |
| Outdoor DATA        | GPIO14         | DATA of outdoor sensor + 4.7 kΩ to 3.3 V     |
| Sensor power        | 3.3 V          | VDD of all sensors + both pull-ups |
| Ground              | GND            | GND of all sensors               |
| Power input         | 5 V / USB      | Board USB-serial / 5 V header    |
| Ethernet            | RJ45           | LAN switch / router              |

> **Power note:** DS18B20s are low-current (~1.5 mA active). The board's
> 3.3 V regulator easily powers six. Power the board from a quality 5 V/1 A
> USB supply.

---

## 6. Cable recommendations

- **Use CAT5e or CAT6** twisted-pair cable. It is cheap, shielded variants
  exist (F/UTP, S/FTP), and the twisted pairs strongly reject noise.
- **Pairing scheme** (important for long runs):
  - **DATA + GND in the *same* twisted pair** — this is the critical signal
    loop; keeping them paired maximises noise immunity.
  - **VDD + GND** (a second pair, or double up a conductor for GND).
- Connect **shield (if any) to GND at the ESP32 end only** (avoid ground
  loops).

| Conductor | CAT5e pair       | Signal |
|-----------|------------------|--------|
| Wire A    | Pair 1 (e.g. blue) | DATA |
| Wire B    | Pair 1 (blue/white)| GND  |
| Wire C    | Pair 2 (orange)    | VDD  |
| Wire D    | Pair 2 (orange/wht)| GND  |

---

## 7. Maximum cable length

- DS18B20 over 1-Wire is commonly reliable to **a few metres up to ~20–30 m
  total bus length** with a 4.7 kΩ pull-up and external power.
- Beyond that, capacitance and reflections degrade the edges. Mitigations:
  - Lower the pull-up to **3.3 kΩ – 2.2 kΩ**.
  - Use a **star/stub-minimised topology** — prefer a single daisy-chain
    (linear bus) over long stubs branching off.
  - For very long or noisy installations, add a **100–120 Ω series resistor**
    at the master DATA pin, and/or a small **100 pF** cap from DATA to GND at
    the far end to round edges.
  - Consider an active 1-Wire driver (e.g. DS2482) for >50 m.

For a typical commercial fridge bank (sensors within ~10–20 m), CAT5e + one
4.7 kΩ pull-up + external power is robust.

---

## 8. Noise-immunity best practices

- Keep DATA and its GND in the **same twisted pair**.
- Route sensor cable **away from compressor power, contactors, and mains**;
  cross power lines at 90° if unavoidable.
- Use **external power**, not parasite power, for multi-sensor long runs.
- Add **100 nF decoupling capacitors** between VDD and GND at each sensor for
  long cables (reduces glitches during conversion current spikes).
- Ground the cable shield **at one end only**.
- Keep **stub lengths short** (the drop from the bus to each sensor).
- Mechanically strain-relieve connectors; vibration from compressors loosens
  joints over time — crimp or solder rather than relying on screw terminals
  alone.

---

## 9. Electrical recommendations for long-term operation

| Item                | Recommendation                                   |
|---------------------|--------------------------------------------------|
| Pull-up             | 4.7 kΩ (start); 3.3 kΩ for long/loaded buses     |
| Power scheme        | External 3.3 V (never parasite for this build)   |
| Decoupling          | 100 nF VDD–GND at each sensor on long runs       |
| Cable               | CAT5e/CAT6 twisted pair, DATA+GND paired         |
| Shield              | Grounded at ESP32 end only                       |
| Series DATA resistor| 100–120 Ω at master for long/reflective buses    |
| Connectors          | Crimped/soldered, strain-relieved, IP-rated in cold/damp |
| Sensor type         | Use genuine Maxim/Analog DS18B20 (clones drift)  |
| Probe style         | Stainless-steel waterproof probes inside fridges |
