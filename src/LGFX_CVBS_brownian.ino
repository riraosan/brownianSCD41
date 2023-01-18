// brownian SCD41 by zeromem
// based on MovingIcons from LovyanGFX by https://twitter.com/lovyan03
// https://github.com/lovyan03/LovyanGFX/blob/master/examples/Sprite/MovingIcons/MovingIcons.ino
// icons replaced with air molecules (oxygen, nitrogen, carbon dioxide) for entertainment purposes only, not scientific accurate
// modified for M5Unified https://github.com/m5stack/M5Unified
// tested on M5Stack Core2 ESP32 IoT Development Kit for AWS IoT EduKit SKU: K010-AWS

// modified by @riraosan_0901 for M5Stack ATOM Lite

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <M5GFX.h>
#include <LGFX_8BIT_CVBS.h>
#include "sfc_f65.h"
#include "molImage.h"
#include "snowImage.h"
#include "j_springImage.h"
#include <Button2.h>

#define TFCARD_CS_PIN -1
#define LGFX          LGFX_8BIT_CVBS

#define LGFX_ONLY
#define USE_DISPLAY

#if defined(MOL)
#define SDU_APP_NAME "MOL NTP Clock"
#elif defined(SNOW)
#define SDU_APP_NAME "SNOW NTP Clock"
#elif defined(JSPRING)
#define SDU_APP_NAME "J-SPRING NTP Clock"
#endif

#include <M5StackUpdater.h>

#define TRANSPARENT 0x0000

// 表示
static LGFX_8BIT_CVBS display;
static Button2        button;

static uint32_t lcd_width;
static uint32_t lcd_height;

struct obj_info_t {
  int_fast16_t x;
  int_fast16_t y;
  int_fast16_t dx;
  int_fast16_t dy;
  int_fast8_t  img;
  float        r;
  float        z;
  float        dr;
  float        dz;

  void move() {
    r += dr;
    x += dx;
    if (x < 0) {
      x = 0;
      if (dx < 0) dx = -dx;
    } else if (x >= lcd_width) {
      x = lcd_width - 1;
      if (dx > 0) dx = -dx;
    }
    y += dy;
    if (y < 0) {
      y = 0;
      if (dy < 0) dy = -dy;
    } else if (y >= lcd_height) {
      y = lcd_height - 1;
      if (dy > 0) dy = -dy;
    }
    z += dz;
    if (z < .5) {
      z = .5;
      if (dz < .0) dz = -dz;
    } else if (z >= 2.0) {
      z = 2.0;
      if (dz > .0) dz = -dz;
    }
  }
};

static constexpr size_t obj_count = 30;
static obj_info_t       objects[obj_count];

static M5Canvas     sprites[2];
static M5Canvas     icons[3];
static int_fast16_t sprite_height;
static int_fast16_t time_height;

static M5Canvas timeSprite[2];

const unsigned short *f65[] = {
    sfc_43x65_f65_0,
    sfc_43x65_f65_1,
    sfc_43x65_f65_2,
    sfc_43x65_f65_3,
    sfc_43x65_f65_4,
    sfc_43x65_f65_5,
    sfc_43x65_f65_6,
    sfc_43x65_f65_7,
    sfc_43x65_f65_8,
    sfc_43x65_f65_9,
    sfc_19x65_f65_colon,
    sfc_19x65_f65_dot};

