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

//game states 
#define ENTER_STATE  0
#define INVALID_MOVE 1
#define VALID_MOVE   2
#define ORIENT       3
#define TRANSITION   4
#define END_LEVEL    5
#define WIN_GAME     6

// EEprom Shit
#define PIXEL_TEST   0
#define LEVEL_BYTE   1
#define LAYER_BYTE   2
#define X_BYTE       3
#define Y_BYTE       4
#define HAS_KEY      5

//game tiles
#define WALL 0
#define PATH 1
#define KEY  2
#define UP_PORTAL 3
#define DOWN_PORTAL 4
#define DOOR 5
#define TRAP 6

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

uint8_t game_state = ENTER_STATE;
uint8_t target_position[3] = {0,0,0};  //z,x,y
const uint8_t levels[][8] PROGMEM =   
{  
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,3,0},
{0,0,0,0,0,0,1,0},
{0,0,0,0,0,0,7,0},

{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,3,0,0},
{0,0,0,0,0,1,0,0},
{0,0,0,0,0,1,7,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},

{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,3,1,0,0,0},
{0,0,0,0,1,7,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},

{3,1,0,0,0,0,0,0},
{0,1,0,0,0,0,0,0},
{0,1,1,7,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},

{7,0,0,0,0,0,0,0},
{1,0,0,0,0,0,0,0},
{1,0,0,0,0,0,0,0},
{1,0,0,0,0,0,0,0},
{1,0,0,0,0,0,0,0},
{1,0,0,0,0,0,0,0},
{1,0,0,0,0,0,0,0},
{3,0,0,0,0,0,0,0},

{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,1,3,0,0,0,0},
{0,0,1,0,0,0,0,0},
{7,1,1,1,0,0,0,0},

{0,0,3,1,1,0,0,0},
{0,0,1,0,1,0,0,0},
{0,0,1,1,1,0,0,0},
{0,0,0,0,1,0,0,0},
{0,0,0,1,1,0,0,0},
{0,0,0,7,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},

{0,0,7,1,1,0,1,1},
{0,0,0,0,1,0,1,0},
{0,0,0,0,1,1,0,1},
{0,0,0,0,0,1,0,1},
{0,0,0,0,0,1,1,1},
{0,0,0,0,0,0,0,1},
{0,0,0,0,0,0,0,1},
{0,0,0,0,0,0,3,1}                                      
};

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

//  write_target_position();
//  player_position[0] = EEPROM.read(2);
//  player_position[1] = EEPROM.read(3);
//  player_position[2] = EEPROM.read(4);
  //read dial settings
  //read_target_position();

  up();
  down();
  left();
  right();
  delay(100);
  
  // For testing purposes, we will set PIXEL_TEST to 1 every time we init to ensure that we can switch it to 2
  EEPROM.write(PIXEL_TEST,1);
  
  if(EEPROM.read(PIXEL_TEST) == 1){
    read_target_position();
    breathe();
  } else if(EEPROM.read(PIXEL_TEST) == 2) {
    Serial.println("SUCCESS!");
  }

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  abortSleep = false;
  sleepTime = 1000;


  Serial.setTimeout(500);
  Serial.begin(9600);
  delay(500);

}

void loop() {

  read_target_position();
  // Serial.println(EEPROM.read(PIXEL_TEST));
  
/*
  Serial.println(EEPROM.read(3));
  Serial.println(EEPROM.read(4));
  Serial.print("Pos 1: ");
  Serial.println(target_position[1]);
  Serial.print("Pos 2: ");
  Serial.println(target_position[2]);
*/

  // Next three if conditionals are watching to see if anything changes on X, Y, and Z potentiometers
  if (EEPROM.read(LAYER_BYTE)!=target_position[0]){
    EEPROM.write(LAYER_BYTE,target_position[0]);
    abortSleep = true;
    // Serial.println("Z Changed");
    previousMillis=currentMillis;
  }

  if (EEPROM.read(X_BYTE)!=target_position[1]){
    EEPROM.write(X_BYTE,target_position[1]);
    abortSleep = true;
    // Serial.println("X Changed");
    previousMillis=currentMillis;
  }
  
  if (EEPROM.read(Y_BYTE)!=target_position[2]){
    EEPROM.write(Y_BYTE,target_position[2]);
    abortSleep = true;
    // Serial.println("Y Changed");
    previousMillis=currentMillis;
  }

  // This is where the code runs when we are awake
  if(abortSleep){
    if(EEPROM.read(PIXEL_TEST) == 1){
      read_target_position();
      breathe();
    } else if(EEPROM.read(PIXEL_TEST) == 2){
      Serial.println("SUCCESS!");
      currentMillis=millis();
      display_orientation();
    }
  }

  // If timer goes above the interval, go to sleep
  if(currentMillis-previousMillis>interval){
    previousMillis=currentMillis;
    abortSleep = false;
    FastLED.clear();
  }

  //sleep.pwrDownMode(); //set sleep mode
  //sleep.sleepDelay(sleepTime,abortSleep); //sleep for: sleepTime
  
}

