�?#include <Ds1302.h>

/*
  UltimateStudentClock - 闈㈠悜瀛︾敓浣滄伅涓庡涔犺妭寰嬬殑楂樺垎缁堟瀬鐗堟暟瀛楁椂閽?  ... (娉ㄩ噴鐣?
*/

#include <LiquidCrystal.h>
#include <Ds1302.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>

/*
  UltimateStudentClock - 闈㈠悜瀛︾敓浣滄伅涓庡涔犺妭寰嬬殑楂樺垎缁堟瀬鐗堟暟瀛楁椂閽?  ... (娉ㄩ噴鐣?
*/

#include <LiquidCrystal.h>
#include <Ds1302.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>

// -------------------- 纭欢寮曡剼 --------------------
const uint8_t PIN_LCD_RS = 12;
const uint8_t PIN_LCD_E  = 11;
const uint8_t PIN_LCD_D4 = 5;
const uint8_t PIN_LCD_D5 = 4;
const uint8_t PIN_LCD_D6 = 3;
const uint8_t PIN_LCD_D7 = 2;

const uint8_t PIN_DS1302_CE   = 8;
const uint8_t PIN_DS1302_IO   = 9;
const uint8_t PIN_DS1302_SCLK = 10;

const uint8_t PIN_KEY_SELECT = A0;
const uint8_t PIN_KEY_PLUS   = A1;
const uint8_t PIN_KEY_MINUS  = A2;
const uint8_t PIN_LM35       = A3;
const uint8_t PIN_BUZZER     = 13;

LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_E, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);
// The project keeps the library include for compatibility, but RTC read/write
// below uses a small DS1302 bit-bang driver. This avoids constructor-order
// differences among DS1302 libraries.
Ds1302 rtc(PIN_DS1302_CE, PIN_DS1302_SCLK, PIN_DS1302_IO);

// -------------------- 鍏ㄥ眬鏃堕棿涓庨厤缃?--------------------
struct DateTimeData {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};

struct ClockConfig {
  uint16_t magic;
  uint8_t alarmHour;
  uint8_t alarmMinute;
  int8_t tempHigh;
  int8_t tempLow;
  uint16_t targetYear;
  uint8_t targetMonth;
  uint8_t targetDay;
  uint16_t target2Year;
  uint8_t target2Month;
  uint8_t target2Day;
  uint16_t target3Year;
  uint8_t target3Month;
  uint8_t target3Day;
  uint8_t activeTarget;
  uint8_t focusProfile;
  uint8_t sprintProfile;
  uint8_t notifyMode;
  uint8_t alarmEnabled;
  uint16_t savedYear;
  uint8_t savedMonth;
  uint8_t savedDay;
  uint8_t savedHour;
  uint8_t savedMinute;
  uint8_t savedSecond;
  uint8_t hasSavedTime;
  uint8_t checksum;
};

const uint16_t CONFIG_MAGIC = 0xC11D;
ClockConfig cfg;
DateTimeData nowTime;
DateTimeData editTime;
bool rtcOkAtBoot = false;
bool rtcFallbackMode = false;

// ========== 灏嗘灇涓惧畾涔夋彁鍓嶅埌姝ゅ锛岀‘淇濆叏灞€鍙�? ==========
enum DdlUrgency {
  DDL_RELAX,
  DDL_NORMAL,
  DDL_TENSE,
  DDL_CRITICAL,
  DDL_OVERDUE
};

// 浠婃棩杞婚噺缁熻�?
uint8_t todayFocusDone = 0;
uint8_t todayWakeAck = 0;
uint8_t lastStatDay = 0;
uint8_t nightKeyCount = 0;
int8_t lastNightCareMinute = -1;
bool wakeConfirmPending = false;
uint32_t wakeConfirmDueMs = 0;
uint8_t wakeRingCycle = 0;
int32_t lastAlarmStamp = -1;

// 娓╁害鏁版嵁
int16_t temperatureC10 = 250;
int8_t tempHistory[6] = {25, 25, 25, 25, 25, 25};
uint8_t tempHistoryIndex = 0;
bool tempAlertLatched = false;

// -------------------- 鐘舵€佹満 --------------------
enum AppState {
  STATE_HOME,
  STATE_SET_TIME,
  STATE_SET_ALARM_TEMP,
  STATE_SET_TARGET,
  STATE_TOOL_MENU,
  STATE_FOCUS_SELECT,
  STATE_FOCUS_RUN,
  STATE_SPRINT_SELECT,
  STATE_SPRINT_RUN,
  STATE_NOTIFY_SELECT,
  STATE_DEMO_RUN,
  STATE_RINGING,
  STATE_TEMP_ALERT,
  STATE_HOUR_ANIM
};

enum ButtonEvent {
  EVT_NONE,
  EVT_SELECT_CLICK,
  EVT_SELECT_LONG,
  EVT_PLUS_CLICK,
  EVT_MINUS_CLICK,
  EVT_PLUS_REPEAT,
  EVT_MINUS_REPEAT,
  EVT_PLUS_MINUS_COMBO,
  EVT_TIMEOUT
};

AppState appState = STATE_HOME;
uint8_t editField = 0;
uint8_t toolIndex = 0;
uint8_t demoStage = 0;
uint8_t lastDemoStage = 255;
uint8_t lastDemoSubStage = 255;
bool demoWakeAck = false;
uint32_t stateEnterMs = 0;
uint32_t lastUserActionMs = 0;
const uint16_t DEMO_STAGE_MS = 12000;
const uint16_t DEMO_SUB_MS = 4000;

// -------------------- 璋冨害鏃堕棿�??--------------------
uint32_t nowMs = 0;
uint32_t lastRtcReadMs = 0;
uint32_t lastSoftwareTickMs = 0;
uint32_t lastTempReadMs = 0;
uint32_t lastTempHistoryMs = 0;
uint32_t lastUiUpdateMs = 0;
uint32_t lastHomePageMs = 0;
uint32_t lastBlinkMs = 0;
uint8_t homePage = 0;
bool blinkOn = true;
int8_t lastHourChime = -1;
int8_t lastDdlReminderHour = -1;
uint8_t hourAnimFrame = 0;
DateTimeData lastRtcSample;
bool hasLastRtcSample = false;
uint32_t lastRtcSampleMs = 0;
// Use the DS1302 crystal-backed clock as the main time source. The software
// tick only bridges a missed/invalid RTC read between polls.
const bool USE_RTC_PRIMARY = true;
const uint16_t RTC_POLL_MS = 1000;
const uint32_t RTC_SYNC_MS = 60000UL;
const int16_t RTC_SYNC_MAX_DRIFT_SEC = 5;
const uint8_t RTC_BACKUP_FAST_FACTOR = 3;
const uint32_t RTC_ANCHOR_SAVE_MS = 10000UL;
const uint8_t RTC_RAM_ANCHOR_MAGIC0 = 0xA6;
const uint8_t RTC_RAM_ANCHOR_MAGIC1 = 0x31;
const uint8_t RTC_RAM_ANCHOR_VERSION = 1;
const uint8_t RTC_RAM_ANCHOR_SIZE = 16;
uint32_t lastRtcAnchorSaveMs = 0;

// -------------------- Toast 鎻愮�? --------------------
char toastLine[17];
uint32_t toastUntilMs = 0;

// -------------------- LCD 宸垎鍒锋柊缂撳�? --------------------
char lcdCache[2][17];

// -------------------- Flash 鏂囨�? --------------------
const char MSG_SAVED[]       PROGMEM = "Saved";
const char MSG_GOOD_START[]  PROGMEM = "Good start";
const char MSG_FOCUS_PLUS[]  PROGMEM = "Focus +1";
const char MSG_NEW_DAY[]     PROGMEM = "New day";
const char MSG_LUNCH[]       PROGMEM = "Lunch time";
const char MSG_WRAP[]        PROGMEM = "Wrap up";
const char MSG_REST[]        PROGMEM = "Rest soon";
const char MSG_WEEKEND[]     PROGMEM = "Almost free";
const char MSG_STEADY[]      PROGMEM = "Keep steady";
const char MSG_REVIEW[]      PROGMEM = "Review mode";
const char MSG_SLEEP[]       PROGMEM = "Sleep early";
const char MSG_PLAN[]        PROGMEM = "Plan";
const char MSG_SUBMIT[]      PROGMEM = "Submit";
const char MSG_OVERDUE[]     PROGMEM = "Overdue";
const char MSG_REPORT[]      PROGMEM = "Report";
const char MSG_SAVE_CODE[]   PROGMEM = "Save code";
const char MSG_BACKUP[]      PROGMEM = "Backup now";
const char MSG_EYES[]        PROGMEM = "Eyes rest";
const char MSG_LAB[]         PROGMEM = "Lab time";
const char MSG_CODE[]        PROGMEM = "Code time";
const char MSG_NAP[]         PROGMEM = "Power nap";
const char MSG_BREAK[]       PROGMEM = "Stretch";
const char MSG_DEEP[]        PROGMEM = "Deep work";
const char MSG_NIGHT[]       PROGMEM = "Too late";
const char MSG_BREATHE[]     PROGMEM = "Breathe";
const char MSG_WARM[]        PROGMEM = "Keep warm";
const char MSG_WATER[]       PROGMEM = "Drink water";
const char MSG_WINDOW[]      PROGMEM = "Close window";
const char MSG_RESET_OK[]    PROGMEM = "Reset OK";

// -------------------- 鑷畾涔夊瓧�??--------------------
byte CH_HOURGLASS_0[8] = {B11111, B10001, B01010, B00100, B01010, B10001, B11111, B00000};
byte CH_HOURGLASS_1[8] = {B11111, B10001, B00100, B00100, B01010, B10001, B11111, B00000};
byte CH_HOURGLASS_2[8] = {B11111, B10001, B00100, B01010, B00100, B10001, B11111, B00000};
byte CH_HOURGLASS_3[8] = {B11111, B10001, B01010, B00100, B00100, B10001, B11111, B00000};
byte CH_UP[8]          = {B00100, B01110, B10101, B00100, B00100, B00100, B00100, B00000};
byte CH_DOWN[8]        = {B00100, B00100, B00100, B00100, B10101, B01110, B00100, B00000};
byte CH_BAR_FULL[8]    = {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111};
byte CH_BAR_HEAD[8]    = {B10000, B11000, B11100, B11110, B11100, B11000, B10000, B00000};

// -------------------- 鎸夐敭浜嬩欢�??--------------------
struct Button {
  uint8_t pin;
  bool stablePressed;
  bool lastRawPressed;
  bool longFired;
  uint32_t lastChangeMs;
  uint32_t pressedAtMs;
  uint32_t lastRepeatMs;
};

Button keySelect = {PIN_KEY_SELECT, false, false, false, 0, 0, 0};
Button keyPlus   = {PIN_KEY_PLUS,   false, false, false, 0, 0, 0};
Button keyMinus  = {PIN_KEY_MINUS,  false, false, false, 0, 0, 0};

bool comboFired = false;
bool suppressComboClicks = false;
uint32_t comboStartMs = 0;

// -------------------- 铚傞福鍣?--------------------
enum BuzzerMode {
  BUZZ_OFF,
  BUZZ_CONFIRM,
  BUZZ_HOUR,
  BUZZ_TEMP,
  BUZZ_RING
};

BuzzerMode buzzerMode = BUZZ_OFF;
uint32_t buzzerStartMs = 0;
uint32_t buzzerNextMs = 0;
bool buzzerOn = false;

