/*
I think this is the code for the clock in the main area of Revspace. But i'm unsure, it's old and a mess...
*/

#include <EEPROM.h>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>

#include <Time.h>      //http://www.arduino.cc/playground/Code/Time
#include <Timezone.h>  //https://github.com/JChristensen/Timezone < i modified timezone.cpp to remove the EEPROM functions


#include "Sixteen.h"

Sixteen display = Sixteen();

// WiFi settings
char ssid[] = "revspace-dingen";   //  your network SSID (name)
char pass[] = "";  // your network password

// Timezone Rules for Europe
// European Daylight time begins on the last sunday of March
// European Standard time begins on the last sunday of October
// Officially both around 2 AM, but let's keep things simple.

TimeChangeRule CEST = { "CEST", Last, Sun, Mar, 1, +120 };  //Daylight time = UTC +2 hours
TimeChangeRule CET = { "CET", Last, Sun, Oct, 1, +60 };     //Standard time = UTC +1 hours
Timezone myTZ(CEST, CET);
TimeChangeRule* tcr;  //pointer to the time change rule, use to get TZ abbrev
time_t utc, local;

// NTP Server settings
unsigned int localPort = 2390;  // local port to listen for UDP packets
IPAddress timeServerIP;         // time.nist.gov NTP server address
const char* ntpServerName = "pool.ntp.org";
const int NTP_PACKET_SIZE = 48;      // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE];  //buffer to hold incoming and outgoing packets
int oldsecond;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

void setup() {
  display.addScreen(5, 4);
  display.addScreen(5, 2);
  display.addScreen(5, 14);
  display.addScreen(5, 12);
  display.addScreen(5, 13);

  Serial.begin(115200);
  Serial.println();


  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    Serial.print(".");
  }
  Serial.println("");

  Serial.println("WiFi connected");

  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  udp.begin(localPort);
  ntpsync();
}

void loop() {
  if (second(now()) != oldsecond) {
    printTime(now());
    oldsecond = second(now());
  }


  if (month(now()) == 12 && day(now()) == 31 && hour(now()) == 23 && minute(now()) == 59) {
    while (minute(now()) == 59) {
      int togo = 60 - second(now());
      String stringOne = "----";
      if (togo < 10) {
        stringOne = "";
        stringOne += togo;
        stringOne += togo;
        stringOne += togo;
        stringOne += togo;
        stringOne += togo;
        stringOne += togo;
        stringOne += togo;
        stringOne += togo;
        stringOne += togo;
        stringOne += togo;

      } else {
        stringOne += togo;
        stringOne += "----";
      }
      char charBuf[11];
      stringOne.toCharArray(charBuf, 11);
      display.scroll(charBuf, 10);
    }
  }

  if (month(now()) == 1 && day(now()) == 1 && hour(now()) == 0 && minute(now()) == 0) {
    while (minute(now()) == 0) {
      String stringOne = "Happy ";

      stringOne += year(now());
      char charBuf[11];
      stringOne.toCharArray(charBuf, 11);
      display.scroll(charBuf, 250);

      stringOne = "*-*";
      stringOne += year(now());
      stringOne += "*-*";
      stringOne.toCharArray(charBuf, 11);
      display.scroll(charBuf, 250);
    }
  }

  // NTP sync every 3 hours.
  if (hour(now()) % 3 == 0 && minute(now()) == 0 && second(now()) == 0) {
    ntpsync();
  }

  // Need to add a if NTPsync fail fallback option.
}

void ntpsync() {

  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);

  sendNTPpacket(timeServerIP);  // send an NTP packet to a time server

  // The delay probably can be a lot shorter and more elegant! I really don't like delays!
  // wait to see if a reply is available
  delay(1000);

  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("no packet yet");
  } else {
    Serial.print("packet received, length=");
    Serial.println(cb);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE);  // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

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
    // print Unix time:
    Serial.println(epoch);

    local = myTZ.toLocal(epoch, &tcr);

    setTime(local);
  }
}



// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress& address) {
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123);  //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}





void printTime(time_t t) {

  Serial.print("Het is nu: ");
  Serial.print(hour(t));
  Serial.print(":");
  Serial.print(minute(t));
  Serial.print(":");
  Serial.println(second(t));

  String stringOne = " ";
  if (hour(t) < 10) {
    stringOne += "0";
  }

  stringOne += hour(t);
  stringOne += ":";
  if (minute(t) < 10) {
    stringOne += "0";
  }
  stringOne += minute(t);
  stringOne += ":";
  if (second(t) < 10) {
    stringOne += "0";
  }
  stringOne += second(t);


  char charBuf[10];
  stringOne.toCharArray(charBuf, 10);

  display.scroll(charBuf, 10);
}

void printDate(time_t t) {

  Serial.print("Het is nu: ");
  Serial.print(day(t));
  Serial.print("-");
  Serial.print(month(t));
  Serial.print("-");
  Serial.println(year(t));

  String stringOne = "";
  if (day(t) < 10) {
    stringOne += "0";
  }

  stringOne += day(t);
  stringOne += "-";
  if (month(t) < 10) {
    stringOne += "0";
  }
  stringOne += month(t);
  stringOne += "-";

  stringOne += year(t);


  char charBuf[11];
  stringOne.toCharArray(charBuf, 11);

  display.scroll(charBuf, 10);
}
