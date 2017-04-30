#include <Arduino.h>
#include <EEPROM.h>
#include <Sleep_n0m1.h>
#include <bitswap.h>
#include <chipsets.h>
#include <color.h>
#include <colorpalettes.h>
#include <colorutils.h>
#include <controller.h>
#include <cpp_compat.h>
#include <dmx.h>
#include <FastLED.h>
#include <fastled_config.h>
#include <fastled_delay.h>
#include <fastled_progmem.h>
#include <fastpin.h>
#include <fastspi.h>
#include <fastspi_bitbang.h>
#include <fastspi_dma.h>
#include <fastspi_nop.h>
#include <fastspi_ref.h>
#include <fastspi_types.h>
#include <hsv2rgb.h>
#include <led_sysdefs.h>
#include <lib8tion.h>
#include <noise.h>
#include <pixelset.h>
#include <pixeltypes.h>
#include <platforms.h>
#include <power_mgt.h>
#include <stdint.h>
#include "tesserhack_levels.c"


// EEPROM registers
#define PIXEL_TEST   0
#define LEVEL_BYTE   1
#define LAYER_BYTE   2
#define X_BYTE       3
#define Y_BYTE       4
#define HAS_KEY      5
#define PAR_BYTE     6

//Constants for pins and calibration

#define DIAL1       A0
#define DIAL2       A1
#define DIAL3       A2
#define LED_PIN     11
#define NUM_LEDS     4
#define BRIGHTNESS   8

//these are the high end analog values to be calibrated against the final board silk
#define POS0        50
#define POS1       320
#define POS2       600
#define POS3       710
#define POS4       810
#define POS5       910
#define POS6      1005
#define POS7      1023

//various terminal screens
#define SCREEN_MAIN  1
#define SCREEN_MAP   2
#define SCREEN_HELP  3

//game states
#define ENTER_STATE  0
#define INVALID_MOVE 1
#define VALID_MOVE   2
#define ORIENT       3
#define TRANSITION   4
#define END_LEVEL    5
#define WIN_GAME     6

//game tiles
#define WALL         0
#define PATH         1
#define KEY          2
#define UP_PORTAL    3
#define DOWN_PORTAL  4
#define DOOR         5
#define TRAP         6
#define HOME         7
#define HIDDEN       8

//game function prototypes
void read_target_position();
void evaluate_position();
void display_feedback();
void update_map();
void end_game();

//Global structs and state variables here
CRGB leds[NUM_LEDS];

// Timer Stuff
int count = 0;
Sleep sleep;
boolean abortSleep;
unsigned long sleepTime;
long previousMillis = 0;
long interval = 10000;
long currentMillis;

uint8_t screen_state = SCREEN_MAIN;
uint8_t game_state = ENTER_STATE;
uint8_t player_position[3] = {0,0,0};  // z,x,y
uint8_t target_position[3] = {0,0,0};  // z,x,y

//par
//uint8_t player_movements = 0;
//const uint8_t level2_pars[] = {2, 3, 4, 4, 5, 6, 7, 7};
//const uint8_t level4_pars[] = {6, 5, 3, 6, 6, 9, 10, 12};

static const uint8_t analog_pins[] = {A2,A1,A0};

extern const uint8_t levels[][8];

byte access_tile(uint8_t layer, uint8_t row, uint8_t column)
{
  return (uint8_t)pgm_read_byte(&levels[(layer*8)+column][row]);
}


//=========================
//  GAME LOGIC
//=========================

void read_target_position()
{
  int reading = 0;
  for (uint8_t i=0; i<3; i++)
  {
    reading = analogRead(analog_pins[i]);
    if (reading < POS0){
      target_position[i] = 7;
    }
    else if (reading < POS1 && reading >= POS0){
      target_position[i] = 6;
    }
    else if (reading < POS2 && reading >= POS1){
      target_position[i] = 5;
    }
    else if (reading < POS3 && reading >= POS2){
      target_position[i] = 4;
    }
    else if (reading < POS4 && reading >= POS3){
      target_position[i] = 3;
    }
    else if (reading < POS5 && reading >= POS4){
      target_position[i] = 2;
    }
    else if (reading < POS6 && reading >= POS5){
      target_position[i] = 1;
    }
    else if (reading < POS7 && reading >= POS6){
      target_position[i] = 0;
    }
  }
}