void startBuzzer(BuzzerMode mode);
void stopBuzzer();
void enterState(AppState s);
void resetDailyStatsIfNeeded();
int32_t dateTimeDeltaSeconds(const DateTimeData &a, const DateTimeData &b);
int64_t dateTimeDeltaSeconds64(const DateTimeData &a, const DateTimeData &b);

// -------------------- 涓撴敞妯″紡 --------------------
struct FocusProfile {
  uint8_t workMin;
  uint8_t restMin;
};

const FocusProfile focusProfiles[3] = {
  {25, 5},
  {45, 10},
  {90, 20}
};

const uint8_t sprintProfiles[3] = {30, 60, 90};

bool focusWorking = true;
uint32_t focusPhaseStartMs = 0;
uint32_t focusPhaseDurationMs = 25UL * 60UL * 1000UL;

uint32_t sprintStartMs = 0;
uint32_t sprintDurationMs = 30UL * 60UL * 1000UL;
int8_t sprintLastStage = -1;

// -------------------- 宸ュ叿鍑芥暟 --------------------
void fillLine(char *line) {
  for (uint8_t i = 0; i < 16; i++) line[i] = ' ';
  line[16] = '\0';
}

uint8_t twoDigitsToBuf(char *line, uint8_t pos, uint8_t value) {
  line[pos] = char('0' + value / 10);
  line[pos + 1] = char('0' + value % 10);
  return pos + 2;
}

void fourDigitsToBuf(char *line, uint8_t pos, uint16_t value) {
  line[pos]     = char('0' + (value / 1000) % 10);
  line[pos + 1] = char('0' + (value / 100) % 10);
  line[pos + 2] = char('0' + (value / 10) % 10);
  line[pos + 3] = char('0' + value % 10);
}

void copyText(char *line, uint8_t pos, const char *text) {
  for (uint8_t i = 0; text[i] != '\0' && pos + i < 16; i++) {
    line[pos + i] = text[i];
  }
}

void copyProgmemText(char *line, uint8_t pos, const char *pText) {
  char c;
  uint8_t i = 0;
  while ((c = pgm_read_byte(pText + i)) != '\0' && pos + i < 16) {
    line[pos + i] = c;
    i++;
  }
}

void setToastP(const char *pText, uint16_t durationMs) {
  fillLine(toastLine);
  copyProgmemText(toastLine, 0, pText);
  toastUntilMs = nowMs + durationMs;
}

void setToastText(const char *text, uint16_t durationMs) {
  fillLine(toastLine);
  copyText(toastLine, 0, text);
  toastUntilMs = nowMs + durationMs;
}

uint8_t checksumConfig(const ClockConfig &c) {
  const uint8_t *p = (const uint8_t *)&c;
  uint8_t sum = 0;
  for (uint8_t i = 0; i < sizeof(ClockConfig) - 1; i++) sum ^= p[i];
  return sum;
}

void loadConfig() {
  EEPROM.get(0, cfg);
  if (cfg.magic != CONFIG_MAGIC || checksumConfig(cfg) != cfg.checksum) {
    cfg.magic = CONFIG_MAGIC;
    cfg.alarmHour = 7;
    cfg.alarmMinute = 30;
    cfg.tempHigh = 30;
    cfg.tempLow = 16;
    cfg.targetYear = 2026;
    cfg.targetMonth = 6;
    cfg.targetDay = 30;
    cfg.target2Year = 2026;
    cfg.target2Month = 6;
    cfg.target2Day = 20;
    cfg.target3Year = 2026;
    cfg.target3Month = 7;
    cfg.target3Day = 5;
    cfg.activeTarget = 0;
    cfg.focusProfile = 1;
    cfg.sprintProfile = 1;
    cfg.notifyMode = 1;
    cfg.alarmEnabled = 1;
    cfg.savedYear = 2026;
    cfg.savedMonth = 5;
    cfg.savedDay = 14;
    cfg.savedHour = 12;
    cfg.savedMinute = 0;
    cfg.savedSecond = 0;
    cfg.hasSavedTime = 0;
    cfg.checksum = checksumConfig(cfg);
    EEPROM.put(0, cfg);
  }
  if (cfg.focusProfile > 2) cfg.focusProfile = 1;
  if (cfg.sprintProfile > 2) cfg.sprintProfile = 1;
  if (cfg.notifyMode > 2) cfg.notifyMode = 1;
  if (cfg.activeTarget > 2) cfg.activeTarget = 0;
  if (cfg.hasSavedTime &&
      cfg.savedYear == 2026 && cfg.savedMonth == 5 && cfg.savedDay == 14 &&
      cfg.savedHour == 12 && cfg.savedMinute == 0 && cfg.savedSecond == 0) {
    cfg.hasSavedTime = 0;
    cfg.checksum = checksumConfig(cfg);
    EEPROM.put(0, cfg);
  }
}

void saveConfig() {
  cfg.checksum = checksumConfig(cfg);
  EEPROM.put(0, cfg);
  setToastP(MSG_SAVED, 1200);
  startBuzzer(BUZZ_CONFIRM);
}

void saveLastClockToEeprom(const DateTimeData &dt) {
  cfg.savedYear = dt.year;
  cfg.savedMonth = dt.month;
  cfg.savedDay = dt.day;
  cfg.savedHour = dt.hour;
  cfg.savedMinute = dt.minute;
  cfg.savedSecond = dt.second;
  cfg.hasSavedTime = 1;
  cfg.checksum = checksumConfig(cfg);
  EEPROM.put(0, cfg);
}

bool loadLastClockFromEeprom(DateTimeData &dt) {
  if (!cfg.hasSavedTime) return false;
  dt.year = cfg.savedYear;
  dt.month = cfg.savedMonth;
  dt.day = cfg.savedDay;
  dt.hour = cfg.savedHour;
  dt.minute = cfg.savedMinute;
  dt.second = cfg.savedSecond;
  return dt.year >= 2024 && dt.year <= 2099 &&
         dt.month >= 1 && dt.month <= 12 &&
         dt.day >= 1 && dt.day <= 31 &&
         dt.hour <= 23 && dt.minute <= 59 && dt.second <= 59;
}

bool isLeap(uint16_t y) {
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

uint8_t daysInMonth(uint16_t y, uint8_t m) {
  if (m == 2) return isLeap(y) ? 29 : 28;
  if (m == 4 || m == 6 || m == 9 || m == 11) return 30;
  return 31;
}

uint8_t weekDay(uint16_t y, uint8_t m, uint8_t d) {
  static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

long dateToDays(uint16_t y, uint8_t m, uint8_t d) {
  long days = 0;
  for (uint16_t yy = 2000; yy < y; yy++) days += isLeap(yy) ? 366 : 365;
  for (uint8_t mm = 1; mm < m; mm++) days += daysInMonth(y, mm);
  days += d;
  return days;
}

void daysToDate(long days, DateTimeData &dt) {
  if (days < 1) days = 1;
  uint16_t year = 2000;
  while (true) {
    uint16_t yearDays = isLeap(year) ? 366 : 365;
    if (days <= yearDays) break;
    days -= yearDays;
    year++;
    if (year > 2099) {
      year = 2099;
      days = 365;
      break;
    }
  }

  uint8_t month = 1;
  while (true) {
    uint8_t monthDays = daysInMonth(year, month);
    if (days <= monthDays) break;
    days -= monthDays;
    month++;
    if (month > 12) {
      month = 12;
      days = daysInMonth(year, month);
      break;
    }
  }

  dt.year = year;
  dt.month = month;
  dt.day = (uint8_t)days;
}

void addSecondsToDateTime(DateTimeData &dt, int32_t deltaSec) {
  long days = dateToDays(dt.year, dt.month, dt.day);
  int64_t secondsOfDay = (int64_t)dt.hour * 3600LL + (int64_t)dt.minute * 60LL + dt.second;
  secondsOfDay += deltaSec;

  int64_t dayDelta = secondsOfDay / 86400LL;
  int32_t rem = (int32_t)(secondsOfDay % 86400LL);
  if (rem < 0) {
    rem += 86400;
    dayDelta--;
  }

  days += (long)dayDelta;
  daysToDate(days, dt);
  dt.hour = (uint8_t)(rem / 3600);
  rem %= 3600;
  dt.minute = (uint8_t)(rem / 60);
  dt.second = (uint8_t)(rem % 60);
}

uint16_t targetYear(uint8_t slot) {
  if (slot == 1) return cfg.target2Year;
  if (slot == 2) return cfg.target3Year;
  return cfg.targetYear;
}

uint8_t targetMonth(uint8_t slot) {
  if (slot == 1) return cfg.target2Month;
  if (slot == 2) return cfg.target3Month;
  return cfg.targetMonth;
}

uint8_t targetDay(uint8_t slot) {
  if (slot == 1) return cfg.target2Day;
  if (slot == 2) return cfg.target3Day;
  return cfg.targetDay;
}

void setTargetYear(uint8_t slot, uint16_t value) {
  if (slot == 1) cfg.target2Year = value;
  else if (slot == 2) cfg.target3Year = value;
  else cfg.targetYear = value;
}

void setTargetMonth(uint8_t slot, uint8_t value) {
  if (slot == 1) cfg.target2Month = value;
  else if (slot == 2) cfg.target3Month = value;
  else cfg.targetMonth = value;
}

void setTargetDay(uint8_t slot, uint8_t value) {
  if (slot == 1) cfg.target2Day = value;
  else if (slot == 2) cfg.target3Day = value;
  else cfg.targetDay = value;
}

int16_t ddlDaysLeft(uint8_t slot) {
  long today = dateToDays(nowTime.year, nowTime.month, nowTime.day);
  long target = dateToDays(targetYear(slot), targetMonth(slot), targetDay(slot));
  long diff = target - today;
  if (diff > 999) diff = 999;
  if (diff < -99) diff = -99;
  return (int16_t)diff;
}

uint8_t nearestDdlSlot() {
  uint8_t best = 0;
  int16_t bestD = 1000;
  for (uint8_t i = 0; i < 3; i++) {
    int16_t d = ddlDaysLeft(i);
    if (d >= 0 && d < bestD) {
      bestD = d;
      best = i;
    }
  }
  if (bestD != 1000) return best;

  best = 0;
  bestD = -100;
  for (uint8_t i = 0; i < 3; i++) {
    int16_t d = ddlDaysLeft(i);
    if (d > bestD) {
      bestD = d;
      best = i;
    }
  }
  return best;
}

int16_t examDaysLeft() {
  return ddlDaysLeft(nearestDdlSlot());
}

DdlUrgency ddlUrgencyForDays(int16_t d) {
  if (d < 0) return DDL_OVERDUE;
  if (d == 0) return DDL_CRITICAL;
  if (d <= 2) return DDL_CRITICAL;
  if (d <= 7) return DDL_TENSE;
  if (d <= 14) return DDL_NORMAL;
  return DDL_RELAX;
}

DdlUrgency ddlUrgency() {
  return ddlUrgencyForDays(examDaysLeft());
}

void clampEditDate(DateTimeData &dt) {
  if (dt.month < 1) dt.month = 12;
  if (dt.month > 12) dt.month = 1;
  uint8_t maxDay = daysInMonth(dt.year, dt.month);
  if (dt.day < 1) dt.day = maxDay;
  if (dt.day > maxDay) dt.day = 1;
}

void changeValue(uint8_t &value, int8_t delta, uint8_t minV, uint8_t maxV) {
  int16_t v = value + delta;
  if (v > maxV) v = minV;
  if (v < minV) v = maxV;
  value = (uint8_t)v;
}

void changeInt8(int8_t &value, int8_t delta, int8_t minV, int8_t maxV) {
  int16_t v = value + delta;
  if (v > maxV) v = minV;
  if (v < minV) v = maxV;
  value = (int8_t)v;
}

void changeYear(uint16_t &year, int8_t delta) {
  int16_t y = (int16_t)year + delta;
  if (y > 2099) y = 2024;
  if (y < 2024) y = 2099;
  year = (uint16_t)y;
}

void enterState(AppState s) {
  appState = s;
  stateEnterMs = nowMs;
  lastUserActionMs = nowMs;
  editField = 0;
  lcd.noCursor();

  if (s == STATE_SET_TIME) {
    editTime = nowTime;
  } else if (s == STATE_TOOL_MENU) {
    toolIndex = 0;
  } else if (s == STATE_FOCUS_RUN) {
    focusWorking = true;
    focusPhaseStartMs = nowMs;
    focusPhaseDurationMs = (uint32_t)focusProfiles[cfg.focusProfile].workMin * 60UL * 1000UL;
  } else if (s == STATE_SPRINT_RUN) {
    sprintStartMs = nowMs;
    sprintDurationMs = (uint32_t)sprintProfiles[cfg.sprintProfile] * 60UL * 1000UL;
    sprintLastStage = -1;
  } else if (s == STATE_DEMO_RUN) {
    demoStage = 0;
    lastDemoStage = 255;
    lastDemoSubStage = 255;
    demoWakeAck = false;
  } else if (s == STATE_HOUR_ANIM) {
    hourAnimFrame = 0;
  }
}

// -------------------- DS1302 涓庝紶鎰熷櫒 --------------------
uint8_t toBcd(uint8_t v) {
  return ((v / 10) << 4) | (v % 10);
}

uint8_t fromBcd(uint8_t v) {
  return ((v >> 4) * 10) + (v & 0x0F);
}

void ds1302BeginTransfer() {
  digitalWrite(PIN_DS1302_SCLK, LOW);
  digitalWrite(PIN_DS1302_CE, HIGH);
  delayMicroseconds(4);
}

void ds1302EndTransfer() {
  digitalWrite(PIN_DS1302_CE, LOW);
  delayMicroseconds(4);
}

void ds1302WriteByte(uint8_t value) {
  pinMode(PIN_DS1302_IO, OUTPUT);
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(PIN_DS1302_IO, (value >> i) & 0x01);
    delayMicroseconds(1);
    digitalWrite(PIN_DS1302_SCLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(PIN_DS1302_SCLK, LOW);
  }
}

uint8_t ds1302ReadByte() {
  uint8_t value = 0;
  pinMode(PIN_DS1302_IO, INPUT);
  for (uint8_t i = 0; i < 8; i++) {
    if (digitalRead(PIN_DS1302_IO)) value |= (1 << i);
    digitalWrite(PIN_DS1302_SCLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(PIN_DS1302_SCLK, LOW);
    delayMicroseconds(1);
  }
  return value;
}

void ds1302WriteRegister(uint8_t reg, uint8_t value) {
  ds1302BeginTransfer();
  ds1302WriteByte(reg & 0xFE);
  ds1302WriteByte(value);
  ds1302EndTransfer();
}

void disableRtcTrickleCharger() {
  ds1302WriteRegister(0x90, 0x00);
}

uint8_t ds1302ReadRegister(uint8_t reg) {
  uint8_t value = 0;
  ds1302BeginTransfer();
  pinMode(PIN_DS1302_IO, OUTPUT);
  uint8_t command = reg | 0x01;
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(PIN_DS1302_IO, (command >> i) & 0x01);
    delayMicroseconds(1);
    digitalWrite(PIN_DS1302_SCLK, HIGH);
    delayMicroseconds(1);
    if (i < 7) {
      digitalWrite(PIN_DS1302_SCLK, LOW);
      delayMicroseconds(1);
    }
  }

  pinMode(PIN_DS1302_IO, INPUT);
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(PIN_DS1302_SCLK, LOW);
    delayMicroseconds(1);
    if (digitalRead(PIN_DS1302_IO)) value |= (1 << i);
    digitalWrite(PIN_DS1302_SCLK, HIGH);
    delayMicroseconds(1);
  }
  digitalWrite(PIN_DS1302_SCLK, LOW);
  ds1302EndTransfer();
  return value;
}

void ds1302BeginReadCommand(uint8_t command) {
  ds1302BeginTransfer();
  pinMode(PIN_DS1302_IO, OUTPUT);
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(PIN_DS1302_IO, (command >> i) & 0x01);
    delayMicroseconds(1);
    digitalWrite(PIN_DS1302_SCLK, HIGH);
    delayMicroseconds(1);
    if (i < 7) {
      digitalWrite(PIN_DS1302_SCLK, LOW);
      delayMicroseconds(1);
    }
  }
  pinMode(PIN_DS1302_IO, INPUT);
}

