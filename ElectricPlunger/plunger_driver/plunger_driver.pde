/*
 * Electric plunger driver
 * 
 * Copyright 2009 Eric Z Ayers
 *
 * License: Creative Commons Attribution 3.0 
 * See LICENSE file for more details
 *
 */

#include <LedMatrix.h>
#include <FatReader.h>
#include <SdReader.h>
#include <avr/pgmspace.h>
#include "WaveUtil.h"
#include "WaveHC.h"

#define DEBUG 0

SdReader card;
FatVolume vol;
uint8_t dirLevel; // indent level for file/dir names
dir_t dirBuf;     // buffer for directory reads

int next_wav_index;
int next_led_index;

/* Returns the number of bytes currently free in RAM */
int freeRam(void)
{
  extern int  __bss_end; 
  extern int  *__brkval; 
  int free_memory; 
  if((int)__brkval == 0) {
    free_memory = ((int)&free_memory) - ((int)&__bss_end); 
  }
  else {
    free_memory = ((int)&free_memory) - ((int)__brkval); 
  }
  return free_memory; 
} 

/* Print error message and halt if SD I/O error */
static void sdErrorCheck(void)
{
  if (!card.errorCode()) return;
  putstring("\n\rSD I/O error: ");
  Serial.print(card.errorCode(), HEX);
  putstring(", ");
  Serial.println(card.errorData(), HEX);
  while(1);
}

static bool isWavFile(dir_t&); // decl
static bool isLedFile(dir_t&); // decl

static bool FindNextWavFile(FatReader& dir, int* last_index) {
  int index = 0;
  dir.rewind();
  while (dir.readDir(dirBuf) > 0) {
    if (dirBuf.name[0] == '.' || !isWavFile(dirBuf))
      continue;
    index++;
    if (index > *last_index) {
      *last_index = index;
      return true;
    }
  }
  *last_index = 0;
  return false;
}

static bool FindNextLedFile(FatReader& dir, int* last_index) {
  int index = 0;
  dir.rewind();
  while (dir.readDir(dirBuf) > 0) {
    if (dirBuf.name[0] == '.' || !isLedFile(dirBuf))
      continue;
    index++;
    if (index > *last_index) {
      *last_index = index;
      return true;
    }
  }
  *last_index = 0;
  return false;
}

/* Pretty prints a FAT 8.3 filename */
static void printName(dir_t &dir)
{
  for (uint8_t i = 0; i < 11; i++) {
    if (dir.name[i] == ' ')continue;
    if (i == 8) Serial.print('.');
    Serial.print(dir.name[i]);
  }
  if (DIR_IS_SUBDIR(dir)) Serial.print('/');
}

/***********************************************************
 *  Wave file control logic
 ***********************************************************/

/* Stores the state of playing the .wav file between loops() */
struct wave_state {
  FatReader wave_file;
  FatReader root;
};
static struct wave_state wstate;

/* Wave playing library object */
WaveHC wave;      // only one allowed!

static bool isWavFile(dir_t &dir)
{
  if (DIR_IS_SUBDIR(dir)) {
    return false;
  }
  if (dir.name[8] == 'W' && dir.name[9] == 'A' && dir.name[10] == 'V') {
    return true;
  }
  return false;
}

/* Starts up each new .wav file as they complete. */
static void wave_play_loop() {
  if (wave.isplaying) {
    return;
  }
  // sdErrorCheck();
  if (FindNextWavFile(wstate.root, &next_wav_index)) {
    if (!wstate.wave_file.open(vol, dirBuf)) {
      // Serial.print("Failed to open WAV file: ");
      // printName(dirBuf);
    } else if (!wave.create(wstate.wave_file)) {
      // Serial.print(" Not a valid WAV: ");
      // printName(dirBuf);
    } else {
#if DEBUG
      Serial.print("Playing WAV file: ");
      printName(dirBuf);
#endif
      wave.play();
    }
  }
}

/***********************************************************
 *  LED Matrix control logic
 ***********************************************************/

struct led_state {
  FatReader led_file;
  unsigned long last_action_time;
  int time_to_next_read;

  // Each line is 4 chars of millisecond duration + 60 chars of LED data + n/l
#define LINE_BUF_SIZE 65
  char line_buf[LINE_BUF_SIZE];
  FatReader root;
};

struct led_state lstate;
// ApuPlunger pin outs

// LED Matrix
// data pin = 7
// clock pin = 8
// output enable pin = 6
LedMatrix matrix(7,8,6);

static bool isLedFile(dir_t &dir)
{
  if (DIR_IS_SUBDIR(dir)) {
    return false;
  }
  if (dir.name[8] == 'L' && dir.name[9] == 'E' && dir.name[10] == 'D') {
    return true;
  }
  return false;
}