void make_valid_move()
{
  player_position[0] = target_position[0];
  player_position[1] = target_position[1];
  player_position[2] = target_position[2];
  write_target_position();
  display_feedback();
}

void write_target_position()
{
  EEPROM.write(2,player_position[0]);
  EEPROM.write(3,player_position[1]);
  EEPROM.write(4,player_position[2]);
}

void display_feedback() // needs functions from Jay, consider merging with evaluate_position()
{
  switch (game_state)
  {
    case ENTER_STATE:
      enter_animation();
      break;

    case VALID_MOVE:
      // valid_animation();
      break;

    case ORIENT:
      display_orientation();
      break;

    case TRANSITION:
      transition_animation();
      break;

    case END_LEVEL:
      level_animation();
      break;

    case WIN_GAME:
      complete_animation();
      break;

    case INVALID_MOVE:
    default:
      invalid_animation();
      break;

  }
}

void evaluate_position()
{
  if (EEPROM.read(PIXEL_TEST)) // High byte means we have not found 0x8 yet
  {
    if ((target_position[0] == 0) && (target_position[1] == 6) && (target_position[2] == 7)) // when dials are at 0X8 enter the game
    {
      game_state = ENTER_STATE;
      EEPROM.write(PIXEL_TEST, 0x0); // Clear byte 0 to indicate game has begun
      player_position[0] = 0;
      player_position[1] = 6;
      player_position[2] = 7;
      write_target_position();
    }
    else
    {
      game_state = INVALID_MOVE;

    }
    display_feedback();
  }
  else
  {
    // we're in the cube: EEPROM[6]==0
    switch (game_state){
      case ENTER_STATE:
        display_feedback();
        game_state = VALID_MOVE;
        break;
      case VALID_MOVE:
        make_valid_move();
        game_state = ORIENT;
        break;
      case ORIENT:  // most of our logic happens here
        display_feedback();
        // jumping around is invalid
        if (((target_position[0] > (player_position[0] + 1)) || (target_position[0] < (player_position[0] - 1))) ||
            ((target_position[1] > (player_position[1] + 1)) || (target_position[1] < (player_position[1] - 1))) ||
            ((target_position[2] > (player_position[2] + 1)) || (target_position[2] < (player_position[2] - 1))))
        {
          game_state = INVALID_MOVE;
          break;
        }

        // Can't move in x or y when moving in z
        if (((target_position[0] > (player_position[0] + 1)) || (target_position[0] < (player_position[0] - 1))) &&
            ((target_position[1] != player_position[1]) || (target_position[2] != player_position[2])))
        {
          game_state = INVALID_MOVE;
          break;
        }

        // Can't move in x or z when moving in y
        if (((target_position[1] > (player_position[1] + 1)) || (target_position[1] < (player_position[1] - 1))) &&
            ((target_position[0] != player_position[0]) || (target_position[2] != player_position[2])))
        {
          game_state = INVALID_MOVE;
          break;
        }

        // Can't move in y or z when moving in x
        if (((target_position[2] > (player_position[2] + 1)) || (target_position[2] < (player_position[2] - 1))) &&
            ((target_position[0] != player_position[0]) || (target_position[1] != player_position[1])))
        {
          game_state = INVALID_MOVE;
          break;
        }

        // Can't move in z-axis lower than 0 or higher than 63.
        if (((target_position[0] > (player_position[0] + 1)) || (target_position[0] < (player_position[0] - 1))) &&
            ((target_position[0] > 63) || (target_position[0] < 0)))
        {
          game_state = INVALID_MOVE;
          break;
        }

        switch (access_tile(target_position[0],target_position[1],target_position[2]))
          // should only get here if we're "in range"
        {
          case 1:
          case 7:
          case 8:
            game_state = VALID_MOVE;
            make_valid_move();
            game_state = ORIENT;
            break;
          case 5:
            if (EEPROM.read(7)){  // we have a key
              game_state = VALID_MOVE;
              make_valid_move();
              EEPROM.write(7,0x0);  // we used the key so clear the flag
              game_state = ORIENT;
              break;
            }
            else
            {
              game_state = INVALID_MOVE;
              Serial.println("Problem6");
              break;
            }
          case 2:
            EEPROM.write(7,0x1); // set the flag that we have a key
            game_state = VALID_MOVE;
            make_valid_move();
            game_state = ORIENT;
            break;
          case 3:
            if (access_tile(player_position[0], player_position[1], player_position[2]) == DOWN_PORTAL) {
              make_valid_move();
              display_feedback();
              EEPROM.write(LAYER_BYTE,target_position[0]);
              game_state = TRANSITION;
            } else {
              game_state = VALID_MOVE;
            }
            break;
          case 4:
            if (access_tile(player_position[0], player_position[1], player_position[2]) == UP_PORTAL) {
              make_valid_move();
              display_feedback();
              EEPROM.write(LAYER_BYTE,target_position[0]);
              game_state = TRANSITION;
            } else {
              game_state = VALID_MOVE;
            }
            break;
          case 0:  // WALL
          default:
            game_state = INVALID_MOVE;
            break;
        }
        break;
      case TRANSITION:
        display_feedback();
        game_state = ORIENT;
        break;
      case INVALID_MOVE:
      default:
        display_feedback();
        // may implement par counting here
        break;
    }
  }
}