uint8_t ds1302ReadDataByte() {
  uint8_t value = 0;
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(PIN_DS1302_SCLK, LOW);
    delayMicroseconds(1);
    if (digitalRead(PIN_DS1302_IO)) value |= (1 << i);
    digitalWrite(PIN_DS1302_SCLK, HIGH);
    delayMicroseconds(1);
  }
  return value;
}

void ds1302EndReadCommand() {
  digitalWrite(PIN_DS1302_SCLK, LOW);
  ds1302EndTransfer();
}

bool ds1302ReadClockBurst(uint8_t *regs) {
  ds1302BeginTransfer();
  ds1302WriteByte(0xBF);
  for (uint8_t i = 0; i < 8; i++) {
    regs[i] = ds1302ReadByte();
  }
  ds1302EndTransfer();
  return true;
}

bool validBcd(uint8_t value) {
  return ((value & 0x0F) <= 9) && (((value >> 4) & 0x0F) <= 9);
}

bool decodeHourRegister(uint8_t hourReg, uint8_t &hour) {
  if (hourReg & 0x80) {
    uint8_t h12Reg = hourReg & 0x1F;
    if (!validBcd(h12Reg)) return false;
    uint8_t h12 = fromBcd(h12Reg);
    if (h12 < 1 || h12 > 12) return false;
    bool pm = (hourReg & 0x20) != 0;
    hour = h12 % 12;
    if (pm) hour += 12;
    return true;
  }

  uint8_t h24Reg = hourReg & 0x3F;
  if (!validBcd(h24Reg)) return false;
  hour = fromBcd(h24Reg);
  return hour <= 23;
}

bool validDateTime(const DateTimeData &dt) {
  if (dt.year < 2024 || dt.year > 2099) return false;
  if (dt.month < 1 || dt.month > 12) return false;
  if (dt.day < 1 || dt.day > daysInMonth(dt.year, dt.month)) return false;
  if (dt.hour > 23 || dt.minute > 59 || dt.second > 59) return false;
  return true;
}

bool sameDateTime(const DateTimeData &a, const DateTimeData &b) {
  return a.year == b.year && a.month == b.month && a.day == b.day &&
         a.hour == b.hour && a.minute == b.minute && a.second == b.second;
}

void tickOneSecond(DateTimeData &dt) {
  dt.second++;
  if (dt.second < 60) return;
  dt.second = 0;
  dt.minute++;
  if (dt.minute < 60) return;
  dt.minute = 0;
  dt.hour++;
  if (dt.hour < 24) return;
  dt.hour = 0;
  dt.day++;
  if (dt.day <= daysInMonth(dt.year, dt.month)) return;
  dt.day = 1;
  dt.month++;
  if (dt.month <= 12) return;
  dt.month = 1;
  dt.year++;
  if (dt.year > 2099) dt.year = 2024;
}

bool updateSoftwareClock() {
  if (appState == STATE_SET_TIME) {
    lastSoftwareTickMs = nowMs;
    return false;
  }
  if (lastSoftwareTickMs == 0) lastSoftwareTickMs = nowMs;
  bool advanced = false;
  while (nowMs - lastSoftwareTickMs >= 1000UL) {
    lastSoftwareTickMs += 1000UL;
    tickOneSecond(nowTime);
    advanced = true;
  }
  if (advanced) resetDailyStatsIfNeeded();
  return advanced;
}

bool readRtcRaw(DateTimeData &dt) {
  uint8_t secReg = ds1302ReadRegister(0x80);
  uint8_t minReg = ds1302ReadRegister(0x82);
  uint8_t hourReg = ds1302ReadRegister(0x84);
  uint8_t dayReg = ds1302ReadRegister(0x86);
  uint8_t monthReg = ds1302ReadRegister(0x88);
  uint8_t yearReg = ds1302ReadRegister(0x8C);
  uint8_t secCheck = ds1302ReadRegister(0x80);

  if ((secReg & 0x7F) != (secCheck & 0x7F)) {
    secReg = secCheck;
    minReg = ds1302ReadRegister(0x82);
    hourReg = ds1302ReadRegister(0x84);
    dayReg = ds1302ReadRegister(0x86);
    monthReg = ds1302ReadRegister(0x88);
    yearReg = ds1302ReadRegister(0x8C);
  }

  uint8_t sec = secReg & 0x7F;
  uint8_t minute = minReg & 0x7F;
  uint8_t day = dayReg & 0x3F;
  uint8_t month = monthReg & 0x1F;

  if (!validBcd(sec) || !validBcd(minute) ||
      !validBcd(day) || !validBcd(month) || !validBcd(yearReg)) {
    return false;
  }

  uint8_t hour;
  if (!decodeHourRegister(hourReg, hour)) return false;

  dt.year = 2000 + fromBcd(yearReg);
  dt.month = fromBcd(month);
  dt.day = fromBcd(day);
  dt.hour = hour;
  dt.minute = fromBcd(minute);
  dt.second = fromBcd(sec);
  return validDateTime(dt);
}

uint8_t rtcRamChecksum(const uint8_t *data, uint8_t len) {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < len - 1; i++) sum ^= data[i];
  return sum;
}

uint8_t ds1302ReadRamByte(uint8_t index) {
  if (index >= 31) return 0;
  return ds1302ReadRegister(0xC0 + index * 2);
}

void ds1302WriteRamByte(uint8_t index, uint8_t value) {
  if (index >= 31) return;
  ds1302WriteRegister(0xC0 + index * 2, value);
}

void encodeAnchorTime(uint8_t *data, uint8_t pos, const DateTimeData &dt) {
  data[pos] = (uint8_t)(dt.year - 2000);
  data[pos + 1] = dt.month;
  data[pos + 2] = dt.day;
  data[pos + 3] = dt.hour;
  data[pos + 4] = dt.minute;
  data[pos + 5] = dt.second;
}

void decodeAnchorTime(const uint8_t *data, uint8_t pos, DateTimeData &dt) {
  dt.year = 2000 + data[pos];
  dt.month = data[pos + 1];
  dt.day = data[pos + 2];
  dt.hour = data[pos + 3];
  dt.minute = data[pos + 4];
  dt.second = data[pos + 5];
}

bool loadRtcBackupAnchor(DateTimeData &correctTime, DateTimeData &rtcRawTime) {
  uint8_t data[RTC_RAM_ANCHOR_SIZE];
  for (uint8_t i = 0; i < RTC_RAM_ANCHOR_SIZE; i++) data[i] = ds1302ReadRamByte(i);

  if (data[0] != RTC_RAM_ANCHOR_MAGIC0 ||
      data[1] != RTC_RAM_ANCHOR_MAGIC1 ||
      data[2] != RTC_RAM_ANCHOR_VERSION ||
      rtcRamChecksum(data, RTC_RAM_ANCHOR_SIZE) != data[RTC_RAM_ANCHOR_SIZE - 1]) {
    return false;
  }

  decodeAnchorTime(data, 3, correctTime);
  decodeAnchorTime(data, 9, rtcRawTime);
  return validDateTime(correctTime) && validDateTime(rtcRawTime);
}

