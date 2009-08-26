/*
 * LedMatrix.h
 * Class for driving the 10x6 LED matrix on the ApuPlunger
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

  // Call this function as often as possible. 
  void RunStateMachine();
  // If you are already in an interrupt, you can use this function.
  void RunStateMachineFromInterrupt();

 private:
  void RunStateMachineImpl();
  int LedStateNewData();
  int LedStateDisplayingRow();
  int LedStateIdle();
  int LedStateComputeRow();
  void SendToShiftRegister(unsigned long value);

  // Pins to communicate with the shift register
  int led_data_pin_;
  int led_clock_pin_;
  int led_output_enable_pin_;

  // Current state in the state machine
  char led_state_;

  // Indicates the last time a row was turned on.
  unsigned long last_action_time_;

  // Each LED can have intensity 0-9, sorted as ascii chars
  char led_values_[LED_NUM_ROWS][LED_NUM_COLS]; 

  // Current row being displayed
  int current_row_;

  // Current intensity value being displayed
  int current_value_;

  // Provides protection against re-entrancy from interrupts.
  bool in_progress_;
};

#endif  // LedMatrix_h
