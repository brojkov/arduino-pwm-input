# arduino-pwm-input
Accurately read fast and noisy PWM signals on an Arduino.

## Background
I wrote this when I could not find Arduino code online that could accurately read 25kHz pwm signals and handle all the edge cases.

The only way to get sufficiently accurate timing on an arduino input is to use the input capture interrupt with a hardware timer. You must use Timer1, as you will need 16 bits. This uses pin PB0, or pin 8 on the Uno. It also means that functionality which relies on Timer1 will not be avaliable.

**Edge cases you must handle for reliable PWM**
* 100% or 0% PWM signal with no edges
* Sporadically missing edges, or edges which do not reliably trigger the interrupt. This can happen when a PWM signal is close to 0 or 100%
* Jitter: Noise that causes edges to arrive early or late.

## How this works
To compute the PWM signal requires two pieces of information: the period of the signal, and the duration for which it is high.

### Basic Operation
We will detect and time two subsequent rising edges, r1 and r2, and then a falling edge, f:
```
PWM Signal            ____________/‾‾‾‾‾‾\____________/‾‾‾‾‾‾\____________/‾‾‾‾‾‾\__
Timer1:               (reset) 0.1.2.3................................................
Input Capture Mode:   (rising) ...*    ...............* (falling) ...............*
Edge captured:                    r1=2                r2=12                      f=26
```
The period is r2 - r1\
The high time is (f - r2) % period\
Note that the time it takes to reconfigure the input capture mode to falling means we may detect f in the next cycle or the one after that. This is the reason to modulo with period. In this example, the next falling edge occurs before this configuration is finished (at Timer1=16), and is therefore missed. However we compute (26 - 12) % 10 = 4, the correct value.

### Handling Jitter
If the period is close to 0 or 100, then the rising and falling edges are very close together.\
If the input signal has jitter, we may not know which cycle the falling edge, f, belongs to.\
We introduct JITTER_MARGIN: the number of Timer1 cycles for which we consider the edges too close to reliably distinguish.
If the edges are too close based on JITTER_MARGIN. We fall back to a value which is recorded shortly after the falling edge, f.\
**Note the side effect of JITTER_MARGIN:**
Values close to 100 or 0 will get pinned to 100 or 0. As explained in the comment, for a 25 kHz input and a JITTER_MARGIN of 12:
* Jitter of 1% in the input signal is tolerated with no errors.
* Any signal above 98% or below 2% will get cut off to 100% or 0% respectively.
The tolerance to jitter changes proportionally with JITTER_MARGIN, and the cut-off value is inversely proportional.

### Handling Missing Edges
This case is much simpler than jitter. We sleep for 3.5 cycles and check if we recorded f.
If we did not get to f, then we are missing edges, and the PWM signal must be 0 or 100.
We read the pwm pin for a fallback value.

## Using and Modifying this code
* Connect your input PWM signal to PB0 / pin 8
* Add your application code to the setup and main loop.
  * The value of the incoming PWM signal is avaliable to you as the variable 'pwm' and ranges from 0 to 255, corresponding to a pwm of 0 to 100%.
  * pwm is updated every time the code in the main loop runs.
  * This update takes about 4x the period of the input PWM signal. You can split up the logic for reading the PWM signal over multiple cycles of the main loop if you need it to run faster.
* Different PWM period? Update these lines:
  * 71 - delayMicroseconds(set to 3.5x the period of your PWM signal. The default is for 25 kHz)
  * 1 - JITTER_MARGIN (Optional. Read the comment)
  * 24 - If the period is much slower than 25 kHz, you will need to modify the Timer 1 prescaler to avoid overflow. Every halving in the prescaler will implicitly double your jitter margin in real time.
* Noisy PWM signal causing errors? Increase JITTER_MARGIN.
* Want to lower the cutoff for signals close to 0 or 100? Decrease JITTER_MARGIN.
