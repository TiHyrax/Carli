// ============================================================
//  握力成神系統 — Arduino Leonardo（最終版）
//  Serial1（D0/D1）→ 自走車 UNO
//  SoftwareSerial D4 → 燈牌 ESP32
//  DFPlayer：SoftwareSerial D10(RX) / D11(TX)
// ============================================================
//  接線：
//    KY-024 AO  → A0
//    DFPlayer TX  → D10（直接接）
//    DFPlayer RX  → D11（串 1kΩ 電阻）
//    DFPlayer SPK_1 → 喇叭正極
//    DFPlayer SPK_2 → 喇叭負極
//    Leonardo D1 → UNO D0 的 S
//    Leonardo D0 → UNO D1 的 S
//    Leonardo D4 → ESP32 D16 的 S
//    Leonardo GND → UNO 任意 G
//    Leonardo GND → ESP32 D16 的 G
// ============================================================

#include <SoftwareSerial.h>
#include <DFPlayerMini_Fast.h>

SoftwareSerial dfSerial(10, 11);
SoftwareSerial espSerial(5, 4);  // RX=D5, TX=D4 → 傳給 ESP32
DFPlayerMini_Fast dfPlayer;

const byte HALL_PIN   = A0;
const byte LED_STATUS = 13;

const int HALL_FAST    = 730;
const int HALL_MID     = 600;
const int HALL_SLOW    = 550;
const int HALL_STARTUP = 550;
const int HALL_RELEASE = 545;

const byte SFX_START    = 1;
const byte SFX_FASTER   = 2;
const byte SFX_SLOWER   = 3;
const byte SFX_FATIGUE  = 4;
const byte SFX_OBSTACLE = 5;
const byte SFX_RELEASE  = 6;

const unsigned long SFX_DURATION[] = {
  0, 3000, 2500, 3000, 4000, 3000, 2500
};

const unsigned long FATIGUE_TIME   = 20000UL;
const unsigned long START_DURATION = 2000UL;
const unsigned long PRINT_INTERVAL = 100UL;

enum SystemState { STATE_WAIT, STATE_ACTIVE, STATE_FINISHED };
SystemState sysState = STATE_WAIT;

bool systemActive    = false;
bool voicePlaying    = false;
unsigned long voiceEndTime = 0;
byte voiceQueue      = 0;
bool isHallHigh      = false;
unsigned long hallHighStartTime = 0;
bool fatiguePlayed   = false;
unsigned long hallActiveStart = 0;
bool releasePlayed   = false;
bool hasGrippedOnce  = false;
byte lastSpeedLevel  = 255;
byte lastSentLevel   = 255;
byte lastEspLevel    = 255;
unsigned long lastPrintTime = 0;
unsigned long lastBlinkTime = 0;

void requestVoice(byte track, bool urgent = false) {
  if (urgent) {
    dfPlayer.stop();
    delay(30);
    dfPlayer.play(track);
    voicePlaying = true;
    voiceEndTime = millis() + SFX_DURATION[track];
    voiceQueue   = 0;
  } else if (!voicePlaying) {
    dfPlayer.play(track);
    voicePlaying = true;
    voiceEndTime = millis() + SFX_DURATION[track];
  } else {
    if (voiceQueue == 0) voiceQueue = track;
  }
}

void updateVoice() {
  if (voicePlaying && millis() >= voiceEndTime) {
    voicePlaying = false;
    if (voiceQueue != 0) {
      byte next = voiceQueue;
      voiceQueue = 0;
      requestVoice(next);
    }
  }
}

void sendSpeedLevel(byte level) {
  if (level != lastSentLevel) {
    Serial1.write('0' + level);
    lastSentLevel = level;
  }
  // 同步傳給 ESP32
  if (level != lastEspLevel) {
    espSerial.listen();
    espSerial.write('0' + level);
    lastEspLevel = level;
    dfSerial.listen();
  }
}

void sendObstacleToEsp() {
  espSerial.listen();
  espSerial.write('O');  // 障礙物訊號
  lastEspLevel = 255;    // 強制下次重送
  dfSerial.listen();
}

byte getSpeedLevel(int val) {
  if (val > HALL_FAST) return 3;
  if (val > HALL_MID)  return 2;
  if (val > HALL_SLOW) return 1;
  return 0;
}

void printHallValue(int val) {
  if (millis() - lastPrintTime < PRINT_INTERVAL) return;
  lastPrintTime = millis();
  String bar = "[";
  int barLen = 0;
  const char* speedText;
  if      (val > HALL_FAST) { barLen = 20; speedText = "快速 <<<"; }
  else if (val > HALL_MID)  { barLen = 13; speedText = "中速 <<";  }
  else if (val > HALL_SLOW) { barLen = 7;  speedText = "慢速 <";   }
  else                      { barLen = 0;  speedText = "未啟動";   }
  for (int i = 0; i < 20; i++) bar += (i < barLen) ? '#' : '-';
  bar += "]";
  Serial.print(F("霍爾值：")); Serial.print(val);
  Serial.print('\t'); Serial.print(bar);
  Serial.print('\t'); Serial.println(speedText);
}

void blinkLED(unsigned long interval) {
  if (millis() - lastBlinkTime >= interval) {
    lastBlinkTime = millis();
    digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
  }
}

