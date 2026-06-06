// MPU-6050 direct I2C reader (no Adafruit library — saves ~9KB flash)
// Wiring: SDA -> PB9, SCL -> PB8, VCC -> 3.3V, GND -> GND, AD0 -> GND (addr 0x68)

#include <Wire.h>

#define MPU_ADDR       0x68
#define REG_PWR_MGMT_1 0x6B
#define REG_ACCEL_XOUT 0x3B
#define REG_GYRO_XOUT  0x43
#define ACCEL_SCALE    16384.0f   // LSB/g  for ±2g default
#define GYRO_SCALE     131.0f     // LSB/(deg/s) for ±250 deg/s default

TwoWire Wire2(PB9, PB8);

// ── helpers ──────────────────────────────────────────────────────────────────
static void writeReg(uint8_t reg, uint8_t val) {
  Wire2.beginTransmission(MPU_ADDR);
  Wire2.write(reg);
  Wire2.write(val);
  Wire2.endTransmission();
}

static int16_t read16(uint8_t reg) {
  Wire2.beginTransmission(MPU_ADDR);
  Wire2.write(reg);
  Wire2.endTransmission(false);
  Wire2.requestFrom(MPU_ADDR, 2);
  return (int16_t)((Wire2.read() << 8) | Wire2.read());
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  Wire2.begin();

  // Wake the MPU-6050 (clears SLEEP bit)
  writeReg(REG_PWR_MGMT_1, 0x00);
  delay(100);

  // Verify WHO_AM_I (register 0x75 should return 0x68)
  Wire2.beginTransmission(MPU_ADDR);
  Wire2.write(0x75);
  Wire2.endTransmission(false);
  Wire2.requestFrom(MPU_ADDR, 1);
  uint8_t who = Wire2.read();

  if (who != 0x68) {
    Serial.print("MPU-6050 NOT found. WHO_AM_I=0x");
    Serial.println(who, HEX);
    Serial.println("Check SDA->PB9, SCL->PB8, 3.3V power, GND.");
    while (1) { delay(10); }
  }
  Serial.println("MPU-6050 connected.");
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
  // Accel (m/s^2)
  float ax = read16(REG_ACCEL_XOUT) / ACCEL_SCALE * 9.80665f;
  float ay = read16(REG_ACCEL_XOUT + 2) / ACCEL_SCALE * 9.80665f;
  float az = read16(REG_ACCEL_XOUT + 4) / ACCEL_SCALE * 9.80665f;

  // Gyro (deg/s)
  float gx = read16(REG_GYRO_XOUT)     / GYRO_SCALE;
  float gy = read16(REG_GYRO_XOUT + 2) / GYRO_SCALE;
  float gz = read16(REG_GYRO_XOUT + 4) / GYRO_SCALE;

  Serial.print("Accel (m/s^2) X: "); Serial.print(ax, 2);
  Serial.print("   Y: ");            Serial.print(ay, 2);
  Serial.print("   Z: ");            Serial.println(az, 2);

  Serial.print("Gyro  (deg/s) X: "); Serial.print(gx, 2);
  Serial.print("   Y: ");            Serial.print(gy, 2);
  Serial.print("   Z: ");            Serial.println(gz, 2);

  Serial.println("---");
  delay(200);
}
