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
int tx_count = 0;

float temperature = 0.0;
float humidity = 0.0;

int count = 0; // WDT count

void setup()
{
    pinMode(SENSOR_POWER_PIN, OUTPUT);
    pinMode(LORAWAN_RST, OUTPUT);
    // pinMode(LED_PIN, OUTPUT);

    // Ra08 restart
    digitalWrite(LORAWAN_RST, 0);
    delay(1000);
    digitalWrite(LORAWAN_RST, 1);

    lorawan_serial.begin(9600); // your esp's baud rate might be different
    log_init();

    delay(1000);
    log_out("---------------- Start----------------");
    delay(100);

    log_out("[First Task]");

    read_sensor();

    lorawan_init();

    while (lorawan_join())
    {
        send_lorawan();
        break;
    }

    log_out("[Task Over]");

    low_power();
}

void loop()
{
    wdt_disable();

    if (count > SLEEP_CYCLE) //(7+1) x 8S  450
    {

        log_out("[Task Start]");

        read_sensor();

        while (lorawan_join())
        {
            send_lorawan();
            break;
        }

        log_out("[Task Over]");

        // count init
        count = 0;
    }

    delay(50);
    low_power();
}

// ---------------------- Task -----------------------------
void read_sensor()
{
    digitalWrite(SENSOR_POWER_PIN, HIGH); // Sensor Power ON

    readSensorStatus = false;
    for (int i = 0; i < 3; i++)
    {

        if (readSensorStatus == false)
            readSensorStatus = aht_init();

        delay(500);
    }

    digitalWrite(SENSOR_POWER_PIN, LOW); // Sensor Power Off
    delay(100);

    pwn_init();
    delay(50);

    int ADC_O_1; // ADC Output First 8 bits
    int ADC_O_2; // ADC Output Next 2 bits

    // Soil Read
    // ADC2  AVCC as reference voltage
    ADMUX = _BV(REFS0) | _BV(MUX1);
    ADCSRA = _BV(ADEN) | _BV(ADPS1) | _BV(ADPS0);

    delay(50);

    soil_adc = 0;
    for (int i = 0; i < 3; i++)
    {
        // start ADC conversion
        ADCSRA |= (1 << ADSC);

        delay(10);

        if ((ADCSRA & 0x40) == 0)
        {
            ADC_O_1 = ADCL;
            ADC_O_2 = ADCH;

            soil_adc += (ADC_O_2 << 8) + ADC_O_1;
            ADCSRA |= 0x40;
        }
        ADCSRA |= (1 << ADIF); // reset as required
        delay(50);
    }
    soil_adc /= 3;

    // Battery Read
    // ADC3  AVCC as reference voltage
    ADMUX = _BV(REFS0) | _BV(MUX1) | _BV(MUX0);
    ADCSRA = _BV(ADEN) | _BV(ADPS1) | _BV(ADPS0);

    bat_adc = 0;
    delay(50);
    for (int i = 0; i < 3; i++)
    {
        // start ADC conversion
        ADCSRA |= (1 << ADSC);

        delay(10);

        if ((ADCSRA & 0x40) == 0)
        {
            ADC_O_1 = ADCL;
            ADC_O_2 = ADCH;

            bat_adc += (ADC_O_2 << 8) + ADC_O_1;
            ADCSRA |= 0x40;
        }
        ADCSRA |= (1 << ADIF); // reset as required
        delay(50);
    }
    bat_adc /= 3;

    // soil_adc = analogRead(A2);
    soil_percent = (int)((soil_adc - SOIL_ADC_WATER) / SOIL_ADC_UNIT);

    // bat_adc = analogRead(A3);
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

// Only need set one time
void lorawan_init()
{
    sendData("AT", 3000);
    sendData("AT", 3000);

    char cmd[80];

    sprintf(cmd, "%s%s", "AT+CDEVEUI=", DEVEUI);
    sendData(cmd, AT_TIMEOUT);
    sprintf(cmd, "%s%s", "AT+CAPPEUI=", APPEUI);
    sendData(cmd, AT_TIMEOUT);
    sprintf(cmd, "%s%s", "AT+CAPPKEY=", APPKEY);
    sendData(cmd, AT_TIMEOUT);

    sendData("AT+CCLASS=0", AT_TIMEOUT);
    sendData("AT+CJOINMODE=0", AT_TIMEOUT);
}

int lorawan_join()
{
    // Wake Up Ra08
    sendData("AT", 2000);
    // Clean Serial
    sendData("AT", 2000);

    return sendData_keyword("AT+CJOIN=1,0,8,8", 30000, "Joined");
}

String create_tx_str()
{
    String temp = "AT+DTRX=1,2,10,";

    char data_str[80];
    // sprintf(data_str, "%02x%02x%02x%02x%04x", (int)temperature, (int)humidity, soil_percent, bat_vol, tx_count);
    sprintf(data_str, "%02x%02x%02x%02x%04x", (int)temperature, (int)humidity, soil_adc, bat_vol, tx_count);
    temp = temp + data_str;

    log_out(temp.c_str());

    return temp;
}

void send_lorawan()
{

    log_out_num("BAT ADC:", bat_adc);
    log_out_num("BAT VOL:", bat_vol);
    log_out_num("SOIL ADC:", soil_adc);
    log_out_num("SOIL PER:", soil_percent);
    log_out_num("TEMPERAUTRE:", (int)temperature);
    log_out_num("HUMIDITY:", (int)humidity);

    log_out_num("TX COUNT:", ++tx_count);

    sendData_keyword(create_tx_str(), 30000, "OK+SENT");
}

// --------------------- Device init --------------------------

bool aht_init()
{
    bool ret = false;
    Wire.begin();
    delay(5000);

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

// --------------------- Watch dog ----------------------------------
void watchdog_init()
{
    // clear various "reset" flags
    MCUSR = 0;
    // allow changes, disable reset
    WDTCSR = bit(WDCE) | bit(WDE);
    WDTCSR = bit(WDIE) | bit(WDP3) | bit(WDP0); // set WDIE, and 8 seconds delay
    wdt_reset();                                // pat the dog
}

ISR(WDT_vect)
{
#if DEBUG_OUT_ENABLE
    // Serial.print("[Watch dog]");
    // Serial.println(count);
    if (count % 50 == 0)
        Serial.print(".");
#endif

    delay(100);
    count++;
    wdt_disable(); // disable watchdog
}

// -------------- Power Control --------------------------
void low_power()
{
    pinMode(PWM_OUT_PIN, INPUT);
    pinMode(A4, INPUT_PULLUP);
    pinMode(A5, INPUT_PULLUP);

    delay(50);

    // disable ADC
    ADCSRA = 0;

    watchdog_init();
    sleep_enable();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    noInterrupts();

    // turn off brown-out enable in software
    MCUCR = bit(BODS) | bit(BODSE);
    MCUCR = bit(BODS);
    interrupts();

    sleep_cpu();
    sleep_disable();
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
