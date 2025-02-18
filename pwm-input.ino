const uint8_t JITTER_MARGIN = 12; // Raise this value for pwm signals with lots of jitter or with lower frequency.
                                  // Lower this value for more accuracy near 0% or 100%.
                                  //
                                  // For a 25 KHz pwm frequency and JITTER_MARGIN of 12:
                                  //    * Jitter of up to 1% is tolerated.
                                  //    * PWM signals above 98% and below 2% will get cut to 100% and 0% respectively.
                                  // 
                                  // If halving pwm frequency, double JITTER_MARGIN for the same jitter tolerance.

void setup() {
  Serial.begin(115200);
  while (!Serial);

  noInterrupts ();  // protected code
  // reset Timer 1
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  TIMSK1 = 0;

  TIFR1 |= _BV(ICF1); // clear Input Capture Flag so we don't get a bogus interrupt
  TIFR1 |= _BV(TOV1); // clear Overflow Flag so we don't get a bogus interrupt

  TCCR1B = _BV(CS10) | // start Timer 1, no prescaler
           _BV(ICES1); // Input Capture Edge Select (1=Rising, 0=Falling)

  TIMSK1 |= _BV(ICIE1); // Enable Timer 1 Input Capture Interrupt
}

volatile uint8_t state = 0; 
volatile uint32_t r1 = 0;
volatile uint32_t r2 = 0;
volatile uint32_t f = 0;
volatile uint32_t fallback = 0;

ISR(TIMER1_CAPT_vect) {
  switch (state) {
    case 0:
      r1 = ICR1;
      state = 1;
      break;
    case 1:
      r2 = ICR1;
      state = 2;
      TCCR1B &= ~_BV(ICES1); // Switch to falling edge
      break;
    case 2:
      f = ICR1;
      for (uint8_t i = 0; i < JITTER_MARGIN + 4; i++) {
        asm volatile ("nop"); // Don't optimize this away!!
      }
      fallback = digitalRead(8);
      state = 3;
      break;
    default:
      break;
  }
}

void loop() {
  // START PWM INPUT DETECT
  noInterrupts();
  state = 0;
  r1 = 0;
  r2 = 0;
  f = 0;
  TCNT1 = 0;
  TCCR1B |= _BV(ICES1);
  TIFR1 |= _BV(ICF1);
  interrupts();  
  delayMicroseconds(140); // 3.5 cycles. At 25 kHz each cycle is 40 microseconds

  uint32_t period = 0;
  uint32_t high = 0;
  uint8_t pwm = 0;

  if (state == 3) {
    period = r2 - r1;
    high = f - r2;
    high = high % period;
    pwm = int((float) high / (float) period * 255.0 + 0.5);
    if (high < JITTER_MARGIN || high > period - JITTER_MARGIN) {
     pwm = fallback;
     pwm *= 255;
    }
  } else {
    pwm = digitalRead(8);
    pwm *= 255;
  }
  // END PWM INPUT DETECT

  // USER CODE
  Serial.print("State: ");
  Serial.println(state);
  Serial.print("PWM: ");
  Serial.println(pwm);
  Serial.print("High: ");
  Serial.println(high);
  Serial.print("Period: ");
  Serial.println(period);
  Serial.println("");
  delay(1000)
}