//===========================
//  END GAME LOGIC
//===========================

//=========================
//  POWER CONTROL
//=========================

void turnoff(){
  for(int dot=0; dot<NUM_LEDS;dot++){
    leds[dot] = CRGB::Black;
    FastLED.clear();
    FastLED.show();
  }
  FastLED.show();
}

//=========================
//  END POWER CONTROL
//=========================

//=========================
//  LED SHIT
//=========================

// Tip for going up
void up(){
  FastLED.setBrightness(8);
  for(int dot = 0; dot < NUM_LEDS; dot++) {
    leds[dot] = CRGB::White;
    FastLED.show();
    // clear this led for the next time around the loop
    leds[dot] = CRGB::Black;
    delay(300);
  }
  FastLED.show();
}

// Tip for going down
void down(){
  FastLED.setBrightness(8);
  for(int dot = 4; dot >= 0; dot--) {
    leds[dot] = CRGB::White;
    FastLED.show();
    // clear this led for the next time around the loop
    leds[dot] = CRGB::Black;
    delay(300);
  }
  FastLED.show();
}

// Tip for going left
void left(){
  FastLED.setBrightness(8);
  for(int i=0;i<2;i++){
    leds[3] = CRGB::White;
    leds[1] = CRGB::White;
    FastLED.show();
    delay(1000);
    FastLED.clear();
    leds[2] = CRGB::White;
    FastLED.show();
    delay(1000);
    FastLED.clear();
  }
  FastLED.show();
}

// Tip for going right
void right(){
  FastLED.setBrightness(8);
  for(int i=0;i<2;i++){
    leds[2] = CRGB::White;
    leds[0] = CRGB::White;
    FastLED.show();
    delay(1000);
    FastLED.clear();
    leds[1] = CRGB::White;
    FastLED.show();
    delay(1000);
    FastLED.clear();
  }
  FastLED.show();
}

// Got dat key
void keymaster(){
  FastLED.setBrightness(8);
  for(int i=0; i<4; i++){
    leds[i] = CRGB::Orange;
    FastLED.show();
  }
  delay(1500);
  FastLED.clear();
  FastLED.show();
}

// Success Level
void transition_animation(){
  FastLED.setBrightness(70);

  for(int i=0;i<61;i++){
    int pos = random16(NUM_LEDS);
    leds[pos] = CHSV(i*4,i*4,i*4);
    FastLED.show();
    delay(70);
  }
  leds[2] = CRGB::Black;
  FastLED.show();
  delay(150);
  leds[0] = CRGB::Black;
  FastLED.show();
  delay(150);
  leds[1] = CRGB::Black;
  FastLED.show();
  delay(150);
  leds[3] = CRGB::Black;
  FastLED.show();

  FastLED.clear();
  FastLED.show();
}

