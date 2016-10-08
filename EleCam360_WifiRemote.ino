/*
 *  WifiRemote Sketch for EleCam360 on Feather HUZZAH ESP8266 
 *  with OLED Display Badge
 *  based on https://learn.adafruit.com/digital-display-badge/
 *  
 *  by @DessertHunter
 */
#include <ESP8266WiFi.h> // https://github.com/esp8266/Arduino  https://github.com/esp8266/Arduino/blob/master/doc/esp8266wifi/readme.md
#include <Adafruit_FeatherOLED_WiFi.h> // https://learn.adafruit.com/adafruit-oled-featherwing/featheroled-library
#include "ESP_PTP_IP.h" // TODO

// *** OLED FeatherWing 128x32 *** 
/*
 * Buttons https://learn.adafruit.com/adafruit-oled-featherwing/pinouts: If you're using ESP8266:
Button A is #0 
Button B is #16 
Button C is #2 
 */
#if defined(ESP8266)
  #define BUTTON_A         0
  #define BUTTON_B         16
  #define BUTTON_C         2
  /*
   * Button B has a 100K pullup on it so it will work with the ESP8266 (which does not have an internal pullup available on that pin). 
   * You will need to set up a pullup on all other pins for the buttons to work.
   */
  #define BOARD_LED_PIN    0
#else
  #error("Please set buttons and led defines!")
#endif


// how long to sleep (in seconds)
#define SLEEP_LENGTH 3

 
Adafruit_FeatherOLED_WiFi oled = Adafruit_FeatherOLED_WiFi();

const char* cszEleCam_SSID     = "EleCam360_208851"; // last digits of MAC Address e.g. 44:33:4C:20:88:51 Vendor: „Shenzhen Bilian electronic“
const char* cszEleCam_Password = "1234567890"; // default password
const char* cszEleCam_Host     = "192.168.1.1"; // DHCP Service delivers IPs from 192.168.1.10+

/**************************************************************************/
/*!
    @brief  Try connect to the EleCam360 WiFi AP
            Handles and refreshes all Icons. Could display own text on display!
            ESP8266 is used in AP Station mode (@see https://github.com/esp8266/Arduino/blob/master/doc/esp8266wifi/station-class.md)
    @return true on success
*/
/**************************************************************************/
bool tryConnectEleCamAP(const unsigned int timeout_seconds = 10)
{
  if ( WiFi.status() == WL_CONNECTED ) // WiFi.isConnected()
  {
    // Already connected! ... only update current RSSI
    int8_t rssi = WiFi.RSSI();
    oled.setConnected(true);
    oled.setRSSI(rssi);
    oled.refreshIcons();
    
    return true;
  }
  
  // ... otherwise attempt to connect to the AP
  oled.setConnected(false);
  oled.setRSSI(0);
  oled.refreshIcons();
  oled.clearMsgArea();
  oled.println("Connecting to...");
  oled.println(cszEleCam_SSID);
  oled.display();

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(cszEleCam_SSID);
  
  WiFi.begin(cszEleCam_SSID, cszEleCam_Password);

  int timeout_counter = timeout_seconds * 4; // 250us
  while ((timeout_counter > 0) && (WiFi.status() != WL_CONNECTED))
  {
    delay(250);
    Serial.print(".");
    timeout_counter--;
  }

  if ((timeout_counter > 0) && (WiFi.status() == WL_CONNECTED))
  {
    int8_t rssi = WiFi.RSSI();
    uint32_t ipAddress = WiFi.localIP();
    oled.setConnected(true);
    oled.setRSSI(rssi);
    oled.setIPAddress(ipAddress);
    oled.refreshIcons();

    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.printf("MAC address: %s\n", WiFi.macAddress().c_str());

    return true; // yes, now connected!
  }
  else
  {
    // Display the error message (timeout)
    oled.clearMsgArea();
    oled.println("Connection abort:");
    oled.println("TIMEOUT!");
    oled.refreshIcons(); // Refresh icons in case the text ran over
    oled.display();
    delay(1000); // 1s anzeigen
  }

  return false; // Fehler oder nicht verbunden
}


/**************************************************************************/
/*!
    @brief  PTP-IP. Connect, sending InitiateCapture (0x100E) and Disconnect
    @return true on success
*/
/**************************************************************************/
bool ptpipPerformInitiateCapture()
{
  ESP_PTP_IP ptp = ESP_PTP_IP();

  if (ptp.Init(cszEleCam_Host))
  {
    Serial.println(">>> PTP/IP connected!");
    
    if (ptp.OpInitiateCapture())
    {
     Serial.println(">>> Yeah smile!");
    }
    
    ptp.Uninit();
  }
  
  delay(1000);
  
  return false;
}

