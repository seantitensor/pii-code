// BRAAVE — MPU-6050 + HT16K33 + AD8232 ECG
// MPU-6050 wiring: SDA -> PB9, SCL -> PB8, VCC -> 3.3V, GND -> GND, AD0 -> GND
// HT16K33 wiring: SDA -> PB9, SCL -> PB8, VCC -> 5V,  GND -> GND
// AD8232 wiring:  OUTPUT -> PA0, LO+ -> PA7, LO- -> PB6, VCC -> 3.3V, GND -> GND
// MPU + HT16K33 share the same I2C bus (Wire2)

#include <Wire.h>

// ── I2C bus ──────────────────────────────────────────────────────────────────
TwoWire Wire2(PB9, PB8);   // SDA=PB9, SCL=PB8

// ── MPU-6050 ─────────────────────────────────────────────────────────────────
#define MPU_ADDR        0x68
#define REG_PWR_MGMT_1  0x6B
#define REG_ACCEL_XOUT  0x3B
#define REG_GYRO_XOUT   0x43
#define ACCEL_SCALE     16384.0f   // LSB/g  (±2g default)
#define GYRO_SCALE      131.0f     // LSB/(deg/s) (±250 deg/s default)

// ── HT16K33 ──────────────────────────────────────────────────────────────────
#define HT_ADDR         0x70       // A2=A1=A0=GND → 0x70

// 7-segment encoding for digits 0-9 (segments: .GFEDCBA)
static const uint8_t SEG7[10] = {
  0x3F, // 0
  0x06, // 1
  0x5B, // 2
  0x4F, // 3
  0x66, // 4
  0x6D, // 5
  0x7D, // 6
  0x07, // 7
  0x7F, // 8
  0x6F, // 9
};
#define SEG_MINUS  0x40
#define SEG_BLANK  0x00

static uint8_t dispBuf[8];

static void htInit() {
  Wire2.beginTransmission(HT_ADDR);
  Wire2.write(0x21);          // oscillator ON
  Wire2.endTransmission();
  delay(1);
  Wire2.beginTransmission(HT_ADDR);
  Wire2.write(0x81);          // display ON, no blink
  Wire2.endTransmission();
  Wire2.beginTransmission(HT_ADDR);
  Wire2.write(0xEF);          // max brightness
  Wire2.endTransmission();
}

static void htFlush() {
  Wire2.beginTransmission(HT_ADDR);
  Wire2.write(0x00);
  for (uint8_t i = 0; i < 8; i++) {
    Wire2.write(dispBuf[i]);
    Wire2.write(0x00);
  }
  Wire2.endTransmission();
}

static void htShowInt(int16_t val) {
  bool neg = (val < 0);
  if (neg) val = -val;
  if (val > 9999) val = 9999;

  dispBuf[0] = (neg && val < 1000) ? SEG_MINUS : (val >= 1000 ? SEG7[val / 1000 % 10] : SEG_BLANK);
  dispBuf[1] = (val >= 100)  ? SEG7[val / 100  % 10] : (neg && val < 100 ? SEG_MINUS : SEG_BLANK);
  dispBuf[2] = 0x00;          // colon — off
  dispBuf[3] = (val >= 10)   ? SEG7[val / 10   % 10] : SEG_BLANK;
  dispBuf[4] = SEG7[val % 10];
  htFlush();
}

// ── MPU-6050 helpers ──────────────────────────────────────────────────────────
static void mpuWrite(uint8_t reg, uint8_t val) {
  Wire2.beginTransmission(MPU_ADDR);
  Wire2.write(reg);
  Wire2.write(val);
  Wire2.endTransmission();
}

static int16_t mpuRead16(uint8_t reg) {
  Wire2.beginTransmission(MPU_ADDR);
  Wire2.write(reg);
  Wire2.endTransmission(false);
  Wire2.requestFrom(MPU_ADDR, 2);
  return (int16_t)((Wire2.read() << 8) | Wire2.read());
}

// ── AD8232 ECG ───────────────────────────────────────────────────────────────
#define ECG_PIN   PA0   // analog output from AD8232
#define LO_PLUS   PA7   // leads-off detect +
#define LO_MINUS  PB6   // leads-off detect -

// 250Hz ticker — incremented in loop, ECG sampled every 4ms
static uint32_t lastEcgMs = 0;

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  // ECG pins
  pinMode(LO_PLUS,  INPUT);
  pinMode(LO_MINUS, INPUT);

  Wire2.begin();

  // Init MPU-6050
  mpuWrite(REG_PWR_MGMT_1, 0x00);  // wake up
  delay(100);

  Wire2.beginTransmission(MPU_ADDR);
  Wire2.write(0x75);                // WHO_AM_I
  Wire2.endTransmission(false);
  Wire2.requestFrom(MPU_ADDR, 1);
  uint8_t who = Wire2.read();
  if (who != 0x68) {
    Serial.print("MPU-6050 NOT found. WHO_AM_I=0x");
    Serial.println(who, HEX);
    while (1) { delay(10); }
  }
  Serial.println("MPU-6050 connected.");

  // Init HT16K33
  htInit();
  Serial.println("HT16K33 connected.");
  Serial.println("AD8232 ready.");
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // ── ECG @ 250Hz (every 4ms) ───────────────────────────────────────────────
  if (now - lastEcgMs >= 4) {
    lastEcgMs = now;
    if (digitalRead(LO_PLUS) || digitalRead(LO_MINUS)) {
      Serial.println("ECG: LEADS OFF");
    } else {
      Serial.print("ECG: ");
      Serial.println(analogRead(ECG_PIN));
    }
  }
  

  // ── IMU @ ~5Hz (every 200ms) ──────────────────────────────────────────────
  static uint32_t lastImuMs = 0;
  if (now - lastImuMs >= 200) {
    lastImuMs = now;

    // Accel (m/s^2)
    float ax = mpuRead16(REG_ACCEL_XOUT)     / ACCEL_SCALE * 9.80665f;
    float ay = mpuRead16(REG_ACCEL_XOUT + 2) / ACCEL_SCALE * 9.80665f;
    float az = mpuRead16(REG_ACCEL_XOUT + 4) / ACCEL_SCALE * 9.80665f;

    // Gyro (deg/s)
    float gx = mpuRead16(REG_GYRO_XOUT)     / GYRO_SCALE;
    float gy = mpuRead16(REG_GYRO_XOUT + 2) / GYRO_SCALE;
    float gz = mpuRead16(REG_GYRO_XOUT + 4) / GYRO_SCALE;

    // Show accel X (cm/s^2) on display
    htShowInt((int16_t)(ax * 100));

    // Serial.print("ACCEL (m/s²) X: "); Serial.print(ax, 2);
    // Serial.print("  Y: ");            Serial.print(ay, 2);
    // Serial.print("  Z: ");            Serial.println(az, 2);

    // Serial.print("GYRO  (°/s)  X: "); Serial.print(gx, 2);
    // Serial.print("  Y: ");            Serial.print(gy, 2);
    // Serial.print("  Z: ");            Serial.println(gz, 2);

    // Serial.println("---");
  }
}
