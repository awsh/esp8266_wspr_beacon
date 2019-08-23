#include <DDS.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "SSID";      // your network SSID (name)
const char* pass = "PASSWORD";  // your network password

unsigned int localPort = 2390;  // local port to listen for UDP packets

IPAddress timeServerIP;         // NTP server address
const char* ntpServerName = "pool.ntp.org"; // NTP Pool Hostname

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

WiFiUDP udp; // A UDP instance to let us send and receive packets over UDP
 
volatile int SymbolIndex = 0;
volatile int TX = 0;
volatile int POWER = 0;
volatile long FREQ = 0;

//#define TONE0 5 //D1
//#define TONE1 4   //D2
//#define TONE2 0 // D3
//#define TONE3 2 //D4


#define LEDx 5
#define LEDy 4
#define LEDz 0

/*

X ---------------
        R0      R2
        |        |
       LED0    LED2
Y ---------------
        R1      R3 
        |        |
       LED1    LED3
Z ---------------

*/


#define TX_LED 15 //D8
 

const int W_CLK = 14; //D5
const int FQ_UD = 12; //D6 
const int DATA = 16; //D0
const int RESET = 13; //D7

const byte LowPWR_WSPR_Data[] = {
 // enter wspr encoded message from wsprgen program
};

const byte HighPWR_WSPR_Data[] = {
 // enter wspr encoded message from wsprgen program
};


DDS dds(W_CLK, FQ_UD, DATA, RESET);

void setup() {

    // set initial pin states
    pinMode(TX_LED, OUTPUT);

    digitalWrite(TX_LED, LOW);

    Serial.begin(115200);
    Serial.println();
    Serial.println();

    // Initialize the AD9850
    dds.init();
    dds.trim(125000000);
    dds.setFrequency(0);

    // We start by connecting to a WiFi network
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        blink_LEDs();
    }
 
    Serial.println("");

    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.println("Starting UDP");
    udp.begin(localPort);
    Serial.print("Local port: ");
    Serial.println(udp.localPort());
}

void loop() {

    if (TX == 0) {
        //get a random server from the pool
        WiFi.hostByName(ntpServerName, timeServerIP);

        sendNTPpacket(timeServerIP); // send an NTP packet to a time server
        // wait to see if a reply is available
        delay(1000);

        int cb = udp.parsePacket();
        if (cb) {
            Serial.print("packet received, length=");
            Serial.println(cb);
            // We've received a packet, read the data from it
            udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

            //the timestamp starts at byte 40 of the received packet and is four bytes,
            // or two words, long. First, extract the two words:

            unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
            unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
            // combine the four bytes (two words) into a long integer
            // this is NTP time (seconds since Jan 1 1900):
            unsigned long secsSince1900 = highWord << 16 | lowWord;
            Serial.print("Seconds since Jan 1 1900 = ");
            Serial.println(secsSince1900);

            // now convert NTP time into everyday time:
            Serial.print("Unix time = ");
            // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
            const unsigned long seventyYears = 2208988800UL;
            // subtract seventy years:
            unsigned long epoch = secsSince1900 - seventyYears;

            // print the hour, minute and second:
            Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
            Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
            Serial.print(':');
            if (((epoch % 3600) / 60) < 10) {
                // In the first 10 minutes of each hour, we'll want a leading '0'
                Serial.print('0');
            }
            Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
            Serial.print(':');
            if ((epoch % 60) < 10) {
                // In the first 10 seconds of each minute, we'll want a leading '0'
                Serial.print('0');
            }
    
            Serial.println(epoch % 60); // print the second
    
            // check second
            if ( ((((epoch % 3600) / 60) % 2) == 0) && ((epoch % 60) == 0) || ((epoch % 60) == 1)  || ((epoch % 60) == 2)  ) {
                TX = 1;

            } else {

                // if seconds greater than 45, check again every second
                if ((epoch % 60) >= 45) {
                    delay(1000);
                } else { // if seconds less than 50, check every 10 seconds
                    delay(10000);
                }
        
            }
        }
    } // End if TX=0


    if (TX == 1) {
    
        Serial.println("Beginning Transmission...");

        digitalWrite(TX_LED, HIGH);         // Turn on the TX LED
        FREQ = random(7040000, 7040200);    // Select a random TX frequency

        if (POWER == 0) {
            for (SymbolIndex = 0; SymbolIndex <= 161; SymbolIndex++) {
                dds.setFrequency(FREQ + (LowPWR_WSPR_Data[SymbolIndex] * 1.4648));  // Change DDS freq
                LED(LowPWR_WSPR_Data[SymbolIndex]);     // Update tone LEDs
                delay(683);
            }
        }

        if (POWER == 1) {
            for (SymbolIndex = 0; SymbolIndex <= 161; SymbolIndex++) {
                dds.setFrequency(FREQ + (HighPWR_WSPR_Data[SymbolIndex] * 1.4648));  // Change DDS freq
                LED(HighPWR_WSPR_Data[SymbolIndex]);    // Update tone LEDs
                delay(683);
            }
        }


        Serial.println("Done transmitting...");
        dds.setFrequency(0);                                                                // Turn off the DDS
        LED(4);                                                                        // Turn off the tone LEDs
        digitalWrite(TX_LED, LOW);                                                          // Turn off the TX LED
        TX = 0;
        POWER = !POWER;                                                                     // Toggle transmit power
    } // End if TX=1
} // End loop

