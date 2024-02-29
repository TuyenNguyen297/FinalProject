/*
   ESP32 ADC multi readings

   All time ADC pins:
   ADC1_CH0,ADC1_CH03,ADC1_CH04
   ADC1_CH05,ADC1_CH06,ADC1_CH07

   Use only without WiFi:
   ADC2_CH0,ADC2_CH01,ADC2_CH02,ADC2_CH03
   ADC2_CH04,ADC2_CH05,ADC2_CH06
   ADC2_CH07,ADC2_CH08,ADC2_CH09

   Arduino espressif doc: https://goo.gl/NpUo3Z
   Espressif doc: https://goo.gl/hcUy5U
   GPIO: https://goo.gl/k8FGGD
*/

#include <esp_adc_cal.h>

// Command to see the REF_VOLTAGE: espefuse.py --port /dev/ttyUSB0 adc_info
// or dc2_vref_to_gpio(25)
#define REF_VOLTAGE 1128
#define TdsSensorPin 32
#define VOL_OFFSET 0.142

esp_adc_cal_characteristics_t *adc_chars = new esp_adc_cal_characteristics_t;

float averageVoltage = 0, tdsValue = 0, temperature = 31;



float analogRead_cal(uint8_t channel, adc_atten_t attenuation) {
  adc1_channel_t channelNum;
  adc1_config_channel_atten(ADC1_CHANNEL_4, attenuation);
  return esp_adc_cal_raw_to_voltage(analogRead(channel), adc_chars);
}

void setup() 
{
  Serial.begin(115200);
  pinMode(TdsSensorPin, INPUT);
  analogReadResolution(12); // https://goo.gl/qwUx2d
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, REF_VOLTAGE, adc_chars);
}

void loop()
{
    /*
      The maximum voltage is limited by VDD_A
      - 0dB attenuaton (ADC_ATTEN_DB_0) gives full-scale voltage 1.1V
      - 2.5dB attenuation (ADC_ATTEN_DB_2_5) gives full-scale voltage 1.5V
      - 6dB attenuation (ADC_ATTEN_DB_6) gives full-scale voltage 2.2V
      - 11dB attenuation (ADC_ATTEN_DB_11) gives full-scale voltage 3.9V
    */
    averageVoltage = analogRead_cal(TdsSensorPin, ADC_ATTEN_DB_11)/1000.0; // read the analog value more stable by the median filtering algorithm, and convert to voltage value
    // Default is 11db, full scale     
    Serial.print("Direct:");
    Serial.print(averageVoltage*1000.0,3);
    Serial.println(" mV");
    Serial.print("Minus Offset:");
    Serial.print(averageVoltage*1000.0 - VOL_OFFSET*1000,3);
    Serial.println(" mV");
       
    
    float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0); //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
    float compensationVolatge = (averageVoltage - VOL_OFFSET) / compensationCoefficient; //temperature compensation
    tdsValue = (133.42 * compensationVolatge * compensationVolatge * compensationVolatge - 255.86 * compensationVolatge * compensationVolatge + 857.39 * compensationVolatge) * 0.5; //convert voltage value to tds value
    
    Serial.print("TDS Value:");
    Serial.print(tdsValue, 0);
    Serial.println("ppm");
    delay(1000);
}
