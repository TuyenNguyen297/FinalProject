#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <VL53L0X.h>
#include <WiFi.h>
#include <WebServer.h>
#include <AutoConnect.h>
#include "FirebaseESP32.h"
#include <esp_adc_cal.h>

#define NumOfMachines  1
#define NumOfInputs    2
#define NumOfOutputs   6

#define FIREBASE_HOST "desalination-7bd70.firebaseio.com"         // Firebase Secrets
#define FIREBASE_AUTH "CWLhG0IlTGiqZtkrt7rXsje6wpmupkxLLoqY6HGs"  // **

#define DEBOUNCE_TIME 1000

#define REF_VOLTAGE 1128                                          // Reference voltage for cal.h functions
#define TdsSensorPin 32                                           //
#define Temperature 30.5                                          // Temperature assumed
#define VREF 3300.0                                               // Analog reference voltage of the ADC
#define resolution 4095.0                                         // 12bit ADC

#define FULL_VOLUME    3.0
#define FULL_REMAINING 2.7
#define EMPTY_POINT    0.0

#define MEASURE_SAMPLES 10.0                                      // Number of sampling
#define MEASURE_SAMPLE_DELAY 5                                    // Delay beetween samplings
#define filterFactor 0.9                                          // Low Pass Filter coefficient, from 0-0.99. The higher the coefficient, the smoother the distance

void FirebaseTask(void *pvParameters);                            // Firebase Task Declaration. Using to send State, TDS, Volume, FlowRate, Command to Firebase and get back the Command from phone
void ComputeTask(void *pvParameters);                             // Compute Task Declaration. Measuring sensors and handling any command from firebase to maintain the machine working state

byte PIN_BTN  = 14;
byte PIN_TDS  = 32;

byte PIN_LED_R = 19;
byte PIN_LED_G = 23;
byte PIN_LED_B = 3;
byte PIN_SDA   = 21;
byte PIN_SCL   = 22;

byte PIN_BUZZER = 2;
byte PIN_SLV    = 4;
byte PIN_PUMP   = 18;

const byte Input[NumOfMachines][NumOfInputs] =
{
  {PIN_BTN, PIN_TDS}
};

const byte Output[NumOfMachines][NumOfOutputs] =
{
  {PIN_LED_R, PIN_LED_G, PIN_LED_B, PIN_BUZZER, PIN_SLV, PIN_PUMP}
};

String IndexPath = "-M8J2nXEUJsSUesgR_2z";
String CMDPath   = "-M8PpSJxHvHiPAVSCaRH";

LiquidCrystal_I2C lcd(0x27, 20, 4);

VL53L0X ToF;                                                                            // I2C device with default address in library is 0x29

AutoConnect portal;                                                                     // Create an object of AutoConnect to handle wifi connection of ESP

FirebaseData firebaseData;                                                              // Create an object of FirebaseData to handle firebase task

esp_adc_cal_characteristics_t *adc_chars = new esp_adc_cal_characteristics_t;           // Create an object of ADC_cal to read ADC more accurately

void handleRoot()                                                                       // Create an HTML page to configure ESP's connection through it's IP address
{
  String page = PSTR(
                  "<html>"
                  "<head>"
                  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                  "<style type=\"text/css\">"
                  "body {"
                  "-webkit-appearance:none;"
                  "-moz-appearance:none;"
                  "font-family:'Arial',sans-serif;"
                  "text-align:center;"
                  "}"
                  ".menu > a:link {"
                  "position: absolute;"
                  "display: inline-block;"
                  "right: 12px;"
                  "padding: 0 6px;"
                  "text-decoration: none;"
                  "}"
                  ".button {"
                  "display:inline-block;"
                  "border-radius:7px;"
                  "background:#73ad21;"
                  "margin:0 10px 0 10px;"
                  "padding:10px 20px 10px 20px;"
                  "text-decoration:none;"
                  "color:#000000;"
                  "}"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<div class=\"menu\">" AUTOCONNECT_LINK(BAR_32) "</div>");
  page += String(F("</body></html>"));
  portal.host().send(200, "text/html", page);
}

