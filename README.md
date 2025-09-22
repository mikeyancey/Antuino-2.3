# antuino2.3

This updated version includes:

 - The Control Knob is now Interrupt based encoder handling which replaces polled encoder motion (updateEncoder).
 - Tidied up DVM for stability and consistency.
 - All <b>Serial.Prints</b> instructions are enclosed by #ifdef DEBUG. Just define DEBUG to debug with <b>Serial.Print</b>.