void saveRtcBackupAnchor(const DateTimeData &correctTime, const DateTimeData &rtcRawTime) {
  if (!validDateTime(correctTime) || !validDateTime(rtcRawTime)) return;

  uint8_t data[RTC_RAM_ANCHOR_SIZE];
  data[0] = RTC_RAM_ANCHOR_MAGIC0;
  data[1] = RTC_RAM_ANCHOR_MAGIC1;
  data[2] = RTC_RAM_ANCHOR_VERSION;
  encodeAnchorTime(data, 3, correctTime);
  encodeAnchorTime(data, 9, rtcRawTime);
  data[RTC_RAM_ANCHOR_SIZE - 1] = rtcRamChecksum(data, RTC_RAM_ANCHOR_SIZE);

  ds1302WriteRegister(0x8E, 0x00);
  for (uint8_t i = 0; i < RTC_RAM_ANCHOR_SIZE; i++) ds1302WriteRamByte(i, data[i]);
  ds1302WriteRegister(0x8E, 0x80);
}

bool correctFastBackupTime(DateTimeData &rtcTime) {
  DateTimeData anchorCorrect;
  DateTimeData anchorRtc;
  if (!loadRtcBackupAnchor(anchorCorrect, anchorRtc)) return false;

  int64_t fastDelta = dateTimeDeltaSeconds64(rtcTime, anchorRtc);
  if (fastDelta < 0) return false;
  if (fastDelta > 2147483647LL) fastDelta = 2147483647LL;

  int32_t correctedDelta = (int32_t)((fastDelta + RTC_BACKUP_FAST_FACTOR / 2) /
                                    RTC_BACKUP_FAST_FACTOR);
  DateTimeData corrected = anchorCorrect;
  addSecondsToDateTime(corrected, correctedDelta);
  if (!validDateTime(corrected)) return false;

  rtcTime = corrected;
  return true;
}

void updateRtcBackupAnchorIfNeeded() {
  if (appState == STATE_SET_TIME) return;
  if (nowMs - lastRtcAnchorSaveMs < RTC_ANCHOR_SAVE_MS) return;
  lastRtcAnchorSaveMs = nowMs;

  DateTimeData rtcRaw;
  if (readRtcRaw(rtcRaw)) saveRtcBackupAnchor(nowTime, rtcRaw);
}

void resetDailyStatsIfNeeded() {
  if (lastStatDay != nowTime.day) {
    lastStatDay = nowTime.day;
    todayFocusDone = 0;
    todayWakeAck = 0;
    nightKeyCount = 0;
    lastNightCareMinute = -1;
    lastDdlReminderHour = -1;
    wakeConfirmPending = false;
    wakeRingCycle = 0;
    lastAlarmStamp = -1;
    tempAlertLatched = false;
  }
}

bool readRtc() {
  DateTimeData fresh;
  if (!readRtcRaw(fresh)) {
    return false;
  }
  rtcFallbackMode = false;

  if (hasLastRtcSample) {
    int32_t rtcStepSec = dateTimeDeltaSeconds(fresh, lastRtcSample);
    uint32_t elapsedMs = nowMs - lastRtcSampleMs;
    int32_t maxStepSec = (int32_t)(elapsedMs / 1000UL) + 1;
    if (maxStepSec < 1) maxStepSec = 1;
    if (rtcStepSec < 0 || rtcStepSec > maxStepSec) {
      return false;
    }
  }

  lastRtcSample = fresh;
  hasLastRtcSample = true;
  lastRtcSampleMs = nowMs;
  nowTime = fresh;
  lastSoftwareTickMs = nowMs;
  resetDailyStatsIfNeeded();
  return true;
}

int32_t dateTimeDeltaSeconds(const DateTimeData &a, const DateTimeData &b) {
  long dayDelta = dateToDays(a.year, a.month, a.day) - dateToDays(b.year, b.month, b.day);
  if (dayDelta > 10) return 864001L;
  if (dayDelta < -10) return -864001L;

  int32_t secondDelta = (int32_t)dayDelta * 86400L;
  secondDelta += (int32_t)a.hour * 3600L + (int32_t)a.minute * 60L + a.second;
  secondDelta -= (int32_t)b.hour * 3600L + (int32_t)b.minute * 60L + b.second;
  return secondDelta;
}

int64_t dateTimeDeltaSeconds64(const DateTimeData &a, const DateTimeData &b) {
  int64_t dayDelta = (int64_t)dateToDays(a.year, a.month, a.day) -
                     (int64_t)dateToDays(b.year, b.month, b.day);
  int64_t secondDelta = dayDelta * 86400LL;
  secondDelta += (int64_t)a.hour * 3600LL + (int64_t)a.minute * 60LL + a.second;
  secondDelta -= (int64_t)b.hour * 3600LL + (int64_t)b.minute * 60LL + b.second;
  return secondDelta;
}

bool syncClockFromRtc() {
  DateTimeData fresh;
  if (!readRtcRaw(fresh)) return false;

  bool hadRtcSample = hasLastRtcSample;
  int32_t driftSec = dateTimeDeltaSeconds(fresh, nowTime);
  lastRtcSample = fresh;
  hasLastRtcSample = true;
  lastRtcSampleMs = nowMs;

  if (!hadRtcSample ||
      (driftSec >= -RTC_SYNC_MAX_DRIFT_SEC && driftSec <= RTC_SYNC_MAX_DRIFT_SEC)) {
    nowTime = fresh;
    lastSoftwareTickMs = nowMs;
    resetDailyStatsIfNeeded();
    return true;
  }

  return false;
}

void writeRtcRaw(const DateTimeData &dt) {
  ds1302WriteRegister(0x8E, 0x00);  // Disable write protect.
  disableRtcTrickleCharger();
  ds1302WriteRegister(0x80, toBcd(dt.second) & 0x7F); // CH=0 starts oscillator.
  ds1302WriteRegister(0x82, toBcd(dt.minute));
  ds1302WriteRegister(0x84, toBcd(dt.hour)); // 24-hour mode.
  ds1302WriteRegister(0x86, toBcd(dt.day));
  ds1302WriteRegister(0x88, toBcd(dt.month));
  ds1302WriteRegister(0x8A, toBcd(weekDay(dt.year, dt.month, dt.day) + 1));
  ds1302WriteRegister(0x8C, toBcd((uint8_t)(dt.year - 2000)));
  ds1302WriteRegister(0x8E, 0x80);  // Re-enable write protect.
}

bool writeRtcFromEdit() {
  writeRtcRaw(editTime);
  rtcFallbackMode = false;
  rtcOkAtBoot = true;
  nowTime = editTime;
  lastRtcReadMs = nowMs;
  lastSoftwareTickMs = nowMs;
  lastRtcSample = editTime;
  hasLastRtcSample = true;
  lastRtcSampleMs = nowMs;
  resetDailyStatsIfNeeded();
  saveLastClockToEeprom(editTime);

  DateTimeData verify;
  if (!readRtcRaw(verify)) return false;
  bool ok = verify.year == editTime.year &&
            verify.month == editTime.month &&
            verify.day == editTime.day &&
            verify.hour == editTime.hour &&
            verify.minute == editTime.minute;
  if (ok) {
    nowTime = verify;
    saveRtcBackupAnchor(nowTime, verify);
    lastRtcAnchorSaveMs = nowMs;
  }
  return ok;
}

void loadFallbackClockForDisplay() {
  rtcFallbackMode = true;
  DateTimeData saved;
  if (loadLastClockFromEeprom(saved)) {
    nowTime = saved;
  } else {
    nowTime.year = 2026;
    nowTime.month = 5;
    nowTime.day = 14;
    nowTime.hour = 12;
    nowTime.minute = 0;
    nowTime.second = 0;
  }
}

bool readRtcStable(DateTimeData &dt, uint8_t attempts) {
  for (uint8_t i = 0; i < attempts; i++) {
    if (readRtcRaw(dt)) return true;
    delay(8);
  }
  return false;
}

void initializeRtcAtBoot() {
  rtcOkAtBoot = false;
  rtcFallbackMode = false;
  delay(50);
  ds1302WriteRegister(0x8E, 0x00); // Allow CH-bit recovery if needed.
  disableRtcTrickleCharger();
  uint8_t rawSecondReg = ds1302ReadRegister(0x80);
  bool clockHalted = (rawSecondReg & 0x80) != 0;

  DateTimeData rtcTime;
  bool rtcValid = readRtcStable(rtcTime, 5);

  if (rtcValid) {
    rtcOkAtBoot = true;
    bool correctedFastBackup = correctFastBackupTime(rtcTime);
    nowTime = rtcTime;
    lastRtcSample = rtcTime;
    hasLastRtcSample = true;
    lastSoftwareTickMs = millis();
    lastRtcSampleMs = lastSoftwareTickMs;

    if (clockHalted || correctedFastBackup) {
      writeRtcRaw(nowTime); // Preserve time, clear CH, and restart oscillator.
      setToastText(correctedFastBackup ? "RTC Fast Fix" : "RTC Restart", 1600);
    } else {
      ds1302WriteRegister(0x8E, 0x80);
      setToastText("RTC OK", 1200);
    }
    DateTimeData rtcRaw;
    if (readRtcRaw(rtcRaw)) saveRtcBackupAnchor(nowTime, rtcRaw);
    lastRtcAnchorSaveMs = lastSoftwareTickMs;
    return;
  }

  // Do not overwrite RTC on a cold-start transient read failure. Use EEPROM
  // only for display fallback; the next manual time setting will write RTC.
  loadFallbackClockForDisplay();
  ds1302WriteRegister(0x8E, 0x80);
  setToastText("RTC Lost SetTime", 2200);
}

void readTemperature() {
  long raw = analogRead(PIN_LM35);
  temperatureC10 = (int16_t)(raw * 5000L / 1023L);
}

int8_t tempC() {
  return (int8_t)(temperatureC10 / 10);
}

int8_t tempTrend() {
  int8_t oldest = tempHistory[(tempHistoryIndex + 1) % 6];
  return tempC() - oldest;
}

// -------------------- 鎸夐敭璇诲彇 --------------------
void updateOneButton(Button &b) {
  bool rawPressed = (digitalRead(b.pin) == LOW);
  if (rawPressed != b.lastRawPressed) {
    b.lastRawPressed = rawPressed;
    b.lastChangeMs = nowMs;
  }

  if (nowMs - b.lastChangeMs >= 25 && rawPressed != b.stablePressed) {
    b.stablePressed = rawPressed;
    if (b.stablePressed) {
      b.pressedAtMs = nowMs;
      b.lastRepeatMs = nowMs;
      b.longFired = false;
    }
  }
}

