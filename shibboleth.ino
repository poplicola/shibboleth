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

#define NUM_LEDS 4
#define DATA_PIN 11

CRGB leds[NUM_LEDS];

int ana0, ana1, ana2;
int breathecheck;

#define POS0 50
#define POS1 320
#define POS2 600
#define POS3 710
#define POS4 810
#define POS5 910
#define POS6 1005
#define POS7 1023

uint8_t player_position[3] = {0,0,0}; // z,x,y
uint8_t nav_position[3] = {0,0,0};  //z,x,y
uint8_t levels[2][8][8] = {  {{0,0,0,0,1,0,0,0},
                             {0,0,0,2,1,3,0,0},
                             {0,0,0,4,1,5,0,0},
                             {0,0,0,0,6,0,0,0},
                             {0,0,0,0,0,0,0,0},
                             {0,0,0,0,0,0,0,0},
                             {0,0,0,0,0,0,0,0},
                             {0,0,0,0,0,0,0,0}},
                             
                             {{0,0,0,0,1,0,0,0},
                             {0,0,0,2,1,3,0,0},
                             {0,0,0,4,1,5,0,0},
                             {0,0,0,0,6,0,0,0},
                             {0,0,0,0,0,0,0,0},
                             {0,0,0,0,0,0,0,0},
                             {0,0,0,0,0,0,0,0},
                             {0,0,0,0,0,0,0,0}}};////levels reference array.

// Timer Stuff
int count = 0;
Sleep sleep;
boolean abortSleep;
unsigned long sleepTime;
long previousMillis=0;
long interval=10000;
long currentMillis;

// (1) navigation (2) par (3) keys (4) par + keys (5) looping between levels (6) open spaces (7) larger / harder (8) hidden doors

static const uint8_t analog_pins[] = {A2,A1,A0};

void setup() {

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  abortSleep = false;
  sleepTime = 250;
  
  // up();
  // down();
  // left();
  // right();
  // keymaster();
  // slevel();
  // scube();
  // sgame();
 
  // breathe();

  // Commented these functions out because I'm testing effects
  /* 
   *  read_nav_position();
   *  display_orientation();
   */

  // Once breathe, indicate downward. Test for x7. Show portal behind you.
  // Repeat for x6 and x5. Walls surround you except going forward.
  // Is turning going to be done from the player perspective, or from the perspective of looking down at the puzzle (probably looking down)?
  // Turn to F5 (arrow pointing that direction), then show blinking portal to left
  // E5 triggers next level animation
}

void loop() {
  
  read_nav_position();

  if (EEPROM.read(3)!=nav_position[1]){
    EEPROM.write(3,nav_position[1]);
    abortSleep = true;
    Serial.println("X Changed");
    previousMillis=currentMillis;
  }
  
  if (EEPROM.read(4)!=nav_position[2]){
    EEPROM.write(4,nav_position[2]);
    abortSleep = true;
    Serial.println("X Changed");
    previousMillis=currentMillis;
  }
  
  if(abortSleep){
    currentMillis=millis();
    down();
  }
  
  Serial.print("SleepCycle: ");
  Serial.println(previousMillis);
  Serial.print("currentmillis: ");
  Serial.println(currentMillis);
  
  if(currentMillis-previousMillis>interval){
    previousMillis=currentMillis;
    abortSleep = false;
  }

  sleep.pwrDownMode(); //set sleep mode
  sleep.sleepDelay(sleepTime,abortSleep); //sleep for: sleepTime
  
}

// Turn all lights off
void turnoff(){
  for(int dot=0; dot<NUM_LEDS;dot++){
    leds[dot] = CRGB::Black;
    FastLED.clear();
    FastLED.show();
  }
  FastLED.show();
}

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
  for(int i=0;i<4;i++){
    leds[i] = CRGB::Orange;
    FastLED.show();
  }
  delay(1500);
  FastLED.clear();
  FastLED.show();
}

