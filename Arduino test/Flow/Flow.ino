volatile double waterVolume;
volatile double waterFlowRate = 0.0;
volatile int previousTime = 0;
boolean isTriggered = false;
double pulseWidth = 0.0;

boolean State = LOW;

void IRAM_ATTR pulseLow()
{
  pulseWidth = millis() - previousTime;
  waterFlowRate = (1000.0 / 5880.0) / pulseWidth;
}

void IRAM_ATTR pulseHigh()   //measure the quantity of square wave
{
  isTriggered = true;
  waterVolume += 1.0 / 5880.0;
  attachInterrupt(digitalPinToInterrupt(27), pulseLow, FALLING);
  previousTime = millis();
}



void setup()
{
  Serial.begin(115200);
  waterVolume = 0;

  pinMode(27, INPUT);
  pinMode(14, INPUT_PULLUP);
  pinMode(18, OUTPUT);
  pinMode(4, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(27), pulseHigh, RISING);  //DIGITAL Pin 2: Interrupt 0
}

void loop()
{
  if (!digitalRead(14))
  {
    State = !State;
    digitalWrite(18, State);
    digitalWrite(4, State);
    Serial.println("ok");
    Serial.println(State);
  }
  Serial.print("Total Water Volume:");
  Serial.print(waterVolume);
  Serial.println("   L");
  Serial.print("Flow Rate:");
  if (isTriggered)
  {
    Serial.print(waterFlowRate);
    isTriggered = false;
  } else
  {
    Serial.print("0.0");
  }
  Serial.println("  L/min");
  delay(500);
}
