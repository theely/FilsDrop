#include <SPI.h>
#include <WiFiNINA.h>
#include "Config.h"



int status = WL_IDLE_STATUS;
WiFiServer server(80);
WiFiClient client;



int connect_to_wifi() {
  String fv = WiFi.firmwareVersion();
  if (fv < "1.0.0") {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to Wifi network:
  int max_attempt = 1;
  while (status != WL_CONNECTED) {
    Serial.print("[WiFi] Connecting to: ");
    Serial.println(config.ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(config.ssid, config.pass);

    //Unfrotunately this is not yet supported
    //Serial.print("[WiFi] Reason code: ");
    //Serial.println(WiFi.reasonCode());

    // wait 5 seconds for connection:
    delay(5 * 1000);
    if (--max_attempt < 0) return -1;
  }

  // you're connected now, so print out the data:
  Serial.println("[WiFi] Connected");
  return 0;
}

int check_wifi_satus() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connection Status: NOT OK");
    WiFi.disconnect();

    Serial.println("[WiFi] Re-connecting...");
    // wait 10 seconds
    delay(10 * 1000);
    if (connect_to_wifi() != 0) {
      Serial.println("[WiFi] Unable to reconnect!");
      return -1;
    }
  }

  Serial.println("[WiFi] Connection Status: OK");
  return 0;
}
void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print where to go in a browser:
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);

}


void start_server() {

  server.begin();
  // you're connected now, so print out the status
  printWiFiStatus();
}


void wifi_ap() {

  Serial.println("[WiFi] Booting AP...");
  //Create open network. Change this line if you want to create an WEP network:
  status = WiFi.beginAP("Filsplay");
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    // don't continue
    while (true);
  }
  delay(10000);
  start_server();
}


String new_ssid = "";
String new_password = "";
void extractSSID_and_password(String command) {

  char parameters[400];
  command.toCharArray(parameters, 400);
  char *ptr;

  ptr = strtok(parameters, "& ");  // takes a list of delimiters
  while (ptr != NULL)
  {
    ptr = strtok(NULL, "& ");  // takes a list of delimiters
    String param = String(ptr);
    if (param.indexOf("SSID=") >= 0) {
      new_ssid = param.substring(param.indexOf("SSID=") + 5, param.length());
    }
    if (param.indexOf("password=") >= 0) {
      new_password = param.substring(param.indexOf("password=") + 9, param.length());
    }
  }
}


void wifi_server() {

  client = server.available();   // listen for incoming clients

  if (client) {
    Serial.println("[WiFi] Client connected...");
    bool new_config = false;
    String currentLine = "";
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();
        // read a byte, then
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.println("<html><body>");
            client.println("<pre style='font-family:monospace; white-space:pre;'>");
            client.println(" 88888888888  88  88                          88                           ");
            client.println(" 88           \"\"  88                          88                           ");
            client.println(" 88               88                          88                           ");
            client.println(" 88aaaaa      88  88  ,adPPYba,  8b,dPPYba,   88  ,adPPYYba,  8b       d8  ");
            client.println(" 88\"\"\"\"\"      88  88  I8[    \"\"  88P'    \"8a  88  \"\"     `Y8  `8b     d8'  ");
            client.println(" 88           88  88   `\"Y8ba,   88       d8  88  ,adPPPPP88   `8b   d8'   ");
            client.println(" 88           88  88  aa    ]8I  88b,   ,a8\"  88  88,    ,88    `8b,d8'    ");
            client.println(" 88           88  88  `\"YbbdP\"'  88`YbbdP\"'   88  `\"8bbdP\"Y8      Y88'     ");
            client.println("                                 88                               d8'      ");
            client.println("                                 88                              d8'       ");
            client.println("</pre>");
            if (new_config) {
              client.print("<h1> Saved!</h1><h2>Please reboot Filsplay.</h2>");
            }

            client.print("<form action='/set' method='get'>");
            client.print("<fieldset>");
            client.print("<legend>WiFi Configuration</legend>");
            client.print("<p>SSID: <input type='text' name='SSID'></p>");
            client.print("<p>Password: <input type='password' name='password'></p>");
            client.print("<p><input type='submit' value='Save'></p>");
            client.print("</fieldset>");
            client.print("</form>");

            client.println("</body></html>");
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {
            if (currentLine.startsWith("GET /set?")) {
              extractSSID_and_password(currentLine);
              Serial.println("[WiFi] New SSID and password set!");
              new_ssid.toCharArray(config.ssid, 100);
              new_password.toCharArray(config.pass, 100);
              write_config();
              new_config = true;
            }
            currentLine = "";
          }
        }
        else if (c != '\r') {    // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // close the connection:
    client.stop();
  }
}
