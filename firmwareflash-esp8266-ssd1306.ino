#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

// Include the correct display library for the display
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`

// Include custom images
#include "images.h"


//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266HTTPUpdateServer.h>

//for LED status
#include <Ticker.h>
Ticker ticker;

const char* host = "esp8266-webupdate";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "admin";

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

//Hardware Init
SSD1306  display(0x3c, 5, 4);



void returnWebPage(String title,String content) {
  String pageContent = "<html><head><link rel='stylesheet' href='https://unpkg.com/purecss@1.0.0/build/pure-min.css' integrity='sha384-nn4HPE8lTHyVtfCBi5yW9d20FjT8BJwUXyWZT9InLYax14RDjBj46LmSztkmNP9w' crossorigin='anonymous'> <title>"+ title +"</title></head><body><div class='center'>" + content + "</div></body></html>";
  server.send(200, "text/html", pageContent);
}


//login page, also called for disconnect
void handleLogin(){
  String msg;
  if (server.hasHeader("Cookie")){   
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
  }
  if (server.hasArg("DISCONNECT")){
    Serial.println("Disconnection");
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    return;
  }
  if (server.hasArg("USERNAME") && server.hasArg("PASSWORD")){
    if (server.arg("USERNAME") == "admin" &&  server.arg("PASSWORD") == "admin" ){
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=1\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
      Serial.println("Log in Successful");
      return;
    }
  msg = "Wrong username/password! try again.";
  Serial.println("Log in Failed");
  }
  String content = "<form class='pure-form pure-form-stacked' action='/login' method='POST'>";
    content += "<fieldset>";
      content += "<legend> Please Login </legend>";
      content += "<label for='USERNAME'> Username </label>";
      content += "<input type='text' name='USERNAME' id='USERNAME' placeholder='username'>";
    
      content += "<label for='PASSWORD'> Password </label>";
      content += "<input type='text' name='PASSWORD' id='PASSWORD' placeholder='password'>";
      
      content += "<input type='submit' name='SUBMIT' value='Submit' class='pure-button pure-button-primary'></form>" + msg + "<br>";
    content += "</fieldset>";
  content += "</form>";
  returnWebPage("Login", content);
}

//root page can be accessed only if authentification is ok
void handleRoot(){
  Serial.println("Enter handleRoot");
  String header;
  if (!is_authentified()){
    String header = "HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    return;
  }
  String content = "<html><body><H2>hello, you successfully connected to esp8266!</H2><br>";
  if (server.hasHeader("User-Agent")){
    content += "the user agent used is : " + server.header("User-Agent") + "<br><br>";
  }
  content += "You can access this page until you <a href=\"/login?DISCONNECT=YES\">disconnect</a></body></html>";
  server.send(200, "text/html", content);
}

//no need authentification
void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void tick()
{
  //toggle state
  int state = digitalRead(16);  // get the current state of GPIO1 pin
  digitalWrite(16, !state);     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  display.clear();
  display.drawXbm(34, 14, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display.display();

  
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);


  //Setup Display
  display.init();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  
  //set led pin as output
  pinMode(16, OUTPUT);
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  ticker.attach(1, tick);
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, WiFi.localIP().toString());
  display.setFont(ArialMT_Plain_16);
  display.display();


  server.on("/", handleRoot);
  server.on("/login", handleLogin);
  server.on("/inline", [](){
    server.send(200, "text/plain", "this works without need of authentification");
  });

  server.onNotFound(handleNotFound);
  //here the list of headers to be recorded
  const char * headerkeys[] = {"User-Agent","Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  //ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize );
  server.begin();
    httpUpdater.setup(&server, update_path, update_username, update_password);

  Serial.println("HTTP server started");

  Serial.println("Version 002");
}



void loop() {
  
  server.handleClient();

  
  yield();
}
