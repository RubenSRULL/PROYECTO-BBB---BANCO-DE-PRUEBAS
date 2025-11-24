// --- LIBRERIAS --- //
#include <HX711.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- //
// --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- //
// --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- //

// --- CONSTANTES --- //
#define STOP 0
#define TESTING 1
#define TARE1 2
#define CALIB1 3
#define TARE2 4
#define CALIB2 5

// --- PINES --- //
#define SCK1 17
#define DT1 16
#define SCK2 27
#define DT2 14
#define TACOMETRO 25
#define SENSOR_CORRIENTE 34
#define MOTOR_PIN 26
#define WAKE_BUTTON_PIN 12

// --- VARIABLES --- //
const char* ssid = "OPPO A53";
const char* password = "611b10a883c5";
//const char* mqtt_server = "10.251.249.63";
const char* mqtt_server = "10.251.249.1";
int vel_init = 0;
int vel_last = 0;
int step = 0;
float stepTime = 0.0;
int ciclos = 0;
bool measure_rpm = false;
bool measure_thrust = false;
bool measure_torque = false;
bool measure_current = false;
int estado = STOP;
int estadoAnterior = estado;
float escala1 = 6000.00 / 160;
float escala2 = 6000.00 / 160;
float palanca = 6;
float peso = 0.0;
float RPM = 0.0;
float empuje = 0.0;
float par = 0.0;
float consumo = 0.0;
unsigned long pulseInterval;
unsigned long lastPulseTime;
unsigned long now;
unsigned long before;
const float voltToAmp = 0.185;
unsigned int porcentaje = 0;
const unsigned int pwmFreq = 50;
const unsigned int pwmResolution = 16;
unsigned int dutyCycle = 0;
const unsigned int minDuty = 3277;
const unsigned int maxDuty = 6554;
float escala = 1.0;

// --- OBJETOS --- //
HX711 balanza1;
HX711 balanza2;
WiFiClient espClient;
PubSubClient client(espClient);
StaticJsonDocument<512> doc;

// --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- //
// --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- //
// --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- //

// --- INTERRUPCIONES --- //
void IRAM_ATTR pulseISR() {
  unsigned long now = micros();
  unsigned long diff = now - lastPulseTime;

  if (diff > 200) {
    pulseInterval = diff;
    lastPulseTime = now;
  }
}


//--- FUNCION LEER VELOCIDAD ---//
float leerVelocidad() {
  unsigned long intervalo;
  noInterrupts();
  intervalo = pulseInterval;
  interrupts();
  if (intervalo == 0) return 0.0;
  float revPerSec = 1000000.0f / (float)intervalo;
  return revPerSec * 60.0f;
}

//--- FUNCION GIRAR MOTOR ---//
void giraMotor(int pct) {
  unsigned int duty = map(pct, 0, 100, minDuty, maxDuty);
  ledcWrite(MOTOR_PIN, duty);
}

//--- FUNCION LEER CONSUMO ---//
float leerConsumo() {
  long lectura = 0;
  for (unsigned int i = 0; i < 20; i++) {
    lectura += analogRead(SENSOR_CORRIENTE);
  }
  lectura /= 10;
  float volt = (float)lectura * 3.3f / 4095.0f;
  float amp = (volt - 1.435) / voltToAmp;
  return amp;
}

//--- FUNCION LEER BALANZA ---//
float leerBalanza(HX711 &balanza) {
  return balanza.get_units(5);
}

// --- FUNCION CONFIGURAR WIFI --- //
void configWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("WiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// --- FUNCION CONFIGURAR MQTT --- //
void configMQTT() {
  client.setBufferSize(2048);
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);
}

// --- FUNCION RECONECTAR MQTT --- //
void MQTTreconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    if (client.connect("ESP32")) {
      Serial.println("Conectado al broker MQTT");
      client.subscribe("esp32/input");
    }
  }
}