bool atDetect(IPAddress softapIP)
{
  //Serial.println("Captive portal started, SoftAP IP:" + softapIP.toString());
  return true;
}
///////////////////////////////////////////////BTN DEBOUNCE/////////////////////////////////////////////////
hw_timer_t * timer1 = NULL;     // Timer for button debouncing
void IRAM_ATTR onTimer1()
{
  attachInterrupt(digitalPinToInterrupt(PIN_BTN), btnPress , FALLING);
  timerAlarmDisable(timer1);    // stop alarm
  timerDetachInterrupt(timer1); // detach interrupt
  timerEnd(timer1);             // end timer
}
///////////////////////////////////////////////ADC READ/////////////////////////////////////////////////
float analogRead_cal(uint8_t channel)
{
  float miliVol;
  analogReadResolution(12); // https://goo.gl/qwUx2d
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, REF_VOLTAGE, adc_chars);
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);
  int adcRead = analogRead(channel);

  if ( adcRead < 682)
  {
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_0, ADC_WIDTH_BIT_12, REF_VOLTAGE, adc_chars);
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_0);
    miliVol = esp_adc_cal_raw_to_voltage(analogRead(channel), adc_chars);
  }
  else
  {
    miliVol = adcRead * (float)VREF / resolution;
  }
  return (miliVol);
}
///////////////////////////////////////////////CLASS/////////////////////////////////////////////////
class Desalination
{
  private:
    String curCMD = "STOP";  // Save current command from phone
    String prevCMD;             // Save previous command from phone
    String espCMD;              // Command from machine
    String prevState;           // Save previous state of the machine
    String State;               // current working state of the machine
    String prvState;            // for lcd updating

    float tdsVoltage = 0;
    String tdsValue;
    String prvtdsValue;

    String Quality;
    String prvQuality;

    String Salinity;
    String prvSalinity;

    String Volume;
    String prvVolume;

    float smoothDistance = 5.0;

    long prevBuzTime = 0;                      // Keep the LCD updating not too quickly
    long prevLcdTime = 0;


    bool keyPressedBuz   = false;
    bool waitToSend      = false;                // Sign that ESP needs to postpone receiving old command for updating the lastest espCMD to firebase.
    bool tankWasJustFull = false;                // Check if Tank has just been out of full range
    bool tankIsFull      = false;                // Check if Tank is in full range right now

    long buzTime  = 0;
    bool buzState = LOW;
    bool debounceBtn = false;

    byte count = 0;