void drawTime(int div, int h, int m, int s) {
  int hA = h / 10;
  int hR = h % 10;
  int mA = m / 10;
  int mR = m % 10;
  int sA = s / 10;
  int sR = s % 10;

  timeSprite[div].fillSprite(TRANSPARENT);

  int fontWidth = 43;

  if (div == 0) {
    // hour(24h)
    timeSprite[div].pushImage(fontWidth * 0 + 12, -33, fontWidth, 65, (uint16_t *)f65[hA], TRANSPARENT);
    timeSprite[div].pushImage(fontWidth * 1 + 12, -33, fontWidth, 65, (uint16_t *)f65[hR], TRANSPARENT);
    timeSprite[div].pushImage(fontWidth * 2 + 18, -33, 19, 65, (uint16_t *)f65[10], TRANSPARENT);
    // minuits
    timeSprite[div].pushImage(fontWidth * 3, -33, fontWidth, 65, (uint16_t *)f65[mA], TRANSPARENT);
    timeSprite[div].pushImage(fontWidth * 4, -33, fontWidth, 65, (uint16_t *)f65[mR], TRANSPARENT);
    timeSprite[div].pushImage(fontWidth * 5 + 6, -33, 19, 65, (uint16_t *)f65[10], TRANSPARENT);
    // seconds
    timeSprite[div].pushImage(fontWidth * 6 - 12, -33, fontWidth, 65, (uint16_t *)f65[sA], TRANSPARENT);
    timeSprite[div].pushImage(fontWidth * 7 - 12, -33, fontWidth, 65, (uint16_t *)f65[sR], TRANSPARENT);
  } else {
    // hour(24h)
    timeSprite[div].pushImage(fontWidth * 0 + 12, 0, fontWidth, 65, (uint16_t *)f65[hA], TRANSPARENT);
    timeSprite[div].pushImage(fontWidth * 1 + 12, 0, fontWidth, 65, (uint16_t *)f65[hR], TRANSPARENT);
    timeSprite[div].pushImage(fontWidth * 2 + 18, 0, 19, 65, (uint16_t *)f65[10], TRANSPARENT);
    // minuits
    timeSprite[div].pushImage(fontWidth * 3, 0, fontWidth, 65, (uint16_t *)f65[mA], TRANSPARENT);
    timeSprite[div].pushImage(fontWidth * 4, 0, fontWidth, 65, (uint16_t *)f65[mR], TRANSPARENT);
    timeSprite[div].pushImage(fontWidth * 5 + 6, 0, 19, 65, (uint16_t *)f65[10], TRANSPARENT);
    // seconds
    timeSprite[div].pushImage(fontWidth * 6 - 12, 0, fontWidth, 65, (uint16_t *)f65[sA], TRANSPARENT);
    timeSprite[div].pushImage(fontWidth * 7 - 12, 0, fontWidth, 65, (uint16_t *)f65[sR], TRANSPARENT);
  }
}

constexpr uint8_t progress[] = {'-', '\\', '|', '/'};

// Connect to wifi
void setupWiFi(void) {
  WiFi.begin("", "");

  // Wait some time to connect to wifi
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
    display.setCursor(display.width() >> 1, display.height() >> 1);
    display.printf("%c", progress[(i + 1) % 4]);
    delay(500);
  }

  configTzTime(PSTR("JST-9"), "ntp.nict.jp");
  for (int i = 0; i < 10; i++) {
    display.setCursor(display.width() >> 1, display.height() >> 1);
    display.printf("%c", progress[(i + 1) % 4]);
    delay(500);
  }

  WiFi.disconnect(true);
}

bool bA = false;
bool bB = false;
bool bC = false;

void handler(Button2 &btn) {
  switch (btn.getType()) {
    case clickType::single_click:
      Serial.print("single ");
      bB = true;
      break;
    case clickType::double_click:
      Serial.print("double ");
      bC = true;
      break;
    case clickType::triple_click:
      Serial.print("triple ");
      break;
    case clickType::long_click:
      Serial.print("long ");
      bA = true;
      break;
    case clickType::empty:
      break;
    default:
      break;
  }

  Serial.print("click");
  Serial.print(" (");
  Serial.print(btn.getNumberOfClicks());
  Serial.println(")");
}

bool buttonAPressed(void) {
  bool temp = bA;
  bA        = false;

  return temp;
}

bool buttonBPressed(void) {
  bool temp = bB;
  bB        = false;

  return temp;
}

bool buttonCPressed(void) {
  bool temp = bC;
  bC        = false;

  return temp;
}

void ButtonUpdate() {
  button.loop();
}

void setupButton(void) {
  // G39 button
  button.setClickHandler(handler);
  button.setDoubleClickHandler(handler);
  button.setTripleClickHandler(handler);
  button.setLongClickHandler(handler);
  button.begin(39);

  SDUCfg.setSDUBtnA(&buttonAPressed);
  SDUCfg.setSDUBtnB(&buttonBPressed);
  SDUCfg.setSDUBtnC(&buttonCPressed);
  SDUCfg.setSDUBtnPoller(&ButtonUpdate);
}

