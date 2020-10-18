// class default I2C address is 0x68
// AD0 low = 0x68
// AD0 high = 0x69

//Binding address
// 0x2CAB33CC6AF6

// declare static variables
#define OUTPUT_READABLE_WORLDACCEL

// include libraries
//#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include "Wire.h"
#endif
//#include "Wire.h"

// declare addresses for both accelerometers
MPU6050 mpu(0x68);  //AD0 low

// declare pins
const int INTERRUPT_PIN = 2;
const int LED_PIN = 13;

// declare variables
bool blinkState = false;
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

// orientation/motion vars - following libraries
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];         // [psi, theta, phi]    Euler angle container
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

// Array for storing accelerometer and gyroscope data
volatile long dataSaved[3] = {0, 0, 0};
volatile float dataGyro[3] = {0, 0, 0};
int arr[6] = {0, 0, 0, 0, 0, 0};

volatile long accel_start[3] = {0, 0, 0};
volatile long accel_end[3] = {0, 0, 0};
volatile float ypr_start[3] = {0.0, 0.0, 0.0};
volatile float ypr_end[3] = {0.0, 0.0, 0.0};

volatile long xDiff = 0;
volatile long yDiff = 0;
volatile long zDiff = 0;
volatile long yawDiff = 0;
volatile long pitchDiff = 0;
volatile long rollDiff = 0;

bool sendFlag = false;

// indicates whether MPU interrupt pin has gone high
volatile bool mpuInterrupt = false;

// comms 1 declarations
#define BASE_ITOA 30
#define ZERO_OFFSET 13500

unsigned long previous_timeA = 0;
unsigned long previous_timeB = 0;
long count = 0;
volatile bool handshake_flag = false;
char buff[20];


// DMP (Digital Motion Processor) to take values
void dmpDataReady() {
  mpuInterrupt = true;
}

void setup_accelerometer(MPU6050 mpu, int INTERRUPT_PIN) {
  // start mpu
  mpu.initialize();
  // to check which accelerometer we are reading
  pinMode(INTERRUPT_PIN, INPUT);
  // Start DMP in accelerometer
  devStatus = mpu.dmpInitialize();

  // Offsets and calibrations
  mpu.setXAccelOffset(-950);
  mpu.setYAccelOffset(1043);
  mpu.setZAccelOffset(183);
  mpu.setXGyroOffset(-9);
  mpu.setYGyroOffset(-4);
  mpu.setZGyroOffset(54);

  if (devStatus == 0) {
    // Calibration Time: generate offsets and calibrate our MPU6050
//    mpu.CalibrateAccel(6);
//    mpu.CalibrateGyro(6);
//    mpu.PrintActiveOffsets();
    // turn on the DMP, now that it's ready
    mpu.setDMPEnabled(true);

    // enable Arduino interrupt detection
    attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
    mpuIntStatus = mpu.getIntStatus();

    // set our DMP Ready flag so the main loop() function knows it's okay to use it
    dmpReady = true;

    // get expected DMP packet size for later comparison
    packetSize = mpu.dmpGetFIFOPacketSize();
  } else {
    // ERROR!
    // 1 = initial memory load failed
    // 2 = DMP configuration updates failed
    // (if it's going to break, usually the code will be 1)
//    Serial.print(F("DMP Initialization failed (code "));
//    Serial.print(devStatus);
//    Serial.println(F(")"));
  }

}

