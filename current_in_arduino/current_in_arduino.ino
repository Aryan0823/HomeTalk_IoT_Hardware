#include <EmonLib.h>

EnergyMonitor emon1;

// Voltage of the electric
int voltage = 230;

// Pin of the SCT sensor
int sct_pin = A0;

void setup() {
  Serial.begin(9600);   
  // Pin, calibration - Current Constant = Ratio/BurdenR. 1800/62 = 29. 
  emon1.current(sct_pin, 29);
}

void loop() {
  // Calculate the current  
  double Irms = emon1.calcIrms(1480);
  
  // Show the current value
  Serial.print("Current: ");
  Serial.print(Irms, 2); // Irms

  // Calculate and show the power value
  double power = Irms * voltage;
  Serial.print(" Power: ");
  Serial.print(power, 2);
  Serial.println("");                 
  
  delay(1000);
}