#include <ESP8266WiFi.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>

// Wi-Fi credentials
const char* ssid = "Junior Ghost Gang";
const char* password = "Coke loVeR";             

// Using SoftwareSerial for ESP8266
SoftwareSerial mySerial(13, 15);  // RX (D7), TX (D8)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

const char* device_token  = "edafait";
String getData, Link;
String URL = "http://192.168.1.4/getdata.php"; 
bool device_Mode = false;    
int FingerID = 0;                                  
unsigned long previousMillis = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  connectToWiFi();
  
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }

  Serial.print("Sensor contains "); 
  Serial.print(finger.getTemplateCount()); 
  Serial.println(" templates");
}

void loop() {
  if (!WiFi.isConnected()) {
    reconnectWiFi();
  }

  // Check device mode here
  if (device_Mode == 0) {
    ChecktoAddID(); // Call to check and add fingerprint ID
  }

  CheckFingerprint();   
  delay(10);
}

void ChecktoAddID() {
  if (WiFi.isConnected()) {
    WiFiClient client;  
    HTTPClient http;    
    getData = "?Get_Fingerid=get_id&device_token=" + String(device_token);
    Link = URL + getData;
    
    http.begin(client, Link);
    int httpCode = http.GET();   
    String payload = http.getString();    

    if (httpCode == 200 && payload.startsWith("add-id")) {
      String add_id = payload.substring(6);
      int id = add_id.toInt();
      getFingerprintEnroll(id); // Call to enroll fingerprint
    } else {
      Serial.println("Failed to get ID or invalid response");
    }
    
    http.end();  
  }
}

void CheckFingerprint() {
  FingerID = getFingerprintID();
  if (FingerID > 0) {
    SendFingerprintID(FingerID); // Send FingerID when a fingerprint is detected
  }
}

void SendFingerprintID(int finger) {
  Serial.println("Sending the Fingerprint ID");
  if (WiFi.isConnected()) {
    WiFiClient client;
    HTTPClient http;    
    getData = "?FingerID=" + String(finger) + "&device_token=" + device_token; 
    Link = URL + getData;
    
    http.begin(client, Link);
    int httpCode = http.GET();   
    String payload = http.getString();    
    Serial.println(httpCode);   
    Serial.println(payload); 

    // Handle login/logout actions if necessary here
    if (payload.startsWith("login")) {
      // Handle login
    } else if (payload.startsWith("logout")) {
      // Handle logout
    }

    http.end();  
  } else {
    Serial.println("WiFi not connected!");
  }
}

int getFingerprintID() {
   int p = finger.getImage();
   switch (p) {
    case FINGERPRINT_OK:
      break;
    case FINGERPRINT_NOFINGER:
      return 0; // No finger detected
    case FINGERPRINT_PACKETRECIEVEERR:
    case FINGERPRINT_IMAGEFAIL:
      return -2; // Error reading fingerprint
    default:
      return -2; // Other error
  }
  
  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      break;
    case FINGERPRINT_IMAGEMESS:
      return -1; // Image is messy
    case FINGERPRINT_PACKETRECIEVEERR:
    case FINGERPRINT_FEATUREFAIL:
    case FINGERPRINT_INVALIDIMAGE:
      return -2; // Error processing image
    default:
      return -2; // Other error
  }

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    return finger.fingerID; // Return matched fingerprint ID
  } else if (p == FINGERPRINT_NOTFOUND) {
    return -1; // No match found
  } else {
    return -2; // Other error
  }   
}

int getFingerprintEnroll(int id) {
  int p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
  }

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) return p;

  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) return p;

  p = finger.createModel();
  if (p != FINGERPRINT_OK) return p;

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint enrolled with ID: " + String(id));
  }
  return p;
}

void reconnectWiFi() {
  if (millis() - previousMillis >= 10000) {
    previousMillis = millis();
    connectToWiFi();    
  }
}

void connectToWiFi() {
    WiFi.mode(WIFI_OFF);
    delay(1000);
    WiFi.mode(WIFI_STA);
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    
    uint32_t periodToConnect = 30000L;
    for (uint32_t StartToConnect = millis(); (millis() - StartToConnect) < periodToConnect;) {
      if (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print("...");
      } else {
        break;
      }
    }
    
    if (WiFi.isConnected()) {
      Serial.println("");
      Serial.println("Connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());  
    } else {
      Serial.println("");
      Serial.println("Not Connected");
      WiFi.mode(WIFI_OFF);
      delay(1000);
    }
    delay(1000);
}
