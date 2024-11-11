#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Definición de pantalla OLED
#define SCREEN_WIDTH 128  // Ancho de la pantalla en píxeles
#define SCREEN_HEIGHT 32  // Alto de la pantalla en píxeles
#define OLED_RESET    -1  // Pin de RESET (-1 si no tiene)
#define SCREEN_ADDRESS 0x3C  // Dirección I2C de la pantalla OLED (cambiar a 0x3D si es necesario)

// Pines I2C para la ESP32 Wemos Lolin32
#define I2C_SDA 5  // GPIO5 para SDA
#define I2C_SCL 4  // GPIO4 para SCL

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MAX30105 particleSensor;

// Variables para calcular ritmo cardíaco y SpO2
uint32_t irBuffer[100];  // Array de muestras de señal infrarroja
uint32_t redBuffer[100];  // Array de muestras de señal de luz roja
int32_t bufferLength;  // Longitud del buffer de muestras
int32_t spo2;  // Valor de oxígeno en sangre
int8_t validSPO2;  // Verificador de oxígeno en sangre
int32_t heartRate;  // Latidos por minuto
int8_t validHeartRate;  // Verificador de latidos

// Insert your network credentials
#define WIFI_SSID "Ange"
#define WIFI_PASSWORD "Angy4918"

// Insert Firebase project API Key
#define API_KEY "AIzaSyAhtoZl-gPb_3jj5YU6_Regi8porOAjaog"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "angy4918@gmail.com"
#define USER_PASSWORD "Angy4918"

// Insert RTDB URL
#define DATABASE_URL "https://pulsioximetro-a486d-default-rtdb.firebaseio.com/"

// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID
String uid;

// Variables to save database paths
String databasePath;
String heartRatePath;
String spo2Path;

// Timer variables
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 5000;

// Function declarations
void initWiFi();
void sendInt(String path, int value);

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);  // Inicializar I2C para ESP32
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Error en la inicialización de la pantalla OLED"));
    while(1);  // Detener si falla
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Iniciando...");
  display.display();
  delay(2000);

  // Inicializar sensor MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("Error al inicializar MAX30102. Verifique las conexiones.");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Error MAX30102");
    display.display();
    while(1);
  }

  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);

  // Initialize WiFi
  initWiFi();

  // Firebase setup
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  // Assign the callback function for the long running token generation task
  config.token_status_callback = tokenStatusCallback;

 // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);

  // Getting the user UID
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Update database paths
  databasePath = "/UsersData/" + uid;
  heartRatePath = databasePath + "/heartRate"; // --> UsersData/<user_uid>/heartRate
  spo2Path = databasePath + "/spo2"; // --> UsersData/<user_uid>/spo2
}

void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Write int values to the database
void sendInt(String path, int value){
  if (Firebase.RTDB.setInt(&fbdo, path.c_str(), value)){
    Serial.print("Writing value: ");
    Serial.print (value);
    Serial.print(" on the following path: ");
    Serial.println(path);
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  }
  else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
}

void loop() {
  // Llenar buffers de muestra
  bufferLength = 100;
  for (int i = 0; i < bufferLength; i++) {
    while (particleSensor.available() == false)
      particleSensor.check();
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }

  // Calcular SpO2 y BPM
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  // Mostrar resultados en OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("BPM: ");
  if (validHeartRate) {
    display.print(heartRate);  // Imprime el BPM si es válido
  } else {
    display.print("...");  // Imprime "..." si no es válido
  }

  display.setCursor(0, 16);
  display.print("SpO2: ");
  if (validSPO2) {
    display.print(spo2);  // Imprime SpO2 si es válido
  } else {
    display.print("...");  // Imprime "..." si no es válido
  }
  display.display();

  // Send new readings to database
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    sendInt(heartRatePath, heartRate);
    sendInt(spo2Path, spo2);
  }

  delay(1000);
}