void setup(void) {
  display.init();
  display.startWrite();

  setupButton();
  setSDUGfx(&display);
  checkSDUpdater(
      SD,            // filesystem (default=SD)
      MENU_BIN,      // path to binary (default=/menu.bin, empty string=rollback only)
      10000,         // wait delay, (default=0, will be forced to 2000 upon ESP.restart() )
      TFCARD_CS_PIN  // (usually default=4 but your mileage may vary)
  );

  setupWiFi();

  if (display.width() < display.height()) {
    display.setRotation(display.getRotation() ^ 1);
  }

  lcd_width  = display.width();
  lcd_height = display.height();

  obj_info_t *a;
  for (size_t i = 0; i < obj_count; ++i) {
    a      = &objects[i];
    a->img = i % 3;
    a->x   = rand() % lcd_width;
    a->y   = rand() % lcd_height;
    a->dx  = ((rand() & 1) + 1) * (i & 1 ? 1 : -1);
    a->dy  = ((rand() & 1) + 1) * (i & 2 ? 1 : -1);
    a->dr  = ((rand() & 1) + 1) * (i & 2 ? 1 : -1);
    a->r   = 0;
    a->z   = (float)((rand() % 10) + 10) / 10;
    a->dz  = (float)((rand() % 10) + 1) / 100;
  }

  icons[0].createSprite(32, 32);
  icons[1].createSprite(32, 32);
  icons[2].createSprite(32, 32);

  icons[0].setSwapBytes(true);
  icons[1].setSwapBytes(true);
  icons[2].setSwapBytes(true);

#if defined(MOL)
  icons[0].pushImage(0, 0, 32, 32, oxygen);
  icons[1].pushImage(0, 0, 32, 32, dioxide);
  icons[2].pushImage(0, 0, 32, 32, nitrogen);
#elif defined(SNOW)
  icons[0].pushImage(0, 0, 32, 32, fulg1);
  icons[1].pushImage(0, 0, 32, 32, fulg2);
  icons[2].pushImage(0, 0, 32, 32, fulg3);
#elif defined(JSPRING)
  icons[0].pushImage(0, 0, 32, 32, _oni);
  icons[1].pushImage(0, 0, 32, 32, _mame);
  icons[2].pushImage(0, 0, 32, 32, _sakura);
#endif

  uint32_t div = 2;
  for (;;) {
    sprite_height = (lcd_height + div - 1) / div;
    bool fail     = false;
    for (std::uint32_t i = 0; !fail && i < 2; ++i) {
      sprites[i].setColorDepth(display.getColorDepth());
      fail = !sprites[i].createSprite(lcd_width, sprite_height);
    }
    if (!fail) break;
    log_e("can't allocate");
    for (std::uint32_t i = 0; i < div; ++i) {
      sprites[i].deleteSprite();
    }
    ++div;
  }

  div = 2;
  for (;;) {
    time_height = (65 + div - 1) / div;
    bool fail   = false;
    for (std::uint32_t i = 0; !fail && i < div; ++i) {
      timeSprite[i].setColorDepth(display.getColorDepth());
      timeSprite[i].setSwapBytes(true);
      fail = !timeSprite[i].createSprite(344, time_height);
    }
    if (!fail) break;
    log_e("can't allocate");
    for (std::uint32_t i = 0; i < 2; ++i) {
      timeSprite[i].deleteSprite();
    }
    ++div;
  }
}

void loop(void) {
  static uint8_t flip      = 0;
  static uint8_t time_flip = 0;

  ButtonUpdate();

  obj_info_t *a;
  for (int i = 0; i != obj_count; i++) {
    objects[i].move();
  }

  for (int_fast16_t y = 0; y < lcd_height; y += sprite_height) {
    flip = flip ? 0 : 1;
    sprites[flip].clear();
    for (size_t i = 0; i != obj_count; i++) {
      a = &objects[i];
      icons[a->img].pushRotateZoom(&sprites[flip], a->x, a->y - y, a->r, a->z, a->z, 0);
    }

    display.display();

    if (y == 0) {
      struct tm timeinfo;
      getLocalTime(&timeinfo);

      for (int_fast16_t z = 0; z < 65; z += time_height) {
        time_flip = time_flip ? 0 : 1;
        timeSprite[time_flip].clear();
        drawTime(time_flip, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        timeSprite[time_flip].pushSprite(&sprites[flip], 8, z + 27, TRANSPARENT);
      }

      if ((timeinfo.tm_hour == 12) && (timeinfo.tm_min == 0) && (timeinfo.tm_sec == 0)) {
        ESP.restart();
      }
    }

    sprites[flip].pushSprite(&display, 0, y);
  }

  display.display();
}
