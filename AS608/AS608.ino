#include <ESP8266WiFi.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// Wi-Fi credentials
const char* ssid = "Junior Ghost Gang";
const char* password = "Coke loVeR";

// SoftwareSerial for fingerprint sensor
SoftwareSerial mySerial(13, 15);  // RX (D7), TX (D8) (Holod) (shada)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

ESP8266WebServer server(80);

// Maximum number of users
const int maxUsers = 100; 
struct User {
  int id;
  bool registered;
};

User users[maxUsers]; // Array to hold user data
int userCount = 0; // Current number of users

void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("Initializing...");

  // Initialize EEPROM
  EEPROM.begin(512); // Initialize EEPROM with a size of 512 bytes
  loadUsersFromEEPROM(); // Load existing users from EEPROM

  // Connect to Wi-Fi
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  int wifiRetries = 0;
  while (WiFi.status() != WL_CONNECTED && wifiRetries < 20) {
    delay(500);
    Serial.print(".");
    wifiRetries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi");
  }

  // Initialize fingerprint sensor
  finger.begin(9600);
  delay(500);
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor found.");
  } else {
    Serial.println("Fingerprint sensor not found.");
    while (1) { delay(1); }
  }

  // Set up routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/getfingerprint", HTTP_GET, handleGetFingerprint);
  server.on("/register", HTTP_GET, handleRegisterFingerprint);
  server.on("/delete", HTTP_GET, handleDeleteUser);

  server.begin();
  Serial.println("Server started. Listening on port 80.");
}

void loop() {
  server.handleClient();
}

void handleRoot() {
  String jsonResponse = "{\"status\": \"Server running\"}";
  server.send(200, "application/json", jsonResponse);
}

void handleGetFingerprint() {
  Serial.println("Starting fingerprint search...");
  int fingerId = getFingerprintID();
  Serial.print("Current Fingerprint ID: ");
  Serial.println(fingerId); // Debugging output

  if (fingerId > 0) {
    if (isUserRegistered(fingerId)) {
      String jsonResponse = "{\"status\": \"Already exists\"}";
      server.send(200, "application/json", jsonResponse);
    } else {
      // Register the new fingerprint
      registerFingerprint(fingerId);
      String jsonResponse = "{\"status\": \"New user\", \"id\": " + String(fingerId) + "}";
      server.send(200, "application/json", jsonResponse);
    }
  } else if (fingerId == 0) {
    server.send(200, "application/json", "{\"status\": \"No fingerprint detected\"}");
  } else {
    server.send(500, "application/json", "{\"status\": \"Error detecting fingerprint\"}");
  }
}

void handleRegisterFingerprint() {
  Serial.println("Starting fingerprint registration...");
  int fingerId = registerNewFingerprint();
  if (fingerId >= 0) {
    String jsonResponse = "{\"status\": \"Registered\", \"id\": " + String(fingerId) + "}";
    server.send(200, "application/json", jsonResponse);
  } else {
    server.send(500, "application/json", "{\"status\": \"Error during registration\"}");
  }
}

int registerNewFingerprint() {
  int p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    Serial.print("Error capturing fingerprint image, code: ");
    Serial.println(p);
    return -1; // Error capturing image
  }

  p = finger.image2Tz(1); // Save the first image in template 1
  if (p != FINGERPRINT_OK) {
    Serial.print("Error converting image to template, code: ");
    Serial.println(p);
    return -1;
  }

  Serial.println("Place the same finger again...");

  p = finger.getImage(); // Capture the same finger again
  if (p != FINGERPRINT_OK) {
    Serial.print("Error capturing fingerprint image, code: ");
    Serial.println(p);
    return -1;
  }

  p = finger.image2Tz(2); // Save the second image in template 2
  if (p != FINGERPRINT_OK) {
    Serial.print("Error converting image to template, code: ");
    Serial.println(p);
    return -1;
  }

  // Now create the model and save it
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.print("Error creating model, code: ");
    Serial.println(p);
    return -1;
  }

  // Store the model in the database
  p = finger.storeModel(userCount + 1); // Store it at the next user ID
  if (p != FINGERPRINT_OK) {
    Serial.print("Error storing model, code: ");
    Serial.println(p);
    return -1;
  }

  Serial.print("Fingerprint stored at ID: ");
  Serial.println(userCount + 1);
  
  // Save to EEPROM
  users[userCount].id = userCount + 1; // Store the ID of the new user
  users[userCount].registered = true;
  EEPROM.write(userCount, users[userCount].id); // Save the user ID in EEPROM
  EEPROM.commit();
  
  userCount++;
  return userCount; // Return the ID of the newly registered fingerprint
}

void handleDeleteUser() {
  if (server.hasArg("id")) {
    int idToDelete = server.arg("id").toInt();
    if (deleteFingerprint(idToDelete)) {
      server.send(200, "application/json", "{\"status\": \"Deleted\"}");
    } else {
      server.send(400, "application/json", "{\"status\": \"Error deleting\"}");
    }
  } else {
    server.send(400, "application/json", "{\"status\": \"No ID provided\"}");
  }
}

int getFingerprintID() {
  Serial.println("Capturing fingerprint image...");
  int p = finger.getImage();

  // Handle image capture results
  if (p != FINGERPRINT_OK) {
    if (p == FINGERPRINT_NOFINGER) {
      Serial.println("No finger detected.");
      return 0; // No finger present
    } else {
      Serial.print("Error capturing fingerprint image, code: ");
      Serial.println(p);
      return -1; // Error capturing image
    }
  }

  Serial.println("Image captured. Converting to template...");
  p = finger.image2Tz();
  
  if (p != FINGERPRINT_OK) {
    Serial.print("Error converting image to template, code: ");
    Serial.println(p);
    return -1;
  }

  Serial.println("Template created. Searching for match...");
  p = finger.fingerFastSearch();
  
  if (p == FINGERPRINT_OK) {
    Serial.print("Match found! Fingerprint ID: ");
    Serial.println(finger.fingerID);
    return finger.fingerID;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("No match found.");
    return -1;
  } else {
    Serial.print("Error during search, code: ");
    Serial.println(p);
    return -1;
  }
}

bool isUserRegistered(int fingerId) {
  for (int i = 0; i < userCount; i++) {
    if (users[i].id == fingerId && users[i].registered) {
      return true; // User is registered
    }
  }
  return false; // User not found
}

void registerFingerprint(int fingerId) {
  if (userCount < maxUsers) {
    users[userCount].id = fingerId;
    users[userCount].registered = true; // Mark as registered
    userCount++;
    
    // Save to EEPROM
    EEPROM.write(userCount - 1, fingerId); // Save the user ID in EEPROM
    EEPROM.commit();
    
    Serial.println("Fingerprint stored!");
  } else {
    Serial.println("User database full.");
  }
}

bool deleteFingerprint(int id) {
  for (int i = 0; i < userCount; i++) {
    if (users[i].id == id) {
      users[i].registered = false; // Mark as unregistered
      Serial.println("Deleted!");

      // Update EEPROM to reflect the deletion
      EEPROM.write(i, 0); // Mark as deleted
      EEPROM.commit();
      return true; // Deletion successful
    }
  }
  Serial.println("User not found.");
  return false; // Deletion failed
}

// Load users from EEPROM on startup
void loadUsersFromEEPROM() {
  for (int i = 0; i < maxUsers; i++) {
    int id = EEPROM.read(i);
    if (id > 0) {
      users[userCount].id = id;
      users[userCount].registered = true; // Mark as registered
      userCount++;
    }
  }
}
