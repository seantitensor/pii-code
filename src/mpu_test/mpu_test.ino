// MPU-6050 + HT16K33 4-digit 7-segment display
// MPU wiring:  SDA -> PB9, SCL -> PB8, VCC -> 3.3V, GND -> GND, AD0 -> GND
// HT16K33 wiring: SDA -> PB9, SCL -> PB8, VCC -> 5V,  GND -> GND
// Both devices share the same I2C bus (Wire2)

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
#define SEG_MINUS  0x40   // minus sign
#define SEG_BLANK  0x00   // blank

// HT16K33 display RAM: 8 x 16-bit words, but a 4-digit display uses
// positions 0,1,2,3 (colon is at word 2 on Adafruit-style backpacks)
static uint8_t dispBuf[8];  // 8 bytes (low bytes of each word pair)

static void htWrite(uint8_t reg, uint8_t val) {
  Wire2.beginTransmission(HT_ADDR);
  Wire2.write(reg);
  Wire2.write(val);
  Wire2.endTransmission();
}

static void htInit() {
  Wire2.beginTransmission(HT_ADDR);
  Wire2.write(0x21);          // System setup: oscillator ON
  Wire2.endTransmission();
  delay(1);
  Wire2.beginTransmission(HT_ADDR);
  Wire2.write(0x81);          // Display setup: display ON, no blink
  Wire2.endTransmission();
  Wire2.beginTransmission(HT_ADDR);
  Wire2.write(0xEF);          // Dimming: max brightness (16/16)
  Wire2.endTransmission();
}

// Write the 8-byte display buffer to the HT16K33
static void htFlush() {
  Wire2.beginTransmission(HT_ADDR);
  Wire2.write(0x00);          // Start at display RAM address 0
  for (uint8_t i = 0; i < 8; i++) {
    Wire2.write(dispBuf[i]);  // Low byte
    Wire2.write(0x00);        // High byte (unused for 7-seg)
  }
  Wire2.endTransmission();
}

// Display a signed integer (-999 to 9999) on the 4-digit display
static void htShowInt(int16_t val) {
  bool neg = (val < 0);
  if (neg) val = -val;
  if (val > 9999) val = 9999;

  dispBuf[0] = (neg && val < 1000) ? SEG_MINUS : (val >= 1000 ? SEG7[val / 1000 % 10] : SEG_BLANK);
  dispBuf[1] = (val >= 100)  ? SEG7[val / 100  % 10] : (neg && val < 100  ? SEG_MINUS : SEG_BLANK);
  dispBuf[2] = 0x00;          // colon position — leave off
  dispBuf[3] = (val >= 10)   ? SEG7[val / 10   % 10] : SEG_BLANK;
  dispBuf[4] = SEG7[val % 10];
  htFlush();
}

// ── MPU helpers ───────────────────────────────────────────────────────────────
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

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

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
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
  // Read accel (m/s^2)
  float ax = mpuRead16(REG_ACCEL_XOUT)     / ACCEL_SCALE * 9.80665f;
  float ay = mpuRead16(REG_ACCEL_XOUT + 2) / ACCEL_SCALE * 9.80665f;
  float az = mpuRead16(REG_ACCEL_XOUT + 4) / ACCEL_SCALE * 9.80665f;

  // Read gyro (deg/s)
  float gx = mpuRead16(REG_GYRO_XOUT)     / GYRO_SCALE;
  float gy = mpuRead16(REG_GYRO_XOUT + 2) / GYRO_SCALE;
  float gz = mpuRead16(REG_GYRO_XOUT + 4) / GYRO_SCALE;

  // Show accel X in cm/s^2 on the display (e.g. 981 = 9.81 m/s^2)
  htShowInt((int16_t)(ax * 100));

  Serial.print("Accel (m/s^2) X: "); Serial.print(ax, 2);
  Serial.print("   Y: ");            Serial.print(ay, 2);
  Serial.print("   Z: ");            Serial.println(az, 2);

  Serial.print("Gyro  (deg/s) X: "); Serial.print(gx, 2);
  Serial.print("   Y: ");            Serial.print(gy, 2);
  Serial.print("   Z: ");            Serial.println(gz, 2);

  Serial.println("---");
  delay(200);
}