// Succes Cube
void level_animation(){
  FastLED.setBrightness(70);

  for(int i=0;i<122;i++){
    int pos = random16(NUM_LEDS);
    leds[pos] = CHSV(i*2,i*2,i*2);
    FastLED.show();
    delay(70);
  }

  for(int dot=0; dot<NUM_LEDS;dot++){
    leds[dot] = CRGB::Blue;
    FastLED.show();
  }
  delay(1500);
  for(int dot=0; dot<NUM_LEDS;dot++){
    leds[dot] = CRGB::Black;
    FastLED.show();
  }
  delay(1500);
  for(int dot=0; dot<NUM_LEDS;dot++){
    leds[dot] = CRGB::Blue;
    FastLED.show();
  }
  delay(1500);

  leds[2] = CRGB::Black;
  FastLED.show();
  delay(150);
  leds[0] = CRGB::Black;
  FastLED.show();
  delay(150);
  leds[1] = CRGB::Black;
  FastLED.show();
  delay(150);
  leds[3] = CRGB::Black;
  FastLED.show();

  FastLED.clear();
  FastLED.show();
}

// Success Game
void complete_animation(){
  FastLED.setBrightness(70);

  for(int finish=0;finish<4;finish++){
    for(int i=0;i<1000;i++){
      leds[random(4)]=CHSV(random(255),random(255),random(255));
      FastLED.show();
    }
    for(int counter=0;counter<4;counter++){
      for(int i=0; i<128; i++){
        for(int x=0;x<4;x++){
          leds[x]=CHSV(i*2,i*2,i*2);
          FastLED.show();
        }
      }
      delay(100);
    }
  }

  FastLED.clear();
  FastLED.show();
}

void enter_animation(){

  for(int dot=0; dot<NUM_LEDS;dot++){
    leds[dot] = CRGB::Blue;
  }

  for(int i=0;i<255;i++){
    FastLED.setBrightness(i);
    FastLED.show();
    delay(5);
  }
  for(int i=255;i>0;i--){
    FastLED.setBrightness(i);
    FastLED.show();
    delay(5);
  }
  for(int i=0;i<255;i++){
    FastLED.setBrightness(i);
    FastLED.show();
    delay(5);
  }
  for(int i=255;i>0;i--){
    FastLED.setBrightness(i);
    FastLED.show();
    delay(5);
  }

  FastLED.clear();
  FastLED.show();
}

void invalid_animation(){
  for(int dot=0; dot<NUM_LEDS;dot++){
    leds[dot] = CRGB::Red;
    FastLED.show();
  }
  delay(3000);
  for(int dot=0; dot<NUM_LEDS;dot++){
    leds[dot] = CRGB::Black;
    FastLED.show();
  }
  delay(1000);
  for(int dot=0; dot<NUM_LEDS;dot++){
    leds[dot] = CRGB::Red;
    FastLED.show();
  }
  delay(3000);
  for(int dot=0; dot<NUM_LEDS;dot++){
    leds[dot] = CRGB::Black;
    FastLED.show();
  }
  delay(3000);
  FastLED.clear();
  FastLED.show();
}

// Colors lights based on what surrounds the player position
void display_orientation(){
  FastLED.setBrightness(70);

  int z_pos,x_pos,y_pos;
  int above, below, left, right;

  z_pos = target_position[0];
  x_pos = target_position[1];
  y_pos = target_position[2];

  if(y_pos==7){
    below = 0;
  } else {
    below = access_tile(z_pos,x_pos, (y_pos+1));
  }

  if(y_pos==0) {
    above = 0;
  } else {
    above = access_tile(z_pos,x_pos,(y_pos-1));
  }

  if(x_pos==0){
    left = 0;
  } else {
    left = access_tile(z_pos,(x_pos-1),y_pos);
  }

  if(x_pos==7) {
    right = 0;
  } else {
    right = access_tile(z_pos,(x_pos+1),y_pos);
  }

  int surroundings[] = {below, right, left, above};

  /*
   * 0=Wall
   * 1=Open Space
   * 2=Key
   * 3=Portal
   * 4=Locked Portal
   * 5=Door
   * 6=Down Portal
   * 7=Entry Portal
  */
  for(int i=0;i<4;i++){
    switch(surroundings[i]){
      case 0:
        leds[i] = CRGB::Black;
        FastLED.show();
//        delay(2000);
//        FastLED.clear();
        break;
      case 1:
      case 7:
      case 8:
        leds[i] = CRGB::White;
        FastLED.show();
//        delay(2000);
//        FastLED.clear();
        break;
      case 2:
        leds[i] = CRGB::Orange;
        FastLED.show();
//        delay(2000);
//        FastLED.clear();
        break;
      case 3:
        leds[i] = CRGB::Purple;
        FastLED.show();
        break;
      case 4:
        for(int i=0;i<3;i++){
          leds[i] = CRGB::Purple;
          FastLED.show();
//          delay(500);
//          FastLED.clear();
//          delay(500);
        }
        break;
      case 5:
        for(int x=0;x<3;x++){
          leds[i] = CRGB::White;
          FastLED.show();
//          delay(500);
//          FastLED.clear();
        }
        break;
      case 6:
        leds[i] = CRGB::Red;
        FastLED.show();
//        delay(2000);
//        FastLED.clear();
        break;
    }
  }
  FastLED.clear();
  FastLED.show();
}