ButtonEvent readButtons() {
  updateOneButton(keySelect);
  updateOneButton(keyPlus);
  updateOneButton(keyMinus);

  if (keyPlus.stablePressed && keyMinus.stablePressed) {
    if (comboStartMs == 0) comboStartMs = nowMs;
    if (!comboFired && nowMs - comboStartMs > 350) {
      comboFired = true;
      suppressComboClicks = true;
      return EVT_PLUS_MINUS_COMBO;
    }
  } else {
    comboStartMs = 0;
    if (!keyPlus.stablePressed && !keyMinus.stablePressed) {
      comboFired = false;
    }
  }

  if (keySelect.stablePressed && !keySelect.longFired && nowMs - keySelect.pressedAtMs > 800) {
    keySelect.longFired = true;
    return EVT_SELECT_LONG;
  }

  if (!suppressComboClicks && keyPlus.stablePressed && nowMs - keyPlus.pressedAtMs > 500 && nowMs - keyPlus.lastRepeatMs > 150) {
    keyPlus.lastRepeatMs = nowMs;
    return EVT_PLUS_REPEAT;
  }

  if (!suppressComboClicks && keyMinus.stablePressed && nowMs - keyMinus.pressedAtMs > 500 && nowMs - keyMinus.lastRepeatMs > 150) {
    keyMinus.lastRepeatMs = nowMs;
    return EVT_MINUS_REPEAT;
  }

  static bool lastSelect = false;
  static bool lastPlus = false;
  static bool lastMinus = false;
  ButtonEvent evt = EVT_NONE;

  if (lastSelect && !keySelect.stablePressed && !keySelect.longFired) evt = EVT_SELECT_CLICK;
  else if (lastPlus && !keyPlus.stablePressed && !suppressComboClicks) evt = EVT_PLUS_CLICK;
  else if (lastMinus && !keyMinus.stablePressed && !suppressComboClicks) evt = EVT_MINUS_CLICK;

  lastSelect = keySelect.stablePressed;
  lastPlus = keyPlus.stablePressed;
  lastMinus = keyMinus.stablePressed;

  if (suppressComboClicks && !keyPlus.stablePressed && !keyMinus.stablePressed && evt == EVT_NONE) {
    suppressComboClicks = false;
  }

  return evt;
}

// -------------------- 铚傞福鍣?--------------------
void startBuzzer(BuzzerMode mode) {
  if (cfg.notifyMode == 0) {
    noTone(PIN_BUZZER);
    buzzerOn = false;
    buzzerMode = BUZZ_OFF;
    return;
  }
  buzzerMode = mode;
  buzzerStartMs = nowMs;
  buzzerNextMs = nowMs;
  buzzerOn = false;
}

void stopBuzzer() {
  noTone(PIN_BUZZER);
  buzzerOn = false;
  buzzerMode = BUZZ_OFF;
}

uint16_t wakeMelodyFreq(uint8_t step, bool urgent) {
  if (urgent) {
    if (step == 0) return 1760;
    if (step == 1) return 2093;
    if (step == 2) return 2349;
    return 2093;
  }
  if (step == 0) return 988;
  if (step == 1) return 1175;
  if (step == 2) return 1319;
  return 1175;
}

void updateBuzzer() {
  if (buzzerMode == BUZZ_OFF) return;

  uint32_t elapsed = nowMs - buzzerStartMs;
  uint16_t freq = 1600;
  uint16_t onMs = 80;
  uint16_t offMs = 900;
  uint32_t maxMs = 2000;

  if (buzzerMode == BUZZ_CONFIRM) {
    freq = 1800; onMs = 60; offMs = 100; maxMs = 120;
  } else if (buzzerMode == BUZZ_HOUR) {
    freq = 1200; onMs = 90; offMs = 160; maxMs = 650;
  } else if (buzzerMode == BUZZ_TEMP) {
    freq = 2200; onMs = 100; offMs = 1500; maxMs = 10000;
  } else if (buzzerMode == BUZZ_RING) {
    maxMs = wakeRingCycle == 0 ? 22000UL : 14000UL;
    uint8_t step = (uint8_t)((elapsed / 850UL) % 4);
    bool urgentWake = (wakeRingCycle > 0 || todayWakeAck >= 2);
    freq = wakeMelodyFreq(step, urgentWake);
    if (urgentWake) {
      onMs = 170; offMs = 280;
    } else if (elapsed < 5000) {
      onMs = 90; offMs = 1150;
    } else if (elapsed < 9000) {
      onMs = 120; offMs = 760;
    } else {
      onMs = 150; offMs = 480;
    }
  }

  if (cfg.notifyMode == 1 && buzzerMode != BUZZ_RING) {
    onMs = onMs / 2 + 20;
    offMs = offMs + 400;
  }

  if (elapsed > maxMs) {
    stopBuzzer();
    return;
  }

  if (nowMs >= buzzerNextMs) {
    if (buzzerOn) {
      noTone(PIN_BUZZER);
      buzzerOn = false;
      buzzerNextMs = nowMs + offMs;
    } else {
      tone(PIN_BUZZER, freq);
      buzzerOn = true;
      buzzerNextMs = nowMs + onMs;
    }
  }
}

// -------------------- 涓氬姟閫昏緫 --------------------
const char *contextMessage() {
  uint8_t wd = weekDay(nowTime.year, nowTime.month, nowTime.day);
  if (wd == 5 && nowTime.hour >= 17) return MSG_WEEKEND;
  if (nowTime.hour >= 23 || nowTime.hour < 5) {
    if (nightKeyCount >= 10) return MSG_SAVE_CODE;
    if (nowTime.hour >= 1 && nowTime.hour < 5) return MSG_NIGHT;
    return MSG_SLEEP;
  }
  if (nowTime.hour >= 6 && nowTime.hour < 10) return MSG_NEW_DAY;
  if (nowTime.hour >= 10 && nowTime.hour < 12) return MSG_DEEP;
  if (nowTime.hour == 12) return MSG_LUNCH;
  if (nowTime.hour >= 13 && nowTime.hour < 15) return MSG_BREATHE;
  if (nowTime.hour >= 15 && nowTime.hour < 18) return MSG_BREAK;
  if (nowTime.hour >= 18 && nowTime.hour < 21) return MSG_WRAP;
  if (nowTime.hour >= 21 && nowTime.hour < 23) return MSG_BACKUP;
  return MSG_STEADY;
}

const char *examMessage() {
  DdlUrgency u = ddlUrgency();
  if (u == DDL_OVERDUE) return MSG_OVERDUE;
  if (u == DDL_CRITICAL) return MSG_SUBMIT;
  if (u == DDL_TENSE) return MSG_REPORT;
  if (u == DDL_NORMAL) return MSG_REVIEW;
  return MSG_PLAN;
}

const char *ddlMessageForDays(int16_t d) {
  DdlUrgency u = ddlUrgencyForDays(d);
  if (u == DDL_OVERDUE) return MSG_OVERDUE;
  if (u == DDL_CRITICAL) return MSG_SUBMIT;
  if (u == DDL_TENSE) return MSG_REPORT;
  if (u == DDL_NORMAL) return MSG_REVIEW;
  return MSG_PLAN;
}

const char *scheduleMessage() {
  if (nowTime.hour >= 8 && nowTime.hour < 10) return MSG_LAB;
  if (nowTime.hour >= 10 && nowTime.hour < 12) return MSG_CODE;
  if (nowTime.hour >= 12 && nowTime.hour < 14) return MSG_NAP;
  if (nowTime.hour >= 14 && nowTime.hour < 17) return MSG_LAB;
  if (nowTime.hour >= 19 && nowTime.hour < 23) return MSG_REPORT;
  if (nowTime.hour >= 23 || nowTime.hour < 5) return MSG_BACKUP;
  return MSG_STEADY;
}

void checkAlarms() {
  if (appState != STATE_HOME) return;

  if (wakeConfirmPending && nowMs >= wakeConfirmDueMs) {
    wakeConfirmPending = false;
    wakeRingCycle = 1;
    enterState(STATE_RINGING);
    startBuzzer(BUZZ_RING);
    return;
  }

  if (nowTime.minute == 0 && nowTime.second == 0 && lastHourChime != nowTime.hour) {
    lastHourChime = nowTime.hour;
    if (nowTime.hour >= 7 && nowTime.hour <= 22) {
      startBuzzer(BUZZ_HOUR);
      enterState(STATE_HOUR_ANIM);
    }
  }

  int32_t alarmStamp = dateToDays(nowTime.year, nowTime.month, nowTime.day);
  if (cfg.alarmEnabled && nowTime.hour == cfg.alarmHour && nowTime.minute == cfg.alarmMinute &&
      nowTime.second == 0 && lastAlarmStamp != alarmStamp) {
    lastAlarmStamp = alarmStamp;
    wakeRingCycle = 0;
    wakeConfirmPending = false;
    enterState(STATE_RINGING);
    startBuzzer(BUZZ_RING);
  }

  DdlUrgency u = ddlUrgency();
  if (nowTime.second == 5 && nowTime.minute == 0 && nowTime.hour >= 8 && nowTime.hour <= 23 &&
      lastDdlReminderHour != nowTime.hour) {
    if (u == DDL_CRITICAL || u == DDL_OVERDUE || (u == DDL_TENSE && nowTime.hour % 3 == 0)) {
      lastDdlReminderHour = nowTime.hour;
      setToastP(examMessage(), 3000);
      startBuzzer(BUZZ_CONFIRM);
    }
  }

  if ((nowTime.hour >= 23 || nowTime.hour < 5) && nowTime.second == 8) {
    bool periodicCare = (nowTime.minute % 40 == 0);
    bool activeLateWork = (nightKeyCount >= 8);
    if ((periodicCare || activeLateWork) && lastNightCareMinute != nowTime.minute) {
      lastNightCareMinute = nowTime.minute;
      setToastP(activeLateWork ? MSG_SAVE_CODE : MSG_EYES, 3500);
      startBuzzer(BUZZ_CONFIRM);
    }
  }

  int8_t t = tempC();
  int8_t trend = tempTrend();
  if (!tempAlertLatched && (t >= cfg.tempHigh || t <= cfg.tempLow || trend <= -3)) {
    tempAlertLatched = true;
    enterState(STATE_TEMP_ALERT);
    if (trend <= -3) setToastP(MSG_WINDOW, 3500);
    else if (t >= cfg.tempHigh) setToastP(MSG_WATER, 3500);
    else setToastP(MSG_WARM, 3500);
    startBuzzer(BUZZ_TEMP);
  }
}

void changeEditTime(int8_t delta) {
  if (editField == 0) changeYear(editTime.year, delta);
  else if (editField == 1) changeValue(editTime.month, delta, 1, 12);
  else if (editField == 2) changeValue(editTime.day, delta, 1, daysInMonth(editTime.year, editTime.month));
  else if (editField == 3) changeValue(editTime.hour, delta, 0, 23);
  else if (editField == 4) changeValue(editTime.minute, delta, 0, 59);
  else if (editField == 5) changeValue(editTime.second, delta, 0, 59);
  clampEditDate(editTime);
}

void changeAlarmTemp(int8_t delta) {
  if (editField == 0) changeValue(cfg.alarmHour, delta, 0, 23);
  else if (editField == 1) changeValue(cfg.alarmMinute, delta, 0, 59);
  else if (editField == 2) changeInt8(cfg.tempHigh, delta, 20, 45);
  else if (editField == 3) changeInt8(cfg.tempLow, delta, 0, 25);
  else if (editField == 4) cfg.alarmEnabled = !cfg.alarmEnabled;
}

