/*
 * demo driver for the LedMatrix library
 */

#include <LedMatrix.h>

// ApuPlunger pin outs
// data pin = 7
// clock pin = 8
// output enable pin = 6
LedMatrix matrix(7,8,6);

void setup() {
  Serial.begin(9600);
  Serial.println("Starting");
}

static int state = 0;
static unsigned long last_change = 0;

void loop() {
  matrix.RunStateMachine();
  demo2();
}

static void demo0() {
  if (matrix.IsReady()) {
    Serial.println("Matrix ready");
    Serial.print("State: ");
    Serial.println(state, DEC);
     switch(state) {
    case 0:
      matrix.DisplayLeds("444444" 
                         "444444"
                         "444444"
                         "444444"
                         "444444"
                         "444444"
                         "444444"
                         "444444"
                         "444444"
                         "444444");
      break;
    case 1:
      matrix.DisplayLeds("000000"
                         "000000"
                         "444444"
                         "444444"
                         "444444"
                         "444444"
                         "444444"
                         "444444"
                         "444444"
                         "444444"
                         "444444");
      break;
    }
    if ((millis() - last_change) > 5000) {
      if (state > 1) {
        state = 0;
      } else {
        state++;
      }
      last_change = millis();    
    }
  }

}

static void demo1() {
  if (matrix.IsReady()) {
     switch(state) {
    case 0:
      matrix.DisplayLeds("111111" 
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
      matrix.DisplayLeds("100000" 
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
      matrix.DisplayLeds("001000" 
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
      matrix.DisplayLeds("111111" 
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


static void fill_rows(int row_count, char *val) {
  for (int i = 0 ; i < LED_NUM_ROWS; ++i) {
    for (int j = 0; j < LED_NUM_COLS; ++j) {
      if (i <= row_count) {
        *val = '9';
      } else {
        *val = '0';
      }
      val++;
    }
  }
}

static void fill_rows2(int row_count, char *val) {
  for (int i = 0 ; i < LED_NUM_ROWS; ++i) {
    for (int j = 0; j < LED_NUM_COLS; ++j) {
      if (i <= row_count) {
        *val = '9';
      } else {
        *val = '0';
      }
      val++;
    }
  }
}

static void demo2() {
  char vals[LED_NUM_ROWS * LED_NUM_COLS];
  unsigned long start;

  for (int i = 0 ; i < LED_NUM_ROWS; ++i) {
    fill_rows2(i, vals);
    // display the LEDs.
    start = millis();
    while (millis() - start < 200) {
      matrix.DisplayLeds(vals);
      do {
        matrix.RunStateMachine();      
      } while(!matrix.IsReady());
    }
  }

  for (int i = LED_NUM_ROWS - 1 ; i >=0 ; --i) {
    fill_rows2(i, vals);
    start = millis();
    while (millis() - start < 200) {
      matrix.DisplayLeds(vals);
      do {
        matrix.RunStateMachine();
      } while (!matrix.IsReady());
    }
  }
}