//=========================
//  END LED SHIT
//=========================

//=================================
//  Serial Functions and globals
//==================================

const char* dial2[] = {"A", "B", "C", "D", "E", "F", "X", "Z"};

void Serial_display_banner()
{
  if (Serial){
    Serial.flush();
    delay(500);
    Serial.print("\033[2J"); //VT100 clear
    Serial.print("\033[H");  //home position
    //ascii art banner string throwing it in progmem
    Serial.print(F("     +___________+\r\n    /:\\         ,:\\\r\n   / : \\       , : \\\r\n  /  :  \\     ,  :  \\\r\n /   :   +-----------+      _                          _   _            _\r\n+....:../:...+   :  /|     | |_ ___  ___ ___  ___ _ __| | | | __ _  ___| | __\r\n|\\   +./.:...`...+ / |     | __/ _ \\/ __/ __|/ _ \\ '__| |_| |/ _` |/ __| |/ /\r\n| \\ ,`/  :   :` ,`/  |     | ||  __/\\__ \\__ \\  __/ |  |  _  | (_| | (__|   <\r\n|  \\ /`. :   : ` /`  |      \\__\\___||___/___/\\___|_|  |_| |_|\\__,_|\\___|_|\\_\\\r\n| , +-----------+  ` |\r\n|,  |   `+...:,.|...`+\r\n+...|...,'...+  |   /\r\n \\  |  ,     `  |  /\r\n  \\ | ,       ` | /\r\n   \\|,         `|/\r\n    +___________+\r\n\r\n"));
    Serial.println(F("\t\tThe Tesserhack is no ordinary 3D space.\r\n Use your badge to navigate the twists, turns, ins and outs of 8 cubes. \r\n\r\n"));
  }
}

void Serial_display_help()
{
  if (Serial){
    Serial.print("\033[40;0H");  // always start at row 40
    Serial.print("\t");
    for (uint8_t i = 0; i<56; i++)
      Serial.print("#");
    Serial.println();

    Serial.print("\t#");
    for (uint8_t i = 0; i<54; i++)
      Serial.print(" ");
    Serial.println("#");

    Serial.println(F("\t# LED: [WHITE - PATH] [PURPLE - PORTAL] [ORANGE - KEY] #"));
    Serial.println(F("\t#   [H]elp - Display Anywhere       [R]efreshes Map   #"));

    Serial.print("\t#");
    for (uint8_t i = 0; i<54; i++)
      Serial.print(" ");
    Serial.println("#");
    Serial.print("\t");
    for (uint8_t i = 0; i<56; i++)
      Serial.print("#");
    Serial.println();
  }
}

void Serial_display_bar()
{
  if (Serial){
    Serial.print("\t\t");
    for (uint8_t i = 0; i<49; i++)
      Serial.print("#");
    Serial.println();
  }
}

void Serial_display_cell()
{
  if (Serial){
    Serial.print("\t\t");
    for (uint8_t i=0; i<8; i++)
      Serial.print("#     ");
    Serial.println("#");
  }
}