void changeTarget(int8_t delta) {
  uint8_t slot = cfg.activeTarget;
  uint16_t y = targetYear(slot);
  uint8_t m = targetMonth(slot);
  uint8_t d = targetDay(slot);

  if (editField == 0) {
    changeValue(cfg.activeTarget, delta, 0, 2);
    return;
  } else if (editField == 1) {
    changeYear(y, delta);
  } else if (editField == 2) {
    changeValue(m, delta, 1, 12);
  } else if (editField == 3) {
    changeValue(d, delta, 1, daysInMonth(y, m));
  }

  uint8_t maxDay = daysInMonth(y, m);
  if (d > maxDay) d = maxDay;
  setTargetYear(slot, y);
  setTargetMonth(slot, m);
  setTargetDay(slot, d);
}

void finishFocusEarly() {
  setToastP(MSG_RESET_OK, 1200);
  startBuzzer(BUZZ_CONFIRM);
  enterState(STATE_HOME);
}

void updateFocusRun() {
  if (appState != STATE_FOCUS_RUN) return;
  uint32_t elapsed = nowMs - focusPhaseStartMs;
  if (elapsed >= focusPhaseDurationMs) {
    if (focusWorking) {
      if (todayFocusDone < 99) todayFocusDone++;
      setToastP((nowTime.hour >= 23 || ddlUrgency() >= DDL_TENSE) ? MSG_SAVE_CODE : MSG_FOCUS_PLUS, 1800);
      startBuzzer(BUZZ_CONFIRM);
      focusWorking = false;
      focusPhaseStartMs = nowMs;
      focusPhaseDurationMs = (uint32_t)focusProfiles[cfg.focusProfile].restMin * 60UL * 1000UL;
    } else {
      setToastP(MSG_SAVED, 1200);
      enterState(STATE_HOME);
    }
  }
}

int8_t sprintStage(uint16_t leftMin) {
  if (leftMin <= 3) return 4;
  if (leftMin <= 10) return 3;
  if (leftMin <= 30) return 2;
  if (leftMin <= 60) return 1;
  return 0;
}

const char *sprintAction(uint16_t leftMin) {
  int8_t st = sprintStage(leftMin);
  if (st == 4) return MSG_SUBMIT;
  if (st == 3) return MSG_BACKUP;
  if (st == 2) return MSG_REPORT;
  if (st == 1) return MSG_SAVE_CODE;
  return MSG_PLAN;
}

void updateSprintRun() {
  if (appState != STATE_SPRINT_RUN) return;
  uint32_t elapsed = nowMs - sprintStartMs;
  if (elapsed >= sprintDurationMs) {
    setToastP(MSG_SUBMIT, 2500);
    startBuzzer(BUZZ_HOUR);
    enterState(STATE_HOME);
    return;
  }

  uint32_t leftMs = sprintDurationMs - elapsed;
  uint16_t leftMin = (uint16_t)((leftMs + 59999UL) / 60000UL);
  int8_t st = sprintStage(leftMin);
  if (st != sprintLastStage) {
    sprintLastStage = st;
    setToastP(sprintAction(leftMin), 2200);
    startBuzzer(BUZZ_CONFIRM);
  }
}

void updateDemoRun() {
  if (appState != STATE_DEMO_RUN) return;
  uint32_t demoElapsed = nowMs - stateEnterMs;
  uint8_t autoStage = (uint8_t)((demoElapsed / DEMO_STAGE_MS) % 8);
  if (autoStage != demoStage && nowMs - lastUserActionMs > 1200) demoStage = autoStage;
  uint8_t subStage = (uint8_t)((demoElapsed % DEMO_STAGE_MS) / DEMO_SUB_MS);

  if (demoStage != lastDemoStage) {
    lastDemoStage = demoStage;
    lastDemoSubStage = 255;
    demoWakeAck = false;
    stopBuzzer();
    if (demoStage == 2) {
      wakeRingCycle = 0;
      startBuzzer(BUZZ_RING);
    } else if (demoStage == 3) {
      wakeRingCycle = 1;
      startBuzzer(BUZZ_RING);
    } else if (demoStage == 7) {
      startBuzzer(BUZZ_HOUR);
    }
  }

  if (subStage != lastDemoSubStage) {
    lastDemoSubStage = subStage;
    if (demoStage == 4) startBuzzer(BUZZ_TEMP);
  }
}

void handleEvent(ButtonEvent evt) {
  if (evt == EVT_NONE) return;
  lastUserActionMs = nowMs;
  if (nowTime.hour >= 23 || nowTime.hour < 5) {
    if (nightKeyCount < 99) nightKeyCount++;
  }

  if (appState == STATE_HOME) {
    if (evt == EVT_SELECT_CLICK) enterState(STATE_SET_TIME);
    else if (evt == EVT_SELECT_LONG) enterState(STATE_TOOL_MENU);
    else if (evt == EVT_PLUS_MINUS_COMBO) enterState(STATE_SET_ALARM_TEMP);
    else if (evt == EVT_PLUS_CLICK) {
      homePage = (homePage + 1) % 5;
      lastHomePageMs = nowMs;
    } else if (evt == EVT_MINUS_CLICK) {
      homePage = (homePage + 4) % 5;
      lastHomePageMs = nowMs;
    }
  } else if (appState == STATE_SET_TIME) {
    if (evt == EVT_SELECT_CLICK) {
      editField++;
      if (editField > 5) {
        bool ok = writeRtcFromEdit();
        if (ok) setToastP(MSG_SAVED, 1200);
        else setToastText("RTC Write Err", 1800);
        startBuzzer(BUZZ_CONFIRM);
        enterState(STATE_HOME);
      }
    } else if (evt == EVT_SELECT_LONG) {
      bool ok = writeRtcFromEdit();
      if (ok) setToastP(MSG_SAVED, 1200);
      else setToastText("RTC Write Err", 1800);
      startBuzzer(BUZZ_CONFIRM);
      enterState(STATE_HOME);
    } else if (evt == EVT_PLUS_CLICK || evt == EVT_PLUS_REPEAT) changeEditTime(1);
    else if (evt == EVT_MINUS_CLICK || evt == EVT_MINUS_REPEAT) changeEditTime(-1);
  } else if (appState == STATE_SET_ALARM_TEMP) {
    if (evt == EVT_SELECT_CLICK) {
      editField++;
      if (editField > 4) {
        saveConfig();
        enterState(STATE_SET_TARGET);
      }
    } else if (evt == EVT_SELECT_LONG) {
      saveConfig();
      enterState(STATE_HOME);
    } else if (evt == EVT_PLUS_CLICK || evt == EVT_PLUS_REPEAT) changeAlarmTemp(1);
    else if (evt == EVT_MINUS_CLICK || evt == EVT_MINUS_REPEAT) changeAlarmTemp(-1);
  } else if (appState == STATE_SET_TARGET) {
    if (evt == EVT_SELECT_CLICK) {
      editField++;
      if (editField > 3) {
        saveConfig();
        enterState(STATE_HOME);
      }
    } else if (evt == EVT_SELECT_LONG) {
      saveConfig();
      enterState(STATE_HOME);
    } else if (evt == EVT_PLUS_CLICK || evt == EVT_PLUS_REPEAT) changeTarget(1);
    else if (evt == EVT_MINUS_CLICK || evt == EVT_MINUS_REPEAT) changeTarget(-1);
  } else if (appState == STATE_TOOL_MENU) {
    if (evt == EVT_SELECT_CLICK) {
      if (toolIndex == 0) enterState(STATE_FOCUS_SELECT);
      else if (toolIndex == 1) enterState(STATE_SPRINT_SELECT);
      else if (toolIndex == 2) enterState(STATE_NOTIFY_SELECT);
      else enterState(STATE_DEMO_RUN);
    } else if (evt == EVT_SELECT_LONG) {
      enterState(STATE_HOME);
    } else if (evt == EVT_PLUS_CLICK || evt == EVT_PLUS_REPEAT) {
      toolIndex = (toolIndex + 1) % 4;
    } else if (evt == EVT_MINUS_CLICK || evt == EVT_MINUS_REPEAT) {
      toolIndex = (toolIndex + 3) % 4;
    }
  } else if (appState == STATE_FOCUS_SELECT) {
    if (evt == EVT_SELECT_CLICK) enterState(STATE_FOCUS_RUN);
    else if (evt == EVT_SELECT_LONG) enterState(STATE_HOME);
    else if (evt == EVT_PLUS_CLICK || evt == EVT_PLUS_REPEAT) {
      cfg.focusProfile = (cfg.focusProfile + 1) % 3;
      saveConfig();
    } else if (evt == EVT_MINUS_CLICK || evt == EVT_MINUS_REPEAT) {
      cfg.focusProfile = (cfg.focusProfile + 2) % 3;
      saveConfig();
    }
  } else if (appState == STATE_FOCUS_RUN) {
    if (evt == EVT_SELECT_LONG || evt == EVT_PLUS_MINUS_COMBO) finishFocusEarly();
  } else if (appState == STATE_SPRINT_SELECT) {
    if (evt == EVT_SELECT_CLICK) enterState(STATE_SPRINT_RUN);
    else if (evt == EVT_SELECT_LONG) enterState(STATE_HOME);
    else if (evt == EVT_PLUS_CLICK || evt == EVT_PLUS_REPEAT) {
      cfg.sprintProfile = (cfg.sprintProfile + 1) % 3;
      saveConfig();
    } else if (evt == EVT_MINUS_CLICK || evt == EVT_MINUS_REPEAT) {
      cfg.sprintProfile = (cfg.sprintProfile + 2) % 3;
      saveConfig();
    }
  } else if (appState == STATE_SPRINT_RUN) {
    if (evt == EVT_SELECT_LONG || evt == EVT_PLUS_MINUS_COMBO) finishFocusEarly();
  } else if (appState == STATE_NOTIFY_SELECT) {
    if (evt == EVT_SELECT_CLICK || evt == EVT_SELECT_LONG) {
      saveConfig();
      enterState(STATE_HOME);
    } else if (evt == EVT_PLUS_CLICK || evt == EVT_PLUS_REPEAT) {
      cfg.notifyMode = (cfg.notifyMode + 1) % 3;
      setToastP(MSG_SAVED, 800);
    } else if (evt == EVT_MINUS_CLICK || evt == EVT_MINUS_REPEAT) {
      cfg.notifyMode = (cfg.notifyMode + 2) % 3;
      setToastP(MSG_SAVED, 800);
    }
  } else if (appState == STATE_DEMO_RUN) {
    if (evt == EVT_SELECT_LONG || evt == EVT_PLUS_MINUS_COMBO) {
      stopBuzzer();
      enterState(STATE_HOME);
    } else if (demoStage == 3 && evt == EVT_SELECT_CLICK) {
      if (!demoWakeAck) {
        demoWakeAck = true;
        stopBuzzer();
      } else {
        demoStage = (demoStage + 1) % 8;
        lastDemoStage = 255;
        stateEnterMs = nowMs - (uint32_t)demoStage * DEMO_STAGE_MS;
      }
    } else if (evt == EVT_SELECT_CLICK || evt == EVT_PLUS_CLICK || evt == EVT_PLUS_REPEAT) {
      demoStage = (demoStage + 1) % 8;
      lastDemoStage = 255;
      stateEnterMs = nowMs - (uint32_t)demoStage * DEMO_STAGE_MS;
    } else if (evt == EVT_MINUS_CLICK || evt == EVT_MINUS_REPEAT) {
      demoStage = (demoStage + 7) % 8;
      lastDemoStage = 255;
      stateEnterMs = nowMs - (uint32_t)demoStage * DEMO_STAGE_MS;
    }
  } else if (appState == STATE_RINGING) {
    stopBuzzer();
    if (wakeRingCycle == 0) {
      wakeConfirmPending = true;
      wakeConfirmDueMs = nowMs + 20000UL;
      wakeRingCycle = 1;
      setToastText("Check in 20s", 1800);
    } else {
      wakeConfirmPending = false;
      wakeRingCycle = 0;
      if (todayWakeAck < 99) todayWakeAck++;
      if (todayWakeAck >= 3) setToastText("Stand up", 1800);
      else if (todayWakeAck == 2) setToastText("Really up?", 1800);
      else setToastP(MSG_GOOD_START, 1600);
    }
    enterState(STATE_HOME);
  } else if (appState == STATE_TEMP_ALERT) {
    stopBuzzer();
    enterState(STATE_HOME);
  } else if (appState == STATE_HOUR_ANIM) {
    enterState(STATE_HOME);
  }
}

