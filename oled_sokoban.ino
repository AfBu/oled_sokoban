#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <avr/pgmspace.h>

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

#define MAX_BOXES 6
#define RESET_BUTTON 2

static const unsigned char PROGMEM box_bmp [] = { 0xFF, 0x81, 0xA5, 0x99, 0x99, 0xA5, 0x81, 0xFF, };
static const unsigned char PROGMEM wall_bmp [] = { 0xFF, 0x20, 0xFF, 0x04, 0xFF, 0x20, 0xFF, 0x04, };
static const unsigned char PROGMEM knight_bmp [] = { 0x7E, 0x81, 0xA5, 0x81, 0x5A, 0xFF, 0xBD, 0x24, };
static const unsigned char PROGMEM target_bmp [] = { 0x00, 0x5A, 0x00, 0x42, 0x42, 0x00, 0x5A, 0x00, };

unsigned char level [] = {
  0x00, 0x00, 0x00, 0x00, 0x3E, 0xE2, 0x8B, 0xA1, 0x85, 0xD1, 0x47, 0x7C, 0x00, 0x00, 0x00, 0x00
};

struct TRG {
  byte a; // active
  byte x; // x position
  byte y; // y position
};

struct BOX {
  byte a; // active
  byte x; // x position
  byte y; // y position
  char mx; // x box movement
  char my; // y box movement
};

struct LVL {
  byte px;
  byte py;
  byte l;
  BOX bxs [MAX_BOXES];
  TRG tgs [MAX_BOXES];
};

static const LVL levels [] PROGMEM = {
  /* 0 */ { 6, 6, 0, { {1,6,2} }, { {1,9,3} } },
  /* 1 */ { 6, 6, 0, { {1,6,2},{1,6,5} }, { {1,9,3},{1,8,5} } },
  /* 2 */ { 6, 6, 0, { {1,6,2},{1,6,5},{1,9,5} }, { {1,9,3},{1,8,5},{1,6,4} } },
  /* 3 */ { 9, 5, 1, { {1,5,3} }, { {1,6,4} } },
  /* 4 */ { 9, 5, 1, { {1,5,3},{1,6,5} }, { {1,6,4},{1,6,5} } },
  /* 5 */ { 9, 5, 1, { {1,5,3},{1,6,5},{1,8,5} }, { {1,6,4},{1,6,5},{1,5,3} } },
  /* 6 */ { 8, 3, 2, { {1,5,3},{1,6,5} }, { {1,7,6},{1,7,4} } },
  /* 7 */ { 8, 3, 2, { {1,5,3},{1,6,5},{1,7,2} }, { {1,7,6},{1,7,4},{1,5,5} } },
  /* 8 */ { 9, 3, 3, { {1,8,3},{1,6,2} }, { {1,9,5},{1,7,5} } },
  /* 9 */ { 9, 3, 3, { {1,8,3},{1,6,2},{1,9,5} }, { {1,7,3},{1,7,5},{1,8,5} } },
};

static const unsigned char level_maps [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x3E, 0xE2, 0x8B, 0xA1, 0x85, 0xD1, 0x47, 0x7C, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x0F, 0xF9, 0x81, 0x89, 0xDB, 0x81, 0x91, 0x9F, 0xF0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x7C, 0x46, 0xD3, 0x81, 0x89, 0xE3, 0x2A, 0x22, 0x3E, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xFC, 0x87, 0xB1, 0x85, 0xC1, 0x57, 0x44, 0x7C, 0x00, 0x00, 0x00, 0x00,
};

unsigned char x;
unsigned char y;
int i;
int joy_x = 0;
int joy_y = 0;
int pl_x = 8;
int pl_y = 8;
int pm_x = 0;
int pm_y = 0;
BOX boxes [MAX_BOXES];
TRG targets [MAX_BOXES];
byte cur_level = 0;

void setup() {
  pinMode(RESET_BUTTON, INPUT_PULLUP);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();
  delay(1000);
  while (digitalRead(RESET_BUTTON)) {
    delay(16);
  }
  load_level( cur_level );
}

void load_level(byte l)
{
  // load level definition from program memory to ram
  LVL lvl;
  memcpy_P(&lvl, &levels[l], sizeof(LVL));

  memcpy_P(&level, &level_maps[lvl.l * 16], 16);
  
  pl_x = lvl.px * 8;
  pl_y = lvl.py * 8;
  pm_x = pm_y = 0;
  memcpy(boxes, lvl.bxs, MAX_BOXES*sizeof(BOX));
  memcpy(targets, lvl.tgs, MAX_BOXES*sizeof(TRG));
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(26,26);
  display.println(String("LEVEL " + String(l+1)));
  display.display();
  delay(1000);
}

