#include <SoftwareSerial.h>
#define DEBUG true

#define LORAWAN_TX 3
#define LORAWAN_RX 4
#define LORAWAN_RST 10

#define AT_TIMEOUT 3000

#define DEVEUI "70B3D57ED0057468"
#define APPEUI "0000000000000000"
#define APPKEY "A614344378DC15A24BFA3B43E6C4C320"

SoftwareSerial softSerial1(LORAWAN_TX, LORAWAN_RX);

void setup()
{
    Serial.begin(9600);
    softSerial1.begin(9600); // your esp's baud rate might be different
    delay(1000);

    Serial.println("Restarting....");

    pinMode(LORAWAN_RST, OUTPUT);

    digitalWrite(LORAWAN_RST, 0);
    delay(2000);
    digitalWrite(LORAWAN_RST, 1);
    Serial.println("Lorawan module restart over.");
    delay(5000);

    sendData("AT", 3000, DEBUG);
    sendData("AT", 3000, DEBUG);

    // char cmd[80];

    // sprintf(cmd, "%s%s", "AT+CDEVEUI=", DEVEUI);
    // sendData(cmd, AT_TIMEOUT, DEBUG);
    // sprintf(cmd, "%s%s", "AT+CAPPEUI=", APPEUI);
    // sendData(cmd, AT_TIMEOUT, DEBUG);
    // sprintf(cmd, "%s%s", "AT+CAPPKEY=", APPKEY);
    // sendData(cmd, AT_TIMEOUT, DEBUG);

    // sendData("AT+CCLASS=0", AT_TIMEOUT, DEBUG);
    // sendData("AT+CJOINMODE=0", AT_TIMEOUT, DEBUG);
    // sendData("AT+CJOIN=1,0,8,8", AT_TIMEOUT, DEBUG);
}

void loop()
{

    while (softSerial1.available() > 0)
    {
        Serial.write(softSerial1.read());
        yield();
    }
    while (Serial.available() > 0)
    {
        softSerial1.write(Serial.read());
        yield();
    }
}

String sendData(String command, const int timeout, boolean debug)
{
    String response = "";
    // command = command + "\r\n";

    Serial.println(command);
    softSerial1.println(command); // send the read character to the Serial

    long int time = millis();

    while ((time + timeout) > millis())
    {
        while (softSerial1.available())
        {

            // The esp has data so display its output to the serial window
            char c = softSerial1.read(); // read the next character.
            response += c;
        }
    }

    if (debug)
    {
        Serial.println(response);
    }

    return response;
}