void checkTimeouts() {
  if ((appState == STATE_SET_TIME || appState == STATE_SET_ALARM_TEMP || appState == STATE_SET_TARGET ||
       appState == STATE_TOOL_MENU || appState == STATE_FOCUS_SELECT || appState == STATE_SPRINT_SELECT ||
       appState == STATE_NOTIFY_SELECT) &&
      nowMs - lastUserActionMs > 9000) {
    if (appState == STATE_SET_TIME) writeRtcFromEdit();
    else if (appState == STATE_SET_ALARM_TEMP || appState == STATE_SET_TARGET || appState == STATE_NOTIFY_SELECT) saveConfig();
    enterState(STATE_HOME);
  }

  if (appState == STATE_TEMP_ALERT && nowMs - stateEnterMs > 4500) {
    enterState(STATE_HOME);
  }

  if (appState == STATE_HOUR_ANIM && nowMs - stateEnterMs > 2800) {
    enterState(STATE_HOME);
  }

  if (appState == STATE_RINGING && nowMs - stateEnterMs > (wakeRingCycle == 0 ? 24000UL : 16000UL)) {
    stopBuzzer();
    wakeConfirmPending = true;
    wakeConfirmDueMs = nowMs + 20000UL;
    wakeRingCycle = 1;
    enterState(STATE_HOME);
  }

}

// -------------------- UI 娓叉�? --------------------
void drawLines(const char *line0, const char *line1) {
  for (uint8_t row = 0; row < 2; row++) {
    const char *line = row == 0 ? line0 : line1;
    for (uint8_t col = 0; col < 16; col++) {
      if (lcdCache[row][col] != line[col]) {
        lcd.setCursor(col, row);
        lcd.write((uint8_t)line[col]);
        lcdCache[row][col] = line[col];
      }
    }
  }
}

void invalidateLcdCache() {
  for (uint8_t r = 0; r < 2; r++) {
    for (uint8_t c = 0; c < 16; c++) lcdCache[r][c] = char(127);
    lcdCache[r][16] = '\0';
  }
}

void weekdayToLine(char *line, uint8_t pos, uint8_t wd) {
  const char *names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  copyText(line, pos, names[wd]);
}

void renderHome(char *line0, char *line1) {
  fillLine(line0);
  fillLine(line1);

  twoDigitsToBuf(line0, 0, nowTime.hour);
  line0[2] = (blinkOn || nowTime.hour >= 23 || nowTime.hour < 6) ? ':' : ' ';
  twoDigitsToBuf(line0, 3, nowTime.minute);
  line0[5] = ':';
  twoDigitsToBuf(line0, 6, nowTime.second);
  line0[8] = ' ';
  twoDigitsToBuf(line0, 9, nowTime.month);
  line0[11] = '/';
  twoDigitsToBuf(line0, 12, nowTime.day);
  if (ddlUrgency() >= DDL_TENSE) line0[15] = blinkOn ? '!' : ' ';
  if (rtcFallbackMode) line0[15] = blinkOn ? '?' : ' ';

  if (toastUntilMs > nowMs) {
    for (uint8_t i = 0; i < 16; i++) line1[i] = toastLine[i];
    return;
  }

  if (homePage == 0) {
    fourDigitsToBuf(line1, 0, nowTime.year);
    line1[4] = '-';
    twoDigitsToBuf(line1, 5, nowTime.month);
    line1[7] = '-';
    twoDigitsToBuf(line1, 8, nowTime.day);
    weekdayToLine(line1, 11, weekDay(nowTime.year, nowTime.month, nowTime.day));
  } else if (homePage == 1) {
    uint8_t slot = (uint8_t)((nowMs / 1700UL) % 3);
    int16_t d = ddlDaysLeft(slot);
    int16_t rawD = d;
    copyText(line1, 0, "D");
    line1[1] = char('1' + slot);
    copyText(line1, 2, " D");
    if (d >= 0) {
      line1[4] = '-';
      if (d >= 100) line1[5] = char('0' + d / 100);
      if (d >= 10) line1[6] = char('0' + (d / 10) % 10);
      line1[7] = char('0' + d % 10);
    } else {
      line1[4] = '+';
      d = -d;
      twoDigitsToBuf(line1, 5, d);
    }
    copyProgmemText(line1, 9, ddlMessageForDays(rawD));
  } else if (homePage == 2) {
    copyText(line1, 0, "Focus ");
    if (todayFocusDone < 10) line1[6] = char('0' + todayFocusDone);
    else twoDigitsToBuf(line1, 6, todayFocusDone);
    copyText(line1, 9, "Wake ");
    if (todayWakeAck < 10) line1[14] = char('0' + todayWakeAck);
    else line1[14] = '9';
  } else if (homePage == 3) {
    int8_t tr = tempTrend();
    if (tr <= -3) copyProgmemText(line1, 0, MSG_WINDOW);
    else if (tempC() >= cfg.tempHigh) copyProgmemText(line1, 0, MSG_WATER);
    else if (tempC() <= cfg.tempLow) copyProgmemText(line1, 0, MSG_WARM);
    else copyProgmemText(line1, 0, contextMessage());
  } else {
    copyText(line1, 0, "Next ");
    copyProgmemText(line1, 5, scheduleMessage());
  }
}

void blankField(char *line, uint8_t pos, uint8_t len) {
  if (!blinkOn) {
    for (uint8_t i = 0; i < len && pos + i < 16; i++) line[pos + i] = ' ';
  }
}

void drawProgress(char *line, uint8_t pos, uint8_t width, uint32_t elapsed, uint32_t total) {
  if (total == 0) total = 1;
  if (elapsed > total) elapsed = total;
  uint8_t filled = (uint8_t)((elapsed * width) / total);
  for (uint8_t i = 0; i < width && pos + i < 16; i++) {
    if (i < filled) line[pos + i] = char(6);
    else if (i == filled && filled < width && blinkOn) line[pos + i] = char(7);
    else line[pos + i] = '.';
  }
}

void drawMiniStepper(char *line, uint8_t pos, uint8_t count, uint8_t active) {
  for (uint8_t i = 0; i < count && pos + i < 16; i++) {
    line[pos + i] = (i == active) ? char(6) : '.';
  }
}

void renderSetTime(char *line0, char *line1) {
  fillLine(line0);
  fillLine(line1);
  const char *labels[] = {"Year", "Month", "Day", "Hour", "Minute", "Second"};
  copyText(line0, 0, "Set ");
  copyText(line0, 4, labels[editField]);
  if (editField == 5) {
    twoDigitsToBuf(line0, 13, editTime.second);
    blankField(line0, 13, 2);
  }

  fourDigitsToBuf(line1, 0, editTime.year);
  line1[4] = '-';
  twoDigitsToBuf(line1, 5, editTime.month);
  line1[7] = '-';
  twoDigitsToBuf(line1, 8, editTime.day);
  line1[10] = ' ';
  twoDigitsToBuf(line1, 11, editTime.hour);
  line1[13] = ':';
  twoDigitsToBuf(line1, 14, editTime.minute);

  if (editField == 0) blankField(line1, 0, 4);
  else if (editField == 1) blankField(line1, 5, 2);
  else if (editField == 2) blankField(line1, 8, 2);
  else if (editField == 3) blankField(line1, 11, 2);
  else if (editField == 4) blankField(line1, 14, 2);
}

void renderSetAlarmTemp(char *line0, char *line1) {
  fillLine(line0);
  fillLine(line1);
  const char *labels[] = {"Alarm Hour", "Alarm Min", "Temp High", "Temp Low", "Alarm On"};
  copyText(line0, 0, labels[editField]);

  twoDigitsToBuf(line1, 0, cfg.alarmHour);
  line1[2] = ':';
  twoDigitsToBuf(line1, 3, cfg.alarmMinute);
  line1[6] = 'H';
  twoDigitsToBuf(line1, 7, cfg.tempHigh);
  line1[10] = 'L';
  twoDigitsToBuf(line1, 11, cfg.tempLow);
  line1[14] = cfg.alarmEnabled ? 'Y' : 'N';

  if (editField == 0) blankField(line1, 0, 2);
  else if (editField == 1) blankField(line1, 3, 2);
  else if (editField == 2) blankField(line1, 7, 2);
  else if (editField == 3) blankField(line1, 11, 2);
  else if (editField == 4) blankField(line1, 14, 1);
}

void renderSetTarget(char *line0, char *line1) {
  fillLine(line0);
  fillLine(line1);
  const char *labels[] = {"Target No", "Target Year", "Target Mon", "Target Day"};
  uint8_t slot = cfg.activeTarget;
  uint16_t y = targetYear(slot);
  uint8_t m = targetMonth(slot);
  uint8_t d = targetDay(slot);
  copyText(line0, 0, labels[editField]);
  copyText(line1, 0, "D");
  line1[1] = char('1' + slot);
  line1[2] = ' ';
  fourDigitsToBuf(line1, 3, y);
  line1[7] = '-';
  twoDigitsToBuf(line1, 8, m);
  line1[10] = '-';
  twoDigitsToBuf(line1, 11, d);
  int16_t left = ddlDaysLeft(slot);
  copyText(line1, 14, "D");
  if (left < 0) line1[15] = '+';
  else if (left > 9) line1[15] = '*';
  else line1[15] = char('0' + left);

  if (editField == 0) blankField(line1, 0, 2);
  else if (editField == 1) blankField(line1, 3, 4);
  else if (editField == 2) blankField(line1, 8, 2);
  else if (editField == 3) blankField(line1, 11, 2);
}

void renderToolMenu(char *line0, char *line1) {
  fillLine(line0);
  fillLine(line1);
  copyText(line0, 0, "Tools");
  line0[6] = char('1' + toolIndex);
  copyText(line0, 7, "/4");
  drawMiniStepper(line0, 11, 4, toolIndex);
  line1[0] = blinkOn ? '>' : char(7);
  if (toolIndex == 0) copyText(line1, 2, "Focus Rhythm");
  else if (toolIndex == 1) copyText(line1, 2, "DDL Sprint");
  else if (toolIndex == 2) copyText(line1, 2, "Notify Mode");
  else copyText(line1, 2, "Demo Show");
}

void renderFocusSelect(char *line0, char *line1) {
  fillLine(line0);
  fillLine(line1);
  copyText(line0, 0, "Focus Rhythm");
  const FocusProfile &p = focusProfiles[cfg.focusProfile];
  twoDigitsToBuf(line1, 0, p.workMin);
  line1[2] = '/';
  twoDigitsToBuf(line1, 3, p.restMin);
  copyText(line1, 7, "SEL Start");
}

