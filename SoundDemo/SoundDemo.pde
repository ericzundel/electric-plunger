/*
 * Plays all files in the root directory of the sdcard
 */
#include <AF_Wave.h>
#include <avr/pgmspace.h>
#include "util.h"
#include "wave.h"

AF_Wave card;
File f;
Wavefile wave;

void setup() {
  // set up serial port
  Serial.begin(9600);

  // set up waveshield pins
  pinMode(2, OUTPUT); 
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  
  if (!card.init_card()) {
    Serial.println("Card init. failed!"); return;
  }
  if (!card.open_partition()) {
    Serial.println("No partition!"); return;
  }
  if (!card.open_filesys()) {
    Serial.println("Couldn't open filesys"); return;
  }

 if (!card.open_rootdir()) {
    Serial.println("Couldn't open dir"); return;
  }
}

void playfile(char *name) {
   if (f) {
      card.close_file(f);
    }
   f = card.open_file(name);
   if (!f) {
      putstring(" Couldn't open file: " );
      Serial.println(name);
      delay(500);
      return;
   }
   if (!wave.create(f)) {
     Serial.println(" Not a valid WAV"); return;
   }
   wave.play();
}

void ls() {
  char name[13];
  int ret;
  
  card.reset_dir();
  Serial.println("Files found:");
  while (1) {
    ret = card.get_next_name_in_dir(name);
    if (!ret) {
       card.reset_dir();
       return;
    }
    Serial.println(name);
  }
}

// first file is at index 1
char* file_at_index(int index) {
  static char name[13];
  
  card.reset_dir();
  while (index) {
    if (!card.get_next_name_in_dir(name)) {
       card.reset_dir();
       return NULL;
    }
    index--;
  }
  return name;
}

int last_played = 1;
void loop() {
  if (!wave.isplaying) {
    // dump to the serial port just for grins
    ls();
    delay (1000);
    
    char * filename = file_at_index(last_played);
    if (filename == NULL) {
      last_played = 1;
    } else {
      Serial.print("Playing file");
      Serial.println(filename);
      card.reset_dir();
      playfile(filename);
      last_played++;
    }
  }
}
