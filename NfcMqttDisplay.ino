/*
NFC Reader

PN532 reader hooked to ESP32 for MQTT communication and 64px OLED display for offline use

Elechouse documentation:
https://www.elechouse.com/elechouse/images/product/PN532_module_V3/PN532_%20Manual_V3.pdf

Display setup thanks to https://how2electronics.com/interfacing-pn532-nfc-rfid-module-with-arduino/
SCL on ESP32 pin D22
SCA on ESP32 pin D21

*/

// for NFC (we're using I2C with the display but original example included SPI)
#if 0
#include <SPI.h>
#include <PN532_SPI.h>
#include <PN532.h>
#include <NfcAdapter.h>

PN532_SPI pn532spi(SPI, 10);
NfcAdapter nfc = NfcAdapter(pn532spi);
#else

#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>

PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc = NfcAdapter(pn532_i2c);
#endif

// for MQTT
#include <PubSubClient.h>
#include <ArduinoJson.h>

// for WiFi
#include <WiFi.h>

// for WiFi
const char* ssid = "mySSID";
const char* password = "myPASSWORD";
// for MQTT
const char* mqtt_server = "192.168.x.x";

//for display:
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static const unsigned char PROGMEM logo_bmp[] =
{ 0b00000000, 0b11000000,
  0b00000001, 0b11000000,
  0b00000001, 0b11000000,
  0b00000011, 0b11100000,
  0b11110011, 0b11100000,
  0b11111110, 0b11111000,
  0b01111110, 0b11111111,
  0b00110011, 0b10011111,
  0b00011111, 0b11111100,
  0b00001101, 0b01110000,
  0b00011011, 0b10100000,
  0b00111111, 0b11100000,
  0b00111111, 0b11110000,
  0b01111100, 0b11110000,
  0b01110000, 0b01110000,
  0b00000000, 0b00110000 };

  // end for display

// Allocate the JSON document
//
// Inside the brackets, 200 is the RAM allocated to this document.
// Don't forget to change this value to match your requirement.
// Use https://arduinojson.org/v6/assistant to compute the capacity.
StaticJsonDocument<256> doc;
// StaticJsonObject allocates memory on the stack, it can be
// replaced by DynamicJsonDocument which allocates in the heap.
//
// DynamicJsonDocument  doc(200);
//for JSON message processing
String msg = "no message detected";
String uid = "no UID detected";
// end for JSON

/************* MQTT TOPICS (change these topics as you wish)  **************************/
const char* outTopic = "d/e32_006/nfc_001";
const char* inTopic = "c/e32_006/nfc_001";

// Callback function header
void callback(char* topic, byte* payload, unsigned int length);

WiFiClient espClient;
PubSubClient client(espClient);

// Callback function
void callback(char* topic, byte* payload, unsigned int length) {
  // In order to republish this payload, a copy must be made
  // as the orignal payload buffer will be overwritten whilst
  // constructing the PUBLISH packet.

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  
  // Allocate the correct amount of memory for the payload copy
  byte* p = (byte*)malloc(length);
  // Copy the payload to the new buffer
  memcpy(p,payload,length);
  client.publish(outTopic, p, length);
  
  // Free the memory
  free(p);
}


void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);    
    Serial.print(".");
  }
  
  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "RandoESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(outTopic, "hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  Serial.begin(115200);
  //while (!Serial) continue;
  Serial.println("NDEF Reader");

  nfc.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
  Serial.println(F("SSD1306 allocation failed for OLED display"));
  }

  // Clear display buffer
  display.clearDisplay();

  if (client.connect("espClient")) {
    client.publish(outTopic,"hello world");
    client.subscribe("inTopic");
  }
}

void sendState() {

// Allocate the JSON document
//StaticJsonDocument<256> doc;

// Inside the brackets, 200 is the RAM allocated to this JSON document.
// Don't forget to change this value to match your requirement.
// Use https://arduinojson.org/v6/assistant to compute the capacity.

JsonObject root = doc.to<JsonObject>();
root["location"] = "Fridge";
JsonObject tag = root.createNestedObject("tag");
tag["uid"] = uid;
tag["msg"] = msg;
serializeJsonPretty(doc, Serial);
Serial.println(".");
Serial.println("above should be pretty json");
Serial.print("Topic is : ");
Serial.println(outTopic);

char buffer[256];
size_t n = serializeJson(doc, buffer);
client.publish(outTopic, buffer, n);

//client.publish(outTopic, serializeJson(doc))

}

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  if (WiFi.status() != WL_CONNECTED) {
    delay(1);
    Serial.print("WIFI Disconnected. Attempting reconnection.");
    setup_wifi();
    return;
  }

  client.loop();

  Serial.println("\nScan an NFC tag\n");
  
  Serial.print("nfc.tagPresent() = ");
  Serial.println(nfc.tagPresent());

  if (nfc.tagPresent())
  {
    NfcTag tag = nfc.read();
    Serial.println(tag.getTagType());
    Serial.print("UID: ");Serial.println(tag.getUidString());
    uid = tag.getUidString();

   // if (tag.hasNdefMessage()) // every tag won't have a message
  //  {
      Serial.print("\nThis NFC Tag contains an NDEF Message with ");
      NdefMessage message = tag.getNdefMessage();
      Serial.print(message.getRecordCount());
      Serial.print(" NDEF Record");
      if (message.getRecordCount() != 1) {
        Serial.print("s");
      }
      Serial.println(".");

      // cycle through the records, printing some info from each
      int recordCount = message.getRecordCount();
      for (int i = 0; i < recordCount; i++)
      {
        Serial.print("\nNDEF Record ");Serial.println(i+1);
        NdefRecord record = message.getRecord(i);
        // NdefRecord record = message[i]; // alternate syntax

        Serial.print("  TNF: ");Serial.println(record.getTnf());
        Serial.print("  Type: ");Serial.println(record.getType()); // will be "" for TNF_EMPTY

        // The TNF and Type should be used to determine how your application processes the payload
        // There's no generic processing for the payload, it's returned as a byte[]
        int payloadLength = record.getPayloadLength();
        byte payload[payloadLength];
        record.getPayload(payload);

        // Print the Hex and Printable Characters
        Serial.print("  Payload (HEX): ");
        PrintHexChar(payload, payloadLength);

        // Force the data into a String (might work depending on the content)
        // Real code should use smarter processing
        String payloadAsString = "";
        for (int c = 0; c < payloadLength; c++) {
          payloadAsString += (char)payload[c];
        }
        Serial.print("  Payload (as String): ");
        Serial.println(payloadAsString);
        msg = payloadAsString;
        Serial.print("msg = ");
        Serial.println(msg);


/*
        for (int c = 0; c < payloadLength; c++) {
          msg += (char)payload[c];
        }
        Serial.print("  Payload (as String): ");
        Serial.println(msg);
*/

        // if id is blank will return ""
        if (uid != "") {
          Serial.print("  ID: ");Serial.println(uid);
        }

        display.clearDisplay();
        display.setCursor(0, 0); //oled display
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.print("NFC Payload:");

        display.setCursor(0, 15); //oled display
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.print(msg);
        display.display();

      }
 //   }

  }

  // call to publish MQTT message with UID and message as JSON object
  sendState();  
  
  delay(500);
}