// Read the next line of data from the file.
static void read_next_line() {
  noInterrupts();
  int result = lstate.led_file.read((uint8_t*)lstate.line_buf, 
                                      LINE_BUF_SIZE);
  interrupts();

  if (result != LINE_BUF_SIZE) {
    // EOF or error
#if DEBUG
    Serial.print("Got result: ");
    Serial.print(result, DEC);
    Serial.println(".  Going to next file");
#endif
    lstate.led_file.close();
  }

  lstate.time_to_next_read = 
      (lstate.line_buf[0] - '0') * 1000 +
      (lstate.line_buf[1] - '0') * 100 +
      (lstate.line_buf[2] - '0') * 10 +
      (lstate.line_buf[3] - '0');
}

static void led_loop() {
  matrix.RunStateMachine();

  if (!matrix.IsReady()) {
    return;
  }

  long unsigned int elapsed = millis() - lstate.last_action_time;
  if (elapsed < lstate.time_to_next_read) {
    // Display the current LED data

    // The first 4 chars are the time in milliseconds to display this data
    // The next 60 chars are the led values [0-9]
    matrix.DisplayLeds(&lstate.line_buf[4]);
    return;
  }

  // The time to expire the current set of data has expired.
  lstate.last_action_time = millis();
  if (lstate.led_file.isOpen()) {
    read_next_line();    
    return;
  }

  // No more data in this file.  Open the next file 
  noInterrupts();
  while (1) {
    // Will return false when we reach the end.
    if (!FindNextLedFile(lstate.root, &next_led_index))
      continue;

    // Not expected.      
    if (!lstate.led_file.open(vol, dirBuf)) {
      Serial.println("led_file.open failed");
      continue;
    }
    // We have a new file open now.
    break;
  }
  interrupts();
}

static void busy_func() {
  matrix.RunStateMachineFromInterrupt();
}

static void GetFileCounts(FatReader& dir, int* wav_count, int* led_count) {
  dir.rewind();
  while (dir.readDir(dirBuf) > 0) {
    if (dirBuf.name[0] == '.')
      continue;
     else if (isLedFile(dirBuf))
      (*led_count)++;
     else if (isWavFile(dirBuf))
      (*wav_count)++;
  }
}

/******************************************************************************
 *  Main Entry Points
 *****************************************************************************/

void setup() {
  int i;
  Serial.begin(9600);
  Serial.print("Free Ram: ");
  Serial.print(freeRam(), DEC);
  Serial.println();
  Serial.println("Starting");
  
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
    
  // if (!card.init(true)) {//play with 4 MHz spi  
  if (!card.init()) {//play with 8 MHz spi  
    putstring_nl("Card init. failed!");
    sdErrorCheck();while(1);
  }

  // Enable optimize read - some cards may timeout
  card.partialBlockRead(true);
  
  uint8_t part;
  for (part = 0; part < 5; part++) {
    if (vol.init(card, part)) break;
  }

  if (part == 5) {
    putstring_nl("No valid FAT partition!");
    sdErrorCheck();while(1);
  }

  putstring("Using partition ");
  Serial.print(part, DEC);
  putstring(", type is FAT");
  Serial.println(vol.fatType(),DEC);
  
  if (!wstate.root.openRoot(vol)) {
    putstring_nl("Can't open root dir!"); while(1);
  }
  if (!lstate.root.openRoot(vol)) {
    putstring_nl("Can't open root dir!"); while(1);
  }
  dirLevel = 0;

  int seed;
  for (int i = 0; i < 100; ++i) {
    if (seed = analogRead(16))
      break;
  }

#if DEBUG
  Serial.print("Seed: ");
  Serial.println(seed);
#endif

  randomSeed(seed);
  // Count the # of wav and led files so we can choose
  // a good one to start with.
  // NOTE: This assumes led's and wav's are in the same
  // directory.
  int wav_count = 0, led_count = 0;
  GetFileCounts(wstate.root, &wav_count, &led_count);
  next_wav_index = random(wav_count);
  next_led_index = random(led_count);
  
#if DEBUG
  Serial.println("Counts:");
  Serial.print("  WAV: ");
  Serial.println(wav_count);
  Serial.print("  LED: ");
  Serial.println(led_count);

  Serial.println("Indexes:");
  Serial.print("  WAV: ");
  Serial.println(next_wav_index);
  Serial.print("  LED: ");
  Serial.println(next_led_index);
#endif

  wstate.wave_file.setBusyFunc(busy_func);
  card.setBusyFunc(busy_func);
}


void loop() {
  wave_play_loop();
  led_loop();
}