  public:
    ///////////////////////////////////////////////INIT CLASS/////////////////////////////////////////////////
    Desalination() {}
    void initIO()
    {
      for ( int i = 0 ; i < NumOfMachines; i++)
      {
        for (int j = 0; j < NumOfInputs; j++)
        {
          pinMode(Input[i][j] , INPUT_PULLUP);
          Serial.println(Input[i][j]);
        }
      }
      for (int i = 0; i < NumOfMachines ; i++)
      {
        for (int j = 0; j < NumOfOutputs; j++)
        {
          pinMode(Output[i][j] , OUTPUT);
          digitalWrite(Output[i][j], LOW);
        }
      }
    }
    ///////////////////////////////////////////////FIREBASE/////////////////////////////////////////////////
    void firebaseAccess()
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        if (waitToSend)
        {
          if (Firebase.setString(firebaseData, "Index/" + CMDPath + "/CMD", espCMD))
          {
            waitToSend = false;
          }
        }
        else
        {
          if (Firebase.getString(firebaseData, "Index/" + CMDPath + "/CMD"))
          {
            curCMD = firebaseData.stringData();
          }
        }
        if (Firebase.setString(firebaseData, "Index/" + IndexPath + "/ROIndex", State + "," + tdsValue + "," + Volume + "," + String(random(1, 255))))
        {
          //          Serial.println("Sent Successfully!");
        }
      }
    }
    ///////////////////////////////////////////////TANK/////////////////////////////////////////////////
    bool TankIsNotFull()
    {
      double vol = Volume.toDouble();
      if (vol >= FULL_VOLUME || vol >= FULL_REMAINING && tankIsFull)
      {
        tankIsFull = true;
      }
      else
      {
        tankIsFull =  false;
      }
      return !tankIsFull;
    }
    ///////////////////////////////////////////////STATE/////////////////////////////////////////////////
    void stateDetermine()
    {
      if (TankIsNotFull())                                                  // Tank not Full condition
      {
        if (State == "FULL")                                                // When Volume has been decreased after been full, State was still "FULL", it need set to "STOP"
        {
          State = "STOP";
          tankWasJustFull = true;
        }
        else if (prevCMD != curCMD)                                         // Check if saved previous online CMD is different from current online CMD
        {
          if (tankWasJustFull)                                              // Before tank is full, both online and offline CMD are RUN. When full, pump must stop until someone
          { // press btn on the machine or phone, but we face a problem if lacking of this condition. There are
            if (curCMD == "RUN")                                            // 2 scenarios: Pump would stop as expected, or pump would Re-run and stop right after that. That's because
            { // the offline CMD for stopping pump was not promptly sent to firebase, esp still get the old onl CMD is RUN
              curCMD = "STOP";                                              // so it will run for awhile until STOP CMD is set on firebase.
            }
            else
            {
              tankWasJustFull = false;
            }
          }
          else if (waitToSend)                                              // Offline controlling has higher priority than online do, so when button is pressed, online CMD would be set to offline CMD
          {
            curCMD  = espCMD;
            prevCMD = espCMD;
          }
          State = curCMD;
          prevCMD = curCMD;
        }
      }
      else
      {
        State = "FULL";                                                      //          prevCMD    = "STOP";
        espCMD     = "STOP";
        waitToSend =  true;
      }
    }
    ///////////////////////////////////////////////BUTTON/////////////////////////////////////////////////
    void btnDebounce()
    {
      Serial.println("press");
      detachInterrupt(digitalPinToInterrupt(PIN_BTN));
      if (!digitalRead(PIN_BTN))
      {
        if (TankIsNotFull())
        {
          State = (State == "FULL" || State == "STOP") ? "RUN" : "STOP";    // State has 4 values (RUN, STOP, FULL, NW) but command has only 2 of them. Therefore, when tank is not no feed inlet water and not full, it might be controlled "RUN" or "STOP"
          espCMD = State;
          waitToSend = true;                                                // Wont receive CMD of phone since btn has been pressed until espCMD would be sent successfully to firebase
        }
        keyPressedBuz = true;                                                  // Detect that btn was pressed to alert buzzer
        prevBuzTime = millis();
      }
      timer1 = timerBegin(1, 80, true);               // start time again
      timerAttachInterrupt(timer1, &onTimer1, true);  // attach interrupt again
      timerAlarmWrite(timer1, 1000 * 1000, false);    // start alarm again
      timerAlarmEnable(timer1);
    }
    ///////////////////////////////////////////////ACTUATORS/////////////////////////////////////////////////
    void controlActuators()
    {
      if (State == "RUN")
      {
        pumpON(); valveON();
      }
      else
      {
        pumpOFF();
        valveOFF();
      }
    }
    ///////////////////////////////////////////////LED/////////////////////////////////////////////////
    void controlLed()
    {
      if (State == "RUN")
      {
        redOFF(); greenON(); blueOFF();
      }
      else
      {
        if ( State == "STOP")
        {
          redON(); greenOFF(); blueOFF();
        }
        else if ( State == "FULL")
        {
          redOFF(); greenOFF(); blueON();
        }
        else
        {
          redON(); greenON(); blueON();
        }
      }
    }
    ///////////////////////////////////////////////BUZZER/////////////////////////////////////////////////
    void controlBuzzer()
    {
      if (keyPressedBuz || prevState != State)
      {
        if (State == "STOP")
        {
          buzTime = 50;
          count = 1;
        }
        else if (State == "RUN")
        {
          buzTime = 50;
          count = 2;
        }
        else if ( State == "FULL")
        {
          buzTime = 1000;
          count = 1;
        }
        else
        {
          buzTime = 300;
          count = 3;
        }
        count *= 2 ;
        prevState = State;
        keyPressedBuz = false;
      }
      if ( count > 0)
      {
        buzState = count % 2 ? HIGH : LOW;
        digitalWrite(PIN_BUZZER, buzState);
        if (millis() - prevBuzTime >= buzTime)
        {
          count --;
          prevBuzTime = millis();
        }
      }
      else
      {
        digitalWrite(PIN_BUZZER, LOW);
      }
    }
    ///////////////////////////////////////////////TDS/////////////////////////////////////////////////
    void getTDS()
    {
      tdsVoltage = analogRead_cal(TdsSensorPin) / 1000.0; // read the analog value more stable by the median filtering algorithm, and convert to voltage value
      float compensationCoefficient = 1.0 + 0.02 * (Temperature - 25.0); //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
      float compensationVolatge = (tdsVoltage) / compensationCoefficient; //temperature compensation
      float tds = (133.42 * compensationVolatge * compensationVolatge * compensationVolatge - 255.86 * compensationVolatge * compensationVolatge + 857.39 * compensationVolatge) * 0.5;
      double volume = Volume.toDouble();
      if (volume <= EMPTY_POINT)
      {
        tdsValue = "0";
      }
      else if ( tds < 28.5)
      {
        tdsValue = "< " + String(28.0, 0);
      }
      else if ( tds < 1000)
      {
        tdsValue = String(tds, 0);
      }
    }
    ///////////////////////////////////////////////SALINITY/////////////////////////////////////////////////
    void cvSalinity()
    {
      char Comparator;
      float tds;
      if (!isDigit(tdsValue.charAt(0)))
      {
        Comparator = tdsValue.charAt(0);
        tds = (tdsValue.substring(1)).toFloat();
      }
      else
      {
        tds = tdsValue.toFloat();
      }
      float EC = tds / 0.64;
      float Sal = EC / 2.12 / 10000;
      Salinity = Comparator == '<' ? Comparator + String(Sal, 3) : (Sal == 0 ? "0     " : String(Sal , 3) + " ");
    }
    ///////////////////////////////////////////////QUALITY/////////////////////////////////////////////////
    void getQuality()
    {
      Quality = Salinity.toFloat() <= 0.025 ? "GOOD" : "BAD ";
    }
    ///////////////////////////////////////////////DISTANCE/////////////////////////////////////////////////
    float getRawDistance()
    {
      float ToF_FACTOR;
      float rawDistance = ToF.readRangeContinuousMillimeters();
//            if ( rawDistance > 99)
//            {
//              ToF_FACTOR = 0.77;//0.7838;0.786
//            }
//            else if ( rawDistance > 90)
//            {
//              ToF_FACTOR = 0.72;//0.7838;0.786
//            }
//            else if (rawDistance > 65)
//            {
//              ToF_FACTOR = 0.806;//0.806
//            }
//            else if (rawDistance > 43)
//            {
//              ToF_FACTOR = 0.8566;//0.8566
//            }
//            else
//            {
//              ToF_FACTOR = 0.908;//0.908
//            }
      if ( rawDistance > 130)
      {
        ToF_FACTOR = 0.75;//0.7838;0.786
      }
      else if ( rawDistance > 120)
      {
        ToF_FACTOR = 0.63;//0.7838;0.786
      }
      else if (rawDistance > 80)
      {
        ToF_FACTOR = 0.77;//0.806
      }
      else if (rawDistance > 60)
      {
        ToF_FACTOR = 0.8;//0.8566
      }
      else
      {
        ToF_FACTOR = 0.908;//0.908
      }
      rawDistance = roundf(rawDistance * ToF_FACTOR * 10) / 100.0;
      return rawDistance;
    }
    double averageRawDistance()
    {
      double measureSum;
      for (int i = 0; i < MEASURE_SAMPLES; i++)
      {
        delay(MEASURE_SAMPLE_DELAY);
        measureSum += getRawDistance();
      }
      measureSum /= MEASURE_SAMPLES;
      return measureSum;
    }
    float smooth(float sensor_reading, float filterValue, float smoothedValue)
    {
      // Checking validity of filterValue; if beyond range, set to max/min value if out of range.
      if (filterValue > 1)
      {
        filterValue = .99;
      }
      else if (filterValue <= 0)
      {
        filterValue = 0;
      }
      smoothedValue = (sensor_reading * (1 - filterValue)) + (smoothedValue  *  filterValue);
      return smoothedValue;
    }
    float getDistance()
    {
      smoothDistance =  smooth(averageRawDistance(), filterFactor , smoothDistance);
      return smoothDistance;
    }
    void getVolume ()
    {
      float R = 5.18;
      float L = 49.0;
      float distance = getDistance();
      distance = distance < 10.35 ? distance : 10.35;
      float S = M_PI * R * R - R * R * acos ( (R - distance) / R ) + (R - distance) * sqrtf (2 * R * distance - distance * distance);
      float V = S * L / 1000.0;
      Volume = String(V, 1);
    }
    ///////////////////////////////////////////////LCD/////////////////////////////////////////////////
    void showDefaultLCD()
    {
      Wire.setClock(10000);
      lcd.setCursor(2, 0); lcd.print("KHKT 2020-2021");

      lcd.setCursor(0, 1) ; lcd.print("TDS:");
      lcd.setCursor(8, 1) ; lcd.print("ppm");

      lcd.setCursor(12, 1); lcd.print("VOL:");
      lcd.setCursor(19, 1); lcd.print("L");

      lcd.setCursor(0, 2); lcd.print("SAL:");
      lcd.setCursor(10, 2); lcd.print("%");

      lcd.setCursor(12, 2) ; lcd.print("STT:");

      lcd.setCursor(5, 3); lcd.print("QLT:");
    }
    void showIndex()
    {
      if (millis() - prevLcdTime > 1000)
      {
        Wire.setClock(10000);
        if (prvtdsValue != tdsValue)
        {
          lcd.setCursor(4, 1);
          lcd.print(tdsValue == "< 28" ? "<28 " : (tdsValue.toDouble() < 10 ? tdsValue + "   " : ((tdsValue.toDouble() >= 10 && tdsValue.toDouble() < 100) ? tdsValue + "  " : tdsValue )));
          prvtdsValue = tdsValue;
        }
        if (prvVolume != Volume)
        {
          lcd.setCursor(16, 1);
          lcd.print(Volume);
          prvVolume = Volume;
        }
        if (prvSalinity != Salinity)
        {
          lcd.setCursor(4, 2);
          lcd.print(Salinity);
          prvSalinity = Salinity;
        }
        if (prvState != State)
        {
          lcd.setCursor(16, 2); lcd.print(State == "RUN" ? "RUN " : State);
          prvState = State;
        }
        if (prvQuality != Quality)
        {
          lcd.setCursor(10, 3);
          lcd.print(Quality);
          prvQuality = Quality;
        }
        prevLcdTime = millis();
      }
    }
    //////////////////////////////////////////DIGITAL WRITE//////////////////////////////////////////////////
    void pumpON()
    {
      digitalWrite(PIN_PUMP  , HIGH);
    }
    void pumpOFF()
    {
      digitalWrite(PIN_PUMP  , LOW );
    }
    void valveON()
    {
      digitalWrite(PIN_SLV   , HIGH);
    }
    void valveOFF()
    {
      digitalWrite(PIN_SLV   , LOW );
    }
    void redON()
    {
      digitalWrite(PIN_LED_R , HIGH);
    }
    void redOFF()
    {
      digitalWrite(PIN_LED_R , LOW );
    }
    void greenON()
    {
      digitalWrite(PIN_LED_G , HIGH);
    }
    void greenOFF()
    {
      digitalWrite(PIN_LED_G , LOW );
    }
    void blueON()
    {
      digitalWrite(PIN_LED_B , HIGH);
    }
    void blueOFF()
    {
      digitalWrite(PIN_LED_B , LOW );
    }
};

