/* 
 * LedMatrix.cpp
 *
 * LED Matrix Driver for Electric Plunger
 *
 * Copyright 2009 Eric Z. Ayers
 *
 * License: Creative Commons Attribution 3.0
 *          See LICENSE file for more details
 *
 * This library drives a 10 x 6 LED matrix.  
 *
 * EXAMPLE USAGE:
 *
 * LedMatrix matrix(2,3,4); // Setup using shift register on pins 2,3,4
 * 
 * unsigned long action_time;
 *
 * setup() {
 *   action_time = millis();
 * }
 *
 * loop() {
 *   matrix.RunStateMachine();
 *
 *   // Check to see if the Matrix is ready for more data.
 *   if (matrix.IsReady()) {
 *     elapsed = millis - action_time;
 *     if (elapsed < 250) {
 *       matrix.DisplayLeds("111111111111111111111111111111"
 *                          "111111111111111111111111111111");
 *     } else if (elapsed < 500) {
 *       matrix.DisplayLeds("111111111111111111111111000000"
 *                          "111111111111111111111111000000");
 *     } else if (elapsed < 750) {
 *       matrix.DisplayLeds("111111111111111111000000000000"
 *                          "111111111111111111000000000000");
 *     } else if (elapsed < 1000) {
 *       matrix.DisplayLeds("111111111111000000000000000000"
 *                          "111111111111000000000000000000");
 *     } else if (elapsed < 1250) {
 *       matrix.DisplayLeds("111111000000000000000000000000"
 *                          "111111000000000000000000000000");
 *     } else if (elapsed < 1500) {
 *       matrix.DisplayLeds("000000000000000000000000000000"
 *                          "000000000000000000000000000000");
 *     } else {
 *       // go back to the first pattern
 *       action_time = millis();
 *     }
 *   }
 * }
 *
 * CIRCUIT:
 *
 * The 'top' part of the circuit is an array of 10 PNP transistors. The 
 * 'bottom' part of the circuit uses 6 pins of a ULN2803A Darlington array.
 *
 * Between these transistors are are wired to a 10x6 LED array.
 *
 * The matrix is controlled by a pair of 74HC595 serial to parallel 
 * 8 bit shift registers hooked up together to make a single logical 
 * 16 bit shift register.  This register requires only 3 pins from the mpu.
 *
 *
 * CONTROLS:
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
 *
 *
 * CODE OVERVIEW:
 * 
 * This library is designed to run as a state machine.  The library keeps
 * track of the last time an action was taken and evaluates whether another
 * action should be taken when LedMatrix::RunStateMachine() is called.  This
 * means that you should call this method frequently.  For example, in the 
 * Arduino environment, each time the loop() function gets called, the 
 * LedMatrix::RunStateMachine() method should be called to allow the matrix 
 * to update itself.
 *
 * The software waits in the IDLE state until the LedMatrix::DisplayLeds()
 * method is called.  Then, the state machine cycles through displaying each
 * row with the shift register and leaving each row on for a 
 * LED_ROW_DISPLAY_CYCLE microseconds (or optionally displaying each 
 * row multiple times by setting LED_MAX_INTENSITY > 1).
 *
 * Once all 10 rows are displayed, the state machine goes back to the IDLE 
 * state.
 * 
 *
 * PERFORMANCE:
 *
 * Each call to RunStateMachine() runs in constant time and does not wait 
 * on any i/o.  The longest time spent is when in the COMPUTE_NEXT_ROW_STATE 
 * where values are shifted out to the shift register which runs at full 
 * processor speed.  The shift register is rated at 25MHz at 4.5V, which is
 * faster than the 20MHz max clock speed of the MPU.
 *
 * 
 * INTERRUPTS:
 * 
 * This code does not use interrupts, but is prepared to be called from an
 * interrupt.  The RunStateMachine() method protects itself from re-entrancy
 * and limits itself to being called no more frequently than every 2 
 * microseconds.
 *
 * The method RunStateMachineFromInterrut() can be called from code that 
 * is in an interrupt to refresh the matrix.
 *
 */

/* Enable for extra debug messages out the serial port, but don't expect the
 *  display to look nice.
 */
#define DEBUG 0

#include "LedMatrix.h"

// Length of time a single row is displayed.
#define LED_ROW_DISPLAY_CYCLE 500
#define LED_MAX_INTENSITY 1

// Definition of state machine states
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

/*
 * Each of these state machine methods executes this state's action and 
 * return the next state.
 */

/*
 * The IDLE state does nothing.  To get out of Idle state, someone
 * must put new data into the matrix using LedMatrix::DisplayLeds().
 */
int LedMatrix::LedStateIdle() {
  return LED_STATE_IDLE;
}

/*
 * LedMatrix::DisplayLeds() puts the machine into this state.  This state
 * prepares the machine for going through the refresh cycle.
 */
int LedMatrix::LedStateNewData() {
  current_row_ = 0;
  current_value_ = 0;
  last_action_time_ = micros();
  return LED_STATE_COMPUTE_ROW;
}

/*
 * Compute the binary value to shift out to the shift register for the
 * next row to display and shift it out.
 */
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

/*
 * Wait for the display cycle time to end.  While in this state, the LEDs 
 * for one row are on.  If LED_MAX_INTENSITY is > 1, th en each row will be
 * displayed multiple times, turning off any columns that have values less
 * than the current intensity.
 */
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

/*
 * Drives the state machine.  Call this fuunction often!
 */
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

/*
 * This routine is like RunStateMachine, but assumes that the processor
 * is already interrupted or interrupts already masked.
 */
void LedMatrix::RunStateMachineFromInterrupt() {
  if (in_progress_) {
    return;
  }
  RunStateMachineImpl();  
}

/*
 * Does the dirty work of calling the right state function and shifting to
 * the next state.
 */
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

/*
 * Writes a 16 biy value to the pair of 74HC595 shift registers. 
 * The outputs are turned off while the valuse is being shifted in.
 */
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

