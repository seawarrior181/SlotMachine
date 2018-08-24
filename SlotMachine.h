#include <avr/pgmspace.h>
const char string_00[] PROGMEM = "PayedOut           ";                           
const char string_01[] PROGMEM = "Wagered            ";
const char string_02[] PROGMEM = "Plays              ";
const char string_03[] PROGMEM = "2 Match            ";
const char string_04[] PROGMEM = "3 Match            ";
const char string_05[] PROGMEM = "Ship 1 Match       ";
const char string_06[] PROGMEM = "Ship 2 Match       ";
const char string_07[] PROGMEM = "Ship 3 Match       ";
const char string_08[] PROGMEM = "1 Alien            ";
const char string_09[] PROGMEM = "2 Alien            ";
const char string_10[] PROGMEM = "3 Alien            ";
const char string_11[] PROGMEM = "EEprom             ";
const char string_12[] PROGMEM = "Credits            ";
const char string_13[] PROGMEM = "Back               ";

const char* const string_table[] PROGMEM = {string_00,
											string_01,
											string_02,
											string_03,
											string_04,
											string_05,
											string_06,
											string_07,
											string_08,
											string_09,
											string_10,
											string_11,
											string_12,
											string_13};

#define MENU_PAYED_OUT						0
#define MENU_WAGERED						1
#define MENU_PLAYS                          2
#define MENU_2_MATCH                        3
#define MENU_3_MATCH                        4
#define MENU_SHIP_1_MATCH                   5
#define MENU_SHIP_2_MATCH                   6
#define MENU_SHIP_3_MATCH                   7
#define MENU_1_ALIEN                        8
#define MENU_2_ALIEN                        9
#define MENU_3_ALIEN                        10
#define MENU_EEPROM                         11
#define MENU_CREDITS                        12
#define MENU_BACK                           13
			
