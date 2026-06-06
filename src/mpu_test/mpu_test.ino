// MPU-6050 gyro reader using the Adafruit MPU6050 library
// Wiring: SDA -> A4, SCL -> A5, VCC -> 3.3V, GND -> GND

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  delay(500);

  // Force I2C onto YOUR pins first
  Wire.setSDA(A4);
  Wire.setSCL(A5);
  Wire.begin();

  Serial.println("Initializing MPU-6050...");

  // Pass our configured Wire bus into begin()
  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("MPU-6050 NOT found. Check SDA->A4, SCL->A5, 3.3V power, GND.");
    while (1) { delay(10); }   // stop here until fixed
  }
  Serial.println("MPU-6050 connected.");

  mpu.setGyroRange(MPU6050_RANGE_250_DEG);   // optional, this is the default
}

void loop() {
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  // Adafruit gives gyro in RADIANS/sec — convert to deg/s for readability
  float gx = gyro.gyro.x * 57.2958;
  float gy = gyro.gyro.y * 57.2958;
  float gz = gyro.gyro.z * 57.2958;

  Serial.print("Gyro (deg/s)  X: ");
  Serial.print(gx, 2);
  Serial.print("   Y: ");
  Serial.print(gy, 2);
  Serial.print("   Z: ");
  Serial.println(gz, 2);

  delay(200);
}