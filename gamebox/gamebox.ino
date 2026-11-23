// GAME BOX - Arduino Uno R3 + TFT ST7735 128x160 (SPI) + Joystick + 2 nut A/B
// Game: SNAKE, IRON STRIKE (chay va ban, co boss), TETRIS, PAC-MAN, TANK BATTLE
// Thu vien: Adafruit GFX Library, Adafruit ST7735 and ST7789 Library
//
// Nhac nen + hieu ung: Timer1 ngat 100 Hz dieu khien Timer2 phat cao do tren chan 3 (BUZZER)
//
// Dieu khien:
//   Menu  : joystick len/xuong chon, nhan A de vao game
//   Snake / Pac-Man : joystick
//   Tetris : joystick trai/phai, xuong = roi nhanh, A = xoay, B = tha
//   Tank : joystick di chuyen, B = ban
//   Iron Strike : joystick trai/phai di chuyen, len = ngam ban len,
//           xuong + A = roi xuong ban, A = nhay (nhan giu nhay cao hon), B = ban (giu de ban lien tuc)
//   Nhan nut joystick = tam dung (A tiep tuc, B ve menu), GIU nut joystick = ve menu

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <EEPROM.h>

// Thoi gian 16 bit (65 s quay vong) la du cho moi phep do khoang ngan, nho hon so hoc 32 bit
#define ELAPSED(t) ((uint16_t)((uint16_t)millis() - (t)))

// ---------- Chan ket noi ----------
#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  8
#define JOY_X    A0
#define JOY_Y    A1
#define JOY_SW   2
#define BUZZER   3
#define BTN_A    4
#define BTN_B    5

// Buzzer: BUZZER_ACTIVE true = buzzer chu dong (vo den kin, chi bip mot tong)
//         BUZZER_ACTIVE false = buzzer thu dong (phat duoc tan so khac nhau)
#define USE_BUZZER    true
#define BUZZER_ACTIVE true
// Buzzer chu dong: true = nhac phat theo cao do bang cach dong/ngat theo tan so not (nghe ra nhieu tong,
// hoi rè); false = chi bat/tat theo nhip. Buzzer thu dong luon phat dung cao do.
#define PITCH_ACTIVE  true
#define MUSIC         true

// ---------- Chinh huong joystick neu bi nguoc ----------
#define SWAP_XY  false
#define INV_X    true
#define INV_Y    true

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// ---------- Mau (RGB565) ----------
#define C_BG     ST77XX_BLACK
#define C_TEXT   ST77XX_WHITE
#define C_YEL    ST77XX_YELLOW
#define C_RED    ST77XX_RED
#define C_SNAKE  ST77XX_GREEN
#define C_HEAD   0x07FF   // cyan
#define C_BORDER 0x7BEF   // xam
#define C_GRAYB  0x4208   // xam toi
#define C_SEL    0x0008   // xanh navy
#define C_ROAD   0x2104   // xam rat toi
#define C_GRASS  0x0400   // xanh la toi
#define C_LEAF   ST77XX_GREEN
#define C_TRUNK  0x8200   // nau
#define C_ROCK   0x8410   // xam
#define C_WIN    0x0010   // kinh xe
#define C_WHEEL  ST77XX_BLACK

uint16_t rs = 1;
uint8_t rnd(uint8_t n) {              // xorshift nho gon, thay cho rnd()
  rs ^= rs << 7; rs ^= rs >> 9; rs ^= rs << 8;
  return ((rs >> 8) * n) >> 8;
}

uint16_t hiS[5];   // diem cao: 0 SNAKE, 1 IRON STRIKE, 2 TETRIS, 3 PAC-MAN, 4 TANK (EEPROM addr = 2*i)

// Cap nhat diem cao, tra ve true neu lap ky luc
bool saveHi(uint8_t g, uint16_t sc) {
  if (sc <= hiS[g]) return false;
  hiS[g] = sc;
  EEPROM.put(g * 2, sc);
  return true;
}

// =====================================================
//                    TIEN ICH CHUNG
// =====================================================
// ---------- Am thanh: Timer1 ngat 100 Hz, nhac + hieu ung chay trong ngat nen khong bi dung khi ve man hinh ----------
volatile uint8_t sfx;         // hieu ung con lai (x10 ms)
uint8_t mn, mrem, mdur;
uint8_t cur;
bool curHi;
const uint8_t *mp, *ml, *tr;   // dang phat, diem lap, nhac hien tai (de tiep tuc sau tam dung)
bool tl;

// Nhac: moi not = {ma not, do dai x10 ms}. Ma not 0 = nghi, 1 = C5 (523 Hz), +1 moi nua cung. 255 = het
const uint8_t NF[12] PROGMEM = {119, 113, 106, 100, 95, 89, 84, 80, 75, 71, 67, 63};   // 62500 / tan so (Timer2 chia 128)
const uint8_t T_SNAKE[] PROGMEM = {
  13,20, 17,20, 20,20, 17,20, 18,20, 22,20, 25,20, 22,20, 20,20, 17,20, 13,20, 17,20, 15,40, 0,40, 255};
const uint8_t T_IRON[] PROGMEM = {
  8,20, 8,10, 13,20, 8,20, 15,20, 13,20, 8,20, 6,20, 6,10, 12,20, 6,20, 13,20, 12,20, 6,20, 8,40, 0,20, 255};
const uint8_t T_START[] PROGMEM = {13,10, 17,10, 20,10, 25,50, 255};
const uint8_t T_OVER[]  PROGMEM = {20,20, 19,20, 18,20, 17,20, 15,25, 13,25, 10,30, 8,110, 255};   // giai dieu thua (di xuong)
const uint8_t T_WIN[]   PROGMEM = {13,10, 17,10, 20,10, 25,20, 20,10, 25,70, 255};
const uint8_t T_TETRIS[] PROGMEM = {
  17,40, 12,20, 13,20, 15,40, 13,20, 12,20, 10,40, 10,20, 13,20, 17,40, 15,20, 13,20, 12,60, 13,20, 15,40, 17,40, 13,40, 10,40, 10,40, 0,40, 255};
const uint8_t T_PAC[] PROGMEM = {13,10, 25,10, 20,10, 25,10, 17,10, 25,10, 20,10, 25,10, 255};
const uint8_t T_TANK[] PROGMEM = {10,20, 10,10, 13,20, 10,20, 15,20, 13,20, 10,40, 255};

ISR(TIMER1_COMPA_vect) {
  if (!USE_BUZZER) return;
  uint8_t f = 0;                                   // gia tri chia Timer2 (0 = tat)
  bool hi = false;
  if (mp) {
    if (!mrem) {                                   // nap not ke tiep
      uint8_t n = pgm_read_byte(mp);
      if (n == 255) { mp = ml; if (mp) n = pgm_read_byte(mp); }
      if (mp) { mn = n; mrem = mdur = pgm_read_byte(mp + 1); mp += 2; }
    }
    if (mp) {
      if (mn && mrem > (mdur >= 6 ? 2 : 1)) {      // cuoi moi not nghi 10-20 ms de tach nhip
        if (BUZZER_ACTIVE && !PITCH_ACTIVE) hi = true;
        else { uint8_t k = mn - 1, o = 0; while (k >= 12) { k -= 12; o++; } f = pgm_read_byte(&NF[k]) >> o; f <<= 1; }   // ha 1 quang tam cho nhac nho hon hieu ung
      }
      mrem--;
    }
  }
  if (sfx) { sfx--; hi = false; f = 23; }          // hieu ung: ~2,7 kHz (gan tan so cong huong buzzer) de len nhac
  if (f != cur || hi != curHi) {
    cur = f; curHi = hi;
    if (f) {                                       // Timer2 CTC, bat/tat OC2B (chan 3) o tan so f
      OCR2A = OCR2B = f - 1;                       // tan so = 16 MHz / 128 / 2 / f
      TCCR2A = _BV(COM2B0) | _BV(WGM21);
      TCCR2B = _BV(CS22) | _BV(CS20);
    } else {
      TCCR2B = 0; TCCR2A = 0;
      digitalWrite(BUZZER, hi);
    }
  }
}

void timerInit() {
  cli();
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);     // CTC, chia 64
  OCR1A = 2499;                                    // 16 MHz / 64 / 2500 = 100 Hz
  TCNT1 = 0;
  TIMSK1 = _BV(OCIE1A);
  sei();
}

void tune(const uint8_t* t, bool lp) {
  if (!MUSIC) t = 0;
  uint8_t sr = SREG; cli();
  mp = t; ml = lp ? t : 0; mrem = 0;
  tr = t; tl = lp;
  SREG = sr;
}

void beep(uint16_t ms) {                           // hieu ung ngan, toi thieu 40 ms, nhac tam im de tieng bip noi bat
  uint8_t n = (ms >> 2) + 4;                       // dai hon va chen len nhac (nhac im trong luc keu)
  uint8_t sr = SREG; cli();
  if (n > sfx) sfx = n;
  SREG = sr;
}
#define soundUpdate()

// 0: khong nhan, 1: nhan ngan, 2: giu lau (nut joystick)
uint8_t readBtn() {
  soundUpdate();
  if (digitalRead(JOY_SW) != LOW) return 0;
  delay(20);
  if (digitalRead(JOY_SW) != LOW) return 0;
  uint16_t t = millis();
  while (digitalRead(JOY_SW) == LOW) {
    if (ELAPSED(t) > 700) {
      while (digitalRead(JOY_SW) == LOW) delay(5);
      return 2;
    }
  }
  return 1;
}

bool pressedPin(uint8_t pin) {
  if (digitalRead(pin) != LOW) return false;
  delay(25);
  if (digitalRead(pin) != LOW) return false;
  while (digitalRead(pin) == LOW) delay(5);
  return true;
}
bool pressedA() { return pressedPin(BTN_A); }
bool pressedB() { return pressedPin(BTN_B); }

