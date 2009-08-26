/* 
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

#define DEBUG 1

#include "WProgram.h"
void setup();
int led_idle();
int led_new_data();
int led_compute_next_row();
int led_displaying_row();
void led_state_machine();
char led_is_ready();
void display_leds(char* values);
void loop();
static int led_data_pin = 8;
static int led_clock_pin = 7;
static int led_output_enable_pin = 6;


#define ROW_1 (1)    // Bottom row
#define ROW_2 (1<<2)
#define ROW_3 (1<<3)
#define ROW_4 (1<<4)
#define ROW_5 (1<<5)
#define ROW_6 (1<<6)
#define ROW_7 (1<<7)
#define ROW_8 (1<<8)
#define ROW_9 (1<<9)
#define ROW_10 (1<<10)
#define NUM_ROWS 10

#define COLUMN_START_BIT 10
#define COLUMN_1 (1)
#define COLUMN_2 (1<<2)
#define COLUMN_3 (1<<3)
#define COLUMN_4 (1<<4)
#define COLUMN_5 (1<<5)
#define COLUMN_6 (1<<6)
#define NUM_COLS 6

void setup() {
  // Communicate to the 74HC595 over 3 pins
  pinMode(led_data_pin, OUTPUT);
  pinMode(led_clock_pin, OUTPUT);
  pinMode(led_output_enable_pin, OUTPUT);

#if DEBUG
  // Serial console for debugging
  Serial.begin(9600);
  Serial.println("Starting");
#endif

}

/*
 * Used by the LED driver
 */
#define LED_COLUMN_DISPLAY_CYCLE 500


#define LED_STATE_IDLE             0
#define LED_STATE_NEW_DATA         1
#define LED_STATE_COMPUTE_NEXT_ROW 2
#define LED_STATE_DISPLAY_ROW      3

typedef struct led_driver {
  int state;
  unsigned long last_action_time;
  // each LED can have intensity 0-9, sorted as ascii chars
  char led_values[NUM_ROWS][NUM_COLS]; 
  int current_row;
};
struct led_driver led_driver_data;

int led_idle() {
  return LED_STATE_IDLE;
}

int led_new_data() {
  led_driver_data.current_row = 0;
  led_driver_data.last_action_time = micros();
  return LED_STATE_COMPUTE_NEXT_ROW;
}

int led_compute_next_row() {
  unsigned long columns = 0;
  for (int i = 0; i < NUM_COLS; i++) {
    columns = columns << 1;
    int led_value = 
        led_driver_data.led_values[led_driver_data.current_row][i];
    if (led_value > '0') {
      columns++;
    }
  }

  // Send the column to the shift register
  unsigned long int to_shift_register =
        (~(1L << led_driver_data.current_row) & 0x03FF)
        | (columns << COLUMN_START_BIT);

#if DEBUG
  Serial.print(" Calculated data: ");
  Serial.println(to_shift_register, BIN);
  delay(100);
#endif

  // turn off all outputs
  digitalWrite(led_output_enable_pin, HIGH);

#if DEBUG
  /* Serial.print("    Writing data: ");*/
#endif

  // shift the data out msbit first
  for (long index = 15; index >= 0; --index) {
    digitalWrite(led_clock_pin, LOW);
    char bit = ((to_shift_register & (1 << index)) > 0) ? HIGH : LOW;
    // Serial.print(bit, DEC);
    digitalWrite(led_data_pin, bit);
    digitalWrite(led_clock_pin, HIGH);
  }

#if DEBUG  
  /* Serial.println(" ");*/
#endif

  // Toggle the clock low & high once more to push the last bit to 
  // the register.
  digitalWrite(led_clock_pin, LOW);
  digitalWrite(led_clock_pin, HIGH);

  digitalWrite(led_output_enable_pin, LOW);

  // Remember the time this executed so we can compute elapsed time
  led_driver_data.last_action_time = micros();

  return LED_STATE_DISPLAY_ROW;
}

int led_displaying_row() {
  // Just wait around displaying the led
  if ((micros() - led_driver_data.last_action_time) 
          > LED_COLUMN_DISPLAY_CYCLE) {
    if (++led_driver_data.current_row >= NUM_ROWS) {
      led_driver_data.current_row = 0;
      return LED_STATE_IDLE;
    }
    return LED_STATE_COMPUTE_NEXT_ROW;
  }
  return LED_STATE_DISPLAY_ROW;
}

void led_state_machine() {
  int next_state;

#if DEBUG  
  // Serial.print("In state: ");
  // Serial.println(led_driver_data.state);
  // delay(500);
#endif

  switch(led_driver_data.state) {
  case LED_STATE_IDLE:
    next_state = led_idle();
    break;
  case LED_STATE_NEW_DATA:
    next_state = led_new_data();
    break;
  case LED_STATE_COMPUTE_NEXT_ROW:
    next_state = led_compute_next_row();
    break;
  case LED_STATE_DISPLAY_ROW:
    next_state = led_displaying_row();
    break;
  default:
    Serial.print("Unknown state: ");
    Serial.println(led_driver_data.state);
    next_state = LED_STATE_IDLE;
  }

  if (next_state != led_driver_data.state) {
#if DEBUG  
    // for debugging
    // Serial.print("In state: ");
    // Serial.print(led_driver_data.state);
    // Serial.print(" To state: ");
    // Serial.println(next_state);
#endif
    led_driver_data.state = next_state;
  }

}

char led_is_ready() {
  return (led_driver_data.state == LED_STATE_IDLE ? 1 : 0);
}

void display_leds(char* values) {
  for (int i = 0; i < NUM_ROWS; ++i) {
    for (int j = 0; j < NUM_COLS; ++j) {
      led_driver_data.led_values[i][j] = *values;
      values++;
    }
  }

#if DEBUG
/*
  for (int i = 0; i < NUM_COLS; ++i) {
    Serial.print("column: ");
    Serial.print(i);
    Serial.print(": ");
    for (int j = 0; j < NUM_ROWS; ++j) {
      Serial.print(led_driver_data.led_values[i][j], BYTE);
    }
    Serial.println("");
  }
*/
#endif
  
  led_driver_data.state = LED_STATE_NEW_DATA;
}

static int state = 0;
static unsigned long last_change = 0;
void loop() {
  led_state_machine();

  if (led_is_ready()) {
    
#if DEBUG
Serial.print("In State: ");
Serial.println(state, DEC);
#endif
     switch(state) {
    case 0:
      display_leds("111111" 
                   "000000" 
                   "000000" 
                   "000000" 
                   "000000"
                   "000000" 
                   "000000" 
                   "000000" 
                   "000000" 
                   "000000");
      break;
    case 1:
      display_leds("100000" 
		   "100000" 
		   "010000" 
		   "010000" 
 		   "001000"
                   "001000" 
                   "000100" 
		   "000100" 
		   "000010" 
                   "000010");
      break;
    case 2:
      display_leds("001000" 
                   "000100" 
                   "000100" 
                   "000010" 
                   "000010"
                   "100000" 
                   "100000" 
                   "010000" 
                   "010000" 
                   "001000");
      break;
    case 3:
      display_leds("111111" 
                   "111111" 
                   "111111" 
                   "111111" 
                   "111111" 
                   "111111" 
                   "111111" 
                   "111111" 
                   "111111" 
                   "111111");
      break;
    default:
      state = 0;
      break;
    }
    if ((millis() - last_change) > 5000) {
      state++;
      last_change = millis();    
    }
  }

}


int main(void)
{
	init();

	setup();
    
	for (;;)
		loop();
        
	return 0;
}