void Serial_populate_row(const uint8_t row_values[])
{
  // Implemented hidden traversables. Lights will indicate what a tile contains.
  if (Serial){
    Serial.print("\t\t#");
    for (uint8_t i=0; i<8; i++)
    {
      Serial.print("  ");
      switch ((uint8_t)pgm_read_byte(&(row_values[i]))) {
        case 0:  // wall
          Serial.print("#");
          break;
        case 5:  // door
          if (EEPROM.read(HAS_KEY)) //if we don't have a key door is a wall
            Serial.print("#");
          else
            Serial.print(" ");  // make door a path
          break;
        case 1:  // keys, portals, and traps are "invisible"
        case 2:
        case 3:
        case 4:
        case 6:
        case 8:
          Serial.print(" ");
          break;
          /*case 2:
             Serial.print ("K");
             break;
          case 3:
             Serial.print("P");
             break;
          case 4:
             Serial.print("L");
             break;
          case 5:
             Serial.print("D");
             break;
          case 6:
             Serial.print("T");
             break; */
        case 7:
          Serial.print("^");
          break;
        default:
          break;
      }
      Serial.print("  #");
    }
  }
}

void Serial_show_map()
{
  Serial.println(F("\t\t   A     B     C     D     E     F     X     Z"));
  for (uint8_t i=0; i<8; i++)
  {
    Serial_display_bar();
    Serial_display_cell();
    Serial_populate_row(levels[(EEPROM.read(LAYER_BYTE)*8)+i]);
    Serial.print(" ");
    Serial.println(i+1);
    Serial_display_cell();
  }
  Serial_display_bar();
  // VT100 Escape sequence Place cursor at current tile on player's achieved position
  Serial_display_footer();
  Serial.print("\033[");
  Serial.print((4*(EEPROM.read(Y_BYTE)+1)));
  Serial.print(";");
  Serial.print(20 +(6*(EEPROM.read(X_BYTE))));
  Serial.print("H");
}

void Serial_show_menu()
{
  if (Serial){
    delay(500);
    Serial.println(F("\t\t\t\t-=[Main Menu]=-\r\n\t[M]ap View\t\t[H]elp Menu\t\tBac[K]\r\n\r\n")); //115
  }
}

void Serial_display_footer()
// displays position as indicated on the dial and other games state info
{
  if (Serial){
    Serial.print("\033[36;0H");
    Serial.print(F("|[L3V3L: "));
    Serial.print(EEPROM.read(LEVEL_BYTE)+1);
    Serial.print(F("] | [Layer: "));
    Serial.print((EEPROM.read(LAYER_BYTE)) ? (EEPROM.read(LAYER_BYTE)+1) : 0);
    Serial.print(F("] | [Origin: "));
    Serial.print(dial2[ EEPROM.read(X_BYTE)]); // convert this to letters
    Serial.print(" ");
    Serial.print(EEPROM.read(Y_BYTE)+1);
    Serial.print(F("] | ["));
    Serial.print(EEPROM.read(HAS_KEY) ? ("KEY") : ("   "));
    Serial.println(F("] | [Serial:Connected] |  VT100 |"));
  }
}

byte Serial_handle_prompt()
{
  if (Serial){
    Serial.print("Enter Menu Choice:" );
  }
  while (!Serial.available()){}
  return Serial.read();
}

