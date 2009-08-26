/* 
 * LedMatrix.cpp
 *
 * LED Matrix Driver for Electric Plunger
 *
 * The 'top' part of the circuit is 10 PNP transistor. The 'bottom' 
 * part of the circuit uses 6 pins of a ULN2803A Darlington array.
 *
 * Between these transistors are are wired to a 10x6 LED array.
 *
 * The matrix is controlled by a pair of 74HC595 serial to parallel 
 * 8 bit shift registers hooked up together to make a single logical 
 * 16 bit shift register.  This register requires only 3 pins from the mpu.
 *
 * The controls:
 *
 * bits 10-15 are to the Darlington array.  Raise high to switch on.
 * bits 0-9 are to the PNP transistors. Pull low to switch on.
 * b0000000000111111 turns all leds on
 * b0111111111000001 turns on the upper left led
 * 
 * Each column is wired with a single current limiting resistor.  Therefore, 
 * only one row should be displayed at a time.  The logic below cycles
 * through each of the 10 rows, turning on the single row PNP transisitor
 * and then enabling each column that should be displayed for that row.
 */

#define DEBUG 0

#include "LedMatrix.h"

// Length of time a single row is displayed.
#define LED_ROW_DISPLAY_CYCLE 500
#define LED_MAX_INTENSITY 1

#define LED_STATE_IDLE             0
#define LED_STATE_NEW_DATA         1
#define LED_STATE_COMPUTE_ROW      2
#define LED_STATE_DISPLAY_ROW      3

LedMatrix::LedMatrix(int data_pin, int clock_pin, int output_enable_pin) 
  : led_data_pin_(data_pin),
    led_clock_pin_(clock_pin),
    led_output_enable_pin_(output_enable_pin),
    last_action_time_(0),
    in_progress_(false) {
  // Communicate to the 74HC595 over 3 pins
  pinMode(led_data_pin_, OUTPUT);
  pinMode(led_clock_pin_, OUTPUT);
  pinMode(led_output_enable_pin_, OUTPUT);
}

int LedMatrix::LedStateIdle() {
  return LED_STATE_IDLE;
}

int LedMatrix::LedStateNewData() {
  current_row_ = 0;
  current_value_ = 0;
  last_action_time_ = micros();
  return LED_STATE_COMPUTE_ROW;
}

// Compute the binary value to shift out to the shift register for the
// next row to display.
int LedMatrix::LedStateComputeRow() {
  unsigned long columns = 0;
  for (int i = 0; i < LED_NUM_COLS; i++) {
    columns = columns << 1;
    int led_value = 
        led_values_[current_row_][i];
    if (led_value > '0' + current_value_) {
      columns++;
    }
  }

  // Send the column to the shift register
  unsigned long int to_shift_register =
        (~(1L << current_row_) & 0x03FF)
        | (columns << LED_COLUMN_START_BIT);

#if DEBUG
  Serial.print(" Calculated data: ");
  Serial.println(to_shift_register, BIN);
  delay(1000);
#endif
  SendToShiftRegister(to_shift_register);

  // Remember the time this executed so we can compute elapsed time
  last_action_time_ = micros();

  return LED_STATE_DISPLAY_ROW;
}

// Wait while displaing a single row.
int LedMatrix::LedStateDisplayingRow() {
  // Just wait around displaying the led
  if ((micros() - last_action_time_) 
          > LED_ROW_DISPLAY_CYCLE) {
    if (++current_value_ > LED_MAX_INTENSITY) {
      current_value_ = 0;
      if (++current_row_ >= LED_NUM_ROWS) {
        current_row_ = 0;
        return LED_STATE_IDLE;
      }
    }
    return LED_STATE_COMPUTE_ROW;
  }
  return LED_STATE_DISPLAY_ROW;
}

// Drives the state machine.
void LedMatrix::RunStateMachine() {

  noInterrupts();
  if (in_progress_ || (micros() - last_action_time_) < 2) {
    interrupts();
    return;
  }
  in_progress_ = true;
  interrupts();

  RunStateMachineImpl();

  noInterrupts();
  in_progress_ = false;
  interrupts();
}

void LedMatrix::RunStateMachineFromInterrupt() {
  if (in_progress_) {
    return;
  }
  RunStateMachineImpl();  
}

void LedMatrix::RunStateMachineImpl() {
  int next_state;

  switch(led_state_) {
  case LED_STATE_IDLE:
    next_state = LedStateIdle();
    break;
  case LED_STATE_NEW_DATA:
    next_state = LedStateNewData();
    break;
  case LED_STATE_COMPUTE_ROW:
    next_state = LedStateComputeRow();
    break;
  case LED_STATE_DISPLAY_ROW:
    next_state = LedStateDisplayingRow();
    break;
  default:
    next_state = LED_STATE_IDLE;
  }

  if (next_state != led_state_) {
    led_state_ = next_state;
  }
}

bool LedMatrix::IsReady() {
  return (led_state_ == LED_STATE_IDLE ? true : false);
}

void LedMatrix::DisplayLeds(char* values) {
  for (int i = 0; i < LED_NUM_ROWS; ++i) {
    for (int j = 0; j < LED_NUM_COLS; ++j) {
      led_values_[i][j] = *values;
      values++;
    }
  }
  led_state_ = LED_STATE_NEW_DATA;
}

void LedMatrix::SendToShiftRegister(unsigned long value) {
  // turn off all outputs
  digitalWrite(led_output_enable_pin_, HIGH);

  // shift the data out msbit first
  for (long index = 15; index >= 0; --index) {
    digitalWrite(led_clock_pin_, LOW);
    char bit = ((value & (1 << index)) > 0) ? HIGH : LOW;
    digitalWrite(led_data_pin_, bit);
    digitalWrite(led_clock_pin_, HIGH);
  }

  // Toggle the clock low & high once more to push the last bit to 
  // the register.
  digitalWrite(led_clock_pin_, LOW);
  digitalWrite(led_clock_pin_, HIGH);

  // Turn the outputs back on
  digitalWrite(led_output_enable_pin_, LOW);
}

