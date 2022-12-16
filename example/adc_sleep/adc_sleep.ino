#include <avr/wdt.h>
#include <avr/sleep.h>

#define SLEEP_CYCLE 0

// pin set
#define VOLTAGE_PIN A3
#define PWM_OUT_PIN 9
#define SENSOR_POWER_PIN 5
#define ADC_PIN A2

#define DEBUG_OUT_ENABLE 1

#define SOIL_ADC_WATER 550
#define SOIL_ADC_AIR 660
#define SOIL_ADC_UNIT ((SOIL_ADC_AIR - SOIL_ADC_WATER) / 100.0)

int soil_adc = 0; // variable to store the value coming from the sensor
int soil_percent = 0;
int bat_adc = 0; // the voltage of battery
int bat_vol = 0;

int count = 0; // WDT count

void setup()
{
    log_init();

    delay(1000);
    low_power();
}

void loop()
{
    wdt_disable();

    if (count > SLEEP_CYCLE) //(7+1) x 8S  450
    {

        log_out("[Task Start]");

        read_sensor();

        send_lorawan();

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
    ADMUX = _BV(REFS0) | _BV(MUX1)| _BV(MUX0);
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

    // soil_adc = analogRead(ADC_PIN);
    soil_percent = (int)((soil_adc - SOIL_ADC_WATER) / SOIL_ADC_UNIT);

    // bat_adc = analogRead(VOLTAGE_PIN);
    bat_vol = (int)(bat_adc * 690.0 / 470.0 * 3300.0 / 1024.0);

    if (soil_percent > 100)
        soil_percent = 100;
    else if (soil_percent < 0)
        soil_percent = 0;

    bat_vol = bat_vol / 100;
}

// -------------------- Lorawan ---------------------------------

void send_lorawan()
{

    log_out_num("BAT ADC:", bat_adc);
    log_out_num("BAT VOL:", bat_vol);
    log_out_num("SOIL ADC:", soil_adc);
    log_out_num("SOIL PER:", soil_percent);
}

// --------------------- Device init --------------------------

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
    Serial.print("[Watch dog]");
    Serial.println(count);
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