void Serial_menu_handler()
{
  char type = 0x0;  // serial input byte
  char* buff = 0x0; // byte buff

  if (Serial)
  {
    Serial.flush();
    switch(screen_state){
      case SCREEN_MAIN:
        Serial_display_banner();
        Serial_show_menu();
        type = Serial_handle_prompt();
        Serial.println(type);
        break;
      case SCREEN_MAP:  // To-add if we haven't entered, do not display the map yet, print some teaser.
        Serial.print("\033[2J");
        Serial.print("\033[H");  //home position
        Serial_show_map();
        // comment out below to have this drop into the loop rather than hold on input
        while(!Serial.available()){}
        type = Serial.read();
        break;
      case SCREEN_HELP:
        Serial_display_help();
        while(!Serial.available()){}
        type = Serial.read();
        break;
      default:
        Serial_display_banner();
        break;
    }
    Serial.println(type);  // display their selection
    switch (type){
      case 'q':
      case 'Q':
        screen_state = SCREEN_MAIN;
        Serial.print("\033[2J");
        break;
      case '\033':
        Serial.flush();
        Serial.print("\033[55;0H");
        Serial.println(F("Escape is impossible"));
        break;
      case 'm':
      case 'M':
        screen_state = SCREEN_MAP;
        Serial.flush();          // We redraw the map each time from top
        Serial.print("\033[2J"); // VT100 clear
        Serial.print("\033[H");  // home position
        break;
      case 'h':
      case 'H':
        screen_state = SCREEN_HELP;
        break;
      case 'k':
      case 'K':
        screen_state= SCREEN_MAIN;
        break;
      case 'r':
      case 'R':
        if(screen_state == SCREEN_MAP){
          read_target_position();
          // may add logic checks here if necessary, only call the functions we have, don't re implement
          screen_state= SCREEN_MAP;
          break;
        }
        else
        {
          break;
        }
      case '?':
        Serial.flush();
        Serial.print("\033[55;0H");
        Serial.println(F("Options: [MHKP?]"));
        break;
      default:
        Serial.flush();
        Serial.print("\033[55;0H");
        Serial.println(F("Invalid choice!"));
        break;
    }  // switch
  }  // if read
}

//==========================
// End Serial Functions
//==========================

void setup() {
  pinMode(LED_PIN,OUTPUT);
  LEDS.addLeds<WS2812B,LED_PIN,GRB>(leds,NUM_LEDS);
  LEDS.setBrightness(BRIGHTNESS);

  up();
  down();
  delay(100);

  // For testing
  EEPROM.write(LAYER_BYTE,0);
  EEPROM.write(X_BYTE,6);
  EEPROM.write(Y_BYTE,7);
  EEPROM.write(HAS_KEY,0x0);

  // For testing purposes, we will set PIXEL_TEST to 1 every time we init to ensure that we can switch it to 2
  EEPROM.write(PIXEL_TEST, 1);

  abortSleep = false;
  sleepTime = 1000;

  //pull earned position from memory
  player_position[0] = EEPROM.read(LEVEL_BYTE);
  player_position[1] = EEPROM.read(X_BYTE);
  player_position[2] = EEPROM.read(Y_BYTE);

  //read dial settings
  read_target_position();

  // block for initial position before home (until they find 0x8 blink invalid)
  if (EEPROM.read(6) == 0x0)
  {
    game_state = ENTER_STATE;
  }
  screen_state = SCREEN_MAIN;

//  //This is a pixel POST on first boot only
//  if (EEPROM.read(PIXEL_TEST) == 0)
//  {
//    for(uint8_t pixel = 0; pixel < NUM_LEDS; pixel++) {
//      leds[pixel] = CRGB::White;
//       FastLED.show();
//       leds[pixel] = CRGB::Black;
//       delay(500);
//     }
//      EEPROM.write(PIXEL_TEST,1);
//   }
//
  Serial.setTimeout(500);
  Serial.begin(9600);
}

void loop()
{
  if(Serial){
    read_target_position();
    evaluate_position();
    Serial_menu_handler();
  } else {
    read_target_position();
    evaluate_position();
    abortSleep = true;
    /*
    // Next three if conditionals are watching to see if anything changes on X, Y, and Z potentiometers
    if (EEPROM.read(LAYER_BYTE) != target_position[0]){
      abortSleep = true;
      // Serial.println("Z Changed");
      previousMillis = currentMillis;
    }
  
    if (EEPROM.read(X_BYTE) != target_position[1]){
      abortSleep = true;
      // Serial.println("X Changed");
      previousMillis = currentMillis;
    }
  
    if (EEPROM.read(Y_BYTE)!=target_position[2]){
      abortSleep = true;
      // Serial.println("Y Changed");
      previousMillis = currentMillis;
    }
    */
  
    if(abortSleep){
      currentMillis = millis();
    }

    if(currentMillis - previousMillis > interval){
      previousMillis = currentMillis;
      abortSleep = false;
      FastLED.clear();
      FastLED.show();
    }

    sleep.pwrDownMode(); //set sleep mode
    sleep.sleepDelay(sleepTime,abortSleep); //sleep for: sleepTime
    Serial_menu_handler();
  }

  //   Serial.println("OUT OF HANDLER"); // for debug
} // loop()
