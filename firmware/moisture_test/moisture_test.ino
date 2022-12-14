#include <Wire.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include "I2C_AHT10.h"
#include <SoftwareSerial.h>

#include "config.h"

SoftwareSerial lorawan_serial(LORAWAN_TX, LORAWAN_RX);
AHT10 aht;

bool readSensorStatus = false;
int soil_adc = 0; // variable to store the value coming from the sensor
int soil_percent = 0;
int bat_adc = 0; // the voltage of battery
int bat_vol = 0;

float temperature = 0.0;
float humidity = 0.0;

int count = 0; // WDT count

void setup()
{
    pinMode(SENSOR_POWER_PIN, OUTPUT);
    pinMode(LORAWAN_RST, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT);

    // Ra08 restart
    digitalWrite(LED_PIN, 1);

    digitalWrite(LORAWAN_RST, 0);
    delay(1000);
    digitalWrite(LORAWAN_RST, 1);
    digitalWrite(LED_PIN, 0);

    lorawan_serial.begin(9600); // your esp's baud rate might be different
    log_init();
    sendData("AT", 3000);
}

void loop()
{
    log_out("");
    log_out("-----------------------------------");

    log_out_num("Count:", count++);
    read_sensor();
    sensor_log();

    log_out("-----------------------------------");
    log_out("");

    if (digitalRead(BUTTON_PIN) == 0)
    {
        delay(100);
        if (digitalRead(BUTTON_PIN) == 0)
        {
            lorawan_join();
        }
    }
}

// ---------------------- Task -----------------------------
void read_sensor()
{
    digitalWrite(SENSOR_POWER_PIN, HIGH); // Sensor Power ON
    digitalWrite(LED_PIN, 1);

    readSensorStatus = false;
    for (int i = 0; i < 3; i++)
    {
        if (readSensorStatus == false)
        {
            readSensorStatus = aht_init();
        }
    }

    if (readSensorStatus == true)
    {
        digitalWrite(LED_PIN, 0);
    }
    else
    {
        while (1)
        {
            log_out("!!!!!!!!!!!!!!!!AHT10 ERROR!!!!!!!!!!!!!!!!");
            delay(2000);
        }
    }

    digitalWrite(SENSOR_POWER_PIN, LOW); // Sensor Power Off
    delay(100);

    pwn_init();
    delay(50);

    soil_adc = analogRead(A2);
    soil_percent = (int)((soil_adc - SOIL_ADC_WATER) / SOIL_ADC_UNIT);

    bat_adc = analogRead(A3);
    bat_vol = (int)(bat_adc * 690.0 / 470.0 * 3300.0 / 1024.0);

    if (soil_percent > 100)
        soil_percent = 100;
    else if (soil_percent < 0)
        soil_percent = 0;

    bat_vol = bat_vol / 100;
}

// -------------------- Lorawan ---------------------------------

void sendData(String command, const int timeout)
{
    String response = "";

    log_out(command.c_str());
    lorawan_serial.println(command); // send the read character to the Serial

    long int time = millis();

    while ((time + timeout) > millis())
    {
        while (lorawan_serial.available())
        {
            char c = lorawan_serial.read(); // read the next character.

            if (c == '\n')
            {
                log_out(response.c_str());
                response = "";
            }
            else if (c == '\r')
                continue;
            else
                response += c;
        }
    }

    log_out(response.c_str());
}


int sendData_keyword(String command, const int timeout, String keyword)
{
    String response = "";

    log_out(command.c_str());
    lorawan_serial.println(command); // send the read character to the Serial

    long int time = millis();

    while ((time + timeout) > millis())
    {
        while (lorawan_serial.available())
        {
            char c = lorawan_serial.read(); // read the next character.

            if (c == '\n')
            {
                log_out(response.c_str());

                if (response.indexOf(keyword) != -1)
                {
                    return 1;
                }

                response = "";
            }
            else if (c == '\r')
                continue;
            else
                response += c;
        }
    }

    log_out(response.c_str());
    return 0;
}

void lorawan_join()
{
    sendData("AT", 1000);

    char cmd[80];

    sprintf(cmd, "%s%s", "AT+CDEVEUI=", DEVEUI);
    sendData(cmd, AT_TIMEOUT);
    sprintf(cmd, "%s%s", "AT+CAPPEUI=", APPEUI);
    sendData(cmd, AT_TIMEOUT);
    sprintf(cmd, "%s%s", "AT+CAPPKEY=", APPKEY);
    sendData(cmd, AT_TIMEOUT);

    sendData("AT+CCLASS=0", AT_TIMEOUT);
    sendData("AT+CJOINMODE=0", AT_TIMEOUT);
    sendData_keyword("AT+CJOIN=1,0,8,8", 30000, "Joined");
    sendData_keyword("AT+DTRX=1,2,8,12345678", 30000, "OK+SENT");
}

// --------------------- Device init --------------------------

bool aht_init()
{
    bool ret = false;
    Wire.begin();
    delay(1000);

    if (aht.begin() == false)
        log_out("AHT10 not detected. Please check wiring. Freezing.");
    else
    {

        while (1)
        {
            if (aht.available() == true)
            {
                temperature = aht.getTemperature();
                humidity = aht.getHumidity();
                ret = true;
                log_out("AHT10 Read Success.");
                break;
            }
            delay(1000);
        }
    }
    Wire.end();

    return ret;
}

void pwn_init()
{
    pinMode(PWM_OUT_PIN, OUTPUT);    // digitalWrite(PWM_OUT_PIN, LOW);
    TCCR1A = bit(COM1A0);            // toggle OC1A on Compare Match
    TCCR1B = bit(WGM12) | bit(CS10); // CTC, scale to clock
    OCR1A = 1;                       // compare A register value (5000 * clock speed / 1024).When OCR1A == 1, PWM is 2MHz
}

// ---------------------- Log ---------------------------

void log_init()
{
#if DEBUG_OUT_ENABLE
    Serial.begin(9600);
#endif
}

void log_out(const char *log)
{
#if DEBUG_OUT_ENABLE
    Serial.println(log);
#endif
}

void log_out_num(const char *log, int num)
{
#if DEBUG_OUT_ENABLE
    Serial.print(log);
    Serial.println(num);
#endif
}

void sensor_log()
{

    log_out("");
    log_out_num("BAT ADC       :", bat_adc);
    log_out_num("BAT VOL       :", bat_vol);
    log_out_num("SOIL ADC      :", soil_adc);
    log_out_num("SOIL PER      :", soil_percent);
    log_out_num("TEMPERAUTRE   :", (int)temperature);
    log_out_num("HUMIDITY      :", (int)humidity);
    log_out("");
}