void setup() {
  Serial1.begin(9600);
  Serial.begin(9600);
  while (!Serial);
  pinMode(HALL_PIN,   INPUT);
  pinMode(LED_STATUS, OUTPUT);
  dfSerial.begin(9600);
  espSerial.begin(9600);
  delay(1000);
  dfSerial.listen();
  if (!dfPlayer.begin(dfSerial)) {
    Serial.println(F("DFPlayer 初始化失敗！"));
  } else {
    Serial.println(F("DFPlayer 正常！"));
  }
  dfPlayer.volume(28);
  delay(500);
  dfPlayer.play(1);
  lastSentLevel = 255;
  lastEspLevel  = 255;
  sendSpeedLevel(0);
  Serial.println(F("Leonardo Ready！持握 2 秒啟動系統"));
}

void loop() {
  int hallValue = analogRead(HALL_PIN);
  updateVoice();
  printHallValue(hallValue);

  if (Serial1.available()) {
    char msg = Serial1.read();
    if (msg == 'O' && sysState != STATE_FINISHED) {
      sendSpeedLevel(0);
      sendObstacleToEsp();
      requestVoice(SFX_OBSTACLE, true);
      sysState     = STATE_FINISHED;
      systemActive = false;
      Serial.println(F("⚠ 障礙物！系統結束 → 播放 0005.mp3"));
    }
  }

  switch (sysState) {
    case STATE_WAIT: {
      blinkLED(800);
      sendSpeedLevel(0);
      if (hallValue > HALL_STARTUP) {
        if (!isHallHigh) {
          isHallHigh        = true;
          hallHighStartTime = millis();
          Serial.println(F(">>> 偵測到握力，計時中..."));
        } else if (millis() - hallHighStartTime >= START_DURATION) {
          sysState        = STATE_ACTIVE;
          systemActive    = true;
          hasGrippedOnce  = false;
          hallActiveStart = millis();
          fatiguePlayed   = false;
          releasePlayed   = false;
          lastSentLevel   = 255;
          lastSpeedLevel  = 255;
          lastEspLevel    = 255;
          sendSpeedLevel(0);
          requestVoice(SFX_START);
          digitalWrite(LED_STATUS, HIGH);
          Serial.println(F("✓ 系統啟動！→ 播放 0001.mp3"));
        }
      } else {
        if (isHallHigh) {
          isHallHigh = false;
          Serial.println(F("✗ 握力中斷，重新計時"));
        }
        digitalWrite(LED_STATUS, LOW);
      }
      break;
    }

    case STATE_ACTIVE: {
      byte speedLevel = getSpeedLevel(hallValue);
      if (speedLevel > 0) {
        hasGrippedOnce  = true;
        releasePlayed   = false;
        if (hallActiveStart == 0) hallActiveStart = millis();
        unsigned long activeDuration = millis() - hallActiveStart;
        if (activeDuration >= FATIGUE_TIME && !fatiguePlayed) {
          fatiguePlayed   = true;
          sendSpeedLevel(0);
          requestVoice(SFX_FATIGUE, true);
          hallActiveStart = millis();
          Serial.println(F("⚠ 疲勞警告！→ 播放 0004.mp3"));
        }
        if (!voicePlaying && lastSpeedLevel != 255 && lastSpeedLevel != 0 && speedLevel > lastSpeedLevel) {
          requestVoice(SFX_FASTER);
          Serial.println(F("↑ 加速！→ 播放 0002.mp3"));
        }
        if (!voicePlaying && lastSpeedLevel != 255 && speedLevel < lastSpeedLevel && speedLevel > 0) {
          requestVoice(SFX_SLOWER);
          Serial.println(F("↓ 減速！→ 播放 0003.mp3"));
        }
        lastSpeedLevel = speedLevel;
        sendSpeedLevel(speedLevel);
      } else {
        if (hallValue < HALL_RELEASE && !releasePlayed && hasGrippedOnce) {
          releasePlayed = true;
          sendSpeedLevel(0);
          requestVoice(SFX_RELEASE);
          Serial.println(F("○ 放開握力 → 播放 0006.mp3"));
        }
        hallActiveStart = 0;
        fatiguePlayed   = false;
        lastSpeedLevel  = 0;
        sendSpeedLevel(0);
      }
      break;
    }

    case STATE_FINISHED: {
      sendSpeedLevel(0);
      blinkLED(300);
      if (hallValue > HALL_STARTUP) {
        if (!isHallHigh) {
          isHallHigh        = true;
          hallHighStartTime = millis();
        } else if (millis() - hallHighStartTime >= START_DURATION) {
          sysState       = STATE_ACTIVE;
          systemActive   = true;
          isHallHigh     = false;
          hasGrippedOnce = false;
          hallActiveStart = millis();
          fatiguePlayed  = false;
          releasePlayed  = false;
          lastSentLevel  = 255;
          lastSpeedLevel = 255;
          lastEspLevel   = 255;
          sendSpeedLevel(0);
          requestVoice(SFX_START);
          digitalWrite(LED_STATUS, HIGH);
          Serial.println(F("↺ 系統重啟 → 播放 0001.mp3"));
        }
      } else {
        isHallHigh = false;
      }
      break;
    }
  }
}