void waitAllReleased() {
  while (digitalRead(JOY_SW) == LOW || digitalRead(BTN_A) == LOW || digitalRead(BTN_B) == LOW) delay(10);
}

// dx, dy trong khoang -512..511, +x = phai, +y = xuong
void readJoy(int &dx, int &dy) {
  int x = analogRead(JOY_X), y = analogRead(JOY_Y);
  if (SWAP_XY) { int t = x; x = y; y = t; }
  if (INV_X) x = 1023 - x;
  if (INV_Y) y = 1023 - y;
  dx = x - 512;
  dy = y - 512;
}

void pn(uint16_t v) {                 // in so (thay Print::printNumber de tiet kiem flash)
  char b[6];
  uint8_t i = 5;
  b[5] = 0;
  do { b[--i] = '0' + v % 10; v /= 10; } while (v);
  tft.print(b + i);
}

void centerText(const __FlashStringHelper* s, int16_t y, uint8_t sz, uint16_t col) {
  int16_t w = strlen_P((PGM_P)s) * 6 * sz;
  tft.setTextSize(sz);
  tft.setTextColor(col);
  tft.setCursor((128 - w) / 2, y);
  tft.print(s);
}

void frame(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  tft.fillRect(x, y, w, 1, c);
  tft.fillRect(x, y + h - 1, w, 1, c);
  tft.fillRect(x, y, 1, h, c);
  tft.fillRect(x + w - 1, y, 1, h, c);
}

// Man hinh ket thuc. Tra ve 1 (choi lai) hoac 2 (ve menu)
uint8_t endScreen(const __FlashStringHelper* title, uint16_t tcol, uint16_t score, uint16_t hi, bool rec) {
  tft.fillRect(6, 36, 116, 88, C_BG);
  frame(6, 36, 116, 88, tcol);
  centerText(title, 42, 2, tcol);
  tft.setTextSize(1);
  tft.setTextColor(C_TEXT);
  tft.setCursor(16, 66);
  tft.print(F("Diem: "));
  pn(score);
  tft.setCursor(16, 78);
  tft.print(F("Cao nhat: "));
  pn(hi);
  centerText(F("A:choi lai B:menu"), 106, 1, C_TEXT);
  delay(500);
  tune(tcol == C_RED ? T_OVER : T_WIN, false);
  waitAllReleased();
  while (true) {
    uint8_t b = readBtn();
    if (b) return b;            // nut joystick: nhan = choi lai, giu = menu
    if (pressedA()) return 1;
    if (pressedB()) return 2;
  }
}

// Tam dung. Tra ve true neu nguoi choi chon ve menu
bool pauseScreen() {
  const uint8_t* t = tr; bool l = tl;
  tune(0, false);
  tft.fillRect(10, 60, 108, 46, C_BG);
  frame(10, 60, 108, 46, C_TEXT);
  centerText(F("TAM DUNG"), 66, 2, C_YEL);
  centerText(F("A: tiep tuc"), 85, 1, C_TEXT);
  centerText(F("B: ve menu"), 95, 1, C_TEXT);
  waitAllReleased();
  bool m;
  while (true) {
    uint8_t b = readBtn();
    if (b) { m = b == 2; break; }
    if (pressedA()) { m = false; break; }
    if (pressedB()) { m = true; break; }
  }
  tune(l ? t : 0, l);
  return m;
}

// =====================================================
//                      HINH DUNG CHUNG
// =====================================================
#define GY 16
#define KW_KH 504   // 21 x 24 (Tank)
const uint16_t PAL[16] PROGMEM = {0x0000, 0xFFFF, 0xFFE0, 0xF800, 0x07E0, 0x07FF, 0x7BEF, 0x4208,
                                  0xFE54, 0x001F, 0xA286, 0xFD20, 0xF81F, 0x780F, 0x4100, 0xC618};
uint16_t dyn[3];      // mau dong: chi so 16, 17, 18

// Ve danh sach hinh chu nhat (x,y,w,h,mau) tu PROGMEM. m > 0: lat guong theo be rong m
void spr(const int8_t* p, uint8_t n, int16_t ox, int16_t oy, uint8_t m) {
  while (n--) {
    int8_t x = pgm_read_byte(p), y = pgm_read_byte(p + 1);
    int8_t w = pgm_read_byte(p + 2), h = pgm_read_byte(p + 3);
    uint8_t c = pgm_read_byte(p + 4);
    p += 5;
    int16_t yy = oy + y;
    if (oy >= GY && yy < GY) { h -= GY - yy; yy = GY; }
    if (h > 0) tft.fillRect(m ? ox + m - x - w : ox + x, yy, w, h, c < 16 ? pgm_read_word(&PAL[c]) : dyn[c - 16]);
  }
}
#define SPR(a, ox, oy, m) spr(a, sizeof(a) / 5, ox, oy, m)

const int8_t HRT[] PROGMEM = {1,0,2,1,16, 4,0,2,1,16, 0,1,7,2,16, 1,3,5,1,16, 2,4,3,1,16, 3,5,1,1,16};
void heart(int16_t x, int16_t y, uint16_t c) {
  dyn[0] = c;
  SPR(HRT, x, y, 0);
}

void hud(uint16_t sc, uint8_t lives) {
  tft.fillRect(0, 0, 128, GY - 1, C_BG);
  tft.fillRect(0, GY - 1, 128, 1, C_BORDER);
  tft.setTextSize(1);
  tft.setTextColor(C_TEXT);
  tft.setCursor(2, 4);
  pn(sc);
  for (uint8_t i = 0; i < 3 && lives < 9; i++) heart(96 + i * 10, 4, i < lives ? C_RED : C_GRAYB);
}

// 0 = khong co gi, 1 = vua tiep tuc sau tam dung (can ve lai), 2 = ve menu
uint8_t sysKeys() {
  uint8_t b = readBtn();
  if (b == 2) return 2;
  return b ? (pauseScreen() ? 2 : 1) : 0;
}

// =====================================================
//                         SNAKE
// =====================================================
const uint8_t CELL = 8;
const uint8_t COLS = 16;   // 16*8 = 128
const uint8_t ROWS = 18;   // 18*8 = 144
const uint8_t OFFY = 16;   // vung diem so phia tren
const uint8_t MAXLEN = 200;

enum { UP, RIGHT, DOWN, LEFT };
const int8_t DX[4] = {0, 1, 0, -1};
const int8_t DY[4] = {-1, 0, 1, 0};

uint8_t sx[MAXLEN], sy[MAXLEN];   // sx[0], sy[0] = dau ran
uint8_t len, dir, nextDir, fx, fy;
uint16_t score, stepMs;
uint16_t lastStep;

int8_t readDir() {
  int dx, dy;
  readJoy(dx, dy);
  if (abs(dx) < 250 && abs(dy) < 250) return -1;
  if (abs(dx) > abs(dy)) return dx > 0 ? RIGHT : LEFT;
  return dy > 0 ? DOWN : UP;
}

void drawCell(uint8_t x, uint8_t y, uint16_t c) {
  tft.fillRect(x * CELL + 1, OFFY + y * CELL + 1, CELL - 2, CELL - 2, c);
}

void eraseCell(uint8_t x, uint8_t y) {
  tft.fillRect(x * CELL, OFFY + y * CELL, CELL, CELL, C_BG);
}

void snakeHeader() {
  hud(score, 9);
  tft.setTextColor(C_YEL);
  tft.setCursor(88, 4);
  pn(hiS[0]);
}

bool onSnake(uint8_t x, uint8_t y) {
  for (uint8_t i = 0; i < len; i++)
    if (sx[i] == x && sy[i] == y) return true;
  return false;
}

void pickFood() {
  do {
    fx = rnd(COLS);
    fy = rnd(ROWS);
  } while (onSnake(fx, fy));
}

void snakeRedraw() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, OFFY - 1, 128, 1, C_BORDER);
  snakeHeader();
  for (uint8_t i = 1; i < len; i++) drawCell(sx[i], sy[i], C_SNAKE);
  drawCell(sx[0], sy[0], C_HEAD);
  drawCell(fx, fy, C_RED);
}

void snakeNew() {
  len = 3;
  dir = nextDir = RIGHT;
  for (uint8_t i = 0; i < len; i++) {
    sx[i] = 5 - i;
    sy[i] = ROWS / 2;
  }
  score = 0;
  stepMs = 200;
  pickFood();
  snakeRedraw();
}

// Tra ve false neu thua. Ran di xuyen tuong.
bool snakeStep() {
  dir = nextDir;
  int8_t nx = sx[0] + DX[dir];
  int8_t ny = sy[0] + DY[dir];

  if (nx < 0) nx = COLS - 1;
  else if (nx >= COLS) nx = 0;
  if (ny < 0) ny = ROWS - 1;
  else if (ny >= ROWS) ny = 0;

  bool eat = (nx == fx && ny == fy);
  // Duoi se di chuyen nen khong tinh la va cham neu khong an moi
  if (onSnake(nx, ny) && !(!eat && nx == sx[len - 1] && ny == sy[len - 1]))
    return false;

  if (!eat) {
    eraseCell(sx[len - 1], sy[len - 1]);
  } else if (len < MAXLEN) {
    len++;
  }

  for (uint8_t i = len - 1; i > 0; i--) {
    sx[i] = sx[i - 1];
    sy[i] = sy[i - 1];
  }
  drawCell(sx[0], sy[0], C_SNAKE);   // dau cu thanh than
  sx[0] = nx;
  sy[0] = ny;
  drawCell(nx, ny, C_HEAD);

  if (eat) {
    score += 10;
    beep(60);
    if (stepMs > 70) stepMs -= 4;
    snakeHeader();
    pickFood();
    drawCell(fx, fy, C_RED);
  }
  return true;
}