void renderFocusRun(char *line0, char *line1) {
  fillLine(line0);
  fillLine(line1);
  copyText(line0, 0, focusWorking ? "Focus Work" : "Focus Rest");
  uint32_t elapsed = nowMs - focusPhaseStartMs;
  uint32_t left = (elapsed >= focusPhaseDurationMs) ? 0 : (focusPhaseDurationMs - elapsed);
  uint16_t leftSec = (uint16_t)(left / 1000UL);
  uint8_t mm = leftSec / 60;
  uint8_t ss = leftSec % 60;
  twoDigitsToBuf(line0, 11, mm);
  line0[13] = ':';
  twoDigitsToBuf(line0, 14, ss);
  drawProgress(line1, 0, 16, elapsed, focusPhaseDurationMs);
}

void renderSprintSelect(char *line0, char *line1) {
  fillLine(line0);
  fillLine(line1);
  copyText(line0, 0, "Submit Sprint");
  twoDigitsToBuf(line1, 0, sprintProfiles[cfg.sprintProfile]);
  copyText(line1, 3, "min SEL Start");
}

void renderSprintRun(char *line0, char *line1) {
  fillLine(line0);
  fillLine(line1);
  copyText(line0, 0, "Sprint ");
  uint32_t elapsed = nowMs - sprintStartMs;
  uint32_t left = (elapsed >= sprintDurationMs) ? 0 : (sprintDurationMs - elapsed);
  uint16_t leftSec = (uint16_t)(left / 1000UL);
  uint8_t mm = leftSec / 60;
  uint8_t ss = leftSec % 60;
  twoDigitsToBuf(line0, 7, mm);
  line0[9] = ':';
  twoDigitsToBuf(line0, 10, ss);
  copyProgmemText(line1, 0, sprintAction((left + 59999UL) / 60000UL));
  drawProgress(line1, 10, 6, elapsed, sprintDurationMs);
}

void renderNotifySelect(char *line0, char *line1) {
  fillLine(line0);
  fillLine(line1);
  copyText(line0, 0, "Notify Mode");
  if (cfg.notifyMode == 0) copyText(line1, 0, "Silent");
  else if (cfg.notifyMode == 1) copyText(line1, 0, "Soft");
  else copyText(line1, 0, "Normal");
}

void renderRinging(char *line0, char *line1) {
  fillLine(line0);
  fillLine(line1);
  copyText(line0, 0, wakeRingCycle == 0 ? "Gentle Alarm" : "Up Confirm");
  uint32_t e = (nowMs - stateEnterMs) / 1000UL;
  uint8_t wave = ((nowMs - stateEnterMs) / 300UL) % 4;
  for (uint8_t i = 0; i < wave && 12 + i < 16; i++) line0[12 + i] = ')';
  if (wakeRingCycle > 0) {
    copyText(line1, 0, "Press awake");
  } else {
    if (e < 10) copyText(line1, 0, "Soft melody");
    else if (e < 30) copyText(line1, 0, "Wake up");
    else copyText(line1, 0, "Urgent");
    copyText(line1, 9, "Key OK");
  }
  drawProgress(line1, 14, 2, nowMs - stateEnterMs, wakeRingCycle == 0 ? 22000UL : 14000UL);
}

void renderDemoRun(char *line0, char *line1) {
  fillLine(line0);
  fillLine(line1);
  copyText(line0, 0, "Demo ");
  line0[5] = char('1' + demoStage);
  copyText(line0, 6, "/8 ");
  uint32_t phase = (nowMs - stateEnterMs) % DEMO_STAGE_MS;
  uint8_t sub = (uint8_t)(phase / DEMO_SUB_MS);

  if (demoStage == 0) {
    copyText(line0, 9, "Clock");
    twoDigitsToBuf(line1, 0, nowTime.hour);
    line1[2] = ':';
    twoDigitsToBuf(line1, 3, nowTime.minute);
    line1[5] = ':';
    twoDigitsToBuf(line1, 6, nowTime.second);
    copyText(line1, 9, "Always");
  } else if (demoStage == 1) {
    copyText(line0, 9, "DDLx3");
    uint8_t slot = sub;
    if (slot > 2) slot = 2;
    int16_t d = ddlDaysLeft(slot);
    copyText(line1, 0, "D");
    line1[1] = char('1' + slot);
    copyText(line1, 3, "D");
    if (d < 0) {
      line1[4] = '+';
      d = -d;
    } else {
      line1[4] = '-';
    }
    if (d > 99) d = 99;
    twoDigitsToBuf(line1, 5, (uint8_t)d);
    copyProgmemText(line1, 8, ddlMessageForDays(ddlDaysLeft(slot)));
  } else if (demoStage == 2) {
    copyText(line0, 9, "Alarm");
    if (sub == 0) copyText(line1, 0, "Soft melody");
    else if (sub == 1) copyText(line1, 0, "Wake stronger");
    else copyText(line1, 0, "Urgent pulse");
  } else if (demoStage == 3) {
    copyText(line0, 9, "Wake");
    if (demoWakeAck) copyText(line1, 0, "Awake confirmed");
    else {
      copyText(line1, 0, "Really awake?");
      copyText(line1, 14, "OK");
    }
  } else if (demoStage == 4) {
    copyText(line0, 9, "Temp");
    if (sub == 0) copyText(line1, 0, "Hot Drink water");
    else if (sub == 1) copyText(line1, 0, "Cold Keep warm");
    else copyText(line1, 0, "Drop Close win");
  } else if (demoStage == 5) {
    copyText(line0, 9, "Focus");
    const FocusProfile &p = focusProfiles[sub > 2 ? 2 : sub];
    twoDigitsToBuf(line1, 0, p.workMin);
    line1[2] = '/';
    twoDigitsToBuf(line1, 3, p.restMin);
    copyText(line1, 6, " rhythm");
  } else if (demoStage == 6) {
    copyText(line0, 9, "Sprint");
    if (sub == 0) copyText(line1, 0, "Plan Save code");
    else if (sub == 1) copyText(line1, 0, "Report Backup");
    else copyText(line1, 0, "Submit NOW");
  } else {
    copyText(line0, 9, "Hour");
    copyText(line1, 0, "Hourglass ");
    uint8_t frame = ((nowMs - stateEnterMs) / 250) % 4;
    line1[10] = char(frame);
    line1[11] = char((frame + 1) % 4);
  }
}

void renderTempAlert(char *line0, char *line1) {
  fillLine(line0);
  fillLine(line1);
  copyText(line0, 0, "Temp Care");
  line0[10] = blinkOn ? '!' : ' ';
  line0[12] = tempTrend() <= -3 ? char(5) : char(4);
  if (toastUntilMs > nowMs) {
    for (uint8_t i = 0; i < 16; i++) line1[i] = toastLine[i];
  } else {
    int8_t t = tempC();
    copyText(line1, 0, "Now ");
    if (t < 0) {
      line1[4] = '-';
      t = -t;
      twoDigitsToBuf(line1, 5, t);
    } else {
      twoDigitsToBuf(line1, 4, t);
    }
    line1[7] = (char)223;
    line1[8] = 'C';
  }
}

void renderHourAnim(char *line0, char *line1) {
  fillLine(line0);
  fillLine(line1);
  copyText(line0, 2, "Hour Moment");
  line1[5] = '[';
  line1[10] = ']';
  uint8_t frame = ((nowMs - stateEnterMs) / 250) % 4;
  line1[7] = char(frame);
  line1[8] = char((frame + 1) % 4);
}

void renderUi() {
  char line0[17];
  char line1[17];

  if (nowMs - lastBlinkMs > 500) {
    blinkOn = !blinkOn;
    lastBlinkMs = nowMs;
  }

  if (appState == STATE_HOME && nowMs - lastHomePageMs > 5000) {
    homePage = (homePage + 1) % 5;
    lastHomePageMs = nowMs;
  }

  if (appState == STATE_HOME) renderHome(line0, line1);
  else if (appState == STATE_SET_TIME) renderSetTime(line0, line1);
  else if (appState == STATE_SET_ALARM_TEMP) renderSetAlarmTemp(line0, line1);
  else if (appState == STATE_SET_TARGET) renderSetTarget(line0, line1);
  else if (appState == STATE_TOOL_MENU) renderToolMenu(line0, line1);
  else if (appState == STATE_FOCUS_SELECT) renderFocusSelect(line0, line1);
  else if (appState == STATE_FOCUS_RUN) renderFocusRun(line0, line1);
  else if (appState == STATE_SPRINT_SELECT) renderSprintSelect(line0, line1);
  else if (appState == STATE_SPRINT_RUN) renderSprintRun(line0, line1);
  else if (appState == STATE_NOTIFY_SELECT) renderNotifySelect(line0, line1);
  else if (appState == STATE_DEMO_RUN) renderDemoRun(line0, line1);
  else if (appState == STATE_RINGING) renderRinging(line0, line1);
  else if (appState == STATE_TEMP_ALERT) renderTempAlert(line0, line1);
  else renderHourAnim(line0, line1);

  drawLines(line0, line1);
}

// -------------------- Arduino 鏍囧噯鍏ュ彛 --------------------
void setup() {
  pinMode(PIN_KEY_SELECT, INPUT_PULLUP);
  pinMode(PIN_KEY_PLUS, INPUT_PULLUP);
  pinMode(PIN_KEY_MINUS, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  lcd.begin(16, 2);
  lcd.createChar(0, CH_HOURGLASS_0);
  lcd.createChar(1, CH_HOURGLASS_1);
  lcd.createChar(2, CH_HOURGLASS_2);
  lcd.createChar(3, CH_HOURGLASS_3);
  lcd.createChar(4, CH_UP);
  lcd.createChar(5, CH_DOWN);
  lcd.createChar(6, CH_BAR_FULL);
  lcd.createChar(7, CH_BAR_HEAD);
  invalidateLcdCache();

  pinMode(PIN_DS1302_CE, OUTPUT);
  pinMode(PIN_DS1302_SCLK, OUTPUT);
  pinMode(PIN_DS1302_IO, INPUT);
  digitalWrite(PIN_DS1302_CE, LOW);
  digitalWrite(PIN_DS1302_SCLK, LOW);

  loadConfig();
  initializeRtcAtBoot();
  readTemperature();
  lastStatDay = nowTime.day;
}

void loop() {
  nowMs = millis();

  if (USE_RTC_PRIMARY) {
    if (appState != STATE_SET_TIME && nowMs - lastRtcReadMs >= RTC_POLL_MS) {
      lastRtcReadMs = nowMs;
      if (readRtc()) checkAlarms();
      else if (updateSoftwareClock()) checkAlarms();
    }
  } else {
    if (updateSoftwareClock()) {
      checkAlarms();
    }
    if (appState != STATE_SET_TIME && nowMs - lastRtcReadMs >= RTC_SYNC_MS) {
      lastRtcReadMs = nowMs;
      if (syncClockFromRtc()) checkAlarms();
    }
  }
  updateRtcBackupAnchorIfNeeded();

  if (nowMs - lastTempReadMs >= 2000) {
    lastTempReadMs = nowMs;
    readTemperature();
  }

  if (nowMs - lastTempHistoryMs >= 120000UL) {
    lastTempHistoryMs = nowMs;
    tempHistoryIndex = (tempHistoryIndex + 1) % 6;
    tempHistory[tempHistoryIndex] = tempC();
  }

  ButtonEvent evt = readButtons();
  handleEvent(evt);
  updateFocusRun();
  updateSprintRun();
  updateDemoRun();
  checkTimeouts();
  updateBuzzer();

  if (nowMs - lastUiUpdateMs >= 120) {
    lastUiUpdateMs = nowMs;
    renderUi();
  }
}
