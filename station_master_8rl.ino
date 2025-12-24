#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Adafruit_MCP23X17.h>
#include <Wire.h>
#include <ESPmDNS.h>

Adafruit_MCP23X17 mcp;
WebServer server(80);

// Configurazione hardware
const uint8_t relayPins[] = {13, 27, 14, 26, 25, 33, 32, 16}; // A1,A2,B1,B2,C1,C2,D1,D2
const uint8_t buttonPins[] = {0, 1, 2, 3, 4, 5, 6, 7}; // GPIOA0-GPIOA7

// Stati del sistema
struct SystemState {
  uint8_t antenna : 2;  // 0=off, 1=verticale, 2=longwire
  uint8_t hf : 2;       // 0=off, 1=RTX, 2=SDR
  uint8_t vuhf : 2;     // 0=off, 1=RTX, 2=SDR
} state = {0, 0, 0};

void updateRelays() {
  // Reset sicuro di tutti i relè
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(relayPins[i], HIGH);
  }

  // Antenne
  if (state.antenna == 1) digitalWrite(relayPins[0], LOW); // Verticale
  if (state.antenna == 2) {                                // Longwire
    digitalWrite(relayPins[0], LOW);
    digitalWrite(relayPins[1], LOW);
  }

  // HF
  if (state.hf == 1) digitalWrite(relayPins[2], LOW);      // RTX
  if (state.hf == 2) {                                     // SDR
    digitalWrite(relayPins[2], LOW);
    digitalWrite(relayPins[3], LOW);
    digitalWrite(relayPins[4], LOW);
  }

  // V/UHF
  if (state.vuhf == 1) digitalWrite(relayPins[6], LOW);    // RTX
  if (state.vuhf == 2) {                                   // SDR
    digitalWrite(relayPins[4], LOW);
    digitalWrite(relayPins[5], LOW);
    digitalWrite(relayPins[6], LOW);
    digitalWrite(relayPins[7], LOW);
  }

  Serial.printf("Stato rele: A1=%d A2=%d B1=%d B2=%d C1=%d C2=%d D1=%d D2=%d\n",
    digitalRead(relayPins[0]) == LOW,
    digitalRead(relayPins[1]) == LOW,
    digitalRead(relayPins[2]) == LOW,
    digitalRead(relayPins[3]) == LOW,
    digitalRead(relayPins[4]) == LOW,
    digitalRead(relayPins[5]) == LOW,
    digitalRead(relayPins[6]) == LOW,
    digitalRead(relayPins[7]) == LOW
  );
}

void updateSystemState(String cmd, int val) {
  if (cmd == "antenna") {
    state.antenna = (val >= 0 && val <= 2) ? val : 0;
  }
  else if (cmd == "hf") {
    state.hf = (val >= 0 && val <= 2) ? val : 0;
    
    if ((state.hf == 1 || state.hf == 2) && state.antenna == 0) {
      state.antenna = 1;
    }
    
    if (state.hf == 2 && state.vuhf == 2) {
      state.vuhf = 0;
    }
  }
  else if (cmd == "vuhf") {
    state.vuhf = (val >= 0 && val <= 2) ? val : 0;
    
    if (state.vuhf == 2 && state.hf == 2) {
      state.hf = 0;
    }
  }
  else if (cmd == "master_off") {
    state = {0, 0, 0};
  }
  
  updateRelays();
}

void sendCurrentState() {
  String json = "{\"antenna\":" + String(state.antenna) + 
               ",\"hf\":" + String(state.hf) + 
               ",\"vuhf\":" + String(state.vuhf) + "}";
  server.send(200, "application/json", json);
}

void handleControl() {
  if (server.method() == HTTP_POST) {
    String cmd = server.arg("cmd");
    int val = server.arg("val").toInt();
    updateSystemState(cmd, val);
  }
  sendCurrentState();
}

void checkButtons() {
  static uint32_t lastCheck = 0;
  static bool lastButtonState[8] = {0};
  
  if (millis() - lastCheck < 50) return;
  lastCheck = millis();

  for (uint8_t i = 0; i < 8; i++) {
    bool currentState = !mcp.digitalRead(buttonPins[i]);
    if (currentState && !lastButtonState[i]) {
      switch(i) {
        case 0: updateSystemState("antenna", 1); break;
        case 1: updateSystemState("antenna", 2); break;
        case 2: updateSystemState("hf", 1); break;
        case 3: updateSystemState("hf", 2); break;
        case 4: updateSystemState("vuhf", 1); break;
        case 5: updateSystemState("vuhf", 2); break;
        case 6: updateSystemState("hf", 0); break;
        case 7: updateSystemState("master_off", 0); break;
      }
    }
    lastButtonState[i] = currentState;
  }
}

void setupNetwork() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("radio");
  WiFi.begin("radio", "radiotest");
  
  Serial.print("Connessione WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnesso! IP: " + WiFi.localIP());
    if (!MDNS.begin("radio")) {
      Serial.println("Errore mDNS!");
    } else {
      Serial.println("Accesso via: http://radio.local");
      MDNS.addService("http", "tcp", 80);
    }
  } else {
    Serial.println("\nFallito! Avvio AP...");
    WiFi.softAP("Radio_Config", "config123");
    Serial.println("AP avviato. IP: " + WiFi.softAPIP());
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Inizializza relè
  for (uint8_t i = 0; i < 8; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
  }

  // Inizializza MCP23017
  Wire.begin();
  if (!mcp.begin_I2C()) {
    Serial.println("ERRORE: MCP23017 non rilevato!");
    while(1);
  }
  for (uint8_t i = 0; i < 8; i++) {
    mcp.pinMode(buttonPins[i], INPUT_PULLUP);
  }

  // Rete e mDNS
  setupNetwork();

  // Filesystem
  if (!LittleFS.begin()) {
    Serial.println("ERRORE: LittleFS non inizializzato!");
    while(1);
  }

  // Server Web
  server.on("/control", HTTP_ANY, handleControl);
  server.on("/getstate", HTTP_GET, sendCurrentState);
  server.serveStatic("/", LittleFS, "/index.html");
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/script.js", LittleFS, "/script.js");
  server.begin();

  Serial.println("Sistema pronto");
}

void loop() {
  server.handleClient();
  checkButtons();
}