// followed libraries test units
int loop_single() {
  // if programming failed, don't try to do anything
  if (!dmpReady) return;

  // wait for MPU interrupt or extra packet(s) available
  while (!mpuInterrupt && fifoCount < packetSize) {
    if (mpuInterrupt && fifoCount < packetSize) {
      // try to get out of the infinite loop
      fifoCount = mpu.getFIFOCount();
    }
  }

  // reset interrupt flag and get INT_STATUS byte
  mpuInterrupt = false;
  mpuIntStatus = mpu.getIntStatus();

  // get current FIFO count
  fifoCount = mpu.getFIFOCount();

  if (fifoCount < packetSize) {

  }

  else if ((mpuIntStatus & (0x01 << MPU6050_INTERRUPT_FIFO_OFLOW_BIT)) || fifoCount >= 1024) {
    // reset so we can continue cleanly
    mpu.resetFIFO();
//    Serial.println(F("FIFO overflow!"));

  } else if (mpuIntStatus & (0x01 << MPU6050_INTERRUPT_DMP_INT_BIT)) {

    while (fifoCount >= packetSize) {
      mpu.getFIFOBytes(fifoBuffer, packetSize);
      fifoCount -= packetSize;

    }

    // for gravity and accelerometer values
#ifdef OUTPUT_READABLE_QUATERNION
    mpu.dmpGetQuaternion(&q, fifoBuffer);
#endif

#ifdef OUTPUT_READABLE_WORLDACCEL
    // display initial world-frame acceleration, adjusted to remove gravity
    // and rotated based on known orientation from quaternion in previous definition
    // values gotten from libraries
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetAccel(&aa, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
    mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

    // storing values taken into our array
    dataSaved[0] = aaWorld.x;
    dataSaved[1] = aaWorld.y;
    dataSaved[2] = aaWorld.z;
    dataGyro[0] = ypr[0] * 180 / M_PI;
    dataGyro[1] = ypr[1] * 180 / M_PI;
    dataGyro[2] = ypr[2] * 180 / M_PI;

    // Success
    return 1;
  }
#endif

  // blink LED to indicate activity
  blinkState = !blinkState;
  digitalWrite(LED_PIN, blinkState);


}

// ==================================================
// ===                 COMMS 1                    ===
// ==================================================
//compress + computeChecksum gives a 1-byte checksum
int compress(int num) {
  if (num < 10) {
      return num;
  }
  return num%10 ^ compress(num/10);
}
int computeChecksum(char *s) {
  int output = 0;
  for (int i = 0; i < strlen(s); i++) {
    output ^= s[i];
  }
  return compress(output);
}


//Function to offset raw values by ZERO_OFFSET
int getOffset(int num) {
  return (num + ZERO_OFFSET)%(ZERO_OFFSET*2);
}

//Function to pad numbers being sad with extra 0s. b: buffer to be padded, s: original numbers
void setPad(char *b, char *s) {
  int padsize = 3 - strlen(s);
  int startIdx = strlen(b);
  for (int i = startIdx; i < startIdx+padsize; i++) {
    b[i] = '0';
  }
  int y = 0;
  startIdx = strlen(b);
  for (int i = startIdx; i < startIdx+strlen(s); i++) {
    b[i] = s[y++];
  }
}
//Task to check handshake with laptop
void checkHandshake() {
  if (!handshake_flag && Serial.available()) {
    if (Serial.read() == 'H') {
      buff[0] = 'A';
      Serial.print(buff);
      handshake_flag = true;
      memset(buff, 0, 20);
//      delay(50);
    }
  }  
}

//Task to send 1st array of values (from arm sensor) ~15-20Hz
void sendArmData() {
  //Send arm sensor
  if (handshake_flag && sendFlag == true) {
    int i = 0;
    char temp[4];
      for (int i = 0; i <6; i++) {
      itoa(getOffset((int)arr[i]), temp, BASE_ITOA);
      
      setPad(buff, temp); //copies temp onto buff with pads

      }
    int checksumDecimal = computeChecksum(buff);
    //checksum from 'a' to 'p' for i==0
    buff[18] = checksumDecimal + 'a';
    
    Serial.print(buff);
    memset(buff, 0, 20);
//    delay(23);
//    previous_timeA = millis();
//    
//    //simulate changing values
//    arr[0] = (arr[0] + 1)%10;
  }
}

// ==================================================
// ===                 COMMS 1 END                ===
// ==================================================

void setup() {
  // for debugging
  Serial.begin(115200);

  // inititalize wire library
  Wire.begin();
  //  Wire.setClock(400000);
  TWBR = 24; // 400kHz I2C clock
#if I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
  Fastwire::setup(400, true);
#endif

  // initialize pins
  setup_accelerometer(mpu, INTERRUPT_PIN);
}

void loop() {

  checkHandshake();
  sendArmData();

  while (1) {
    if (loop_single() == 1) {
      accel_start[0] = dataSaved[0];
      accel_start[1] = dataSaved[1];
      accel_start[2] = dataSaved[2];
      ypr_start[0] = dataGyro[0];
      ypr_start[1] = dataGyro[1];
      ypr_start[2] = dataGyro[2];
      break;
    }
  }
//  delay(50);

  while (1) {
    if (loop_single() == 1) {
      accel_end[0] = dataSaved[0];
      accel_end[1] = dataSaved[1];
      accel_end[2] = dataSaved[2];
      ypr_end[0] = dataGyro[0];
      ypr_end[1] = dataGyro[1];
      ypr_end[2] = dataGyro[2];
      break;
    }
  }
//  delay(50);

  // Difference between 2 xyz and ypr values to detect sudden movement
  xDiff = accel_end[0] - accel_start[0];
  yDiff = accel_end[1] - accel_start[1];
  zDiff = accel_end[2] - accel_start[2];
  yawDiff = (ypr_end[0] - ypr_start[0]) * 100;
  pitchDiff = (ypr_end[1] - ypr_start[1]) * 100;
  rollDiff = (ypr_end[2] - ypr_start[2]) * 100;

  xDiff = abs(xDiff);
  yDiff = abs(yDiff);
  zDiff = abs(zDiff);
  yawDiff = abs(yawDiff);
  pitchDiff = abs(pitchDiff);
  rollDiff = abs(rollDiff);

  //  Serial.print("[");
  //  Serial.print(dataSaved[0]);
  //  Serial.print(",");
  //  Serial.print(dataSaved[1]);
  //  Serial.print(",");
  //  Serial.print(dataSaved[2]);
  //  Serial.print(",");
  //  Serial.print(dataGyro[0]);
  //  Serial.print(",");
  //  Serial.print(dataGyro[1]);
  //  Serial.print(",");
  //  Serial.print(dataGyro[2]);
  //  Serial.println("]");

  if ((abs(yawDiff) >= 10 || abs(pitchDiff) >= 10 || abs(rollDiff) >= 10) &&
      (abs(xDiff) >= 150 || abs(yDiff) >= 150 || abs(zDiff) >= 150)) {
//    Serial.println("SendData");
    sendFlag = true;
    arr[0] = xDiff;
    arr[1] = yDiff;
    arr[2] = zDiff;
    arr[3] = yawDiff;
    arr[4] = pitchDiff;
    arr[5] = rollDiff;
  } else {
    arr[0] = 0;
    arr[1] = 0;
    arr[2] = 0;
    arr[3] = 0;
    arr[4] = 0;
    arr[5] = 0;
  }

//  Serial.print("[");
//  Serial.print(arr[0]);
//  Serial.print(",");
//  Serial.print(arr[1]);
//  Serial.print(",");
//  Serial.print(arr[2]);
//  Serial.print(",");
//  Serial.print(arr[3]);
//  Serial.print(",");
//  Serial.print(arr[4]);
//  Serial.print(",");
//  Serial.print(arr[5]);
//  Serial.println("]");

}