// --- FUNCION CALLBACK MQTT --- //
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("\n--- MQTT MESSAGE RECEIVED ---");
  Serial.println("LENGTH: " + String(length));
  String jsonStr;
  jsonStr.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) {
    jsonStr += (char)payload[i];
  }

  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Payload: ");
  Serial.println(jsonStr);

  StaticJsonDocument<8192> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);

  if (error) {
    Serial.print("ERROR leyendo JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Leer acción
  String action = doc["action"] | "";
  Serial.print("Acción recibida: ");
  Serial.println(action);

  if (action == "start") {
    Serial.println("\n--- CONFIGURACIÓN RECIBIDA ---");
    serializeJsonPretty(doc["data"], Serial);
    vel_init  = doc["data"]["vel_init"].as<int>();
    vel_last  = doc["data"]["vel_last"].as<int>();
    step      = doc["data"]["step"].as<int>();
    stepTime  = doc["data"]["stepTime"].as<float>();
    ciclos    = doc["data"]["cicles"].as<int>();

    measure_rpm     = doc["data"]["measure_rpm"].as<bool>();
    measure_thrust  = doc["data"]["measure_thrust"].as<bool>();
    measure_torque  = doc["data"]["measure_torque"].as<bool>();
    measure_current = doc["data"]["measure_current"].as<bool>();

    Serial.println("\n--- VALORES ACTUALIZADOS ---");
    Serial.print("vel_init: "); Serial.println(vel_init);
    Serial.print("vel_last: "); Serial.println(vel_last);
    Serial.print("step: "); Serial.println(step);
    Serial.print("stepTime: "); Serial.println(stepTime);
    Serial.print("cicles: "); Serial.println(ciclos);
    Serial.print("measure_rpm: "); Serial.println(measure_rpm);
    Serial.print("measure_thrust: "); Serial.println(measure_thrust);
    Serial.print("measure_torque: "); Serial.println(measure_torque);
    Serial.print("measure_current: "); Serial.println(measure_current);
    Serial.println("\nVariables internas actualizadas.");
    estado = TESTING;
    before = millis();
    porcentaje = vel_init;
  }

  else if (action == "stop") {
    estado = STOP;
  }
  else if (action == "tare1"){
    if (estado != TESTING){
      estado = TARE1;
    }
  }
  else if (action == "tare2"){
    if (estado != TESTING){
      estado = TARE2;
    }
  }
  else if (action == "calibrate1"){
    if (estado != TESTING){
      estado = CALIB1;
    }
  }
  else if (action == "calibrate2"){
    if (estado != TESTING){
      estado = CALIB2;
    }
  }
  else Serial.println("Comando no reconocido.");

  Serial.println("--- END MQTT MESSAGE ---\n");
}


// --- FUNCION PUBLICAR MQTT JSON --- //
void JSONpublisher(int porcentaje, float RPM, float consumo, float empuje, float par) {
  doc.clear();
  doc["%"] = porcentaje;
  doc["RPM"] = RPM;
  doc["Intensidad"] = consumo;
  doc["Empuje"] = empuje;
  doc["Par"] = par;
  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  client.publish("esp32/output", buffer, n);
}

// --- FUNCION CONFIGURAR HX711 --- //
void configHX711() {
  Serial.println("Configurando Celdas de Carga");
  balanza1.begin(DT1, SCK1);
  while (!balanza1.is_ready());
  Serial.println("Celda 1 lista");
  balanza2.begin(DT2, SCK2);
  while (!balanza2.is_ready());
  Serial.println("Celda 2 lista");
}

// --- FUNCION INICIAR TEST --- //
void testInit() {
  if (porcentaje > vel_last) {
    porcentaje = vel_init;
    giraMotor(porcentaje);
    delay(1000);
    ciclos--;
    Serial.println("TEST FINALIZADO");
    if (ciclos == 0){
      estado = STOP;
    }
      return;
  }
  now = millis();
  if ((now - before) >= (stepTime * 1000.0)) {
    before = now;

    if (porcentaje < vel_last + step) {
      Serial.println(porcentaje);
      giraMotor(porcentaje);

      if (measure_rpm){
        RPM = leerVelocidad();
      }

      if (measure_thrust){
        empuje = -leerBalanza(balanza1);
      }

      if (measure_torque){
        par = -leerBalanza(balanza2) * palanca;
      }

      if (measure_current){
        consumo = leerConsumo();
      }
      
      JSONpublisher(porcentaje, RPM, consumo, empuje, par);
      Serial.println("DATOS PUBLICADOS");
      porcentaje += step;
    }
  }
}


// --- FUNCION DETENER TEST --- //
void testStop(){
  giraMotor(0);
  delay(2000);
  RPM = 0.0;
  empuje = 0.0;
  par = 0.0;
  consumo = 0.0;
}
// --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- //
// --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- //
// --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- //

// --- SETUP --- //
void setup() {
  Serial.begin(115200);
  while (!Serial){;}
  configWifi();
  configMQTT();
  pinMode(TACOMETRO, INPUT_PULLUP);
  pinMode(SENSOR_CORRIENTE, INPUT);
  attachInterrupt(digitalPinToInterrupt(TACOMETRO), pulseISR, FALLING);
  configHX711();
  ledcAttach(MOTOR_PIN, pwmFreq, pwmResolution);
  ledcWrite(MOTOR_PIN, map(0, 0, 100, minDuty, maxDuty));
  delay(5000);
  testStop();
}

// --- LOOP --- //
void loop() {
  if (!client.connected()) {
    MQTTreconnect();
  }
  client.loop();

  switch (estado) {
    case STOP:
      testStop();
      if (estadoAnterior != STOP){
        Serial.println("STOPPED");
      }
      break;

    case TESTING:
      if (estadoAnterior != TESTING){
        Serial.println("TESTING");
      }
      testInit();
      break;

    case TARE1:
      balanza1.tare(20);
      Serial.println("TARA 1");
      estado = STOP;
      break;

    case CALIB1:
      balanza1.set_scale();
      delay(1000);
      escala = balanza1.get_value(10) / peso;
      balanza1.set_scale(escala);
      Serial.println("CALIBRADA 1");
      estado = STOP;
      break;

    case TARE2:
      balanza2.tare(20);
      Serial.println("TARA 2");
      estado = STOP;
      break;

    case CALIB2:
      balanza2.set_scale();
      delay(1000);
      escala = balanza2.get_value(10) / peso;
      balanza2.set_scale(escala);
      Serial.println("CALIBRADA 2");
      estado = STOP;
      break;
  }
  estadoAnterior = estado;
}