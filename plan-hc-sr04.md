# Plan: HC-SR04 Ultrasonic Distance Sensor Integration

## Context

- **Platform**: Arduino (`.ino` sketch), same environment as the existing `mpu_test.ino`
- **Framework**: Arduino core (Wire, Serial, `pulseIn`, `micros`, `delayMicroseconds`)
- **Existing code**: `pii-code/src/mpu_test/mpu_test.ino` — reads an MPU-6050 gyro over I²C and prints to Serial at 115200 baud
- **Goal**: Add HC-SR04 support as a new, self-contained sketch (and optionally show how to merge it with the MPU sketch)

---

## HC-SR04 Hardware Protocol (from datasheet)

| Parameter | Value |
|---|---|
| Supply voltage | 5 V |
| Working current | 15 mA |
| Trigger input | ≥ 10 µs HIGH TTL pulse on TRIG pin |
| Echo output | HIGH pulse whose width = round-trip time |
| Distance formula | `distance_cm = (echo_us × 0.0343) / 2` |
| Min range | 2 cm |
| Max range | 400 cm |
| Measurement angle | 15° |
| Recommended cycle | ≥ 60 ms between measurements (avoid echo overlap) |

**Timing sequence**
1. Pull TRIG LOW for 2 µs (clean start)
2. Pull TRIG HIGH for ≥ 10 µs
3. Pull TRIG LOW
4. Wait for ECHO pin to go HIGH (sensor fires 8× 40 kHz bursts)
5. Measure how long ECHO stays HIGH (`pulseIn`)
6. Convert pulse width → distance

---

## Files to Create / Modify

| Action | File |
|---|---|
| **Create** | `pii-code/src/hcsr04/hcsr04.ino` — standalone HC-SR04 sketch |
| **Create** | `pii-code/src/combined/combined.ino` *(optional, Step 4)* — MPU-6050 + HC-SR04 together |

No existing files need to be modified for the standalone sketch.

---

## Implementation Steps

### Step 1 — Create the standalone HC-SR04 sketch

**File**: `pii-code/src/hcsr04/hcsr04.ino`

```
pii-code/src/hcsr04/hcsr04.ino
```

Contents:

```cpp
// HC-SR04 Ultrasonic Distance Sensor
// Wiring:
//   VCC  -> 5V
//   GND  -> GND
//   TRIG -> Pin 9
//   ECHO -> Pin 10
//
// Sensor specs:
//   Trigger: >=10 us HIGH pulse
//   Echo:    HIGH pulse width proportional to round-trip time
//   Range:   2 cm – 400 cm
//   Cycle:   >= 60 ms between measurements

#define TRIG_PIN  9
#define ECHO_PIN  10

#define SOUND_CM_PER_US  0.0343f   // speed of sound at ~20°C, cm/µs
#define TIMEOUT_US       30000UL   // ~5 m max; avoids blocking on no echo

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  delay(500);  // let sensor settle
  Serial.println("HC-SR04 ready.");
}

// Returns distance in cm, or -1.0 on timeout / out-of-range
float readDistanceCm() {
  // 1. Ensure TRIG is LOW before firing
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // 2. Fire a >=10 us trigger pulse
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // 3. Measure echo pulse width (µs)
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, TIMEOUT_US);

  if (duration == 0) {
    return -1.0f;  // timeout — no object detected in range
  }

  // 4. Convert: distance = (time * speed_of_sound) / 2
  float distance = (duration * SOUND_CM_PER_US) / 2.0f;

  // 5. Clamp to sensor's valid range
  if (distance < 2.0f || distance > 400.0f) {
    return -1.0f;
  }

  return distance;
}

void loop() {
  float dist = readDistanceCm();

  if (dist < 0.0f) {
    Serial.println("Distance: out of range");
  } else {
    Serial.print("Distance: ");
    Serial.print(dist, 1);
    Serial.println(" cm");
  }

  delay(60);  // minimum recommended cycle time
}
```

**Key design decisions:**
- `pulseIn` with a timeout (`TIMEOUT_US = 30000`) prevents the sketch from blocking forever if no echo returns.
- The `-1.0f` sentinel makes it easy for callers to detect invalid readings.
- The 60 ms `delay` matches the datasheet's recommended minimum cycle time to prevent the previous echo from interfering with the next trigger.

---

### Step 2 — Pin assignment

Choose two free digital pins on your Arduino. The sketch defaults to:

| Signal | Arduino Pin |
|---|---|
| TRIG | D9 |
| ECHO | D10 |

Change `TRIG_PIN` / `ECHO_PIN` at the top of the file if those pins are already in use (e.g., by a servo or SPI peripheral).

> **Note**: The HC-SR04 ECHO pin outputs 5 V. Most Arduino boards (Uno, Nano, Mega) are 5 V tolerant on digital inputs, so no level shifter is needed. If you are using a 3.3 V board (e.g., Arduino Due, Nano 33, Raspberry Pi Pico), add a voltage divider (1 kΩ + 2 kΩ) or a level-shifter on the ECHO line.

---

### Step 3 — Wiring summary

```
HC-SR04          Arduino
-------          -------
VCC    --------> 5V
GND    --------> GND
TRIG   --------> D9
ECHO   --------> D10
```

---

### Step 4 (Optional) — Combined MPU-6050 + HC-SR04 sketch

**File**: `pii-code/src/combined/combined.ino`

Merge both sensors into one sketch:
- `setup()`: initialise Serial, I²C (Wire on A4/A5), MPU-6050, and the HC-SR04 pins.
- `loop()`:
  1. Call `readDistanceCm()` (non-blocking with timeout).
  2. Call `mpu.getEvent()` for gyro data.
  3. Print both to Serial.
  4. `delay(60)` to respect the HC-SR04 cycle time (fast enough for the gyro too).

No library conflicts — HC-SR04 uses only GPIO and `pulseIn`; MPU-6050 uses I²C. They are completely independent.

---

## Validation Checklist

- [ ] Sketch compiles without errors in Arduino IDE / CLI
- [ ] Serial Monitor (115200 baud) shows distance values when an object is placed in front of the sensor
- [ ] Readings are stable at ~10 cm, ~30 cm, ~100 cm (compare with a ruler)
- [ ] `out of range` is printed when no object is within 400 cm
- [ ] No blocking observed when ECHO pin is left floating (timeout fires within ~30 ms)
- [ ] If combining with MPU sketch: gyro values still update correctly alongside distance

---

## Notes & Caveats

- **`pulseIn` is blocking** — it halts the CPU while waiting for the echo. For a more responsive system (e.g., running a motor controller simultaneously), replace `pulseIn` with an external interrupt on the ECHO pin and use `micros()` to timestamp the rising and falling edges. This is a future enhancement if needed.
- **Temperature compensation**: Speed of sound varies with temperature (~0.6 m/s per °C). The MPU-6050 provides a temperature reading (`temp.temperature`) that could be used to refine the formula: `speed_cm_us = (331.3 + 0.606 * temp_C) / 10000.0`.
- **Power**: The HC-SR04 draws up to 15 mA from the 5 V rail — well within Arduino's onboard regulator limits.