// Success Level
void slevel(){
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
void scube(){
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
void sgame(){
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

// Checks when the badge is first turned on to see if the player has set the pot to 0, x, and 8. If not, red flashy. If so, blue pulsey.
void breathe(){
  ana0 = analogRead(A0);
  ana1 = analogRead(A1);
  ana2 = analogRead(A2);
  
  breathecheck=EEPROM.read(6);

  // If you haven't yet passed the initial 0x8 test....
  if(breathecheck!=1){
    if(ana2>895 && ana1>128 && ana1<400 && ana0<128){
      EEPROM.write(6, 1);
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
    } else {
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
    }
  }
  FastLED.clear();
  FastLED.show();
}

// Colors lights based on what surrounds the player position
void display_orientation(){
  FastLED.setBrightness(8);

  int z_pos,x_pos,y_pos;
  int above, below, left, right;

  for(int i=0;i<3;i++){
    player_position[i]=nav_position[i];
  }

  z_pos = player_position[0];
  x_pos = player_position[1];
  y_pos = player_position[2];

  if(y_pos==7){
    below = 0;
  } else {
    below = levels[z_pos][y_pos+1][x_pos];
  }
  
  if(y_pos==0) {
    above = 0;
  } else {
    above = levels[z_pos][y_pos-1][x_pos];
  }

  if(x_pos==0){
    left = 0;
  } else {
    left = levels[z_pos][y_pos][x_pos-1];
  }
  
  if(x_pos==7) {
    right = 0;
  } else {
    right = levels[z_pos][y_pos][x_pos+1];
  }

  int surroundings[] = {below, right, left, above};
  
  /*for(int i=0;i<4;i++){
    Serial.print(i);
    Serial.print(": ");
    Serial.println(surroundings[i]);
    delay(1000);
  }*/
  
  /*
   * 0=Wall
   * 1=Open Space
   * 2=Key
   * 3=Portal
   * 4=Locked Portal
   * 5=Door
   * 6=Trap
  */
  for(int i=0;i<4;i++){
    switch(surroundings[i]){
      case 0:
        leds[i] = CRGB::Black;
        FastLED.show();
        delay(2000);
        FastLED.clear();
        break;
      case 1:
        leds[i] = CRGB::White;
        FastLED.show();
        delay(2000);
        FastLED.clear();
        break;
      case 2:
        leds[i] = CRGB::Orange;
        FastLED.show();
        delay(2000);
        FastLED.clear();
        break;
      case 3:
        leds[i] = CRGB::Purple;
        FastLED.show();
        delay(2000);
        FastLED.clear();
        break;
      case 4:
        for(int i=0;i<3;i++){
          leds[i] = CRGB::Purple;
          FastLED.show();
          delay(500);
          FastLED.clear();
          delay(500);
        }
        break;
      case 5:
        for(int x=0;x<3;x++){
          leds[i] = CRGB::White;
          FastLED.show();
          delay(500);
          FastLED.clear();
        }
        break;
      case 6:
        leds[i] = CRGB::Red;
        FastLED.show();
        delay(2000);
        FastLED.clear();
        break;
    }
  }
  FastLED.clear();
  FastLED.show();
}

void read_nav_position(){
  int reading = 0;
  for (uint8_t i=0; i<3; i++)
  {
    reading = analogRead(analog_pins[i]);
    if (reading < POS0){
      nav_position[i] = 7;
    }
    else if (reading < POS1 && reading >= POS0){
      nav_position[i] = 6;
    }
    else if (reading < POS2 && reading >= POS1){
      nav_position[i] = 5;
    }
    else if (reading < POS3 && reading >= POS2){
      nav_position[i] = 4;
    }
    else if (reading < POS4 && reading >= POS3){
      nav_position[i] = 3;
    }
    else if (reading < POS5 && reading >= POS4){
      nav_position[i] = 2;
    }
    else if (reading < POS6 && reading >= POS5){
      nav_position[i] = 1;
    }
    else if (reading < POS7 && reading >= POS6){
      nav_position[i] = 0;
    }
    
  }
}
