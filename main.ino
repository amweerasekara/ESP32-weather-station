#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "Time_a.h"
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <cppQueue.h>
#include <PubSubClient.h>
#include "AsyncUDP.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define DHTPIN 5     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11
#define IMPLEMENTATION FIFO
#define OVERWRITE true
#define queue_size 3000

const char* ssid     = "mal123";
const char* password = "BDF5R13M896";
const char* mqtt_server = "broker.hivemq.com";
char udpmsg[60]; 

WiFiServer server(80);
String header; // Variable to store the HTTP request
unsigned long currentTime = millis();
unsigned long previousTime = 0; 
const long timeoutTime = 2000;

WiFiClient espClient;
PubSubClient client(espClient);
char mqtt_msg[60];

WiFiUDP ntpUDP;
AsyncUDP udp;
NTPClient timeClient(ntpUDP);
time_t et;
char httpRequestData[200];

const int oneWireBus = 4; 
    
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

DHT dht(DHTPIN, DHTTYPE);

// do not change the following two lines
const char* serverName = "https://phys.cmb.ac.lk/esp/post_data.php";
String apiKeyValue = "7cac68de958b354865fb8c4d6e9e95e6";

int location=25; // Use the location number allocated for you

float temperatureC; // DS1820 temperature 
uint32_t t;        //unix time
float h;
String formattedTime;

typedef struct strRec {
  uint32_t t;
  float temperatureC;
  float h;
} Rec ;

cppQueue q(sizeof(Rec), 2000, IMPLEMENTATION, OVERWRITE);

void connect_wifi(){
  int count=0;
  Serial.print("Connecting to ");
  Serial.println(ssid);
  Serial.println(WiFi.status());
  WiFi.begin(ssid, password);
  Serial.println(WiFi.status());
  while (WiFi.status() != WL_CONNECTED) {
    if(count>40) break;
    delay(500);
    Serial.print(".");
    count++;
  }  
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

void get_ntp_time(){
  if(WiFi.status() == WL_CONNECTED){
    int count=0;
    Serial.println("updating time");
    timeClient.begin();
    timeClient.setTimeOffset(19800); // Use 0 for GMT when recording unix timestap
    while(!timeClient.update()) {
      if(count>40) break;
      timeClient.forceUpdate();
      count++;
    }
    setTime(timeClient.getEpochTime());  // update ESP32 time 
    et=timeClient.getEpochTime();
    Serial.println(et);
    }
  else{
   Serial.println("updating time failed WiFi not connected"); 
  }
}

void setup() {
 Serial.begin(115200);
 connect_wifi();
 get_ntp_time();
 // Start the DS18B20 sensor
 sensors.begin();
 dht.begin();
 if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);

  client.setServer(mqtt_server, 1883);
  
  WiFi.disconnect();
}
void loop() {
  server.begin();
  connect_wifi();
  t = now()-19800;
  formattedTime = timeClient.getFormattedTime();
  sensors.requestTemperatures(); 
  temperatureC = sensors.getTempCByIndex(0);
 
  Serial.println(formattedTime);
  Serial.print(temperatureC);
  Serial.println("ºC");
 
  h = dht.readHumidity();
  if (isnan(h)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  Serial.print(F("Humidity: "));
  Serial.println(h);
  
  Rec rec;
  rec.t=t;
  rec.temperatureC=temperatureC;
  rec.h=h;
  q.push(&rec);

  int len=q.getCount();
  Serial.print("Queue Length =");
  Serial.println(len);

  if(len%5==0) {
    get_ntp_time();
    if(WiFi.status() == WL_CONNECTED){
      for(int i=0; i<len; i++){
        q.peek(&rec);
        HTTPClient http;
        http.begin(serverName);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        sprintf(httpRequestData, "api_key=7cac68de958b354865fb8c4d6e9e95e6&sensor=2&location=%d&Parameter=Temperature&Value=%2.4f&Reading_Time=%lu ",location,rec.temperatureC,rec.t) ;       
        int httpResponseCode = http.POST(httpRequestData);  
              
        if (httpResponseCode>0) {
          Serial.print("Tempurature Success ");
        }
        else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        }
        http.end();
      
        http.begin(serverName);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        sprintf(httpRequestData, "api_key=7cac68de958b354865fb8c4d6e9e95e6&sensor=2&location=%d&Parameter=Humidity&Value=%2.4f&Reading_Time=%lu ",location,rec.h,rec.t) ;       
        httpResponseCode = http.POST(httpRequestData);    
            
        if (httpResponseCode>0) {
          Serial.print("Humidity Success ");
        }
        else {
          Serial.print("Error code: ");
          Serial.println(httpResponseCode);
        }
        http.end();
        Serial.print(httpRequestData);
        q.pop(&rec);
        delay(50);
      }
    }
  }

  if(WiFi.status() == WL_CONNECTED){
  
  sprintf(mqtt_msg,"Temperature: %2.2f Humidity: %2.2f Time : %lu  ", temperatureC,h, t );
  client.connect("mel5405-41"); 
  client.publish("mel5405-41/ESP32", mqtt_msg); 
  Serial.println(mqtt_msg); 
  

  sprintf(udpmsg,"Temperature: %2.2f Humidity: %2.2f Time : %lu  ", temperatureC,h, t );
  if(udp.connect(IPAddress(192,168,8,100),44444)){
    udp.print(udpmsg);
    Serial.print("udp sent");
  }

  }


  // clear display
  display.clearDisplay();
  //display time
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Time");
  display.setCursor(0,9);
  display.print(formattedTime);
  
  // display temperature
  display.setTextSize(1);
  display.setCursor(0,25);
  display.print("Temperature: ");
  display.setTextSize(1);
  display.setCursor(0,34);
  display.print(temperatureC);
  display.print(" ");
  display.setTextSize(1);
  display.cp437(true);
  display.write(167);
  display.setTextSize(1);
  display.print("C");
  
  // display humidity
  display.setTextSize(1);
  display.setCursor(0, 48);
  display.print("Humidity: ");
  display.setTextSize(1);
  display.setCursor(0, 57);
  display.print(h);
  display.print(" %"); 
  
  display.display(); 

  char timeval[80];
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
                        
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");            
            client.println("<body><h1>Welcome to ESP32 web server</h1>");
            client.println("<table><tr><td>"); 
            client.println("Time </td><td>:&nbsp&nbsp"); 
            client.println(formattedTime);
            client.println("</td></tr><tr><td>Temperature </td><td>:&nbsp&nbsp"); 
            client.println(temperatureC);
            client.println("<sup>o</sup>C</td></tr><tr><td>Humidity </td><td>:&nbsp&nbsp"); 
            client.println(h);              
            client.println("%</td></tr></table></body></html>");
            client.println();  // The HTTP response ends with another blank line
            break;
          } 
          else { 
            currentLine = "";
          }
        } 
        else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }


  Serial.println("Sleeping for 3 minutes");
  esp_sleep_enable_timer_wakeup(180000000);
  int ret=esp_light_sleep_start();

}
