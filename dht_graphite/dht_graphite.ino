// DHT temp sensor to graphite via the Ethernet shield

#include "DHT.h"
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

#define DHTPIN 2     // what pin we're connected to

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11 
#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Connect pin 1 (on the left) of the sensor to +5V (red)
// Connect pin 2 of the sensor to whatever your DHTPIN is (yellow)
// Connect pin 4 (on the right) of the sensor to GROUND (black)
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor
DHT dht(DHTPIN, DHTTYPE);

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {  
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,1,177);

// Enter the IP address of the server you're connecting to:
IPAddress server(192,168,1,31); 

// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 23 is default for telnet;
// if you're using Processing's ChatServer, use  port 10002):
EthernetClient client;

unsigned long last_connect_time = 0;    //last time we connected to the server
const unsigned long interval = 60*1000; //delay between connections
unsigned long epoch = 0; // epoch time

unsigned int localPort = 8888; // local port to listen to UDP packets
const int NTP_PACKET_SIZE=48;  // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// UDP Instance
EthernetUDP Udp;

void setup() {
  Serial.begin(9600); 
  Serial.println("starting DHT..");
  dht.begin();
  
  // start the Ethernet connection:
  Serial.println("starting Ethernet..");
  Ethernet.begin(mac, ip);

  // give the Ethernet shield a second to initialize:
  delay(1000);

  // ntp socket
  Serial.println("starting UDP..");
  Udp.begin(localPort);
  
  // sync to NTP
  delay(1000);
  syncNTP();
}

void loop() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  float t = dht.readTemperature(1);
  unsigned long cur_epoch;

  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(t) || isnan(h)) {
    Serial.println("Failed to read from DHT");
  }
  else {
    Serial.print("Humidity: "); 
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Temperature: "); 
    Serial.print(t);
    Serial.println(" *F");
    
    // if you get a connection, report back via serial:
    Serial.print("Connecting.. ");
    if (client.connect(server, 2003)) {
      Serial.println("connected.");
      
      // get current epoch time
      cur_epoch = millis() / 1000 + epoch;
      Serial.print("current epoch: ");
      Serial.println(cur_epoch);

      Serial.println("Sending data to Graphite..");
      // temp
      client.print("house.temp.livingroom ");
      client.print(t);
      client.print(" ");
      client.println(cur_epoch);
      
      // humidity
      client.print("house.humidity.livingroom ");
      client.print(h);
      client.print(" ");
      client.println(cur_epoch);
      
      // done, disconnect
      client.stop();
    } 
    else {
      // Failed to connect
      Serial.println("failed.");
      // Do not trip the NTP sync later in this loop by setting these to equal
      cur_epoch = epoch;
    }
  }
  
  // Check time
  // millis() overflows at 4,294,967,295
  // Wait for it to over flow and then sync to NTP again
  if(epoch > cur_epoch) {
    Serial.println("millis() overflowed, forcing NTP sync");
    syncNTP();
  }
  else {
    delay(60000);
  }
}


void syncNTP() {
  epoch = 0;
  
  while(epoch == 0) {
    sendNTPpacket(server); // send an NTP packet to a time server
  
    delay(1000);
    if ( Udp.parsePacket() ) {  
      // We've received a packet, read the data from it
      Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer
  
      //the timestamp starts at byte 40 of the received packet and is four bytes,
      // or two words, long. First, esxtract the two words:
  
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;  
      Serial.print("Seconds since Jan 1 1900 = " );
      Serial.println(secsSince1900);               
  
      // now convert NTP time into everyday time:
      Serial.print("Unix time = ");
      // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
      const unsigned long seventyYears = 2208988800UL;     
      // subtract seventy years:
      epoch = secsSince1900 - seventyYears;  
      // print Unix time:
      Serial.println(epoch);                               


      // print the hour, minute and second:
      Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
      Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
      Serial.print(':');  
      if ( ((epoch % 3600) / 60) < 10 ) {
        // In the first 10 minutes of each hour, we'll want a leading '0'
        Serial.print('0');
      }
      Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
      Serial.print(':'); 
      if ( (epoch % 60) < 10 ) {
        // In the first 10 seconds of each minute, we'll want a leading '0'
        Serial.print('0');
      }
      Serial.println(epoch %60); // print the second
    }
    
    // Sync successful? If not, delay before trying again
    if(epoch == 0) {
      delay(10000);
    }
  }
}

// send an NTP request to the time server at the given address 
unsigned long sendNTPpacket(IPAddress& address)
{
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
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket(); 
}
