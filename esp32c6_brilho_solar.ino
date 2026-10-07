/*
  ============================================================
  LOGGER ROBUSTO V2 - ESP32-C6 + J11 + PCF8563 + SD + BATERIA
  ============================================================

  Versao focada em robustez temporal apos diagnostico do PCF8563.

  Principais reforcos em relacao ao firmware anterior:
  - Validacao rigorosa do PCF8563:
      * leitura de CTRL1 e CTRL2
      * diagnostico de VL, STOP, TEST1, TESTC
      * validacao BCD antes da conversao
      * diagnostico bruto dos registradores quando ha falha
  - Ajuste robusto do RTC:
      * STOP temporario
      * limpeza de CTRL2
      * escrita BCD com mascaras corretas
      * limpeza de VL
      * liberacao do oscilador
      * releitura obrigatoria
  - Console Serial na janela de boot:
      HELP, SCAN, READ, DUMP, DIAG, TICK N,
      SET AAAA-MM-DD HH:MM:SS, SET_COMPILE,
      RESET_CTRL, CLR_VL, START_RTC, STOP_RTC
  - SoftClock em RTC memory do ESP32-C6 como redundancia entre deep sleeps.
  - Reparo automatico do PCF8563 usando SoftClock quando possivel.
  - CSV com auditoria temporal: time_source, timestamp_ok, softclock_status,
    rtc_ctrl1, rtc_ctrl2, rtc_stop, rtc_testc, rtc_bcd_ok, rtc_raw_*.
  - Status de bateria e erro de SD no CSV.

  Hardware considerado:
    ESP32-C6
    I2C principal: SDA=GPIO6, SCL=GPIO7
    PCF8563: 0x51
    BH1750 J11: 0x23, ADDR em GPIO1 LOW
    TCA9554: 0x20, P2 mantendo RAK3172 em reset
    SD: CS=21, MISO=20, SCK=19, MOSI=18
    Bateria: GPIO0 com divisor 510k / 1.1M

  Serial Monitor:
    115200 baud
    Final de linha: Nova linha ou Both NL & CR

  ============================================================
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <math.h>
#include <string.h>

#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_idf_version.h"

// ============================================================
// PINOS
// ============================================================

#define I2C_SDA 6
#define I2C_SCL 7

#define PIN_ADDR_J11 1
#define PIN_ADDR_J8  8

#define SD_CS   21
#define SD_MISO 20
#define SD_SCK  19
#define SD_MOSI 18

#define PIN_BAT 0
#define PIN_BOOT_BUTTON 9
#define RAK_BOOT0_ESP 3

// ============================================================
// ENDERECOS I2C
// ============================================================

#define ADDR_TCA9554  0x20
#define ADDR_BH1750   0x23
#define ADDR_PCF8563  0x51

// ============================================================
// BH1750 - SEM MTreg
// ============================================================

#define BH1750_POWER_DOWN        0x00
#define BH1750_POWER_ON          0x01
#define BH1750_RESET             0x07
#define BH1750_ONE_TIME_HIGH_RES 0x20

// ============================================================
// PCF8563
// ============================================================

#define PCF8563_REG_CTRL1     0x00
#define PCF8563_REG_CTRL2     0x01
#define PCF8563_REG_SECONDS   0x02
#define PCF8563_REG_MINUTES   0x03
#define PCF8563_REG_HOURS     0x04
#define PCF8563_REG_DAYS      0x05
#define PCF8563_REG_WEEKDAYS  0x06
#define PCF8563_REG_MONTHS    0x07
#define PCF8563_REG_YEARS     0x08

#define PCF8563_BIT_VL        0x80
#define PCF8563_BIT_STOP      0x20
#define PCF8563_BIT_TEST1     0x80
#define PCF8563_BIT_TESTC     0x08

// ============================================================
// TCA9554
// ============================================================

#define TCA_REG_INPUT    0x00
#define TCA_REG_OUTPUT   0x01
#define TCA_REG_POLARITY 0x02
#define TCA_REG_CONFIG   0x03

#define TCA_P2_NRST_RAK 2

// ============================================================
// CONFIGURACOES GERAIS
// ============================================================

#define SERIAL_BAUD 115200

#define I2C_FREQ_HZ     100000UL
#define I2C_TIMEOUT_MS  100UL

#define SD_SPI_FREQ_HZ  1000000UL

#define SAMPLE_INTERVAL_SEC 300UL

#define HORA_INICIO_MEDICAO 5
#define HORA_FIM_MEDICAO    19

#define BOOT_WINDOW_SECONDS 15
#define WDT_TIMEOUT_SEC     30

#define MAX_TENTATIVAS_J11 5
#define MAX_TENTATIVAS_SD  5

#define RTC_YEAR_MIN 2024
#define RTC_YEAR_MAX 2099

// true: janela de boot apenas em POWERON/RESET/WDT.
// false: janela de boot tambem apos deep sleep.
const bool BOOT_WINDOW_ONLY_COLD_BOOT = true;

// Conversao nominal usada no projeto.
const float LUX_POR_WM2 = 120.0f;

// Divisor resistivo da bateria:
// R1: bateria -> ADC
// R2: ADC -> GND
const float BAT_R1_OHMS = 510000.0f;
const float BAT_R2_OHMS = 1100000.0f;
const float BAT_DIV_RATIO = (BAT_R1_OHMS + BAT_R2_OHMS) / BAT_R2_OHMS;

const float BAT_CAL_A = 1.0000f;
const float BAT_CAL_B = 0.0000f;

// ============================================================
// SOFTCLOCK
// ============================================================

#define SOFTCLOCK_MAGIC       0xC1C0A55AUL
#define SOFTCLOCK_CRC_XOR     0x5A5AA5A5UL
#define SOFTCLOCK_TOLERANCIA_SEC 30UL
#define SOFTCLOCK_SLEEP_MAX_VALIDO_SEC (18UL * 3600UL)

// ============================================================
// OBJETOS
// ============================================================

SPIClass spiSD(FSPI);

// ============================================================
// VARIAVEIS EM RTC MEMORY DO ESP32
// ============================================================

RTC_DATA_ATTR uint32_t bootCounter = 0;
RTC_DATA_ATTR uint32_t cycleCounter = 0;
RTC_DATA_ATTR uint32_t lastEpochUsado = 0;

RTC_DATA_ATTR uint32_t softClockMagic = 0;
RTC_DATA_ATTR uint32_t softEpochAntesSleep = 0;
RTC_DATA_ATTR uint32_t softSleepProgramadoSec = 0;
RTC_DATA_ATTR uint32_t softClockCRC = 0;
RTC_DATA_ATTR uint32_t softClockReparos = 0;
RTC_DATA_ATTR uint32_t softClockFalhas = 0;

// ============================================================
// ESTADOS GLOBAIS
// ============================================================

bool g_wdt_started = false;
bool tcaOK = false;
bool sdOK = false;

String resetReasonStr = "UNKNOWN";
String wakeReasonStr  = "UNKNOWN";
String ultimoSdErro   = "NONE";

uint32_t g_epochBase = 0;
uint32_t g_millisBase = 0;
bool g_tempoValido = false;
String g_fonteTempo = "NONE";
String g_softStatus = "FAIL";
bool g_softValido = false;
uint32_t g_epochSoft = 0;
bool g_diffRtcSoftKnown = false;
int32_t g_diffRtcSoft = 0;

// ============================================================
// CSV
// ============================================================

const char* CSV_HEADER =
  "timestamp,epoch_s,boot_count,cycle_count,reset_reason,wake_reason,delta_t_s,"
  "time_source,timestamp_ok,softclock_status,softclock_epoch,softclock_diff_rtc_s,"
  "softclock_repairs,softclock_fails,"
  "rtc_status,rtc_err,rtc_vl,rtc_ctrl1,rtc_ctrl2,rtc_stop,rtc_test1,rtc_testc,rtc_bcd_ok,rtc_date_ok,"
  "rtc_raw_sec,rtc_raw_min,rtc_raw_hour,rtc_raw_day,rtc_raw_wday,rtc_raw_month,rtc_raw_year,"
  "j11_status,j11_err,j11_attempts,j11_raw,j11_lux,j11_wm2,"
  "vbat,vbat_status,sd_status,sd_init_attempts,sd_write_attempts,sd_err,next_sleep_s";

// ============================================================
// ESTRUTURAS
// ============================================================

struct DateTimeRTC {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
  int weekday;

  bool i2cOk;
  bool controlReadOk;
  bool timeReadOk;
  bool plausible;
  bool valid;
  bool vlKnown;
  bool vlSet;
  bool stopSet;
  bool test1Set;
  bool testcSet;
  bool bcdOk;
  bool dateOk;
  bool centuryBit;

  uint8_t ctrl1;
  uint8_t ctrl2;
  uint8_t raw[7];

  char err[32];
};

struct J11Reading {
  bool ok;
  bool saturated;
  uint16_t raw;
  float lux;
  float wm2;
  int attempts;
  char status[12];
  char err[32];
};

// ============================================================
// FUNCOES BASICAS
// ============================================================

void copyErr(char *dst, size_t len, const char *src) {
  if (dst == nullptr || len == 0) return;
  strncpy(dst, src, len - 1);
  dst[len - 1] = '\0';
}

void feedWdt() {
  if (g_wdt_started) {
    esp_task_wdt_reset();
  }
}

void safeDelay(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    feedWdt();
    delay(20);
  }
}

uint8_t bcd2dec(uint8_t val) {
  return ((val >> 4) * 10) + (val & 0x0F);
}

uint8_t dec2bcd(uint8_t val) {
  return ((val / 10) << 4) | (val % 10);
}

bool bcdNibblesOk(uint8_t val) {
  return ((val & 0x0F) <= 9) && (((val >> 4) & 0x0F) <= 9);
}

bool bcdRangeOk(uint8_t raw, uint8_t mask, int minVal, int maxVal) {
  uint8_t v = raw & mask;
  if (!bcdNibblesOk(v)) return false;
  int dec = bcd2dec(v);
  return dec >= minVal && dec <= maxVal;
}

bool isLeapYear(int y) {
  if (y % 400 == 0) return true;
  if (y % 100 == 0) return false;
  return (y % 4 == 0);
}

int daysInMonth(int year, int month) {
  switch (month) {
    case 1: return 31;
    case 2: return isLeapYear(year) ? 29 : 28;
    case 3: return 31;
    case 4: return 30;
    case 5: return 31;
    case 6: return 30;
    case 7: return 31;
    case 8: return 31;
    case 9: return 30;
    case 10: return 31;
    case 11: return 30;
    case 12: return 31;
    default: return 0;
  }
}

bool validDateTime(int y, int mo, int d, int h, int mi, int s) {
  if (y < RTC_YEAR_MIN || y > RTC_YEAR_MAX) return false;
  if (mo < 1 || mo > 12) return false;
  if (d < 1 || d > daysInMonth(y, mo)) return false;
  if (h < 0 || h > 23) return false;
  if (mi < 0 || mi > 59) return false;
  if (s < 0 || s > 59) return false;
  return true;
}

uint32_t daysBeforeMonth(int year, int month) {
  static const uint16_t daysNormal[] =
    {0,31,59,90,120,151,181,212,243,273,304,334};

  if (month < 1 || month > 12) return 0;

  uint32_t d = daysNormal[month - 1];
  if (month > 2 && isLeapYear(year)) d++;
  return d;
}

uint32_t daysSince1970(int year, int month, int day) {
  uint32_t days = 0;

  for (int y = 1970; y < year; y++) {
    days += isLeapYear(y) ? 366 : 365;
  }

  days += daysBeforeMonth(year, month);
  days += day - 1;

  return days;
}

uint32_t dateTimeToEpoch(const DateTimeRTC &dt) {
  if (!dt.valid) return 0;

  uint32_t days = daysSince1970(dt.year, dt.month, dt.day);

  return days * 86400UL +
         dt.hour * 3600UL +
         dt.minute * 60UL +
         dt.second;
}

void epochToDateTime(uint32_t epoch, DateTimeRTC &dt) {
  memset(&dt, 0, sizeof(DateTimeRTC));
  copyErr(dt.err, sizeof(dt.err), "SOFT_INIT");

  if (epoch == 0) {
    copyErr(dt.err, sizeof(dt.err), "SOFT_ZERO");
    return;
  }

  uint32_t dias = epoch / 86400UL;
  uint32_t resto = epoch % 86400UL;

  dt.hour = resto / 3600UL;
  resto %= 3600UL;
  dt.minute = resto / 60UL;
  dt.second = resto % 60UL;

  int year = 1970;
  while (true) {
    uint16_t diasAno = isLeapYear(year) ? 366 : 365;
    if (dias >= diasAno) {
      dias -= diasAno;
      year++;
    } else {
      break;
    }

    if (year > 2099) {
      copyErr(dt.err, sizeof(dt.err), "SOFT_RANGE");
      return;
    }
  }

  int month = 1;
  while (month <= 12) {
    int dim = daysInMonth(year, month);
    if (dias >= (uint32_t)dim) {
      dias -= dim;
      month++;
    } else {
      break;
    }
  }

  dt.year = year;
  dt.month = month;
  dt.day = dias + 1;
  dt.weekday = (daysSince1970(dt.year, dt.month, dt.day) + 4) % 7;

  dt.plausible = validDateTime(dt.year, dt.month, dt.day,
                               dt.hour, dt.minute, dt.second);
  dt.dateOk = dt.plausible;
  dt.bcdOk = true;
  dt.valid = dt.plausible;

  if (dt.valid) {
    copyErr(dt.err, sizeof(dt.err), "SOFTCLOCK");
  } else {
    copyErr(dt.err, sizeof(dt.err), "SOFT_INVALID");
  }
}

int weekdayFromDate(int year, int month, int day) {
  uint32_t d = daysSince1970(year, month, day);
  // 1970-01-01 foi quinta-feira. Domingo=0.
  return (d + 4) % 7;
}

void formatTimestamp(const DateTimeRTC &dt, char *buffer, size_t len) {
  if (buffer == nullptr || len == 0) return;

  if (!dt.valid) {
    snprintf(buffer, len, "0000-00-00 00:00:00");
    return;
  }

  snprintf(buffer, len, "%04d-%02d-%02d %02d:%02d:%02d",
           dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
}

bool montarTimestampSeguro(const DateTimeRTC &dt, char *buffer, size_t len) {
  if (buffer == nullptr || len < 20) return false;

  if (!dt.valid || !validDateTime(dt.year, dt.month, dt.day,
                                  dt.hour, dt.minute, dt.second)) {
    snprintf(buffer, len, "0000-00-00 00:00:00");
    return false;
  }

  int n = snprintf(buffer, len, "%04d-%02d-%02d %02d:%02d:%02d",
                   dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

  if (n != 19 || buffer[0] == '\0') {
    snprintf(buffer, len, "0000-00-00 00:00:00");
    return false;
  }

  return true;
}

String hex2(uint8_t v) {
  char b[5];
  snprintf(b, sizeof(b), "0x%02X", v);
  return String(b);
}

String boolStr(bool v) {
  return v ? "SIM" : "NAO";
}

String floatToStr(float x, int casas) {
  if (isnan(x)) return String("nan");
  return String(x, casas);
}

String resetReasonToString(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT_RESET";
    case ESP_RST_SW:        return "SOFTWARE_RESET";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WATCHDOG";
    case ESP_RST_TASK_WDT:  return "TASK_WATCHDOG";
    case ESP_RST_WDT:       return "OTHER_WATCHDOG";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO_RESET";
    default:                return "UNKNOWN";
  }
}

String wakeReasonToString() {
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER: return "TIMER";
    case ESP_SLEEP_WAKEUP_EXT0:  return "EXT0";
    case ESP_SLEEP_WAKEUP_EXT1:  return "EXT1";
    case ESP_SLEEP_WAKEUP_GPIO:  return "GPIO";
    case ESP_SLEEP_WAKEUP_UART:  return "UART";
    default:                    return "NOT_SLEEP";
  }
}

bool isWakeFromDeepSleep() {
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
}

// ============================================================
// WATCHDOG
// ============================================================

void startWatchdog() {
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_err_t errAdd = esp_task_wdt_add(NULL);

  if (errAdd == ESP_OK) {
    g_wdt_started = true;
    feedWdt();
    Serial.println("[WDT] Task adicionada ao watchdog existente.");
    return;
  }

  esp_task_wdt_config_t config;
  memset(&config, 0, sizeof(config));
  config.timeout_ms = WDT_TIMEOUT_SEC * 1000UL;
  config.idle_core_mask = (1 << portNUM_PROCESSORS) - 1;
  config.trigger_panic = true;

  esp_err_t errInit = esp_task_wdt_init(&config);
  if (errInit != ESP_OK && errInit != ESP_ERR_INVALID_STATE) {
    Serial.print("[WDT] Falha ao iniciar. Erro=");
    Serial.println((int)errInit);
    return;
  }

  errAdd = esp_task_wdt_add(NULL);
  if (errAdd != ESP_OK && errAdd != ESP_ERR_INVALID_STATE) {
    Serial.print("[WDT] Falha ao adicionar task. Erro=");
    Serial.println((int)errAdd);
    return;
  }

  g_wdt_started = true;
  feedWdt();
  Serial.print("[WDT] Ativo. Timeout=");
  Serial.print(WDT_TIMEOUT_SEC);
  Serial.println(" s");
#else
  esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
  esp_task_wdt_add(NULL);
  g_wdt_started = true;
  feedWdt();
  Serial.print("[WDT] Ativo. Timeout=");
  Serial.print(WDT_TIMEOUT_SEC);
  Serial.println(" s");
#endif
}

// ============================================================
// I2C
// ============================================================

bool i2cPing(uint8_t addr) {
  Wire.beginTransmission(addr);
  uint8_t err = Wire.endTransmission(true);
  return err == 0;
}

bool i2cWriteByte(uint8_t addr, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

bool i2cWriteReg(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

bool i2cReadRegs(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len) {
  if (buf == nullptr || len == 0) return false;

  Wire.beginTransmission(addr);
  Wire.write(reg);
  uint8_t err = Wire.endTransmission(false);
  if (err != 0) return false;

  uint8_t n = Wire.requestFrom((int)addr, (int)len, (int)true);

  uint32_t t0 = millis();
  while (Wire.available() < len && millis() - t0 < I2C_TIMEOUT_MS) {
    feedWdt();
    delay(1);
  }

  if (Wire.available() < len) return false;

  for (uint8_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }

  return n == len;
}

bool i2cWriteRegs(uint8_t addr, uint8_t startReg, const uint8_t *buf, uint8_t len) {
  if (buf == nullptr || len == 0) return false;

  Wire.beginTransmission(addr);
  Wire.write(startReg);
  for (uint8_t i = 0; i < len; i++) {
    Wire.write(buf[i]);
  }
  return Wire.endTransmission(true) == 0;
}

void recuperarI2C() {
  Serial.println("[I2C] Recuperando barramento...");

  Wire.end();
  safeDelay(20);

  pinMode(I2C_SDA, INPUT_PULLUP);
  pinMode(I2C_SCL, INPUT_PULLUP);
  safeDelay(5);

  bool sdaLow = digitalRead(I2C_SDA) == LOW;
  bool sclLow = digitalRead(I2C_SCL) == LOW;

  Serial.print("[I2C] Antes da recuperacao: SDA=");
  Serial.print(sdaLow ? "LOW" : "HIGH");
  Serial.print(" | SCL=");
  Serial.println(sclLow ? "LOW" : "HIGH");

  pinMode(I2C_SCL, OUTPUT_OPEN_DRAIN);
  for (int i = 0; i < 18; i++) {
    digitalWrite(I2C_SCL, LOW);
    delayMicroseconds(10);
    digitalWrite(I2C_SCL, HIGH);
    delayMicroseconds(10);
  }

  // Condicao STOP manual
  pinMode(I2C_SDA, OUTPUT_OPEN_DRAIN);
  digitalWrite(I2C_SDA, LOW);
  delayMicroseconds(10);
  digitalWrite(I2C_SCL, HIGH);
  delayMicroseconds(10);
  digitalWrite(I2C_SDA, HIGH);
  delayMicroseconds(10);

  pinMode(I2C_SDA, INPUT_PULLUP);
  pinMode(I2C_SCL, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(I2C_FREQ_HZ);
  Wire.setTimeOut(I2C_TIMEOUT_MS);

  Serial.println("[I2C] Recuperacao concluida.");
}

void scanI2C() {
  Serial.print("[I2C] Enderecos encontrados: ");

  bool achou = false;

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();

    if (err == 0) {
      Serial.printf("0x%02X ", addr);
      if (addr == ADDR_PCF8563) Serial.print("(PCF8563) ");
      if (addr == ADDR_BH1750)  Serial.print("(BH1750) ");
      if (addr == ADDR_TCA9554) Serial.print("(TCA9554) ");
      achou = true;
    }

    delay(2);
  }

  if (!achou) Serial.print("nenhum");
  Serial.println();
}

// ============================================================
// PINOS SEGUROS E TCA9554
// ============================================================

void configurarPinosSeguros() {
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);

  pinMode(RAK_BOOT0_ESP, OUTPUT);
  digitalWrite(RAK_BOOT0_ESP, LOW);

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  pinMode(PIN_ADDR_J11, OUTPUT);
  digitalWrite(PIN_ADDR_J11, LOW);

  pinMode(PIN_ADDR_J8, OUTPUT);
  digitalWrite(PIN_ADDR_J8, LOW);
}

void selecionarJ11() {
  pinMode(PIN_ADDR_J11, OUTPUT);
  digitalWrite(PIN_ADDR_J11, LOW);

  pinMode(PIN_ADDR_J8, OUTPUT);
  digitalWrite(PIN_ADDR_J8, LOW);

  safeDelay(30);
}

bool tcaWriteReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ADDR_TCA9554);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

bool inicializarTCA9554() {
  Serial.println("[TCA] Verificando TCA9554 em 0x20...");

  if (!i2cPing(ADDR_TCA9554)) {
    Serial.println("[TCA] Nao encontrado. Continuando sem TCA.");
    tcaOK = false;
    return false;
  }

  tcaOK = true;

  Serial.println("[TCA] Encontrado. Mantendo RAK3172 em reset.");

  tcaWriteReg(TCA_REG_POLARITY, 0x00);
  safeDelay(20);
  tcaWriteReg(TCA_REG_CONFIG, 0xFF);
  safeDelay(20);
  tcaWriteReg(TCA_REG_OUTPUT, 0x00);
  safeDelay(20);
  tcaWriteReg(TCA_REG_CONFIG, 0xFB);
  safeDelay(20);

  return true;
}

void manterRAKEmReset() {
  digitalWrite(RAK_BOOT0_ESP, LOW);
  if (tcaOK) {
    tcaWriteReg(TCA_REG_OUTPUT, 0x00);
  }
}

// ============================================================
// RTC PCF8563
// ============================================================

void clearRTCStruct(DateTimeRTC &dt) {
  memset(&dt, 0, sizeof(DateTimeRTC));
  copyErr(dt.err, sizeof(dt.err), "NOT_READ");
}

void printRTCBruto(const DateTimeRTC &dt) {
  Serial.printf(
    "[RTC_RAW] CTRL1=0x%02X CTRL2=0x%02X SEC=0x%02X MIN=0x%02X HOUR=0x%02X DAY=0x%02X WDAY=0x%02X MONTH=0x%02X YEAR=0x%02X\n",
    dt.ctrl1, dt.ctrl2,
    dt.raw[0], dt.raw[1], dt.raw[2], dt.raw[3], dt.raw[4], dt.raw[5], dt.raw[6]
  );
}

void dumpRTCRegisters() {
  Serial.println();
  Serial.println("========== DUMP PCF8563 ==========");

  uint8_t regs[16];
  if (!i2cReadRegs(ADDR_PCF8563, 0x00, regs, 16)) {
    Serial.println("[DUMP] Falha ao ler registradores 0x00 a 0x0F.");
    Serial.println("==================================");
    return;
  }

  const char *names[16] = {
    "Control_status_1", "Control_status_2", "VL_seconds", "Minutes",
    "Hours", "Days", "Weekdays", "Century_months", "Years",
    "Minute_alarm", "Hour_alarm", "Day_alarm", "Weekday_alarm",
    "CLKOUT_control", "Timer_control", "Timer"
  };

  for (int i = 0; i < 16; i++) {
    Serial.printf("0x%02X  %-16s = 0x%02X  bin=", i, names[i], regs[i]);
    for (int b = 7; b >= 0; b--) Serial.print((regs[i] >> b) & 1);
    Serial.println();
  }

  Serial.println("==================================");
}

bool readRTC(DateTimeRTC &dt, bool verboseOnError = true) {
  clearRTCStruct(dt);

  if (!i2cPing(ADDR_PCF8563)) {
    dt.i2cOk = false;
    copyErr(dt.err, sizeof(dt.err), "PING_FAIL");
    if (verboseOnError) Serial.println("[RTC] PING_FAIL: PCF8563 nao respondeu em 0x51.");
    return false;
  }

  dt.i2cOk = true;

  uint8_t regs[9];
  if (!i2cReadRegs(ADDR_PCF8563, PCF8563_REG_CTRL1, regs, 9)) {
    copyErr(dt.err, sizeof(dt.err), "READ_FAIL");
    if (verboseOnError) Serial.println("[RTC] READ_FAIL: falha ao ler registradores 0x00..0x08.");
    return false;
  }

  dt.controlReadOk = true;
  dt.timeReadOk = true;
  dt.ctrl1 = regs[0];
  dt.ctrl2 = regs[1];
  for (int i = 0; i < 7; i++) dt.raw[i] = regs[i + 2];

  dt.vlKnown = true;
  dt.vlSet = (dt.raw[0] & PCF8563_BIT_VL) != 0;
  dt.stopSet = (dt.ctrl1 & PCF8563_BIT_STOP) != 0;
  dt.test1Set = (dt.ctrl1 & PCF8563_BIT_TEST1) != 0;
  dt.testcSet = (dt.ctrl1 & PCF8563_BIT_TESTC) != 0;
  dt.centuryBit = (dt.raw[5] & 0x80) != 0;

  bool secOk   = bcdRangeOk(dt.raw[0], 0x7F, 0, 59);
  bool minOk   = bcdRangeOk(dt.raw[1], 0x7F, 0, 59);
  bool hourOk  = bcdRangeOk(dt.raw[2], 0x3F, 0, 23);
  bool dayOk   = bcdRangeOk(dt.raw[3], 0x3F, 1, 31);
  bool wdayOk  = bcdRangeOk(dt.raw[4], 0x07, 0, 6);
  bool monthOk = bcdRangeOk(dt.raw[5], 0x1F, 1, 12);
  bool yearOk  = bcdRangeOk(dt.raw[6], 0xFF, 0, 99);

  dt.bcdOk = secOk && minOk && hourOk && dayOk && wdayOk && monthOk && yearOk;

  if (!dt.bcdOk) {
    copyErr(dt.err, sizeof(dt.err), "INVALID_BCD");
    if (verboseOnError) printRTCBruto(dt);
    return false;
  }

  dt.second  = bcd2dec(dt.raw[0] & 0x7F);
  dt.minute  = bcd2dec(dt.raw[1] & 0x7F);
  dt.hour    = bcd2dec(dt.raw[2] & 0x3F);
  dt.day     = bcd2dec(dt.raw[3] & 0x3F);
  dt.weekday = bcd2dec(dt.raw[4] & 0x07);
  dt.month   = bcd2dec(dt.raw[5] & 0x1F);
  dt.year    = 2000 + bcd2dec(dt.raw[6]);

  dt.dateOk = validDateTime(dt.year, dt.month, dt.day,
                            dt.hour, dt.minute, dt.second);
  dt.plausible = dt.dateOk;

  if (!dt.dateOk) {
    copyErr(dt.err, sizeof(dt.err), "INVALID_DATE");
    if (verboseOnError) printRTCBruto(dt);
    return false;
  }

  if (dt.test1Set) {
    copyErr(dt.err, sizeof(dt.err), "CTRL1_TEST1_SET");
    if (verboseOnError) printRTCBruto(dt);
    return false;
  }

  if (dt.testcSet) {
    copyErr(dt.err, sizeof(dt.err), "CTRL1_TESTC_SET");
    if (verboseOnError) printRTCBruto(dt);
    return false;
  }

  if (dt.stopSet) {
    copyErr(dt.err, sizeof(dt.err), "CTRL1_STOP_SET");
    if (verboseOnError) printRTCBruto(dt);
    return false;
  }

  if (dt.vlSet) {
    copyErr(dt.err, sizeof(dt.err), "VL_SET");
    if (verboseOnError) printRTCBruto(dt);
    return false;
  }

  dt.valid = true;
  copyErr(dt.err, sizeof(dt.err), "OK");
  return true;
}

void printRTCReport() {
  Serial.println();
  Serial.println("========== LEITURA RTC ==========");

  DateTimeRTC dt;
  bool ok = readRTC(dt, false);

  if (!dt.i2cOk) {
    Serial.println("[RTC] status=FAIL | err=PING_FAIL");
    Serial.println("=================================");
    return;
  }

  printRTCBruto(dt);

  char ts[32];
  formatTimestamp(dt, ts, sizeof(ts));

  Serial.print("[RTC] status=");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print(" | err=");
  Serial.print(dt.err);
  Serial.print(" | VL=");
  Serial.print(dt.vlSet ? "SET" : "CLEAR");
  Serial.print(" | STOP=");
  Serial.print(dt.stopSet ? "SET" : "CLEAR");
  Serial.print(" | TEST1=");
  Serial.print(dt.test1Set ? "SET" : "CLEAR");
  Serial.print(" | TESTC=");
  Serial.print(dt.testcSet ? "SET" : "CLEAR");
  Serial.print(" | bcdOk=");
  Serial.print(dt.bcdOk ? "SIM" : "NAO");
  Serial.print(" | dateOk=");
  Serial.print(dt.dateOk ? "SIM" : "NAO");
  Serial.print(" | timestamp=");
  Serial.println(ts);
  Serial.println("=================================");
}

bool normalizarControleRTC() {
  bool ok = true;

  ok &= i2cWriteReg(ADDR_PCF8563, PCF8563_REG_CTRL1, 0x00);
  safeDelay(10);
  ok &= i2cWriteReg(ADDR_PCF8563, PCF8563_REG_CTRL2, 0x00);
  safeDelay(10);

  if (ok) {
    Serial.println("[RTC_CTRL] CTRL1=0x00 e CTRL2=0x00 gravados.");
  } else {
    Serial.println("[RTC_CTRL] Falha ao zerar registradores de controle.");
  }

  return ok;
}

bool clearVLOnly() {
  uint8_t sec;
  if (!i2cReadRegs(ADDR_PCF8563, PCF8563_REG_SECONDS, &sec, 1)) {
    Serial.println("[CLR_VL] Falha ao ler segundos.");
    return false;
  }

  sec &= 0x7F;
  if (!i2cWriteReg(ADDR_PCF8563, PCF8563_REG_SECONDS, sec)) {
    Serial.println("[CLR_VL] Falha ao limpar VL.");
    return false;
  }

  Serial.println("[CLR_VL] VL limpo. ATENCAO: isso nao garante hora correta.");
  return true;
}

bool startRTC() {
  uint8_t ctrl1;
  if (!i2cReadRegs(ADDR_PCF8563, PCF8563_REG_CTRL1, &ctrl1, 1)) return false;
  ctrl1 &= ~PCF8563_BIT_STOP;
  bool ok = i2cWriteReg(ADDR_PCF8563, PCF8563_REG_CTRL1, ctrl1);
  Serial.println(ok ? "[RTC] START_RTC: STOP=CLEAR." : "[RTC] START_RTC falhou.");
  return ok;
}

bool stopRTC() {
  uint8_t ctrl1;
  if (!i2cReadRegs(ADDR_PCF8563, PCF8563_REG_CTRL1, &ctrl1, 1)) return false;
  ctrl1 |= PCF8563_BIT_STOP;
  bool ok = i2cWriteReg(ADDR_PCF8563, PCF8563_REG_CTRL1, ctrl1);
  Serial.println(ok ? "[RTC] STOP_RTC: STOP=SET." : "[RTC] STOP_RTC falhou.");
  return ok;
}

bool setRTC_PCF8563(const DateTimeRTC &dt) {
  if (!dt.valid) {
    Serial.println("[RTC_SET] Estrutura DateTime invalida.");
    return false;
  }

  if (!validDateTime(dt.year, dt.month, dt.day,
                     dt.hour, dt.minute, dt.second)) {
    Serial.println("[RTC_SET] Data/hora fora da faixa valida.");
    return false;
  }

  if (dt.year < 2000 || dt.year > 2099) {
    Serial.println("[RTC_SET] PCF8563 neste firmware usa anos 2000..2099.");
    return false;
  }

  Serial.println("[RTC_SET] Ajuste robusto do PCF8563 iniciado.");

  if (!i2cWriteReg(ADDR_PCF8563, PCF8563_REG_CTRL1, PCF8563_BIT_STOP)) {
    Serial.println("[RTC_SET] Falha ao colocar STOP.");
    return false;
  }
  safeDelay(10);

  if (!i2cWriteReg(ADDR_PCF8563, PCF8563_REG_CTRL2, 0x00)) {
    Serial.println("[RTC_SET] Falha ao limpar CTRL2.");
    return false;
  }
  safeDelay(10);

  int wday = weekdayFromDate(dt.year, dt.month, dt.day);

  uint8_t data[7];
  data[0] = dec2bcd((uint8_t)dt.second) & 0x7F;           // VL limpo
  data[1] = dec2bcd((uint8_t)dt.minute) & 0x7F;
  data[2] = dec2bcd((uint8_t)dt.hour)   & 0x3F;
  data[3] = dec2bcd((uint8_t)dt.day)    & 0x3F;
  data[4] = dec2bcd((uint8_t)wday)      & 0x07;
  data[5] = dec2bcd((uint8_t)dt.month)  & 0x1F;           // century=0
  data[6] = dec2bcd((uint8_t)(dt.year - 2000));

  if (!i2cWriteRegs(ADDR_PCF8563, PCF8563_REG_SECONDS, data, 7)) {
    Serial.println("[RTC_SET] Falha ao escrever registradores de tempo.");
    return false;
  }
  safeDelay(10);

  if (!i2cWriteReg(ADDR_PCF8563, PCF8563_REG_CTRL2, 0x00)) {
    Serial.println("[RTC_SET] Falha ao limpar CTRL2 apos escrita.");
    return false;
  }
  safeDelay(10);

  if (!i2cWriteReg(ADDR_PCF8563, PCF8563_REG_CTRL1, 0x00)) {
    Serial.println("[RTC_SET] Falha ao liberar oscilador / limpar CTRL1.");
    return false;
  }

  safeDelay(350);

  DateTimeRTC check;
  bool okRead = readRTC(check, true);

  if (!okRead || !check.valid) {
    Serial.print("[RTC_SET] Releitura falhou. err=");
    Serial.println(check.err);
    return false;
  }

  char ts[32];
  formatTimestamp(check, ts, sizeof(ts));

  Serial.print("[RTC_SET] OK por releitura: ");
  Serial.println(ts);

  return true;
}

bool parseDateTimeCommand(String payload, DateTimeRTC &dt) {
  payload.trim();
  clearRTCStruct(dt);

  int y, mo, d, h, mi, s;
  int n = sscanf(payload.c_str(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s);
  if (n != 6) return false;

  dt.year = y;
  dt.month = mo;
  dt.day = d;
  dt.hour = h;
  dt.minute = mi;
  dt.second = s;
  dt.weekday = weekdayFromDate(y, mo, d);
  dt.dateOk = validDateTime(y, mo, d, h, mi, s);
  dt.bcdOk = true;
  dt.plausible = dt.dateOk;
  dt.valid = dt.dateOk;
  copyErr(dt.err, sizeof(dt.err), dt.valid ? "OK" : "INVALID_DATE");

  return dt.valid;
}

bool getCompileDateTime(DateTimeRTC &dt) {
  clearRTCStruct(dt);

  char monStr[4];
  int day, year;
  int hour, minute, second;

  if (sscanf(__DATE__, "%3s %d %d", monStr, &day, &year) != 3) return false;
  if (sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second) != 3) return false;

  const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *p = strstr(months, monStr);
  if (p == nullptr) return false;

  int month = ((p - months) / 3) + 1;

  dt.year = year;
  dt.month = month;
  dt.day = day;
  dt.hour = hour;
  dt.minute = minute;
  dt.second = second;
  dt.weekday = weekdayFromDate(year, month, day);
  dt.dateOk = validDateTime(year, month, day, hour, minute, second);
  dt.bcdOk = true;
  dt.plausible = dt.dateOk;
  dt.valid = dt.dateOk;
  copyErr(dt.err, sizeof(dt.err), dt.valid ? "OK" : "INVALID_DATE");

  return dt.valid;
}

void tickTest(int samples) {
  if (samples < 3) samples = 3;
  if (samples > 120) samples = 120;

  Serial.println();
  Serial.println("========== TICK RTC ==========");
  Serial.printf("[TICK] Amostras=%d | intervalo ~1,1 s\n", samples);

  int changes = 0;
  int invalids = 0;
  int backwards = 0;
  int lastSec = -1;
  uint32_t lastEpoch = 0;

  for (int i = 0; i < samples; i++) {
    DateTimeRTC dt;
    bool ok = readRTC(dt, false);

    char ts[32];
    formatTimestamp(dt, ts, sizeof(ts));

    Serial.printf("[TICK] %03d/%03d | ok=%s | err=%s | sec=%02d | %s\n",
                  i + 1, samples, ok ? "SIM" : "NAO", dt.err, dt.second, ts);

    if (!ok) invalids++;

    if (lastSec >= 0 && dt.second != lastSec) changes++;
    lastSec = dt.second;

    if (ok) {
      uint32_t ep = dateTimeToEpoch(dt);
      if (lastEpoch > 0 && ep < lastEpoch) backwards++;
      lastEpoch = ep;
    }

    safeDelay(1100);
  }

  Serial.printf("[TICK] Mudancas de segundo=%d/%d | invalidas=%d | voltas=%d\n",
                changes, samples - 1, invalids, backwards);
  if (changes >= samples - 2 && invalids == 0 && backwards == 0) {
    Serial.println("[TICK] OK: RTC avancando normalmente.");
  } else {
    Serial.println("[TICK] ALERTA: verificar oscilador, alimentacao, STOP/VL ou barramento I2C.");
  }
  Serial.println("==============================");
}

void runDiagnostic() {
  Serial.println();
  Serial.println("#############################################");
  Serial.println("# DIAGNOSTICO RTC/I2C");
  Serial.println("#############################################");
  scanI2C();
  printRTCReport();
  dumpRTCRegisters();
  tickTest(10);
}

// ============================================================
// SOFTCLOCK
// ============================================================

uint32_t tempoAtualEstimadoEpoch() {
  if (!g_tempoValido || g_epochBase == 0) return 0;
  uint32_t segundosAcordado = (millis() - g_millisBase) / 1000UL;
  return g_epochBase + segundosAcordado;
}

void salvarSoftClockAntesDoSleep(uint32_t sleepSec) {
  uint32_t epochAgora = tempoAtualEstimadoEpoch();

  if (epochAgora == 0) {
    softClockMagic = 0;
    softClockCRC = 0;
    Serial.println("[SOFTCLOCK] Nao salvo: ciclo atual sem tempo valido.");
    return;
  }

  softClockMagic = SOFTCLOCK_MAGIC;
  softEpochAntesSleep = epochAgora;
  softSleepProgramadoSec = sleepSec;
  softClockCRC = softClockMagic ^
                 softEpochAntesSleep ^
                 softSleepProgramadoSec ^
                 SOFTCLOCK_CRC_XOR;

  Serial.print("[SOFTCLOCK] Backup antes do sleep: epoch=");
  Serial.print(softEpochAntesSleep);
  Serial.print(" | sleep_s=");
  Serial.println(softSleepProgramadoSec);
}

bool estimarTempoPeloESP32(uint32_t &epochEstimado) {
  epochEstimado = 0;

  if (softClockMagic != SOFTCLOCK_MAGIC) return false;

  uint32_t crc = softClockMagic ^
                 softEpochAntesSleep ^
                 softSleepProgramadoSec ^
                 SOFTCLOCK_CRC_XOR;

  if (crc != softClockCRC) return false;
  if (softEpochAntesSleep == 0) return false;

  if (softSleepProgramadoSec < 10 ||
      softSleepProgramadoSec > SOFTCLOCK_SLEEP_MAX_VALIDO_SEC) {
    return false;
  }

  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) return false;

  uint32_t segundosDesdeBoot = millis() / 1000UL;
  epochEstimado = softEpochAntesSleep + softSleepProgramadoSec + segundosDesdeBoot;

  return true;
}

bool obterTempoConfiavel(DateTimeRTC &now,
                         DateTimeRTC &rtcFisico,
                         bool &rtcFisicoValido) {
  rtcFisicoValido = readRTC(rtcFisico, true);

  uint32_t epochRTC = rtcFisicoValido ? dateTimeToEpoch(rtcFisico) : 0;

  g_epochSoft = 0;
  g_softValido = estimarTempoPeloESP32(g_epochSoft);
  g_softStatus = g_softValido ? "OK" : "FAIL";
  g_diffRtcSoftKnown = false;
  g_diffRtcSoft = 0;

  Serial.print("[SOFTCLOCK] status=");
  Serial.print(g_softStatus);
  if (g_softValido) {
    Serial.print(" | epoch_est=");
    Serial.print(g_epochSoft);
  }
  Serial.println();

  if (rtcFisicoValido) {
    now = rtcFisico;

    g_epochBase = epochRTC;
    g_millisBase = millis();
    g_tempoValido = true;
    g_fonteTempo = "RTC";

    if (g_softValido) {
      g_diffRtcSoft = (int32_t)epochRTC - (int32_t)g_epochSoft;
      g_diffRtcSoftKnown = true;

      Serial.print("[TEMPO] RTC - SOFTCLOCK = ");
      Serial.print(g_diffRtcSoft);
      Serial.println(" s");

      uint32_t diffAbs = (g_diffRtcSoft >= 0) ?
        (uint32_t)g_diffRtcSoft : (uint32_t)(-g_diffRtcSoft);

      if (diffAbs > SOFTCLOCK_TOLERANCIA_SEC) {
        Serial.println("[TEMPO] Aviso: diferenca grande. Mantendo RTC fisico.");
      }
    }

    return true;
  }

  if (!rtcFisicoValido && g_softValido) {
    epochToDateTime(g_epochSoft, now);

    if (!now.valid) {
      g_tempoValido = false;
      g_fonteTempo = "NONE";
      softClockFalhas++;
      Serial.println("[TEMPO] SoftClock gerou data invalida.");
      return false;
    }

    g_epochBase = g_epochSoft;
    g_millisBase = millis();
    g_tempoValido = true;
    g_fonteTempo = "SOFTCLOCK";

    Serial.println("[TEMPO] RTC falhou. Usando horario reconstruido pelo ESP32.");

    if (i2cPing(ADDR_PCF8563)) {
      bool reparou = setRTC_PCF8563(now);

      if (reparou) {
        DateTimeRTC check;
        bool okCheck = readRTC(check, true);

        if (okCheck && check.valid) {
          softClockReparos++;
          Serial.println("[TEMPO] PCF8563 reajustado e validado por releitura.");
        } else {
          Serial.print("[TEMPO] Reparo escrito, mas validacao falhou. err=");
          Serial.println(check.err);
        }
      } else {
        Serial.println("[TEMPO] Falha ao reajustar PCF8563 com SoftClock.");
      }
    } else {
      Serial.println("[TEMPO] PCF8563 nao respondeu. Nao foi possivel reajustar.");
    }

    return true;
  }

  clearRTCStruct(now);
  g_epochBase = 0;
  g_millisBase = millis();
  g_tempoValido = false;
  g_fonteTempo = "NONE";
  softClockFalhas++;

  Serial.println("[TEMPO] Falha: sem RTC valido e sem SoftClock valido.");
  return false;
}

// ============================================================
// CONSOLE SERIAL DE BOOT
// ============================================================

void printConsoleHelp() {
  Serial.println();
  Serial.println("========== COMANDOS NA JANELA DE BOOT ==========");
  Serial.println("HELP");
  Serial.println("SCAN");
  Serial.println("READ");
  Serial.println("DUMP");
  Serial.println("DIAG");
  Serial.println("TICK 20");
  Serial.println("SET 2026-06-27 09:30:00");
  Serial.println("SET_COMPILE");
  Serial.println("RESET_CTRL");
  Serial.println("CLR_VL");
  Serial.println("START_RTC");
  Serial.println("STOP_RTC");
  Serial.println("HOLD");
  Serial.println("================================================");
}

void travarParaManutencao() {
  Serial.println();
  Serial.println("================================================");
  Serial.println("[BOOT] MODO DE MANUTENCAO ATIVADO");
  Serial.println("[BOOT] ESP32 acordado para regravacao pela IDE.");
  Serial.println("[BOOT] Para sair, pressione RESET/EN.");
  Serial.println("================================================");

  while (true) {
    feedWdt();
    Serial.println("[BOOT] Manutencao ativa...");
    delay(2000);
  }
}

void processBootCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  String upper = cmd;
  upper.toUpperCase();

  if (upper == "HELP") {
    printConsoleHelp();
  } else if (upper == "SCAN") {
    scanI2C();
  } else if (upper == "READ") {
    printRTCReport();
  } else if (upper == "DUMP") {
    dumpRTCRegisters();
  } else if (upper == "DIAG") {
    runDiagnostic();
  } else if (upper == "RESET_CTRL") {
    normalizarControleRTC();
    printRTCReport();
  } else if (upper == "CLR_VL") {
    clearVLOnly();
    printRTCReport();
  } else if (upper == "START_RTC") {
    startRTC();
    printRTCReport();
  } else if (upper == "STOP_RTC") {
    stopRTC();
    printRTCReport();
  } else if (upper == "HOLD") {
    travarParaManutencao();
  } else if (upper.startsWith("TICK")) {
    int samples = 20;
    if (upper.length() > 4) {
      String arg = upper.substring(4);
      arg.trim();
      int v = arg.toInt();
      if (v > 0) samples = v;
    }
    tickTest(samples);
  } else if (upper == "SET_COMPILE") {
    DateTimeRTC dt;
    if (getCompileDateTime(dt)) {
      char ts[32];
      formatTimestamp(dt, ts, sizeof(ts));
      Serial.print("[SET_COMPILE] Ajustando para ");
      Serial.println(ts);
      setRTC_PCF8563(dt);
    } else {
      Serial.println("[SET_COMPILE] Falha ao obter data/hora de compilacao.");
    }
  } else if (upper.startsWith("SET ")) {
    String payload = cmd.substring(4);
    DateTimeRTC dt;
    if (!parseDateTimeCommand(payload, dt)) {
      Serial.println("[SET] Formato invalido. Use: SET 2026-06-27 09:30:00");
      return;
    }
    setRTC_PCF8563(dt);
  } else {
    Serial.print("[BOOT_CMD] Comando desconhecido: ");
    Serial.println(cmd);
    Serial.println("[BOOT_CMD] Digite HELP.");
  }
}

void bootSeguranca15s() {
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);

  if (BOOT_WINDOW_ONLY_COLD_BOOT && isWakeFromDeepSleep()) {
    Serial.println("[BOOT] Wake por TIMER. Janela de boot pulada.");
    return;
  }

  Serial.println();
  Serial.println("================================================");
  Serial.println("[BOOT] JANELA DE SEGURANCA DE 15 s");
  Serial.println("[BOOT] BOOT/GPIO9 trava para manutencao.");
  Serial.println("[BOOT] Digite HELP para comandos do RTC.");
  Serial.println("================================================");

  uint32_t janelaFim = millis() + BOOT_WINDOW_SECONDS * 1000UL;
  int ultimoSegundoImpresso = -1;

  while ((int32_t)(janelaFim - millis()) > 0) {
    feedWdt();

    int restante = (int)((janelaFim - millis() + 999UL) / 1000UL);
    if (restante != ultimoSegundoImpresso) {
      ultimoSegundoImpresso = restante;
      Serial.printf("[BOOT] %2d s restantes...\n", restante);
    }

    if (digitalRead(PIN_BOOT_BUTTON) == LOW) {
      travarParaManutencao();
    }

    if (Serial.available()) {
      String cmd = Serial.readStringUntil('\n');
      processBootCommand(cmd);
      // Depois de um comando, mantem pelo menos 5 s adicionais de janela.
      janelaFim = millis() + 5000UL;
      ultimoSegundoImpresso = -1;
    }

    delay(50);
  }

  Serial.println("[BOOT] Janela encerrada. Prosseguindo...");
}

// ============================================================
// BH1750 J11
// ============================================================

void bh1750PowerDown() {
  selecionarJ11();
  if (i2cPing(ADDR_BH1750)) {
    i2cWriteByte(ADDR_BH1750, BH1750_POWER_DOWN);
  }
}

bool readJ11Once(float &lux, uint16_t &raw, char *err, size_t errLen) {
  lux = NAN;
  raw = 0;

  selecionarJ11();

  if (!i2cPing(ADDR_BH1750)) {
    copyErr(err, errLen, "NO_ACK_0x23");
    return false;
  }

  if (!i2cWriteByte(ADDR_BH1750, BH1750_POWER_ON)) {
    copyErr(err, errLen, "POWER_ON_FAIL");
    return false;
  }
  safeDelay(10);

  if (!i2cWriteByte(ADDR_BH1750, BH1750_RESET)) {
    copyErr(err, errLen, "RESET_FAIL");
    return false;
  }
  safeDelay(10);

  if (!i2cWriteByte(ADDR_BH1750, BH1750_ONE_TIME_HIGH_RES)) {
    copyErr(err, errLen, "MEASURE_CMD_FAIL");
    return false;
  }

  safeDelay(180);

  uint8_t n = Wire.requestFrom((int)ADDR_BH1750, 2, (int)true);

  uint32_t t0 = millis();
  while (Wire.available() < 2 && millis() - t0 < I2C_TIMEOUT_MS) {
    feedWdt();
    delay(1);
  }

  if (n != 2 || Wire.available() < 2) {
    copyErr(err, errLen, "READ_FAIL");
    bh1750PowerDown();
    return false;
  }

  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  raw = ((uint16_t)msb << 8) | lsb;
  lux = raw / 1.2f;

  if (raw >= 65535) {
    copyErr(err, errLen, "SATURATED");
  } else {
    copyErr(err, errLen, "OK");
  }

  bh1750PowerDown();
  return true;
}

J11Reading readJ11Robusto() {
  J11Reading r;
  memset(&r, 0, sizeof(J11Reading));
  r.lux = NAN;
  r.wm2 = NAN;
  copyErr(r.status, sizeof(r.status), "FAIL");
  copyErr(r.err, sizeof(r.err), "INIT");

  for (int i = 1; i <= MAX_TENTATIVAS_J11; i++) {
    feedWdt();
    r.attempts = i;
    Serial.printf("[J11] Tentativa %d/%d\n", i, MAX_TENTATIVAS_J11);

    float lux = NAN;
    uint16_t raw = 0;
    char err[32] = "INIT";

    bool ok = readJ11Once(lux, raw, err, sizeof(err));

    if (ok) {
      r.ok = true;
      r.raw = raw;
      r.lux = lux;
      r.wm2 = lux / LUX_POR_WM2;
      copyErr(r.err, sizeof(r.err), err);

      if (strcmp(err, "SATURATED") == 0) {
        r.saturated = true;
        copyErr(r.status, sizeof(r.status), "SAT");
      } else {
        r.saturated = false;
        copyErr(r.status, sizeof(r.status), "OK");
      }

      Serial.printf("[J11] OK | raw=%u | lux=%.1f | W/m2=%.3f\n",
                    r.raw, r.lux, r.wm2);
      return r;
    }

    copyErr(r.err, sizeof(r.err), err);
    Serial.print("[J11] Falha: ");
    Serial.println(r.err);

    recuperarI2C();
    safeDelay(250);
  }

  Serial.print("[J11] Falha final: ");
  Serial.println(r.err);
  return r;
}

// ============================================================
// BATERIA
// ============================================================

void iniciarADC() {
  pinMode(PIN_BAT, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BAT, ADC_11db);
  safeDelay(100);
}

float lerBateriaV() {
  for (int i = 0; i < 5; i++) {
    analogReadMilliVolts(PIN_BAT);
    safeDelay(5);
  }

  uint32_t somaMv = 0;
  int validas = 0;

  for (int i = 0; i < 32; i++) {
    feedWdt();
    int mv = analogReadMilliVolts(PIN_BAT);
    if (mv > 0) {
      somaMv += (uint32_t)mv;
      validas++;
    }
    safeDelay(4);
  }

  if (validas == 0) return NAN;

  float vAdc = (somaMv / (float)validas) / 1000.0f;
  float vBat = vAdc * BAT_DIV_RATIO;
  return BAT_CAL_A * vBat + BAT_CAL_B;
}

String statusBateria(float vbat) {
  if (isnan(vbat)) return "ADC_FAIL";
  if (vbat < 0.2f) return "ADC_ZERO";
  if (vbat < 2.5f) return "BAT_INVALID";
  if (vbat < 3.3f) return "BAT_LOW";
  if (vbat > 4.6f) return "BAT_HIGH";
  return "OK";
}

// ============================================================
// SD
// ============================================================

bool iniciarSD(int &tentativas) {
  tentativas = 0;

  for (int i = 1; i <= MAX_TENTATIVAS_SD; i++) {
    feedWdt();
    tentativas = i;

    Serial.printf("[SD] init tentativa %d/%d\n", i, MAX_TENTATIVAS_SD);

    SD.end();
    spiSD.end();
    safeDelay(100);

    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    safeDelay(100);

    if (SD.begin(SD_CS, spiSD, SD_SPI_FREQ_HZ)) {
      uint8_t cardType = SD.cardType();
      uint64_t cardSizeMB = SD.cardSize() / (1024ULL * 1024ULL);

      Serial.printf("[SD] OK | cardType=%u | cardSize=%llu MB\n",
                    cardType, (unsigned long long)cardSizeMB);

      sdOK = true;
      ultimoSdErro = "NONE";
      return true;
    }

    Serial.println("[SD] Falha ao inicializar.");
    ultimoSdErro = "BEGIN_FAIL";
    sdOK = false;
    safeDelay(300);
  }

  return false;
}

void montarNomeArquivo(const DateTimeRTC &dt, char *filename, size_t len) {
  if (dt.valid) {
    snprintf(filename, len, "/J%02d%02d%02d.CSV", dt.year % 100, dt.month, dt.day);
  } else {
    snprintf(filename, len, "/J_NO_RTC.CSV");
  }
}

uint64_t tamanhoArquivo(const char* nomeArquivo) {
  File f = SD.open(nomeArquivo, FILE_READ);
  if (!f) return 0;
  uint64_t s = f.size();
  f.close();
  return s;
}

bool garantirCabecalho(const char *filename) {
  bool precisaCabecalho = false;

  if (!SD.exists(filename)) {
    precisaCabecalho = true;
  } else {
    File f = SD.open(filename, FILE_READ);
    if (f) {
      if (f.size() == 0) precisaCabecalho = true;
      f.close();
    } else {
      precisaCabecalho = true;
    }
  }

  if (!precisaCabecalho) return true;

  File f = SD.open(filename, FILE_APPEND);
  if (!f) {
    ultimoSdErro = "HEADER_OPEN_FAIL";
    return false;
  }

  f.println(CSV_HEADER);
  f.flush();
  f.close();

  Serial.print("[SD] Cabecalho criado em ");
  Serial.println(filename);

  return true;
}

bool gravarLinhaSDComTentativa(const char *filename,
                               const String &linhaPrefix,
                               uint32_t nextSleep,
                               int &tentativaUsada) {
  tentativaUsada = 0;

  for (int tentativa = 1; tentativa <= MAX_TENTATIVAS_SD; tentativa++) {
    feedWdt();
    tentativaUsada = tentativa;

    if (!sdOK) {
      int tentativasInitExtra = 0;
      iniciarSD(tentativasInitExtra);
    }

    if (!sdOK) {
      ultimoSdErro = "SD_NOT_READY";
      safeDelay(500);
      continue;
    }

    if (!garantirCabecalho(filename)) {
      ultimoSdErro = "HEADER_FAIL";
      sdOK = false;
      safeDelay(500);
      continue;
    }

    String linhaFinal = linhaPrefix;
    linhaFinal += String(tentativa);
    linhaFinal += ",";
    linhaFinal += "NONE";
    linhaFinal += ",";
    linhaFinal += String(nextSleep);
    linhaFinal += "\n";

    uint64_t antes = tamanhoArquivo(filename);

    File f = SD.open(filename, FILE_APPEND);
    if (!f) {
      ultimoSdErro = "OPEN_FAIL";
      sdOK = false;
      safeDelay(500);
      continue;
    }

    size_t escritos = f.print(linhaFinal);
    f.flush();
    f.close();

    uint64_t depois = tamanhoArquivo(filename);

    if (escritos == linhaFinal.length() && depois > antes) {
      ultimoSdErro = "NONE";
      Serial.printf("[SD] Linha gravada e verificada. Tentativa=%d | tamanho=%llu bytes\n",
                    tentativa, (unsigned long long)depois);
      return true;
    }

    ultimoSdErro = "VERIFY_FAIL";
    sdOK = false;
    Serial.println("[SD] Falha de verificacao da escrita.");
    safeDelay(500);
  }

  return false;
}

// ============================================================
// AGENDAMENTO / DEEP SLEEP
// ============================================================

bool horarioDiurno(const DateTimeRTC &dt) {
  if (!dt.valid) return true;
  return dt.hour >= HORA_INICIO_MEDICAO && dt.hour < HORA_FIM_MEDICAO;
}

uint32_t segundosDoDia(const DateTimeRTC &dt) {
  return dt.hour * 3600UL + dt.minute * 60UL + dt.second;
}

uint32_t segundosAteProximoMultiplo5min(const DateTimeRTC &dt) {
  if (!dt.valid) return SAMPLE_INTERVAL_SEC;

  uint32_t sday = segundosDoDia(dt);
  uint32_t resto = sday % SAMPLE_INTERVAL_SEC;
  uint32_t espera = SAMPLE_INTERVAL_SEC - resto;

  if (espera < 20) espera += SAMPLE_INTERVAL_SEC;
  return espera;
}

uint32_t segundosAteInicioDiurno(const DateTimeRTC &dt) {
  if (!dt.valid) return SAMPLE_INTERVAL_SEC;

  uint32_t sday = segundosDoDia(dt);
  uint32_t inicio = HORA_INICIO_MEDICAO * 3600UL;

  if (sday < inicio) return inicio - sday;
  return 86400UL - sday + inicio;
}

void prepararAntesDoSono() {
  bh1750PowerDown();
  manterRAKEmReset();
  selecionarJ11();

  SD.end();
  spiSD.end();

  safeDelay(50);
}

void entrarDeepSleep(uint32_t sleepSec) {
  if (sleepSec < 10) sleepSec = 10;

  Serial.printf("[SLEEP] Dormindo %lu s\n", (unsigned long)sleepSec);

  salvarSoftClockAntesDoSleep(sleepSec);
  prepararAntesDoSono();

  feedWdt();
  safeDelay(100);

  esp_sleep_enable_timer_wakeup((uint64_t)sleepSec * 1000000ULL);

  Serial.println("[SLEEP] Zzz...");
  Serial.flush();

  esp_deep_sleep_start();
}

// ============================================================
// SETUP PRINCIPAL
// ============================================================

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(700);

  bootCounter++;
  cycleCounter++;

  resetReasonStr = resetReasonToString(esp_reset_reason());
  wakeReasonStr = wakeReasonToString();

  configurarPinosSeguros();

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(I2C_FREQ_HZ);
  Wire.setTimeOut(I2C_TIMEOUT_MS);

  selecionarJ11();

  Serial.println();
  Serial.println("================================================");
  Serial.println("LOGGER ROBUSTO V2 - J11 + PCF8563 + SD + BATERIA");
  Serial.println("SoftClock ESP32 ativo como redundancia temporal");
  Serial.println("RTC validado por CTRL1/CTRL2/VL/STOP/TESTC/BCD");
  Serial.println("ESP32-C6 | J11 unico | BH1750 sem MTreg");
  Serial.println("RTC PCF8563: 0x51 | BH1750 J11: 0x23 | TCA: 0x20");
  Serial.println("================================================");

  Serial.printf("[BOOT] bootCounter=%lu\n", (unsigned long)bootCounter);
  Serial.printf("[BOOT] cycleCounter=%lu\n", (unsigned long)cycleCounter);
  Serial.print("[BOOT] resetReason=");
  Serial.println(resetReasonStr);
  Serial.print("[BOOT] wakeReason=");
  Serial.println(wakeReasonStr);

  bootSeguranca15s();

  startWatchdog();
  recuperarI2C();

  if (!isWakeFromDeepSleep()) {
    scanI2C();
  }

  inicializarTCA9554();
  manterRAKEmReset();
  iniciarADC();

  // Tempo: RTC fisico + SoftClock
  DateTimeRTC now;
  DateTimeRTC rtcFisico;
  bool rtcFisicoValido = false;
  bool tempoValido = obterTempoConfiavel(now, rtcFisico, rtcFisicoValido);

  char timestamp[32] = {0};
  bool timestampOK = montarTimestampSeguro(now, timestamp, sizeof(timestamp));

  if (!timestampOK && tempoValido) {
    Serial.println("[TEMPO] Aviso: timestamp inconsistente apesar de tempoValido=OK.");
  }

  String vlStr = rtcFisico.vlKnown ? (rtcFisico.vlSet ? "SET" : "CLEAR") : "UNKNOWN";

  Serial.print("[RTC] status=");
  Serial.print(rtcFisicoValido ? "OK" : "FAIL");
  Serial.print(" | err=");
  Serial.print(rtcFisico.err);
  Serial.print(" | VL=");
  Serial.print(vlStr);
  Serial.print(" | CTRL1=");
  Serial.print(hex2(rtcFisico.ctrl1));
  Serial.print(" | CTRL2=");
  Serial.print(hex2(rtcFisico.ctrl2));
  Serial.print(" | fonte_tempo=");
  Serial.print(g_fonteTempo);
  Serial.print(" | tempo_status=");
  Serial.print(tempoValido ? "OK" : "FAIL");
  Serial.print(" | timestamp_ok=");
  Serial.print(timestampOK ? "SIM" : "NAO");
  Serial.print(" | timestamp=");
  Serial.println(timestamp);

  uint32_t epoch = dateTimeToEpoch(now);

  int32_t deltaTS = -1;
  if (tempoValido && lastEpochUsado > 0 && epoch > lastEpochUsado) {
    deltaTS = (int32_t)(epoch - lastEpochUsado);
  }

  if (tempoValido && epoch > 0) {
    lastEpochUsado = epoch;
  }

  // Deep sleep noturno
  if (tempoValido && !horarioDiurno(now)) {
    uint32_t sleepNoite = segundosAteInicioDiurno(now);
    Serial.println("[NOITE] Fora da janela 05h-19h.");
    Serial.printf("[NOITE] Proximo ciclo diurno em %lu s\n", (unsigned long)sleepNoite);
    entrarDeepSleep(sleepNoite);
  }

  // Leitura J11
  J11Reading j11 = readJ11Robusto();

  // Bateria
  float vbat = lerBateriaV();
  String vbatStatus = statusBateria(vbat);

  if (isnan(vbat)) {
    Serial.println("[BAT] Vbat=nan | status=ADC_FAIL");
  } else {
    Serial.printf("[BAT] Vbat=%.3f V | status=%s\n", vbat, vbatStatus.c_str());
  }

  // SD
  int tentativasSDInit = 0;
  iniciarSD(tentativasSDInit);

  char filename[32];
  montarNomeArquivo(now, filename, sizeof(filename));

  uint32_t nextSleep = segundosAteProximoMultiplo5min(now);

  String linha = "";
  linha += String(timestamp);
  linha += ",";
  linha += String(epoch);
  linha += ",";
  linha += String(bootCounter);
  linha += ",";
  linha += String(cycleCounter);
  linha += ",";
  linha += resetReasonStr;
  linha += ",";
  linha += wakeReasonStr;
  linha += ",";
  linha += String(deltaTS);
  linha += ",";

  linha += g_fonteTempo;
  linha += ",";
  linha += timestampOK ? "SIM" : "NAO";
  linha += ",";
  linha += g_softStatus;
  linha += ",";
  linha += String(g_epochSoft);
  linha += ",";
  linha += g_diffRtcSoftKnown ? String(g_diffRtcSoft) : String("nan");
  linha += ",";
  linha += String(softClockReparos);
  linha += ",";
  linha += String(softClockFalhas);
  linha += ",";

  linha += rtcFisicoValido ? "OK" : "FAIL";
  linha += ",";
  linha += String(rtcFisico.err);
  linha += ",";
  linha += vlStr;
  linha += ",";
  linha += hex2(rtcFisico.ctrl1);
  linha += ",";
  linha += hex2(rtcFisico.ctrl2);
  linha += ",";
  linha += rtcFisico.stopSet ? "SET" : "CLEAR";
  linha += ",";
  linha += rtcFisico.test1Set ? "SET" : "CLEAR";
  linha += ",";
  linha += rtcFisico.testcSet ? "SET" : "CLEAR";
  linha += ",";
  linha += rtcFisico.bcdOk ? "SIM" : "NAO";
  linha += ",";
  linha += rtcFisico.dateOk ? "SIM" : "NAO";
  linha += ",";
  linha += hex2(rtcFisico.raw[0]);
  linha += ",";
  linha += hex2(rtcFisico.raw[1]);
  linha += ",";
  linha += hex2(rtcFisico.raw[2]);
  linha += ",";
  linha += hex2(rtcFisico.raw[3]);
  linha += ",";
  linha += hex2(rtcFisico.raw[4]);
  linha += ",";
  linha += hex2(rtcFisico.raw[5]);
  linha += ",";
  linha += hex2(rtcFisico.raw[6]);
  linha += ",";

  linha += String(j11.status);
  linha += ",";
  linha += String(j11.err);
  linha += ",";
  linha += String(j11.attempts);
  linha += ",";
  linha += String(j11.raw);
  linha += ",";
  linha += floatToStr(j11.lux, 1);
  linha += ",";
  linha += floatToStr(j11.wm2, 3);
  linha += ",";

  linha += floatToStr(vbat, 3);
  linha += ",";
  linha += vbatStatus;
  linha += ",";

  linha += sdOK ? "OK" : "FAIL";
  linha += ",";
  linha += String(tentativasSDInit);
  linha += ",";
  // A partir daqui a funcao de gravacao acrescenta:
  // sd_write_attempts,sd_err,next_sleep_s\n
  int tentativaWrite = 0;

  if (sdOK) {
    bool gravou = gravarLinhaSDComTentativa(filename, linha, nextSleep, tentativaWrite);

    if (gravou) {
      Serial.println("[SD] Registro concluido.");
    } else {
      Serial.print("[SD] Registro NAO concluido. Erro=");
      Serial.println(ultimoSdErro);
    }
  } else {
    Serial.println("[SD] Sem SD. Registro nao gravado neste ciclo.");
  }

  Serial.printf("[NEXT] Proximo ciclo em %lu s\n", (unsigned long)nextSleep);

  entrarDeepSleep(nextSleep);
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  // Nao usado. O sistema acorda, mede, grava e volta ao deep sleep no setup().
}

