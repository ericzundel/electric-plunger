/*
  LedMatrix.h
  lass for driving the 10x6 LED matrix on the ApuPlunger
 */

#ifndef LedMatrix_h
#define LedMatrix_h

#include "WProgram.h"

#define LED_NUM_ROWS 10
#define LED_NUM_COLS 6
#define LED_COLUMN_START_BIT 10

class LedMatrix {
 public:
  LedMatrix(int dataPin, int clockPin, int outputEnablePin);

  // Populate the 10x6 led matrix.  Each character is a value from '0' to '9' 
  // where '0' is off, and '9' is the brightest setting.  Populate the matrix
  // with a 60 character long string one row at a time.
  void DisplayLeds(char *values);

  // Returns true if the matrix is ready to display new data.
  bool IsReady();

  // Call this function as often as possible 
  // (maybe it should fire on a timer interrupt?)
  void RunStateMachine();
  void RunStateMachineFromInterrupt();

 private:
  void RunStateMachineImpl();
  int LedStateNewData();
  int LedStateDisplayingRow();
  int LedStateIdle();
  int LedStateComputeRow();
  void SendToShiftRegister(unsigned long value);

  int led_data_pin_;
  int led_clock_pin_;
  int led_output_enable_pin_;
  char led_state_;
  unsigned long last_action_time_;
  // Each LED can have intensity 0-9, sorted as ascii chars
  char led_values_[LED_NUM_ROWS][LED_NUM_COLS]; 
  int current_row_;
  int current_value_;
  bool in_progress_;
};

#endif