Desalination RO;
///////////////////////////////////////////////ACCESS BUTTONPRESS METHOD OF RO OBJECT/////////////////////////////////////////////
void btnPress()
{
  RO.btnDebounce();
}
void setup()
{
//  Serial.begin(115200);
  RO.initIO();
  xTaskCreatePinnedToCore(ComputeTask,
                          "ComputeTask",
                          10000,            // Stack size
                          NULL,
                          1,
                          NULL,
                          0);

  xTaskCreatePinnedToCore(FirebaseTask,
                          "FirebaseTask",   // A name just for humans
                          10000,            // This stack size can be checked & adjusted
                          NULL,
                          2,                // Priority, with 3 (configMAX_PRIORITIES - 1)
                          NULL,
                          1);

}

void FirebaseTask( void * pvParameters )
{
  portal.onDetect(atDetect);
  if (portal.begin())
  {
    WebServerClass& server = portal.host();
    server.on("/", handleRoot);
    Serial.println("Started, IP:" + WiFi.localIP().toString());
  }
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  Firebase.setReadTimeout(firebaseData, 60 * 1000);
  Firebase.setwriteSizeLimit(firebaseData, "tiny");

  for (;;)
  {
    portal.handleClient();
    RO.firebaseAccess();
    vTaskDelay(200);
  }
}

void ComputeTask( void * pvParameters )
{
  Wire.begin();
  lcd.init();
  lcd.backlight();

  ToF.init();
  ToF.setTimeout(500);
  ToF.startContinuous();

  attachInterrupt(digitalPinToInterrupt(PIN_BTN), btnPress , FALLING);
  RO.showDefaultLCD();

  for (;;)
  {
    RO.stateDetermine();
    RO.controlLed();
    RO.controlBuzzer();
    RO.controlActuators();
    RO.getTDS();
    RO.cvSalinity();
    RO.getQuality();
    RO.getVolume();
    RO.showIndex();
    vTaskDelay(100);
  }
}

void loop()
{
}
