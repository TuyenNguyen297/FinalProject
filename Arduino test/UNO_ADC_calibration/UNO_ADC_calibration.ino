#define TdsSensorPin A1

unsigned int ADCValue;
double Voltage;
double Vcc;


float averageVoltage = 0, tdsValue = 0, temperature = 25;

void setup()
{
  Serial.begin(115200);
  pinMode(TdsSensorPin, INPUT);
  analogReference(EXTERNAL);
}

void loop()
{
  Vcc = readVcc() / 1000.0;
  Serial.print( "VCC: ");
  Serial.print( Vcc, DEC );
  Serial.println( "V");

  ADCValue = analogRead(TdsSensorPin);
  
  averageVoltage = (ADCValue / 1023.0) * 3.9;
   
  float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0); //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
  float compensationVolatge = averageVoltage / compensationCoefficient; //temperature compensation
  tdsValue = (133.42 * compensationVolatge * compensationVolatge * compensationVolatge - 255.86 * compensationVolatge * compensationVolatge + 857.39 * compensationVolatge) * 0.5; //convert voltage value to tds value

  Serial.print("Voltage:");
  Serial.print(averageVoltage, 2);
  Serial.println("V");
  Serial.print("TDS Value:");
  Serial.print(tdsValue, 0);
  Serial.println("ppm");
  delay(4000);
}

long readVcc()
{
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA, ADSC));
  result = ADCL;
  result |= ADCH << 8;
  result = 1125300L / result; // Back-calculate AVcc in mV
  return result;
}