byte access_tile(uint8_t layer, uint8_t row, uint8_t column)
{
  return (uint8_t)pgm_read_byte(&levels[(layer*8)+row][column]);
}

//display_functions

void Serial_display_bar(){
  if (Serial){
    Serial.print("\t\t");
    for (uint8_t i = 0; i<49; i++) 
      Serial.print("#");
    Serial.println();
  }
  
}

void Serial_display_cell(){
   if (Serial){
       Serial.print("\t\t");
       for(uint8_t i=0; i<8; i++)
            Serial.print("#     ");
       Serial.println("#");
   }     
}

void Serial_populate_row(const uint8_t row_values[]){
  if (Serial){
    Serial.print("\t\t#");
    for(uint8_t i=0; i<8; i++)
    {    
        Serial.print("  ");
        switch ((uint8_t)pgm_read_byte(&(row_values[i]))) {
          case 0:
             Serial.print("#");
             break;
          case 1:
             Serial.print(" ");
             break;
          case 2:
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
             break;
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
  
  Serial.print("\033[2J"); //VT100 clear
  Serial.print("\033[H");  //home position
  Serial.println("\t\t   A     B     C     D     E     F     X     Z");
  Serial.println("\t\t   0     1     2     3     4     5     6     7");
  for(uint8_t i=0; i<8; i++)
  {
     Serial_display_bar();
     Serial_display_cell();
     Serial_populate_row(levels[(EEPROM.read(LAYER_BYTE)*8)+i]);
     Serial.print(" ");
     Serial.println(i);
     Serial_display_cell();
     
  }
  Serial_display_bar();
  
}

void Serial_display_footer()
{
  if (Serial){  
    Serial.print(F("|[TL: 00:13:37] | [Layer: "));
    Serial.print(EEPROM.read(LAYER_BYTE));
    Serial.print(F("] | [Origin: "));
    Serial.print(EEPROM.read(X_BYTE)); // convert this to letters
    Serial.print(" ");
    Serial.print(EEPROM.read(Y_BYTE));
    Serial.println(F("] | [Serial:Connected] |  VT100 |  9600 8N1 |"));
  
  }
}

uint8_t Serial_handle_prompt(){
  if(Serial){
   Serial.print(":" );
    
  }
  while (!Serial.available()){}
  return (uint8_t) Serial.parseInt();
    
}

/*
void read_target_position(){
  char input;
  target_position[0] = EEPROM.read(2);
  if(Serial){
    Serial.print("ROW");
    target_position[1] = Serial_handle_prompt();
    Serial.print(target_position[1]);
    Serial.print("COL");  
    target_position[2] = Serial_handle_prompt();
    Serial.println(target_position[2]);    
  }
}
*/

void write_target_position()
{
   EEPROM.write(LAYER_BYTE,target_position[0]);
   EEPROM.write(X_BYTE,target_position[1]);
   EEPROM.write(Y_BYTE,target_position[2]);

}



void display_feedback() // needs funcitons from Jay, consdier merging with evaluate_position()
{
  if(Serial)
  {
     Serial.print(F("LIGHTS: "));
  
    switch (game_state)
    {
       case ENTER_STATE: 
          Serial.print(F("ENTER_STATE "));
              //enter_animation();
 
       break;
       
       case VALID_MOVE:   
            Serial.print(F("VALID_MOVE "));
          // valid_animation();
       break;
       
       case ORIENT:       
             Serial.print(F("ORIENTATION "));
         // orientation_display();
       break;
       
       case TRANSITION:   
             Serial.print(F("TRANSITION "));
          // transition_animation();
       break;
       
       case END_LEVEL:
           Serial.print(F("END_LEVEL "));   
          //level_animation();
       break;
       
       case WIN_GAME:
           Serial.print(F("WIN!! "));
          //complete_animation();
       break;
       
       case INVALID_MOVE:
       default:   
           Serial.print(F("INVALID "));
          //invalid_animation();
       break;
      
    }
  }
}

void evaluate_position(){ //only chage state here, only print if you are in a code spot we expect not to reach
 target_position[0] = EEPROM.read(LAYER_BYTE);
 target_position[1] = EEPROM.read(X_BYTE);
 target_position[2] = EEPROM.read(Y_BYTE);
 if (EEPROM.read(PIXEL_TEST) != 0) // High byte means we have not found 0x8 yet
  {
     if ((target_position[0] == 0) && (target_position[1] == 7) && (target_position[2] == 6)) // when dials are at 0X8 enter the game 
     {
        game_state = ENTER_STATE;
        write_target_position(); //save our position
        EEPROM.write(PIXEL_TEST,0x0);  // clear enterstate bit we'll no longer branch here
  
     }
     else
     {
         game_state = INVALID_MOVE;
     }
  
  }
  else // we're in the cube EEPROM[6]==0
  {    
       if ( ( ((target_position[0] != target_position[0])) || //always stay on the same layer , how do we advance portal?
                    ((target_position[1] != target_position[1]) || (target_position[2] != target_position[2]))) && // no diagonal movements 
                    ((target_position[1] > (target_position[1] + 1)) || (target_position[1] < (target_position[1]-1)) ) || //no move is ever non-adjacent
                    ((target_position[2] > (target_position[2] + 1)) || (target_position[2] < (target_position[2]-1)) ) 
                   )  
       {
          game_state = INVALID_MOVE;
          return;
       } 

       //evaluate the tile we want to get to and set the state accordingly
       switch(access_tile(target_position[0],target_position[1],target_position[2])){

        case PATH:
            game_state = ORIENT;
        break;
        case KEY:
            EEPROM.write(HAS_KEY,0x0); // clear the key byte indicating we have a key
            game_state = ORIENT;
        break;
        case UP_PORTAL:
            if ((target_position[0]+1) == 8)
            {
               Serial.println(F("LEVEL DESIGN ERROR"));
               game_state = INVALID_MOVE;
            }
            game_state = TRANSITION;
        break;
        case DOWN_PORTAL:
            if (!target_position[0])
            {
               Serial.println(F("LEVEL DESIGN ERROR"));
               game_state = INVALID_MOVE;
            }
            game_state = TRANSITION;
        break;
        case DOOR:
            if(EEPROM.read(HAS_KEY) == 0) {// we have a key
                 game_state = ORIENT;
                 EEPROM.write(HAS_KEY,0xFF); //set the key byte as it is used
            }
            else
            {
                 game_state = INVALID_MOVE;
                 Serial.print(F("NEED A KEY"));
            }
        break;        
        case TRAP:
                 game_state = ORIENT;
            // is trap to a begining layer layer or level?
        case  WALL:
        default:
            game_state = INVALID_MOVE;
        break;
       }
      if(game_state != INVALID_MOVE)
      {
          target_position[0] = target_position[0];
          target_position[1] = target_position[1];
          target_position[2] = target_position[2];
      }
  }
}

void update_map(){ // this is the logic to update the position of the player on the map and handle keys
 Serial.print(F("GAME STATE: "));
  switch (game_state)
  {
      case INVALID_MOVE:
        Serial.print(F("INVALID MOVE "));
        break;
      case ORIENT:
        Serial.print(F("VALID - ORIENT "));
        write_target_position(); // save new spot to EEPROM
        break;
      case TRANSITION:
        Serial.print(F("VALID - TRANSITION "));
         if(access_tile(target_position[0],target_position[1],target_position[2]) == UP_PORTAL)
         {
            Serial.print(F("FORWARD "));
            target_position[0]++;
            write_target_position(); 
   //         Serial_show_map();
         }
         else if (access_tile(target_position[0],target_position[1],target_position[2]) == DOWN_PORTAL)
         {
            Serial.print(F("BACKWARD "));
            if(!target_position[0]) {
              Serial.print(F("LEVEL DESIGN ERROR DOWN PORTAL IN FIRST LAYER"));
              break;
            }
            target_position[0]--;
            write_target_position(); 
//            Serial_show_map();
         }
         else
         {
            Serial.print(F("LOGIC ERROR - TILE "));
         }
         break;
        case ENTER_STATE:
           game_state = ORIENT;
        break;
        default:
           Serial.print(F("LOGIC ERROR - STATE "));
        break;
         
  }
  Serial.print(F("New Player Position L["));
  Serial.print(EEPROM.read(LAYER_BYTE));
  Serial.print(F("] X["));
  Serial.print(EEPROM.read(X_BYTE));
  Serial.print(F("] Y["));
  Serial.print(EEPROM.read(Y_BYTE));
  Serial.println(F("]"));
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
  if(target_position[0]==0 && target_position[1]==6 && target_position[2]==7){
    EEPROM.write(PIXEL_TEST,2);
  }

  // If you have passed the test, breathe
  if (EEPROM.read(PIXEL_TEST)==2){
    if(ana2>895 && ana1>128 && ana1<400 && ana0<128){
      EEPROM.write(PIXEL_TEST, 1);
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
    } else { // If you haven't yet passed the initial 0x8 test, red flashes
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
  
  FastLED.clear();
  FastLED.show();
  }
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
    below = access_tile(z_pos,(y_pos+1),x_pos);
  }
  
  if(y_pos==0) {
    above = 0;
  } else {
    above = access_tile(z_pos,(y_pos-1),x_pos);
  }

  if(x_pos==0){
    left = 0;
  } else {
    left = access_tile(z_pos,y_pos,(x_pos-1));
  }
  
  if(x_pos==7) {
    right = 0;
  } else {
    right = access_tile(z_pos,y_pos,(x_pos+1));
  }

  int surroundings[] = {below, right, left, above};
  
  /*for(int i=0;i<4;i++){
    Serial.print(i);
    Serial.print(": ");
    Serial.println(surroundings[i]);
    delay(1000);
  }*/

  Serial.print("Z: ");
  Serial.println(z_pos);
  Serial.print("X: ");
  Serial.println(x_pos);
  Serial.print("Y: ");
  Serial.println(y_pos);
  Serial.print("below: ");
  Serial.println(below);
  Serial.print("above: ");
  Serial.println(above);
  Serial.print("left: ");
  Serial.println(left);
  Serial.print("right: ");
  Serial.println(right);
  
  
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

void read_target_position(){
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
