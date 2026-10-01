// ============================================================
//  燈牌端 — ESP32 + ILI9488 TFT 螢幕（3.5" SPI 480x320）
//  使用 LovyanGFX 函式庫
//  接收 Leonardo D4 傳來的速度段（'0'~'3'）或障礙物（'O'）
// ============================================================
//  接線：
//    TFT VCC   → ESP32 3.3V
//    TFT GND   → ESP32 GND
//    TFT CS    → ESP32 GPIO15
//    TFT RESET → ESP32 GPIO4
//    TFT DC    → ESP32 GPIO2
//    TFT MOSI  → ESP32 GPIO23
//    TFT SCK   → ESP32 GPIO18
//    TFT LED   → ESP32 3.3V
//    TFT MISO  → ESP32 GPIO19
//    ESP32 D16 的 S → Leonardo D4
//    ESP32 D16 的 G → Leonardo GND
// ============================================================

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel_instance;
  lgfx::Bus_SPI       _bus_instance;
  lgfx::Light_PWM     _light_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI3_HOST;
      cfg.freq_write = 40000000;
      cfg.freq_read  = 16000000;
      cfg.pin_sclk = 18;
      cfg.pin_mosi = 23;
      cfg.pin_miso = 19;
      cfg.pin_dc   = 2;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs   = 15;
      cfg.pin_rst  = 4;
      cfg.pin_busy = -1;
      cfg.panel_width  = 320;
      cfg.panel_height = 480;
      cfg.offset_rotation = 0;
      cfg.readable = true;
      cfg.invert   = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = -1;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;

// ── 接收 Leonardo 的腳位 ──
#define RX_PIN 16

// ── 顏色 ──
#define COLOR_BG     TFT_WHITE
#define COLOR_RED    TFT_RED
#define COLOR_GREEN  TFT_GREEN
#define COLOR_YELLOW TFT_YELLOW
#define COLOR_BLACK  TFT_BLACK

// ── 螢幕尺寸（橫式）──
#define SCREEN_W 480
#define SCREEN_H 320

char lastState = '?';

void drawCircleIcon(int x, int y, int r, uint32_t color) {
  tft.fillCircle(x, y, r, color);
  tft.drawCircle(x, y, r, COLOR_BLACK);
}

void drawX(int cx, int cy, int size, uint32_t color) {
  int thick = size / 5;
  for (int i = -thick; i <= thick; i++) {
    tft.drawLine(cx - size/2, cy - size/2 + i, cx + size/2, cy + size/2 + i, color);
    tft.drawLine(cx - size/2 + i, cy - size/2, cx + size/2 + i, cy + size/2, color);
    tft.drawLine(cx - size/2, cy + size/2 + i, cx + size/2, cy - size/2 + i, color);
    tft.drawLine(cx - size/2 + i, cy + size/2, cx + size/2 + i, cy - size/2, color);
  }
}

void drawDisplay(char state) {
  tft.fillScreen(COLOR_BG);

  int centerY  = SCREEN_H / 2 - 40;
  int r        = 70;
  int spacing  = 140;
  int textY    = SCREEN_H - 70;

  tft.setTextColor(COLOR_BLACK, COLOR_BG);
  tft.setTextSize(4);

  if (state == '3') {
    drawCircleIcon(SCREEN_W/2 - spacing, centerY, r, COLOR_RED);
    drawCircleIcon(SCREEN_W/2,           centerY, r, COLOR_RED);
    drawCircleIcon(SCREEN_W/2 + spacing, centerY, r, COLOR_RED);
    tft.setCursor(SCREEN_W/2 - 48, textY);
    tft.print("Fast");

  } else if (state == '2') {
    drawCircleIcon(SCREEN_W/2 - spacing/2, centerY, r, COLOR_YELLOW);
    drawCircleIcon(SCREEN_W/2 + spacing/2, centerY, r, COLOR_YELLOW);
    tft.setCursor(SCREEN_W/2 - 72, textY);
    tft.print("Medium");

  } else if (state == '1') {
    drawCircleIcon(SCREEN_W/2, centerY, r, COLOR_GREEN);
    tft.setCursor(SCREEN_W/2 - 48, textY);
    tft.print("Slow");

  } else if (state == '0') {
    drawX(SCREEN_W/2 - spacing, centerY, r*2, COLOR_RED);
    drawX(SCREEN_W/2,           centerY, r*2, COLOR_RED);
    drawX(SCREEN_W/2 + spacing, centerY, r*2, COLOR_RED);
    tft.setCursor(SCREEN_W/2 - 48, textY);
    tft.print("Stop");

  } else if (state == 'O') {
    drawX(SCREEN_W/2 - spacing, centerY, r*2, COLOR_RED);
    drawX(SCREEN_W/2,           centerY, r*2, COLOR_RED);
    drawX(SCREEN_W/2 + spacing, centerY, r*2, COLOR_RED);
    tft.setCursor(SCREEN_W/2 - 120, textY);
    tft.print("Obstacle!");
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, -1);

  tft.init();
  tft.setRotation(1);  // 橫式
  tft.fillScreen(COLOR_BG);

  drawDisplay('0');
  Serial.println("ESP32 燈牌啟動！");
}

void loop() {
  if (Serial2.available()) {
    char msg = Serial2.read();
    if (msg != lastState && (msg == '0' || msg == '1' || msg == '2' || msg == '3' || msg == 'O')) {
      lastState = msg;
      drawDisplay(msg);
      Serial.print("收到："); Serial.println(msg);
    }
  }
}