void sendNTPpacket(IPAddress& address) {
    // send an NTP request to the time server at the given address

    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12]  = 49;
    packetBuffer[13]  = 0x4E;
    packetBuffer[14]  = 49;
    packetBuffer[15]  = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    udp.beginPacket(address, 123); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
} // End sendNTPpacket()


void blink_LEDs(void) {
    for (int i = 0; i < 4; i++) {
        LED(i);
        delay(100);
    } 
    LED(4);
}   

void LED(int led_pin) {
    //WSPR 0 = (LEDx HIGH) (LEDy LOW)  (LEDz DIS)
    //WSPR 1 = (LEDx DIS)  (LEDy HIGH) (LEDz LOW)
    //WSPR 2 = (LEDx LOW)  (LEDy HIGH) (LEDz DIS)
    //WSPR 3 = (LEDx DIS)  (LEDy LOW) (LEDz HIGH)
    switch(led_pin) {
        case 0: // WSPR DATA 0
            pinMode(LEDx, OUTPUT);
            pinMode(LEDy, OUTPUT);
            pinMode(LEDz, INPUT);
            digitalWrite(LEDx, HIGH);
            digitalWrite(LEDy, LOW);
            break;
        case 1: // WSPR DATA 1
            pinMode(LEDx, INPUT);
            pinMode(LEDy, OUTPUT);
            pinMode(LEDz, OUTPUT);
            digitalWrite(LEDy, HIGH);
            digitalWrite(LEDz, LOW);
            break;
        case 2: // WSPR DATA 2
            pinMode(LEDx, OUTPUT);
            pinMode(LEDy, OUTPUT);
            pinMode(LEDz, INPUT);
            digitalWrite(LEDx, LOW);
            digitalWrite(LEDy, HIGH);
            break;
        case 3: // WSPR DATA 3
            pinMode(LEDx, INPUT);
            pinMode(LEDy, OUTPUT);
            pinMode(LEDz, OUTPUT);
            digitalWrite(LEDy, LOW);
            digitalWrite(LEDz, HIGH);
            break;
        case 4: // LEDs OFF
            pinMode(LEDx, OUTPUT);
            pinMode(LEDy, OUTPUT);
            pinMode(LEDz, OUTPUT);
            digitalWrite(LEDx, LOW);
            digitalWrite(LEDy, LOW);
            digitalWrite(LEDz, LOW);
            break;
    }
} // END LED
