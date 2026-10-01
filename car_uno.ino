// ============================================================
//  自走車端 — Arduino UNO（有線版）
//  接收 Leonardo Serial1 傳來的速度段（'0'~'3'）
//  循跡：D2/D3 紅外線感測器
//  避障：HC-SR04（D4/D12），偵測到障礙物回傳 'O' 給 Leonardo
//  障礙物停止後，等 Leonardo 重新發速度段才繼續
// ============================================================

const byte LEFT1     = 8;
const byte LEFT2     = 9;
const byte LEFT_PWM  = 10;
const byte RIGHT1    = 7;
const byte RIGHT2    = 6;
const byte RIGHT_PWM = 5;
const byte IR_LEFT   = 3;
const byte IR_RIGHT  = 2;
const byte TRIG_PIN  = 4;
const byte ECHO_PIN  = 12;

const byte SPD_FAST_L = 180;
const byte SPD_FAST_R = 195;
const byte SPD_MID_L  = 130;
const byte SPD_MID_R  = 143;
const byte SPD_SLOW_L = 95;
const byte SPD_SLOW_R = 108;
const byte SPD_TURN   = 60;

const int  OBSTACLE_DIST       = 5;
const long ULTRASONIC_INTERVAL = 50;

byte currentSpeedLevel = 0;
byte currentPWM_L      = 0;
byte currentPWM_R      = 0;
bool obstacleDetected  = false;
unsigned long lastUltrasonicTime = 0;

void motorStop() {
  analogWrite(LEFT_PWM,  0);
  analogWrite(RIGHT_PWM, 0);
  digitalWrite(LEFT1,  LOW); digitalWrite(LEFT2,  LOW);
  digitalWrite(RIGHT1, LOW); digitalWrite(RIGHT2, LOW);
}

void motorForward(byte sL, byte sR) {
  digitalWrite(LEFT1,  HIGH); digitalWrite(LEFT2,  LOW);
  digitalWrite(RIGHT1, LOW); digitalWrite(RIGHT2, HIGH);
  analogWrite(LEFT_PWM,  sL);
  analogWrite(RIGHT_PWM, sR);
}

void motorTurnLeft(byte sL, byte sR) {
  digitalWrite(LEFT1,  HIGH); digitalWrite(LEFT2,  LOW);
  digitalWrite(RIGHT1, HIGH); digitalWrite(RIGHT2, LOW);
  analogWrite(LEFT_PWM,  SPD_TURN);
  analogWrite(RIGHT_PWM, sR);
}

void motorTurnRight(byte sL, byte sR) {
  digitalWrite(LEFT1,  HIGH); digitalWrite(LEFT2,  LOW);
  digitalWrite(RIGHT1, HIGH); digitalWrite(RIGHT2, LOW);
  analogWrite(LEFT_PWM,  sL);
  analogWrite(RIGHT_PWM, SPD_TURN);
}

void speedToPWM(byte level, byte &sL, byte &sR) {
  switch (level) {
    case 3: sL = SPD_FAST_L; sR = SPD_FAST_R; break;
    case 2: sL = SPD_MID_L;  sR = SPD_MID_R;  break;
    case 1: sL = SPD_SLOW_L; sR = SPD_SLOW_R; break;
    default: sL = 0; sR = 0; break;
  }
}

void updateMotor() {
  if (obstacleDetected || currentSpeedLevel == 0) {
    motorStop();
    return;
  }
  bool irL = digitalRead(IR_LEFT);
  bool irR = digitalRead(IR_RIGHT);
  if (!irL && irR) {
    motorTurnLeft(currentPWM_L, currentPWM_R);
  } else if (irL && !irR) {
    motorTurnRight(currentPWM_L, currentPWM_R);
  } else {
    motorForward(currentPWM_L, currentPWM_R);
  }
}

long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 30000);
  if (dur == 0) return 999;
  return dur / 58;
}

void setup() {
  Serial.begin(9600);
  pinMode(LEFT1,     OUTPUT);
  pinMode(LEFT2,     OUTPUT);
  pinMode(LEFT_PWM,  OUTPUT);
  pinMode(RIGHT1,    OUTPUT);
  pinMode(RIGHT2,    OUTPUT);
  pinMode(RIGHT_PWM, OUTPUT);
  pinMode(IR_LEFT,   INPUT);
  pinMode(IR_RIGHT,  INPUT);
  pinMode(TRIG_PIN,  OUTPUT);
  pinMode(ECHO_PIN,  INPUT);
  motorStop();
}

void loop() {
  if (Serial.available()) {
    char msg = Serial.read();
    if (msg >= '0' && msg <= '3') {
      currentSpeedLevel = msg - '0';
      speedToPWM(currentSpeedLevel, currentPWM_L, currentPWM_R);
      // 收到速度段後解除障礙物鎖定
      if (obstacleDetected && currentSpeedLevel > 0) {
        obstacleDetected = false;
      }
      updateMotor();
    }
  }
  if (millis() - lastUltrasonicTime >= ULTRASONIC_INTERVAL) {
    lastUltrasonicTime = millis();
    long dist = measureDistance();
    if (dist <= OBSTACLE_DIST && !obstacleDetected) {
      obstacleDetected = true;
      motorStop();
      Serial.write('O');  // 回傳給 Leonardo
    }
  }

  if (!obstacleDetected && currentSpeedLevel > 0) {
    updateMotor();
  }
}