void playSnake() {
  while (true) {
    snakeNew();
    tune(T_SNAKE, true);
    delay(600);
    lastStep = millis();

    while (true) {
      uint8_t b = readBtn();
      if (b == 2) return;
      if (b == 1) {
        if (pauseScreen()) return;
        snakeRedraw();
        lastStep = millis();
      }

      int8_t d = readDir();
      if (d >= 0 && d != (dir + 2) % 4) nextDir = d;

      if (ELAPSED(lastStep) >= stepMs) {
        lastStep = millis();
        if (!snakeStep()) break;
      }
    }

    beep(400);
    bool rec = saveHi(0, score);
    if (endScreen(F("GAME OVER"), C_RED, score, hiS[0], rec) == 2) return;
  }
}

// =====================================================
//                BLAST (chay va ban, 1 man)
// =====================================================
// Man hinh dung doc: HUD y=0..15, vung choi y=16..159 (18 hang o 8px).
// Ban do cuon ngang. Nen la hinh chu nhat don sac nen khi cuon chi
// ve lai 2 mep moi khoi -> nhanh du tren Uno khong can framebuffer.
#define C_SKY  0x5D5F
#define C_DIRT 0x8200
#define C_SKIN 0xFE54
#define C_GUN  0x7BEF
#define C_DARK 0x4208
#define C_DGRN 0x03E0
#define C_DDIRT 0x5900
#define C_UNI  0x8410

const uint8_t LVL_W = 200;              // chieu rong man (o 8px) = 1600px
const int16_t CAM_MAX = 1472;           // 200*8 - 128
const uint8_t BPW = 10, BPH = 16;       // kich thuoc nguoi choi

struct Blk { uint8_t c, r, l, h; };     // cot, hang, so o ngang, so o doc
const Blk blks[] PROGMEM = {
  // mat dat (3 hang: 15..17)
  {0, 15, 38, 3}, {42, 15, 28, 3}, {74, 15, 26, 3}, {104, 15, 30, 3}, {139, 15, 25, 3}, {167, 15, 33, 3},
  // ban nhay (di xuyen tu duoi len)
  {14, 12, 5, 1}, {21, 9, 4, 1}, {30, 12, 6, 1}, {47, 12, 5, 1}, {53, 9, 6, 1}, {62, 12, 5, 1},
  {78, 12, 6, 1}, {86, 9, 5, 1}, {92, 12, 4, 1}, {106, 12, 5, 1}, {113, 9, 4, 1}, {119, 12, 6, 1},
  {127, 9, 5, 1}, {141, 12, 6, 1}, {149, 9, 5, 1}, {156, 12, 5, 1}, {170, 12, 5, 1}, {176, 9, 4, 1}
};
const uint8_t N_BLK = sizeof(blks) / sizeof(Blk);

// Doi tuong xuat hien (SAP XEP THEO COT TANG DAN)
// type: 0 linh chay, 1 phao (turret), 2 may bay (tha bom), 3 boss, 4 vat pham ban tan (S), 5 linh ban
struct Spw { uint8_t col, row, type; };
const Spw spws[] PROGMEM = {
  {16, 12, 4}, {24, 15, 0}, {28, 15, 1}, {34, 15, 5}, {36, 6, 2}, {46, 15, 0}, {50, 15, 1},
  {54, 6, 2}, {58, 15, 0}, {60, 15, 1}, {64, 12, 5}, {66, 5, 2}, {76, 15, 0}, {80, 12, 1},
  {84, 6, 2}, {88, 9, 0}, {90, 15, 5}, {93, 12, 4}, {96, 7, 2}, {108, 15, 0}, {110, 15, 1},
  {115, 9, 5}, {117, 5, 2}, {121, 12, 1}, {125, 6, 2}, {129, 9, 0}, {132, 15, 5}, {142, 12, 0},
  {144, 15, 1}, {150, 9, 4}, {152, 5, 2}, {155, 7, 2}, {158, 12, 5}, {160, 15, 1}, {162, 15, 0},
  {172, 12, 0}, {175, 15, 5}, {178, 9, 1}, {194, 15, 3}
};
const uint8_t N_SPW = sizeof(spws) / sizeof(Spw);

struct Ent { int16_t x, y4, by; int8_t vy, hp, dir; uint8_t type, t, st, tm; bool on; };
struct Bul { int16_t x, y; int8_t vx, vy; uint8_t kind; bool on; };
struct Fx  { int16_t x, y; uint8_t t; };
#define MAXE 7
#define MAXB 6
Ent ents[MAXE];
Bul pb[MAXB], eb[MAXB];
Fx fxs[4];

int16_t camX, gx, gy4, safeX;
int8_t gvy, face;
bool wpn;   // false = sung thuong, true = S ban tan
bool onGnd, aPrev, moving, aimUp, bossAlive, hudDirty;
uint8_t dropTimer, invF, shootCd, glives, nextSpw, bossFlash, winTimer, animF;
uint16_t bscore;

// 0 trong, 1 dat dac, 2 ban (chi dung duoc tu tren xuong)
uint8_t tileType(int16_t tc, int16_t tr) {
  if (tc < 0 || tc >= LVL_W) return 1;
  if (tr < 0) return 0;
  Blk b;
  for (uint8_t i = 0; i < N_BLK; i++) {
    memcpy_P(&b, &blks[i], sizeof(Blk));
    if (tc >= b.c && tc < b.c + b.l && tr >= b.r && tr < b.r + b.h) return b.h == 1 ? 2 : 1;
  }
  return 0;
}

// Di chuyen ngang, tra ve true neu bi chan
bool physX(int16_t &x, int16_t y, uint8_t w, uint8_t h, int8_t dx) {
  int16_t nx = x + dx;
  if (nx < 0 || nx + w > LVL_W * 8) return true;
  int16_t col = (dx > 0) ? (nx + w - 1) >> 3 : nx >> 3;
  for (int16_t r = y >> 3; r <= ((y + h - 1) >> 3); r++)
    if (tileType(col, r) == 1) return true;
  x = nx;
  return false;
}

// Trong luc + va cham doc. y4 = toa do y * 4. Tra ve true neu dang dung tren dat
bool physY(int16_t x, int16_t &y4, int8_t &vy, uint8_t w, uint8_t h, bool oneWay) {
  if (vy < 28) vy += 3;
  int16_t ny4 = y4 + vy;
  int16_t oy = y4 >> 2, ny = ny4 >> 2;
  int16_t c0 = x >> 3, c1 = (x + w - 1) >> 3;
  if (vy > 0) {
    int16_t fo = oy + h, fn = ny + h;
    for (int16_t r = (fo + 7) >> 3; r <= (fn >> 3); r++) {
      for (int16_t c = c0; c <= c1; c++) {
        uint8_t t = tileType(c, r);
        if (t == 1 || (t == 2 && oneWay)) {
          y4 = (r * 8 - h) * 4;
          vy = 0;
          return true;
        }
      }
    }
  } else if (vy < 0) {
    for (int16_t r = (oy - 1) >> 3; r >= (ny >> 3); r--) {
      for (int16_t c = c0; c <= c1; c++) {
        if (tileType(c, r) == 1) {
          y4 = (r * 8 + 8) * 4;
          vy = 0;
          return false;
        }
      }
    }
  }
  y4 = ny4;
  return false;
}