void loop() {
  if ( level_finished() ) {
    well_done();
    cur_level++;
    if (cur_level == 10) end_game();
    load_level( cur_level );
  }
  
  player_movement();

  display.clearDisplay();

  // map
  draw_level();
  draw_boxes();

  // player
  draw_player();

  // rst btn
  if (!digitalRead(RESET_BUTTON)) {
    load_level( cur_level );
  }
  
  display.display();
}

void end_game()
{
  cur_level = 0;
  display.fillRect(0, 16, 128, 32, WHITE);
  display.setTextSize(2);
  display.setTextColor(BLACK);
  display.setCursor(7,25);
  display.println(String("GAME OVER!"));
  display.display();
  while (digitalRead(RESET_BUTTON)) {
    delay(16);
  }
}

void well_done()
{
  display.fillRect(0, 16, 128, 32, WHITE);
  display.setTextSize(2);
  display.setTextColor(BLACK);
  display.setCursor(7,25);
  display.println(String("WELL DONE!"));
  display.display();
  delay(1000);
}

void player_movement()
{
  joy_x = (analogRead(1) - 512) / 100;
  joy_y = -(analogRead(0) - 512) / 100;

  if (pm_x == 0 && pm_y == 0) {
    if (joy_x > 0) { pm_x = 1; } else 
    if (joy_x < 0) { pm_x = -1; } else
    if (joy_y > 0) { pm_y = 1; } else 
    if (joy_y < 0) { pm_y = -1; }

    // check if colliding with box and move it
    char i = have_box(pl_x/8+pm_x,pl_y/8+pm_y);
    if ( i >= 0 ) {
      if (can_go(boxes[i].x+pm_x,boxes[i].y+pm_y)) {
        boxes[i].x += pm_x;
        boxes[i].y += pm_y;
        boxes[i].mx = - pm_x * 8;
        boxes[i].my = - pm_y * 8;
      }
    }

    // check if player can move
    if (!can_go(pl_x/8+pm_x,pl_y/8+pm_y)) {
      pm_x = 0;
      pm_y = 0;
    }
  }
}

void draw_boxes()
{
  for (i = 0; i < MAX_BOXES; i++) {
    if (boxes[i].a > 0) {
      if ( boxes[i].mx > 0 ) boxes[i].mx--; 
      if ( boxes[i].mx < 0 ) boxes[i].mx++; 
      if ( boxes[i].my > 0 ) boxes[i].my--; 
      if ( boxes[i].my < 0 ) boxes[i].my++; 
      display.drawBitmap(boxes[i].x*8+boxes[i].mx, boxes[i].y*8+boxes[i].my, box_bmp, 8, 8, 1);
    }
    if (targets[i].a > 0) {
      display.drawBitmap(targets[i].x*8, targets[i].y*8, target_bmp, 8, 8, 1);
    }
  }
}

void draw_player()
{
  if ( pm_x != 0 ) {
    pl_x += pm_x;
    if (pl_x % 8 == 0) pm_x = 0;
  }
  if ( pm_y != 0 ) {
    pl_y += pm_y;
    if (pl_y % 8 == 0) pm_y = 0;
  }
  display.drawBitmap(pl_x, pl_y, knight_bmp, 8, 8, 1);
}

bool level_finished()
{
  byte box_cnt = 0;
  byte box_fin = 0;
  for (byte i = 0; i < MAX_BOXES; i++) {
    if (boxes[i].a > 0) {
      box_cnt++;
      if (have_target(boxes[i].x, boxes[i].y)) box_fin++;
    }
  }
  
  return (box_fin == box_cnt);
}

bool have_target(unsigned char x, unsigned char y)
{
  for (byte i = 0; i < MAX_BOXES; i++) {
    if (targets[i].a > 0 && targets[i].x == x && targets[i].y == y) {
      return true;
    }
  }
  return false;
}

// check if there is box on given position, if yes, it's id is returned, if no -1 is returned
char have_box(unsigned char x, unsigned char y)
{
  for (i = 0; i < MAX_BOXES; i++) {
    if (boxes[i].a > 0 && boxes[i].x == x && boxes[i].y == y) {
      return i;
    }
  }
  return -1;
}

// chech if player can go to position (boxes and walls both blocks players movement)
bool can_go(unsigned char x, unsigned char y)
{
  if ( have_box(x, y) >= 0 ) return false; 
  
  i = level[x] & (0x01 << y);
  return (i == 0);
}

void draw_level()
{
  for (x = 0; x < 16; x++) {
    for (y = 0; y < 8; y++) {
      i = level[x] & (0x01 << y);
      if ( i > 0 ) display.drawBitmap(x * 8, y * 8, wall_bmp, 8, 8, 1);
    }
  }
}

