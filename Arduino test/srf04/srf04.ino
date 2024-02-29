#define TRIG_PIN A5
#define ECHO_PIN A4
#define TIME_OUT 5000
 
float GetDistance()
{
  long duration;
  float distanceCm;
   
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  duration = pulseIn(ECHO_PIN, HIGH, TIME_OUT);
 
  // convert to distance
  distanceCm = duration / 29.1 / 2.0;
  
  return distanceCm;
}
 
void setup() {  
  Serial.begin(9600);
 
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}
 
void loop() {
  float distance = GetDistance();

    Serial.println(distance);
}