bool ov(int16_t ax, int16_t ay, int16_t aw, int16_t ah, int16_t bx, int16_t by, int16_t bw, int16_t bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

// ---------- ve ----------
#define gr(x, y, w, h, c) tft.fillRect(x, y, w, h, c)

void spanFill(int16_t x0, int16_t x1, int16_t y0, int16_t y1, uint16_t c) {
  if (x1 > x0 && y1 > y0) tft.fillRect(x0, y0, x1 - x0, y1 - y0, c);
}

#define skyFill(x, y, w, h) tft.fillRect(x, y, w, h, C_SKY)

// Dat / ban: co (2px) + dat, vach toi moi 8px (cuon ngang khong doi)
void blkFill(int16_t x0, int16_t x1, int16_t y0, int16_t y1, int16_t by0) {
  spanFill(x0, x1, max(y0, by0), min(y1, by0 + 2), C_LEAF);
  spanFill(x0, x1, max(y0, by0 + 2), y1, C_DIRT);
}


// Ve lai nen (troi + dat/ban) trong mot vung (toa do man hinh)
void bgRect(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (x < 0) { w += x; x = 0; }
  if (x + w > 128) w = 128 - x;
  if (y < GY) { h -= GY - y; y = GY; }
  if (y + h > 160) h = 160 - y;
  if (w <= 0 || h <= 0) return;
  skyFill(x, y, w, h);
  Blk b;
  for (uint8_t i = 0; i < N_BLK; i++) {
    memcpy_P(&b, &blks[i], sizeof(Blk));
    int16_t bx0 = b.c * 8 - camX, bx1 = bx0 + b.l * 8;
    if (bx1 <= x || bx0 >= x + w) continue;
    int16_t by0 = GY + b.r * 8, by1 = by0 + b.h * 8;
    if (by1 <= y || by0 >= y + h) continue;
    blkFill(max(bx0, x), min(bx1, x + w), max(by0, y), min(by1, y + h), by0);
  }
}

// Cuon nen sang trai (camera di tu oc -> nc): chi sua 2 mep cua moi khoi
void scrollBg(int16_t oc, int16_t nc) {
  int16_t s = nc - oc;
  if (s <= 0) return;
  Blk b;
  for (uint8_t i = 0; i < N_BLK; i++) {
    memcpy_P(&b, &blks[i], sizeof(Blk));
    int16_t x0 = b.c * 8 - nc, x1 = x0 + b.l * 8;      // toa do sau khi cuon
    if (x1 + s <= 0 || x0 >= 128) continue;
    bgRect(x0, GY + b.r * 8, s, b.h * 8);              // mep trai (khoi moi hien ra)
    bgRect(x1, GY + b.r * 8, s, b.h * 8);              // mep phai (tro lai troi)
  }
}

// Linh: dau/than (7) | chan 0 (4) | chan 1 (4) | chan nhay (2) | sung ngang (2) | sung len (2)
const int8_t SOLD[] PROGMEM = {
  3,0,5,2,16, 1,1,2,1,16, 3,2,5,3,8, 6,3,1,1,7, 2,5,6,6,17, 2,9,6,1,7, 6,6,2,3,8,
  2,11,3,4,18, 2,14,3,1,7, 5,11,3,5,18, 5,15,3,1,7,
  1,11,3,5,18, 1,15,3,1,7, 6,11,3,4,18, 6,15,3,1,7,
  2,11,6,3,18, 2,14,6,1,7,
  7,6,6,2,6, 12,6,1,2,7,
  4,-3,2,7,6, 4,-4,2,1,7
};
void drawSoldier(int16_t x, int16_t y, int8_t fc, uint8_t fm, bool up, uint16_t shirt, uint16_t pants, uint16_t hat) {
  dyn[0] = hat; dyn[1] = shirt; dyn[2] = pants;
  uint8_t m = fc > 0 ? 0 : 10;
  spr(SOLD, 7, x, y, m);
  spr(SOLD + (fm == 2 ? 15 : (fm ? 11 : 7)) * 5, fm == 2 ? 2 : 4, x, y, m);
  spr(SOLD + (up ? 19 : 17) * 5, 2, x, y, m);
}

// Phao: than (7) | nong ngang (2) | nong cheo (2)
const int8_t TUR[] PROGMEM = {
  0,5,12,5,7, 0,5,12,1,6, 1,8,2,1,6, 9,8,2,1,6, 2,2,8,4,6, 4,0,4,3,6, 5,3,2,2,3,
  10,3,4,2,6, 13,3,1,2,7,
  9,1,3,3,6, 11,-1,3,3,7
};
void drawTurret(int16_t x, int16_t y, int8_t fc, bool up) {
  spr(TUR, 7, x, y, 12);
  spr(TUR + (up ? 9 : 7) * 5, 2, x, y, fc > 0 ? 0 : 12);
}

const int8_t FLY[] PROGMEM = {
  1,2,8,4,13, 4,3,2,2,2, 0,3,1,2,6,
  0,0,3,2,12, 7,0,3,2,12,
  0,6,3,2,12, 7,6,3,2,12,
  4,6,3,3,7, 5,9,1,1,3
};
void drawFlyer(int16_t x, int16_t y, uint8_t fl, bool armed) {
  spr(FLY, 3, x, y, 0);
  spr(FLY + (fl ? 3 : 5) * 5, 2, x, y, 0);
  if (armed) spr(FLY + 35, 2, x, y, 0);
}

const int8_t BOSS[] PROGMEM = {
  0,10,24,22,7, 2,12,20,4,6, 4,0,16,10,6, 4,0,16,2,1, -8,13,10,4,6, -8,13,2,4,7,
  2,28,6,4,7, 16,28,6,4,7, 7,3,3,3,16, 14,3,3,3,16, 8,20,8,8,17
};
void drawBoss(int16_t x, int16_t y, bool flash) {
  dyn[0] = flash ? C_TEXT : C_RED;
  dyn[1] = flash ? C_YEL : C_RED;
  SPR(BOSS, x, y, 0);
}

const int8_t FXS[] PROGMEM = {-3,-1,7,3,2, -1,-3,3,7,2, -1,-1,3,3,1, -5,-2,11,5,11, -2,-5,5,11,11, -2,-2,5,5,2};
const int8_t BMB[] PROGMEM = {0,0,5,5,7, 1,1,2,2,6, 2,-1,1,1,2};

void drawPickup(int16_t x, int16_t y) {
  gr(x, y, 8, 8, C_RED);
  tft.setTextColor(C_TEXT, C_RED);
  tft.setCursor(x + 1, y);
  tft.print('S');
}

void drawFx(uint8_t i) {
  spr(FXS + (fxs[i].t < 4 ? 0 : 15), 3, fxs[i].x - camX, fxs[i].y + GY, 0);
}

void drawBomb(int16_t x, int16_t y) {
  SPR(BMB, x, y, 0);
}

void gunHud() {
  hudDirty = false;
  hud(bscore, glives);
  if (wpn) {
    tft.setTextColor(C_YEL);
    tft.setCursor(44, 4);
    tft.print('S');
  }
  if (bossAlive) {
    int16_t hp = 0;
    for (uint8_t i = 0; i < MAXE; i++)
      if (ents[i].on && ents[i].type == 3) hp = ents[i].hp;
    frame(56, 3, 34, 8, C_TEXT);
    tft.fillRect(57, 4, hp * 32 / 30, 6, C_RED);
  }
}

// ---------- doi tuong ----------
const uint8_t ESZ[] PROGMEM = {   // rong, cao, mau, diem
  10, 16, 2, 10,   12, 10, 4, 20,   10, 8, 1, 15,   24, 32, 30, 100,   8, 8, 1, 5,   10, 16, 2, 15
};
void entSize(uint8_t type, uint8_t &w, uint8_t &h) {
  w = pgm_read_byte(&ESZ[type * 4]);
  h = pgm_read_byte(&ESZ[type * 4 + 1]);
}

void addEnt(uint8_t type, uint8_t col, uint8_t row) {
  for (uint8_t i = 0; i < MAXE; i++) {
    if (ents[i].on) continue;
    uint8_t w, h;
    entSize(type, w, h);
    int16_t y = (type == 2) ? row * 8 : row * 8 - h;
    memset(&ents[i], 0, sizeof(Ent));
    ents[i].on = true;
    ents[i].type = type;
    ents[i].x = col * 8;
    ents[i].y4 = y * 4;
    ents[i].by = y;
    ents[i].hp = pgm_read_byte(&ESZ[type * 4 + 2]);       // linh 2, phao 4, may bay 1
    if (type == 3) { bossAlive = true; hudDirty = true; }
    return;
  }
}

void addFx(int16_t x, int16_t y) {
  for (uint8_t i = 0; i < 4; i++) {
    if (!fxs[i].t) { fxs[i].x = x; fxs[i].y = y; fxs[i].t = 1; return; }
  }
}

void spawnEB(int16_t x, int16_t y, int8_t vx, int8_t vy, uint8_t kind) {
  for (uint8_t i = 0; i < MAXB; i++) {
    if (!eb[i].on) { eb[i].on = true; eb[i].x = x; eb[i].y = y; eb[i].vx = vx; eb[i].vy = vy; eb[i].kind = kind; return; }
  }
}

// Ngam nguoi choi theo 8 huong (sx, sy trong -1..1)
void aim8(int16_t x, int16_t y, int8_t &sx, int8_t &sy) {
  int16_t px = gx + 5 - x, py = (gy4 >> 2) + 8 - y;
  int16_t ax = abs(px), ay = abs(py);
  sx = (ax * 2 > ay) ? (px > 0 ? 1 : -1) : 0;
  sy = (ay * 2 > ax) ? (py > 0 ? 1 : -1) : 0;
  if (!sx && !sy) sx = -1;
}

void killEnt(uint8_t i) {
  Ent &e = ents[i];
  uint8_t w, h;
  entSize(e.type, w, h);
  int16_t cx = e.x + w / 2, cy = (e.y4 >> 2) + h / 2;
  addFx(cx, cy);
  if (e.type == 3) {
    bossAlive = false;
    winTimer = 50;
    invF = 255;
    addFx(cx - 8, cy - 8);
    addFx(cx + 8, cy + 8);
    addFx(cx, cy + 10);
  } else {
    beep(40);
  }
  bscore += pgm_read_byte(&ESZ[e.type * 4 + 3]);
  e.on = false;
  hudDirty = true;
}

void killPlayer() {
  if (glives > 0) glives--;
  wpn = false;
  addFx(gx + 5, (gy4 >> 2) + 8);
  invF = 60;
  for (uint8_t i = 0; i < MAXB; i++) eb[i].on = false;
  gx = max(safeX, camX);
  gy4 = 0;
  gvy = 0;
  hudDirty = true;
}

void hurt() {
  if (!invF) killPlayer();
}

void playerFire(bool kL, bool kR, bool kU) {
  int8_t ax = face, ay = 0;
  if (kU) { ay = -1; if (!kL && !kR) ax = 0; }
  int8_t sp = (ax && ay) ? 5 : 6;
  int8_t vx = ax * sp, vy = ay * sp;
  int16_t bx = gx + (ax > 0 ? BPW + 2 : (ax < 0 ? -4 : 4));
  int16_t by = (gy4 >> 2) + ((ay < 0 && ax == 0) ? -4 : 6);
  uint8_t n = wpn ? 3 : 1;
  for (uint8_t k = 0; k < n; k++) {
    int8_t kk = wpn ? (int8_t)k - 1 : 0;
    int8_t svx = vx + (-vy) * kk / 3;
    int8_t svy = vy + vx * kk / 3;
    for (uint8_t i = 0; i < MAXB; i++) {
      if (!pb[i].on) {
        pb[i].on = true;
        pb[i].x = bx;
        pb[i].y = by;
        pb[i].vx = svx;
        pb[i].vy = svy;
        break;
      }
    }
  }
  shootCd = wpn ? 7 : 5;
  beep(12);                                        // tieng ban
}

void updatePlayer(bool kL, bool kR, bool kU, bool kD, bool aNow, bool bNow) {
  int8_t vx = 0;
  if (kL) { vx = -3; face = -1; }
  else if (kR) { vx = 3; face = 1; }
  moving = (vx != 0);
  int16_t gyp = gy4 >> 2;
  if (vx) physX(gx, gyp, BPW, BPH, vx);
  if (gx < camX) gx = camX;
  if (gx > LVL_W * 8 - BPW) gx = LVL_W * 8 - BPW;

  // nhay / roi xuong ban
  if (aNow && !aPrev && onGnd) {
    if (kD && tileType((gx + 5) >> 3, (gyp + BPH) >> 3) == 2) dropTimer = 10;
    else gvy = -30;
  }
  if (!aNow && gvy < -12) gvy = -12;     // nha nut A som = nhay thap
  aPrev = aNow;

  onGnd = physY(gx, gy4, gvy, BPW, BPH, dropTimer == 0);
  if (dropTimer) dropTimer--;
  if (onGnd) safeX = gx;
  if (invF && !winTimer) invF--;

  aimUp = kU;
  if (shootCd) shootCd--;
  if (bNow && !shootCd) playerFire(kL, kR, kU);
}

void updateBullets() {
  for (uint8_t i = 0; i < MAXB; i++) {
    for (uint8_t w = 0; w < 2; w++) {
      Bul &q = w ? eb[i] : pb[i];
      if (!q.on) continue;
      if (q.kind == 1 && q.vy < 8) q.vy++;                   // bom: roi nhanh dan
      q.x += q.vx;
      q.y += q.vy;
      uint8_t tt = tileType((q.x + 2) >> 3, (q.y + 3) >> 3);
      if (q.kind == 1 && tt) {                                // bom no
        addFx(q.x + 2, q.y + 2);
        q.on = false;
      } else if (q.x < camX - 4 || q.x > camX + 130 || q.y < 0 || q.y > 146 || tt == 1) q.on = false;
    }
  }
}

void spawnEnts() {
  while (nextSpw < N_SPW) {
    Spw s;
    memcpy_P(&s, &spws[nextSpw], sizeof(Spw));
    if (s.col * 8 >= camX + 144) break;
    addEnt(s.type, s.col, s.row);
    nextSpw++;
  }
}

void updateEnts() {
  for (uint8_t i = 0; i < MAXE; i++) {
    Ent &e = ents[i];
    if (!e.on) continue;
    e.t++;
    uint8_t w, h;
    entSize(e.type, w, h);
    int16_t ey = e.y4 >> 2;
    if (e.x + 40 < camX || ey > 150) { e.on = false; continue; }
    switch (e.type) {
      case 0:
      case 5: {   // 0 = linh chay (khong ban, nhay qua ho), 5 = linh ban (dung ngam ban 8 huong)
        bool g = physY(e.x, e.y4, e.vy, w, h, true);
        int8_t toP = (gx < e.x) ? -1 : 1;
        if (!e.dir) e.dir = toP;
        int16_t ahead = e.dir > 0 ? e.x + w + 1 : e.x - 2;
        bool floor = tileType(ahead >> 3, (ey + h + 1) >> 3) != 0;
        if (e.type == 0) {
          if (g && (!floor || (uint8_t)(e.t + e.x) % 70 == 0)) e.vy = -28;   // nhay
          if (physX(e.x, ey, w, h, e.dir * 2) && g) e.vy = -28;
        } else {
          if (e.st == 0) {                                // di bo ve phia nguoi choi
            e.dir = toP;
            if (g && floor && (e.t & 1)) physX(e.x, ey, w, h, e.dir);
            if (++e.tm > 45 && e.x - camX < 122) { e.st = 1; e.tm = 0; }
          } else {                                        // dung ban 2 phat
            e.tm++;
            int8_t sx, sy;
            aim8(e.x + 5, ey + 6, sx, sy);
            if (sx) e.dir = sx;
            e.st = (sy < 0) ? 2 : 1;
            if (e.tm == 8 || e.tm == 18) {
              spawnEB(e.x + 5 + sx * 7, ey + 5 + (sy < 0 ? -6 : sy * 6), sx * 3, sy * 3, 0);
            }
            if (e.tm > 30) { e.st = 0; e.tm = 0; }
          }
        }
        break;
      }
      case 1: {   // phao
        if (e.t % 50 == 25 && e.x - camX > 0 && e.x - camX < 120) {
          int8_t vx, vy;
          aim8(e.x + 6, ey + 4, vx, vy);
          spawnEB(e.x + 5, ey + 3, vx * 3, vy * 3, 0);
        }
        break;
      }
      case 2: {   // may bay
        uint8_t ph = e.t;
        int8_t tri = ((ph & 31) < 16 ? (ph & 15) : 15 - (ph & 15)) - 8;
        e.x -= 2;
        e.y4 = (e.by + tri) * 4;
        if (!e.st && e.x - camX < 124 && (e.x + 5) - (gx + 5) <= ((moving && face > 0) ? 20 : 3)) {
          spawnEB(e.x + 3, ey + 8, 0, 3, 1);              // tha bom khi toi cho nguoi choi
          e.st = 1;
        }
        break;
      }
      case 3: {   // boss: ban 3 vien
        if (e.t % 40 == 20 && e.x - camX < 128) {
          int8_t vx, vy;
          aim8(e.x - 8, ey + 14, vx, vy);
          for (int8_t k = -1; k <= 1; k++)
            spawnEB(e.x - 9, ey + 13, vx * 3 - vy * k, vy * 3 + vx * k, 0);
        }
        break;
      }
      default: break;
    }
  }
}

void checkHits() {
  int16_t gyp = gy4 >> 2;
  // dan nguoi choi -> dich
  for (uint8_t i = 0; i < MAXB; i++) {
    if (!pb[i].on) continue;
    for (uint8_t j = 0; j < MAXE; j++) {
      if (!ents[j].on || ents[j].type == 4) continue;
      uint8_t w, h;
      entSize(ents[j].type, w, h);
      if (ov(pb[i].x, pb[i].y, 3, 3, ents[j].x, ents[j].y4 >> 2, w, h)) {
        pb[i].on = false;
        ents[j].hp--;
        if (ents[j].type == 3) { bossFlash = 3; hudDirty = true; }
        if (ents[j].hp <= 0) killEnt(j);
        break;
      }
    }
  }
  // dan dich -> nguoi choi
  for (uint8_t i = 0; i < MAXB; i++) {
    if (eb[i].on && ov(eb[i].x, eb[i].y, eb[i].kind ? 5 : 3, eb[i].kind ? 5 : 3, gx + 2, gyp + 2, BPW - 4, BPH - 4)) {
      eb[i].on = false;
      hurt();
    }
  }
  // dich / vat pham cham nguoi choi
  for (uint8_t j = 0; j < MAXE; j++) {
    if (!ents[j].on) continue;
    uint8_t w, h;
    entSize(ents[j].type, w, h);
    int16_t ey = ents[j].y4 >> 2;
    if (ents[j].type == 4) {
      if (ov(ents[j].x, ey, 8, 8, gx, gyp, BPW, BPH)) {
        wpn = true;
        bscore += 5;
        ents[j].on = false;
        hudDirty = true;
        beep(80);
      }
    } else if (ov(ents[j].x + 1, ey + 1, w - 2, h - 2, gx + 2, gyp + 2, BPW - 4, BPH - 4)) {
      hurt();
    }
  }
  // roi xuong ho
  if ((gy4 >> 2) > 150) killPlayer();
}

void eraseAll() {
  bgRect(gx - camX - 4, (gy4 >> 2) + GY - 4, 18, BPH + 4);
  for (uint8_t i = 0; i < MAXE; i++) {
    if (!ents[i].on) continue;
    uint8_t w, h;
    entSize(ents[i].type, w, h);
    bgRect(ents[i].x - camX - 10, (ents[i].y4 >> 2) + GY - 4, w + 20, h + 9);
  }
  for (uint8_t i = 0; i < MAXB; i++) {
    if (pb[i].on) bgRect(pb[i].x - camX, pb[i].y + GY, 3, 3);
    if (eb[i].on) bgRect(eb[i].x - camX, eb[i].y + GY - 1, 6, 7);
  }
  for (uint8_t i = 0; i < 4; i++)
    if (fxs[i].t) bgRect(fxs[i].x - camX - 6, fxs[i].y + GY - 6, 13, 13);
}

void drawAll() {
  for (uint8_t i = 0; i < MAXE; i++) {
    Ent &e = ents[i];
    if (!e.on) continue;
    int16_t sx = e.x - camX, sy = (e.y4 >> 2) + GY;
    if (sx > 130 || sx < -40) continue;
    int8_t fc = (gx < e.x) ? -1 : 1;
    switch (e.type) {
      case 0: drawSoldier(sx, sy, e.dir ? e.dir : fc, e.vy ? 2 : (e.t >> 2) & 1, false, C_UNI, C_DARK, C_DARK); break;
      case 5: drawSoldier(sx, sy, e.dir ? e.dir : fc, (e.st || e.vy) ? (e.vy ? 2 : 0) : (e.t >> 2) & 1, e.st == 2, C_RED, C_DARK, C_RED); break;
      case 1: drawTurret(sx, sy, fc, (gy4 >> 2) + 8 < (e.y4 >> 2) - 10); break;
      case 2: drawFlyer(sx, sy, (e.t >> 2) & 1, !e.st); break;
      case 3: drawBoss(sx, sy, bossFlash > 0); break;
      default: drawPickup(sx, sy); break;
    }
  }
  for (uint8_t i = 0; i < MAXB; i++) {
    if (pb[i].on) gr(pb[i].x - camX, pb[i].y + GY, 3, 3, C_YEL);
    if (eb[i].on) { if (eb[i].kind) drawBomb(eb[i].x - camX, eb[i].y + GY); else gr(eb[i].x - camX, eb[i].y + GY, 3, 3, ST77XX_ORANGE); }
  }
  bool vis = (invF == 0) || ((invF >> 2) & 1);
  if (vis)
    drawSoldier(gx - camX, (gy4 >> 2) + GY, face, !onGnd ? 2 : (moving ? ((animF >> 1) & 1) : 0), aimUp, C_TEXT, 0x001F, C_RED);
  for (uint8_t i = 0; i < 4; i++)
    if (fxs[i].t) drawFx(i);
}

void gunInit() {
  tft.fillScreen(C_BG);
  camX = 0;
  gx = 24;
  gy4 = (120 - BPH) * 4;
  gvy = 0;
  face = 1;
  onGnd = true;
  aPrev = (digitalRead(BTN_A) == LOW);
  wpn = false;
  moving = false;
  aimUp = false;
  bossAlive = false;
  hudDirty = false;
  dropTimer = 0;
  invF = 0;
  shootCd = 0;
  glives = 3;
  nextSpw = 0;
  bossFlash = 0;
  winTimer = 0;
  animF = 0;
  safeX = gx;
  bscore = 0;
  for (uint8_t i = 0; i < MAXE; i++) ents[i].on = false;
  for (uint8_t i = 0; i < MAXB; i++) { pb[i].on = false; eb[i].on = false; }
  for (uint8_t i = 0; i < 4; i++) fxs[i].t = 0;
  gunHud();
  bgRect(0, GY, 128, 144);
  spawnEnts();
  drawAll();
  delay(700);
  bgRect(0, GY, 128, 144);
  drawAll();
}

// Tra ve 0 = thua, 1 = thang, 2 = ve menu
uint8_t gunRun() {
  uint16_t last = millis();
  while (true) {
    while (ELAPSED(last) < 40) { }
    last = millis();
    animF++;

    uint8_t b = sysKeys();
    if (b == 2) return 2;
    if (b) {
      tft.fillScreen(C_BG);
      gunHud();
      bgRect(0, GY, 128, 144);
      last = millis();
      aPrev = (digitalRead(BTN_A) == LOW);
    }

    int dx, dy;
    readJoy(dx, dy);
    bool kL = dx < -250, kR = dx > 250, kU = dy < -250, kD = dy > 250;
    bool aNow = digitalRead(BTN_A) == LOW, bNow = digitalRead(BTN_B) == LOW;

    eraseAll();

    for (uint8_t i = 0; i < 4; i++) {
      if (fxs[i].t) { fxs[i].t++; if (fxs[i].t > 8) fxs[i].t = 0; }
    }
    updatePlayer(kL, kR, kU, kD, aNow, bNow);
    updateBullets();

    int16_t oldCam = camX;
    int16_t nc = gx - 56;
    if (nc > camX) camX = nc;
    if (camX > CAM_MAX) camX = CAM_MAX;

    spawnEnts();
    updateEnts();
    checkHits();
    scrollBg(oldCam, camX);
    drawAll();
    if (hudDirty) gunHud();

    if (bossFlash) bossFlash--;
    if (winTimer) {
      winTimer--;
      if (!winTimer) return 1;
    }
    if (glives == 0) {
      delay(500);
      return 0;
    }
  }
}

void playBlast() {
  while (true) {
    gunInit();
    tune(T_IRON, true);
    uint8_t res = gunRun();
    if (res == 2) return;
    if (res == 1) bscore += glives * 50;
    bool rec = saveHi(1, bscore);
    uint8_t b = (res == 1) ? endScreen(F("YOU WIN!"), C_SNAKE, bscore, hiS[1], rec)
                           : endScreen(F("GAME OVER"), C_RED, bscore, hiS[1], rec);
    if (b == 2) return;
  }
}

// =====================================================
//                 BO DEM DUNG CHUNG (Tetris / Pac-Man / Tank)
// =====================================================
uint8_t gbuf[KW_KH];

// =====================================================
//                        TETRIS
// =====================================================
#define TX 57
#define TY 16
#define TB(y, x) gbuf[(y) * 10 + (x)]
// 4 xoay x 7 khoi (I O T S Z J L), moi o 4x4 bit
const uint16_t pcs[28] PROGMEM = {
  0x0F00, 0x2222, 0x00F0, 0x4444, 0x6600, 0x6600, 0x6600, 0x6600,
  0x4E00, 0x4640, 0x0E40, 0x4C40, 0x6C00, 0x4620, 0x06C0, 0x8C40,
  0xC600, 0x2640, 0x0C60, 0x4C80, 0x8E00, 0x6440, 0x0E20, 0x44C0,
  0x2E00, 0x4460, 0x0E80, 0xC440
};
const uint16_t tcl[8] PROGMEM = {0, 0x07FF, 0xFFE0, 0xF81F, 0x07E0, 0xF800, 0x001F, 0xFD20};
int8_t tpx, tpy;
uint8_t trot, tty, tnx, tlv, tln;
uint16_t tsc;

bool tfit(int8_t px, int8_t py, uint8_t r) {
  uint16_t m = pgm_read_word(&pcs[tty * 4 + r]);
  for (uint8_t i = 0; i < 16; i++) {
    if (!(m & (0x8000 >> i))) continue;
    int8_t x = px + (i & 3), y = py + (i >> 2);
    if (x < 0 || x >= 10 || y >= 20 || (y >= 0 && TB(y, x))) return false;
  }
  return true;
}

void tcell(int8_t x, int8_t y, uint8_t c) {
  if (y < 0) return;
  tft.fillRect(TX + x * 7, TY + y * 7, 7, 7, C_BG);
  if (c) {
    uint16_t k = pgm_read_word(&tcl[c]);
    tft.fillRect(TX + x * 7, TY + y * 7, 6, 6, k);
  }
}

void tdrawP(bool on) {
  uint16_t m = pgm_read_word(&pcs[tty * 4 + trot]);
  for (uint8_t i = 0; i < 16; i++)
    if (m & (0x8000 >> i)) tcell(tpx + (i & 3), tpy + (i >> 2), on ? tty + 1 : 0);
}

void tpanel() {
  tft.fillRect(0, 16, 54, 130, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(C_TEXT);
  tft.setCursor(0, 20);
  tft.print(F("DIEM\n"));
  pn(tsc);
  tft.print(F("\n\nCAP\n"));
  pn(tlv);
  tft.print(F("\n\nHANG\n"));
  pn(tln);
}

void tfull() {
  tft.fillScreen(C_BG);
  frame(TX - 1, TY - 1, 72, 142, C_BORDER);
  for (int8_t y = 0; y < 20; y++)
    for (int8_t x = 0; x < 10; x++)
      if (TB(y, x)) tcell(x, y, TB(y, x));
  tdrawP(true);
  tpanel();
}

bool tspawn() {
  tty = tnx;
  tnx = rnd(7);
  trot = 0; tpx = 3; tpy = -1;
  return tfit(tpx, tpy, trot);
}

// 0 = thua, 2 = ve menu
uint8_t tetrisRun() {
  uint16_t tf = millis(), tm = 0;
  bool aP = true, bP = true;
  while (true) {
    uint8_t b = sysKeys();
    if (b == 2) return 2;
    if (b) { tfull(); tf = millis(); aP = bP = true; }
    int dx, dy;
    readJoy(dx, dy);
    bool a = digitalRead(BTN_A) == LOW, bb = digitalRead(BTN_B) == LOW;
    if (ELAPSED(tm) > 110 && (dx < -250 || dx > 250)) {
      int8_t d = dx > 0 ? 1 : -1;
      if (tfit(tpx + d, tpy, trot)) { tdrawP(false); tpx += d; tdrawP(true); }
      tm = millis();
    }
    if (a && !aP) {                                   // xoay (co day tuong)
      uint8_t r = (trot + 1) & 3;
      if (tfit(tpx, tpy, r)) { tdrawP(false); trot = r; tdrawP(true); beep(15); }
    }
    aP = a;
    bool hard = bb && !bP;
    bP = bb;
    if (hard) {                                       // tha nhanh
      tdrawP(false);
      while (tfit(tpx, tpy + 1, trot)) { tpy++; tsc += 2; }
      tdrawP(true);
    }
    uint16_t gi = (dy > 250) ? 45 : max(100, 700 - tlv * 45);
    if (hard || ELAPSED(tf) >= gi) {
      tf = millis();
      if (!hard && tfit(tpx, tpy + 1, trot)) { tdrawP(false); tpy++; tdrawP(true); }
      else {
        uint16_t m = pgm_read_word(&pcs[tty * 4 + trot]);
        for (uint8_t i = 0; i < 16; i++) {
          int8_t y = tpy + (i >> 2);
          if ((m & (0x8000 >> i)) && y >= 0) TB(y, tpx + (i & 3)) = tty + 1;
        }
        uint8_t n = 0;
        for (int8_t y = 19; y >= 0; y--) {
          bool f = true;
          for (uint8_t x = 0; x < 10; x++) if (!TB(y, x)) { f = false; break; }
          if (f) { memmove(&gbuf[10], &gbuf[0], y * 10); memset(gbuf, 0, 10); n++; y++; }
        }
        beep(25);
        if (n) {
          tsc += (n == 1 ? 40 : n == 2 ? 100 : n == 3 ? 300 : 1200) * tlv;
          tln += n;
          tlv = min(15, tln / 10 + 1);
          beep(80);
        }
        if (!tspawn()) { delay(400); return 0; }
        if (n) tfull(); else { tdrawP(true); tpanel(); }
      }
    }
    delay(6);
  }
}

void playTetris() {
  while (true) {
    memset(gbuf, 0, 200);
    tsc = 0; tln = 0; tlv = 1;
    tnx = rnd(7);
    tspawn();
    waitAllReleased();
    tfull();
    tune(T_TETRIS, true);
    if (tetrisRun() == 2) return;
    if (endScreen(F("GAME OVER"), C_RED, tsc, hiS[2], saveHi(2, tsc)) == 2) return;
  }
}

// =====================================================
//                        PAC-MAN
// =====================================================
// Ban do 16 x 18 o, moi o 8px; HUD o tren y=0..15. Dung chung bo dem gbuf.
const uint8_t PM_W = 16, PM_H = 18;
const uint16_t pmMap[PM_H] PROGMEM = {
  0xFFFF,0x8001,0xB7B1,0x8001,0xB4B1,0x8481,
  0xF4BF,0x8001,0xF7BF,0x8001,0xB7B1,0x9009,
  0xD4AF,0x8481,0xBCF5,0x8001,0x8001,0xFFFF
};
#define pmDots gbuf                               // 288 o = 36 byte
uint8_t pmX, pmY, pmDir, pmWant, pmLives, pmTick, pmGX[2], pmGY[2];
uint16_t pmScore, pmLeft;
const int8_t PMDX[4] = {0, 1, 0, -1};
const int8_t PMDY[4] = {-1, 0, 1, 0};

bool pmWall(int8_t x, int8_t y) {
  if (x < 0 || x >= PM_W || y < 0 || y >= PM_H) return true;
  return pgm_read_word(&pmMap[y]) & (0x8000 >> x);
}
bool pmDot(uint8_t x, uint8_t y) {
  uint8_t n = y * PM_W + x;
  return pmDots[n >> 3] & (1 << (n & 7));
}
void pmCell(uint8_t x, uint8_t y) {
  int16_t px = x * 8, py = GY + y * 8;
  if (pmWall(x, y)) { tft.fillRect(px, py, 8, 8, 0x001F); tft.fillRect(px + 1, py + 1, 6, 6, 0x000C); }
  else { tft.fillRect(px, py, 8, 8, C_BG); if (pmDot(x, y)) tft.fillRect(px + 3, py + 3, 2, 2, C_YEL); }
}
void pmActor(uint8_t x, uint8_t y, uint16_t color, bool player) {
  int16_t cx = x * 8 + 4, cy = GY + y * 8 + 4;
  tft.fillRect(cx - 1, cy - 3, 3, 7, color);
  tft.fillRect(cx - 3, cy - 1, 7, 3, color);
  tft.fillRect(cx - 2, cy - 2, 5, 5, color);
  if (player) tft.fillRect(cx + PMDX[pmDir], cy + PMDY[pmDir], 1, 1, C_BG);
  else { tft.fillRect(cx - 1, cy - 1, 1, 1, C_TEXT); tft.fillRect(cx + 1, cy - 1, 1, 1, C_TEXT); }
}
void pmHud() {
  hud(pmScore, pmLives);
}
void pmDraw() {
  tft.fillScreen(C_BG); pmHud();
  for (uint8_t y = 0; y < PM_H; y++) for (uint8_t x = 0; x < PM_W; x++) pmCell(x, y);
  for (uint8_t i = 0; i < 2; i++) pmActor(pmGX[i], pmGY[i], i ? 0xF81F : C_RED, false);
  pmActor(pmX, pmY, C_YEL, true);
}
void pmNewLife() {
  pmX = 1; pmY = 1; pmDir = pmWant = RIGHT;
  pmGX[0] = 7; pmGX[1] = 8; pmGY[0] = pmGY[1] = 9;
  pmDraw(); delay(450);
}
bool pmHit() { return (pmX == pmGX[0] && pmY == pmGY[0]) || (pmX == pmGX[1] && pmY == pmGY[1]); }

// Ma duoi theo nguoi choi (ma 1 thinh thoang di ngau nhien), khong quay dau neu con duong khac, khong chong len nhau
uint8_t pmGD[2];
void pmGhostStep(uint8_t i) {
  uint8_t best = 4; int8_t bs = 127;
  for (uint8_t d = 0; d < 4; d++) {
    int8_t nx = pmGX[i] + PMDX[d], ny = pmGY[i] + PMDY[d];
    if (pmWall(nx, ny) || (nx == pmGX[1 - i] && ny == pmGY[1 - i])) continue;
    int8_t sc = (i && !rnd(4)) ? rnd(30) : abs(nx - pmX) + abs(ny - pmY);
    if (d == ((pmGD[i] + 2) & 3)) sc += 9;
    if (sc < bs) { bs = sc; best = d; }
  }
  if (best > 3) return;
  pmGD[i] = best;
  pmCell(pmGX[i], pmGY[i]);
  pmGX[i] += PMDX[best]; pmGY[i] += PMDY[best];
  pmActor(pmGX[i], pmGY[i], i ? 0xF81F : C_RED, false);
}

// 0 = thua, 1 = thang, 2 = ve menu
uint8_t pacmanRun() {
  uint16_t last = millis();
  while (true) {
    while (ELAPSED(last) < 145) { }
    last = millis();
    uint8_t b = sysKeys();
    if (b == 2) return 2;
    if (b) { pmDraw(); last = millis(); }
    int dx, dy;
    readJoy(dx, dy);
    if (abs(dx) > abs(dy) && abs(dx) > 250) pmWant = dx > 0 ? RIGHT : LEFT;
    else if (abs(dy) > 250) pmWant = dy > 0 ? DOWN : UP;
    pmCell(pmX, pmY);
    if (!pmWall(pmX + PMDX[pmWant], pmY + PMDY[pmWant])) pmDir = pmWant;
    if (!pmWall(pmX + PMDX[pmDir], pmY + PMDY[pmDir])) { pmX += PMDX[pmDir]; pmY += PMDY[pmDir]; }
    if (pmDot(pmX, pmY)) {
      uint8_t n = pmY * PM_W + pmX;
      pmDots[n >> 3] &= ~(1 << (n & 7));
      pmLeft--; pmScore += 10; pmCell(pmX, pmY); pmHud();
    }
    pmActor(pmX, pmY, C_YEL, true);
    bool hit = pmHit();
    if (!hit && ++pmTick >= 2) {
      pmTick = 0;
      pmGhostStep(0); pmGhostStep(1);
      hit = pmHit();
    }
    if (hit) { pmLives--; beep(250); if (!pmLives) return 0; pmNewLife(); }
    else if (!pmLeft) return 1;
  }
}

void playPacman() {
  while (true) {
    pmScore = 0; pmLives = 3; pmTick = 0; pmLeft = 0;
    memset(pmDots, 0, 36);
    for (uint8_t y = 0; y < PM_H; y++)
      for (uint8_t x = 0; x < PM_W; x++)
        if (!pmWall(x, y) && !(x == 1 && y == 1)) { uint8_t n = y * PM_W + x; pmDots[n >> 3] |= 1 << (n & 7); pmLeft++; }
    waitAllReleased(); pmNewLife();
    tune(T_PAC, true);
    uint8_t r = pacmanRun();
    if (r == 2) return;
    if (r == 1) { pmScore += 100; beep(250); }
    if (endScreen(r == 1 ? F("YOU WIN!") : F("GAME OVER"), r == 1 ? C_YEL : C_RED, pmScore, hiS[3], saveHi(3, pmScore)) == 2) return;
  }
}

// =====================================================
//                     TANK BATTLE
// =====================================================
// Luoi 21 x 24 o (6 px). Xe tang 9x9 px. gbuf: 0 trong, 1 gach (pha duoc), 2 thep, 3 can cu
#define KC 6
#define KW 21
#define KH 24
#define KG(c, r) gbuf[(r) * KW + (c)]
struct KT { uint8_t x, y, d, t; bool on; uint8_t p1, p2, p3; };       // d: 0 len, 1 phai, 2 xuong, 3 trai
struct KB { uint8_t x, y; int8_t dx, dy; bool on; uint8_t p1, p2, p3; };   // o 0-1: dan nguoi choi, 2-5: dan dich
KT kt[4];                                         // 0 = nguoi choi
KB kb[6];
uint8_t kLeft, kToSpawn, kWave, kLives, kInv, kCd, kSpT, kSpI;
uint16_t kSc;

void kHud() {
  hud(kSc, kLives);
  tft.setTextColor(C_HEAD);
  tft.setCursor(60, 4);
  tft.print('E');
  pn(kLeft);
}

const int8_t KCL[] PROGMEM = {
  0,0,6,6,10, 0,2,6,1,14, 0,5,6,1,14, 3,0,1,2,14, 1,3,1,2,14,
  0,0,6,6,6, 1,1,4,4,15, 2,2,2,2,6,
  0,0,6,6,0, 1,2,4,3,3, 2,0,2,2,2
};
void kCell(uint8_t c, uint8_t r) {
  uint8_t v = KG(c, r);
  spr(KCL + (v == 1 ? 0 : (v == 2 ? 5 : 8)) * 5, v == 1 ? 5 : (v == 2 ? 3 : 3), c * KC, GY + r * KC, 0);
}

// Xe tang 4 huong: than, 2 xich, thap phao, nong
const int8_t TNK[] PROGMEM = {
  0,0,9,9,16, 0,0,2,9,7, 7,0,2,9,7, 3,3,3,3,9, 4,0,1,4,6,
  0,0,9,9,16, 0,0,9,2,7, 0,7,9,2,7, 3,3,3,3,9, 5,4,4,1,6,
  0,0,9,9,16, 0,0,2,9,7, 7,0,2,9,7, 3,3,3,3,9, 4,5,1,4,6,
  0,0,9,9,16, 0,0,9,2,7, 0,7,9,2,7, 3,3,3,3,9, 0,4,4,1,6
};
void kDraw(uint8_t i) {
  KT &t = kt[i];
  if (!t.on || (i == 0 && (kInv & 4))) { tft.fillRect(t.x, t.y, 9, 9, C_BG); return; }
  dyn[0] = i == 0 ? C_YEL : 0xC618;
  spr(TNK + t.d * 25, 5, t.x, t.y, 0);
}

// Ban do: cac khoi gach 2x2 o xep thanh luoi (lan di rong 2 o), vai khoi thep, can cu o duoi giua
void kMapInit() {
  memset(gbuf, 0, KW * KH);
  for (uint8_t r = 3; r < 17; r++)
    for (uint8_t c = 1; c < 19; c++) {
      uint8_t bc = c >> 2, br = (r - 3) >> 2, mc = c & 3, mr = (r - 3) & 3;
      if ((mc == 1 || mc == 2) && (mr == 0 || mr == 1))
        KG(c, r) = ((bc == 2 && (br == 1 || br == 2)) || (br == 3 && (bc == 0 || bc == 4))) ? 2 : 1;
    }
  for (uint8_t c = 8; c < 12; c++) KG(c, 21) = 1;
  for (uint8_t r = 22; r < 24; r++) { KG(8, r) = 1; KG(11, r) = 1; KG(9, r) = KG(10, r) = 3; }
}

void kFull() {
  tft.fillScreen(C_BG);
  kHud();
  for (uint8_t r = 0; r < KH; r++)
    for (uint8_t c = 0; c < KW; c++)
      if (KG(c, r)) kCell(c, r);
  for (uint8_t i = 0; i < 4; i++) if (kt[i].on) kDraw(i);
}

bool kBlk(int16_t x, int16_t y) {
  if (x < 0 || x > 117 || y < GY || y > 151) return true;
  uint8_t xx = x, yy = y - GY;
  for (uint8_t r = yy / KC; r <= (uint8_t)(yy + 8) / KC; r++)
    for (uint8_t c = xx / KC; c <= (uint8_t)(xx + 8) / KC; c++)
      if (KG(c, r)) return true;
  return false;
}

bool kTT(uint8_t i, int16_t x, int16_t y) {
  for (uint8_t j = 0; j < 4; j++)
    if (j != i && kt[j].on && abs((int16_t)kt[j].x - x) < 9 && abs((int16_t)kt[j].y - y) < 9) return true;
  return false;
}

bool kMove(uint8_t i, uint8_t d, uint8_t sp) {
  for (; sp; sp--) {
    int16_t nx = kt[i].x + DX[d] * sp, ny = kt[i].y + DY[d] * sp;
    if (!kBlk(nx, ny) && !kTT(i, nx, ny)) {
      tft.fillRect(kt[i].x, kt[i].y, 9, 9, C_BG);
      kt[i].x = nx; kt[i].y = ny;
      return true;
    }
  }
  return false;
}

void kShoot(uint8_t i) {
  for (uint8_t k = i ? 2 : 0; k < (i ? 6 : 2); k++) {
    if (kb[k].on) continue;
    KT &t = kt[i];
    kb[k].on = true;
    kb[k].dx = DX[t.d] * 4; kb[k].dy = DY[t.d] * 4;
    kb[k].x = t.x + 3 + DX[t.d] * 5;
    kb[k].y = t.y + 3 + DY[t.d] * 5;
    return;
  }
}

void kBoom(int16_t x, int16_t y) {
  tft.fillRect(x, y, 9, 9, ST77XX_ORANGE);
  tft.fillRect(x + 2, y + 2, 5, 5, C_YEL);
  delay(60);
  tft.fillRect(x, y, 9, 9, C_BG);
}

void kStart() {
  kMapInit();
  for (uint8_t i = 0; i < 4; i++) kt[i].on = false;
  for (uint8_t i = 0; i < 6; i++) kb[i].on = false;
  kt[0].on = true; kt[0].x = 30; kt[0].y = 151; kt[0].d = 0;
  kToSpawn = kLeft = min(18, 6 + kWave * 2);
  kSpT = 30; kInv = 60; kCd = 0;
  kFull();
}

// 0 = thua, 1 = qua man, 2 = menu
uint8_t tankRun() {
  uint16_t last = millis();
  while (true) {
    while (ELAPSED(last) < 33) { }
    last = millis();
    uint8_t b = sysKeys();
    if (b == 2) return 2;
    if (b) { kFull(); last = millis(); }
    int dx, dy;
    readJoy(dx, dy);
    int8_t nd = -1;
    if (abs(dx) > abs(dy)) { if (abs(dx) > 250) nd = dx > 0 ? 1 : 3; }
    else if (abs(dy) > 250) nd = dy > 0 ? 2 : 0;
    if (kInv) { kInv--; kDraw(0); }
    if (nd >= 0) {
      bool turn = kt[0].d != nd;
      kt[0].d = nd;
      if (kMove(0, nd, 2) || turn) kDraw(0);
    }
    if (kCd) kCd--;
    if (digitalRead(BTN_B) == LOW && !kCd) { kShoot(0); kCd = 8; beep(12); }

    // dan
    for (uint8_t k = 0; k < 6; k++) {
      KB &q = kb[k];
      if (!q.on) continue;
      tft.fillRect(q.x, q.y, 2, 2, C_BG);
      q.x += q.dx; q.y += q.dy;
      if (q.x > 125 || q.y < GY || q.y > 158) { q.on = false; continue; }
      uint8_t cc = (q.x + 1) / KC, rr = (uint8_t)(q.y + 1 - GY) / KC;
      uint8_t &cell = KG(cc, rr);
      if (cell) {
        q.on = false;
        if (cell == 1) { cell = 0; tft.fillRect(cc * KC, GY + rr * KC, KC, KC, C_BG); }
        else if (cell == 3) { kBoom(54, GY + 132); delay(400); return 0; }
        continue;
      }
      for (uint8_t i = 0; i < 4; i++) {
        if (!kt[i].on || (i == 0) != (k >= 2) || !ov(q.x, q.y, 2, 2, kt[i].x, kt[i].y, 9, 9)) continue;
        q.on = false;
        if (i == 0 && kInv) break;
        kt[i].on = false;
        kDraw(i);
        kBoom(kt[i].x, kt[i].y);
        if (i) { kSc += 100; kLeft--; }
        else {
          kLives--; kInv = 60;
          kt[0].on = true; kt[0].x = 30; kt[0].y = 151; kt[0].d = 0;
          for (uint8_t m = 0; m < 6; m++) if (kb[m].on) { tft.fillRect(kb[m].x, kb[m].y, 2, 2, C_BG); kb[m].on = false; }
          kDraw(0);
        }
        kHud();
        if (!kLives) { delay(300); return 0; }
        break;
      }
      if (q.on) tft.fillRect(q.x, q.y, 2, 2, C_TEXT);
    }
    if (!kLeft) { kWave++; kSc += 200; return 1; }

    // sinh dich
    if (kToSpawn && ++kSpT >= 60) {
      for (uint8_t i = 1; i < 4; i++) {
        if (kt[i].on) continue;
        uint8_t sx2 = (kSpI % 3) * 58;
        if (kTT(i, sx2, GY)) break;
        kSpT = 0; kToSpawn--; kSpI++;
        kt[i].on = true; kt[i].x = sx2; kt[i].y = GY; kt[i].d = 2; kt[i].t = 0;
        kDraw(i);
        break;
      }
    }
    // AI dich: di thang, ket tuong thi doi huong, ban ngau nhien
    for (uint8_t i = 1; i < 4; i++) {
      KT &t = kt[i];
      if (!t.on) continue;
      t.t++;
      if (t.t & 1) { if (kMove(i, t.d, 1)) kDraw(i); else t.t = 63; }
      if (!(t.t & 63)) {
        t.d = rnd(3) ? (rnd(2) ? 2 : (kt[0].x < t.x ? 3 : 1)) : rnd(4);
        kDraw(i);
      }
      if (!rnd(40)) kShoot(i);
    }
  }
}

void playTank() {
  while (true) {
    kSc = 0; kWave = 0; kLives = 3; kSpI = 0;
    waitAllReleased();
    kStart();
    tune(T_TANK, true);
    uint8_t r;
    do {
      r = tankRun();
      if (r == 1) { tune(T_WIN, false); delay(900); kStart(); tune(T_TANK, true); }
    } while (r == 1);
    if (r == 2) return;
    if (endScreen(F("GAME OVER"), C_RED, kSc, hiS[4], saveHi(4, kSc)) == 2) return;
  }
}

// =====================================================
//                         MENU
// =====================================================
const uint8_t N_GAMES = 5;

const char NMS[] PROGMEM = "SNAKE\0\0\0\0\0\0\0IRON STRIKE\0TETRIS\0\0\0\0\0\0PAC-MAN\0\0\0\0\0TANK BATTLE";

void drawCard(uint8_t i, bool sel) {
  int16_t y = 28 + i * 24;
  tft.fillRect(2, y, 124, 22, sel ? C_SEL : C_BG);
  if (sel) frame(2, y, 124, 22, C_YEL);
  tft.setTextSize(1);
  tft.setTextColor(sel ? C_YEL : C_TEXT);
  tft.setCursor(10, y + 7);
  tft.print((const __FlashStringHelper*)&NMS[i * 12]);
}

uint8_t menu() {
  waitAllReleased();
  tune(0, false);
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, 128, 24, ST77XX_BLUE);
  centerText(F("GAME BOX"), 5, 2, C_TEXT);
  tft.fillRect(0, 24, 128, 2, C_YEL);

  uint8_t sel = 0;
  for (uint8_t i = 0; i < N_GAMES; i++) drawCard(i, i == sel);

  uint16_t lm = 0;
  while (true) {
    int dx, dy;
    readJoy(dx, dy);
    if (ELAPSED(lm) > 200 && (dy < -250 || dy > 250)) {
      drawCard(sel, false);
      if (dy < 0) sel = sel ? sel - 1 : N_GAMES - 1;
      else sel = (sel + 1) % N_GAMES;
      drawCard(sel, true);
      beep(10);
      lm = millis();
    }
    if (readBtn() || pressedA()) return sel;
  }
}

// =====================================================
//                      ARDUINO
// =====================================================
void setup() {
  pinMode(JOY_SW, INPUT_PULLUP);
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  rs = analogRead(A5) | 1;

  tft.initR(INITR_BLACKTAB);   // lech/sai mau: thu INITR_GREENTAB hoac INITR_REDTAB
  tft.setRotation(0);

  for (uint8_t i = 0; i < 5; i++) {
    EEPROM.get(i * 2, hiS[i]);
    if (hiS[i] == 0xFFFF) hiS[i] = 0;
  }
  timerInit();
}

void loop() {
  uint8_t g = menu();
  if (g == 0) playSnake();
  else if (g == 1) playBlast();
  else if (g == 2) playTetris();
  else if (g == 3) playPacman();
  else playTank();
}