/**************************************************************************/
/*!
    @brief
*/
/**************************************************************************/
void updateVbat() 
{
  float vbatFloat = 0.0F;      // millivolts
  
  // read the battery level from the ESP8266 analog in pin.
  // analog read level is 10 bit 0-1023 (0V-1V).
  // our 1M & 220K voltage divider takes the max
  // lipo value of 4.2V and drops it to 0.758V max.
  // this means our min analog read value should be 580 (3.14V)
  // and the max analog read value should be 774 (4.2V).
  int level = analogRead(A0);
  
  // convert battery level to percent
  level = map(level, 580, 774, 0, 100);
  Serial.print("Battery level: "); Serial.print(level); Serial.println("%");
  
   // Eigene Spannung auf dem HUZZAH ESP8266 zu messen erfordert externe Beschaltung!  https://learn.adafruit.com/using-ifttt-with-adafruit-io/wiring#battery-tracking
  // TODO:  https://learn.adafruit.com/using-ifttt-with-adafruit-io/arduino-code-1
  vbatFloat = 4300; // 500mv = "0.50V"

  oled.setBattery(vbatFloat/1000);
}

/**************************************************************************/
/*!
    @brief  The setup function runs once when the board comes out of reset
*/
/**************************************************************************/
void setup() {
  Serial.begin(115200);
  delay(10);

  // Wait for Serial Monitor
  //  while(!Serial) delay(1);

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  // Setup the LED pin
  pinMode(BOARD_LED_PIN, OUTPUT);

  // Setup the OLED display
  oled.init();
  oled.clearDisplay();

  // Get a VBAT reading before refreshing if VBAT is available
  oled.setBatteryIcon(true);
  updateVbat();

  oled.setConnectedVisible(true); // Enables or disables the connected icon in the bottom 8 pixel status bar
  oled.setRSSIVisible(true); // Enables or disables the RSSI icon in the top 8 pixel status bar
  oled.setIPAddressVisible(false); // Enables or disables the IP address in the bottom 8 pixel status bar
  
  // Refresh the screen
  oled.refreshIcons(); // Calling this function will redraw all of the status icons, and should be called if you have made any changes to the values using the helper functions like .setRSSI, .setIPAddress, etc.
  oled.clearMsgArea();

  // WiFi
  Serial.printf("Wi-Fi mode set to WIFI_STA %s\n", WiFi.mode(WIFI_STA) ? "" : "Failed!");
  WiFi.setAutoConnect(false); // Configure module to automatically connect on power on to the last used access point
  WiFi.setAutoReconnect(false); // Set whether module will attempt to reconnect to an access point in case it is disconnected.
}

/**************************************************************************/
/*!
    @brief  This loop function runs over and over again
*/
/**************************************************************************/
void loop() {

  // Update the battery level
  // TODO_CS: updateVbat();

  // Versuchen mit der EleCam360 zu Verbinden...
  if ( tryConnectEleCamAP() )
  {
    //if (! digitalRead(BUTTON_A)) Serial.print("A"); // 
    //if (! digitalRead(BUTTON_B)) Serial.print("B");
    //if (! digitalRead(BUTTON_C)) Serial.print("C");

    // Verbunden, prüfen, ob ein Knopf gedrückt wurde...
    if (LOW == digitalRead(BUTTON_B))
    {
      Serial.print("[B] pressed");

      oled.clearMsgArea();
      oled.println("Trigger...");
      oled.println("CAPTURE image");
      oled.display();

      if (ptpipPerformInitiateCapture())
      {
        // Ok
        oled.clearMsgArea();
        oled.println("Capture...");
        oled.println("DONE");
        oled.display();
      }
      else
      {
        // Fehler
        oled.clearMsgArea();
        oled.println("Capture...");
        oled.println("FAILED!");
        oled.display();
      }
      delay(1000); // Kurz anzeigen
    }
    else
    {
      // Nachricht ausgeben, das ein Knop gedrückt werden soll
      oled.clearMsgArea();
      oled.println("Press button");
      oled.println("[B]: capture image");
      //oled.println("[C]: start/stop record video");    
      oled.display();
    }
  }
  else
  {
    Serial.println("Not connected!");
  }

  //togglePin(BOARD_LED_PIN);
  
  delay(3000);

    // we are done here. go back to sleep.
  //Serial.println("zzzz");
  //ESP.deepSleep(SLEEP_LENGTH * 1000000, WAKE_RF_DISABLED);
 // TODO: The bulk of the work for this example happens in the setup() function. The sketch restarts after it wakes from sleep, so the main loop() never runs.
}

