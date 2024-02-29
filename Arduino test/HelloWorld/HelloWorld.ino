//YWROBOT
//Compatible with the Arduino IDE 1.0
//Library version:1.1
#include <LiquidCrystal_I2C.h>

byte PIN_SLV = 4;
byte PIN_PUMP = 18;
byte PIN_BTN = 14;

LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 16 chars and 2 line display

bool state = 0;
void setup()
{
  lcd.init();
  pinMode(4, OUTPUT);// initialize the lcd
  pinMode(18, OUTPUT);
  pinMode(14, INPUT);
  digitalWrite(4, LOW);
  digitalWrite(18, LOW);
  // Print a message to the LCD.
  lcd.backlight();
}


void loop()
{
  if (!digitalRead(PIN_BTN))
  {
    state = !state;
    digitalWrite(4, state);
    digitalWrite(18, state);
  }
  lcd.setCursor(0, 0);
  lcd.print("TDS:");
  lcd.setCursor(7, 0);
  lcd.print("1000");
  lcd.setCursor(13, 0);
  lcd.print(" ppm");
  lcd.setCursor(0, 0);
  delay(1000);
  //delay(400);

  //  lcd.setCursor(0, 2);
  //  lcd.print("V:");
  //  lcd.print("1.1");
  //  lcd.print("L");
  //
  //  lcd.setCursor(0, 3);
  //  lcd.print("Stt:");
  //  lcd.print("RUN");
  //
  //  lcd.setCursor(10, 1);
  //  lcd.print("Fl:");
  //  lcd.print("1.2");
  //  lcd.print("L/m");
  //
  //  lcd.setCursor(10, 2);
  //  lcd.print("Salt:");
  //  lcd.print("0.003");
  //  lcd.print("%");
  //
  //  lcd.setCursor(10, 3);
  //  lcd.print("Qu:");
  //  lcd.print("GOOD");
}