char buffer[20];    						// make sure this is large enough for the largest string it must hold (or graphics get corrupted)
//------------------------
// The graphics:
//const PROGMEM byte reel[] =
const byte reel[] =
{				// 0	star
	B10011001,  //0
	B01011010,
	B00111100,
	B11111111,
	B11111111,
	B00111100,
	B01011010,
	B10011001,
				// 1	one spot on dice
	B00000000,  // 8
	B00000000,
	B00000000,
	B00011000,
	B00011000,
	B00000000,
	B00000000,
	B00000000,
				// 2	three bars
	B11111111,	// 16
	B11111111,
	B00000000,
	B11111111,
	B11111111,
	B00000000,
	B11111111,
	B11111111,
				// 3	heart
	B01100110,	// 24
	B11111111,
	B11111111,
	B11111111,
	B11111111,
	B01111110,
	B00111100,
	B00011000,
				// 4	two spots on dice
	B00000000,	// 32
	B01100000,
	B01100000,
	B00000000,
	B00000000,
	B00000110,
	B00000110,
	B00000000,
				// 5	seven
	B00000000,	// 40
	B01111110,
	B01111110,
	B00001100,
	B00011000,
	B00111000,
	B00111000,
	B00000000,
				// 6	dollar sign
	B00011000,	// 48
	B00111100,
	B01011010,
	B00111000,
	B00011100,
	B01011010,
	B00111100,
	B00011000,
				// 7	three spots on dice
	B00000000,
	B01100000,
	B01100000,
	B00011000,
	B00011000,
	B00000110,
	B00000110,
	B00000000,
				// 8	inverse 9 spots, hashtag #
	B00100100,
	B00100100,
	B11111111,
	B00100100,
	B00100100,
	B11111111,
	B00100100,
	B00100100,
				// 9	one bar
	B00000000,
	B00000000,
	B00000000,
	B11111111,
	B11111111,
	B00000000,
	B00000000,
	B00000000,
				// 10	four on dice
	B00000000,
	B01100110,
	B01100110,
	B00000000,
	B00000000,
	B01100110,
	B01100110,
	B00000000,
				// 11	inverse seven
	B11111111,
	B10000001,
	B10000001,
	B11110011,
	B11100111,
	B11000111,
	B11000111,
	B11111111,
				// 12	9 spots
	B11011011,
	B11011011,
	B00000000,
	B11011011,
	B11011011,
	B00000000,
	B11011011,
	B11011011,
				// 13	five on dice
	B00000000,
	B01100110,
	B01100110,
	B00011000,
	B00011000,
	B01100110,
	B01100110,
	B00000000,
				// 14	two bars
	B00000000,
	B11111111,
	B11111111,
	B00000000,
	B00000000,
	B11111111,
	B11111111,
	B00000000,
				// 15 Alien 0 (120)
	B01000010, 
	B00100100,
	B01111110,
	B11011011,
	B11111111,
	B11111111,
	B10100101,
	B00100100,
				// 16	smile face (128)
	B00000000,
	B00100100,
	B00000000,
	B00011000,
	B01000010,
	B01000010,
	B00111100,
	B00011000,
				// 17 	6 on dice (136)
	B00000000,
	B11011011,
	B11011011,
	B00000000,
	B00000000,
	B11011011,
	B11011011,
	B00000000,
				// 18 SpaceShip (144)
	B00000000,
	B00000000,
	B00111100,
	B01111110,
	B10101011,
	B01111110,
	B00111100,
	B00000000,
				// 19 Alien 1 (152)
	B00011000,   
	B00111100,
	B01111110,
	B11011011,
	B11111111,
	B00100100,
	B01011010,
	B10100101,
				// 20 Alien 2 (160)
	B00011000, 
	B00111100,
	B01111110,
	B11011011,
	B11111111,
	B00100100,
	B01011010,
	B01000010,
				// 21 Alien 3 (168)
	B00000000, 
	B10000001,
	B11111111,
	B11011011,
	B11111111,
	B01111110,
	B00100100,
	B01000010,
				// 22	one
	B00010000,
	B00110000,
	B00010000,
	B00010000,
	B00010000,
	B00010000,
	B00010000,
	B00111000,
				// 23	two
	B00111000,
	B01000100,
	B10000010,
	B00000100,
	B00001000,
	B00010000,
	B00100000,
	B11111110,
				// 24	three
	B11111111,	// 192
	B00000010,
	B00000100,
	B00011100,
	B00000010,
	B00000100,
	B00001000,
	B11100000
};

/*************************************************
 * Public Constants
 *************************************************/

#define NOTE_B0  31
#define NOTE_C1  33
#define NOTE_CS1 35
#define NOTE_D1  37
#define NOTE_DS1 39
#define NOTE_E1  41
#define NOTE_F1  44
#define NOTE_FS1 46
#define NOTE_G1  49
#define NOTE_GS1 52
#define NOTE_A1  55
#define NOTE_AS1 58
#define NOTE_B1  62
#define NOTE_C2  65
#define NOTE_CS2 69
#define NOTE_D2  73
#define NOTE_DS2 78
#define NOTE_E2  82
#define NOTE_F2  87
#define NOTE_FS2 93
#define NOTE_G2  98
#define NOTE_GS2 104
#define NOTE_A2  110
#define NOTE_AS2 117
#define NOTE_B2  123
#define NOTE_C3  131
#define NOTE_CS3 139
#define NOTE_D3  147
#define NOTE_DS3 156
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_FS3 185
#define NOTE_G3  196
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698  	// 4
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047	// 5
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397  	// 3
#define NOTE_FS6 1480
#define NOTE_G6  1568 	// 1
#define NOTE_GS6 1661
#define NOTE_A6  1760  	// 2
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_CS7 2217
#define NOTE_D7  2349
#define NOTE_DS7 2489
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_FS7 2960
#define NOTE_G7  3136
#define NOTE_GS7 3322
#define NOTE_A7  3520
#define NOTE_AS7 3729
#define NOTE_B7  3951
#define NOTE_C8  4186
#define NOTE_CS8 4435
#define NOTE_D8  4699
#define NOTE_DS8 4978