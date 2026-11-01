#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <DHT.h> 
#include <SD.h> // Incluimos la biblioteca para manejar la tarjeta SD
#include <esp_sleep.h>
#include <ESP32Servo.h>


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

const char* ssid = "TU_RED_WIFI";// Nombre de tu red WiFi
const char* password = "TU_CONTRASENA.";// Contraseña de tu red WiFi
const char* ntpServer = "pool.ntp.org";

const long  gmtOffset_sec = -6 * 3600; // Ajusta el desfase horario a tu ubicación en segundos
const int   daylightOffset_sec = 0; // Ajusta el desfase horario por horario de verano si aplica

Servo servo;  // Crear un objeto Servo para controlar el servomotor

#define DHT_PIN_1 32
#define DHT_PIN_2 33
#define DHT_PIN_3 25
#define DHT_TYPE DHT22
const int SERVO_PIN = 13;
const int relayPin = 12;

  int output = 0;
  int output2 = 0;
  double prevError = 0;
  double integral = 0;
  int estadoRelee = 0;

DHT dht1(DHT_PIN_1, DHT_TYPE);
DHT dht2(DHT_PIN_2, DHT_TYPE);
DHT dht3(DHT_PIN_3, DHT_TYPE);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

char tempStr1[6];
char humStr1[6];
char tempStr2[6];
char humStr2[6];
char tempStr3[6];
char humStr3[6];

File dataFile; // Definimos una variable para el archivo CSV

const char* server = "api.thingspeak.com"; // Servidor de ThingSpeak
const char* apiKey = "TU_API_KEY_THINGSPEAK"; // Clave de escritura de ThingSpeak
const int httpPort = 80; // Puerto HTTP de ThingSpeak
const unsigned long postingInterval = 60 * 1000; // Intervalo de tiempo entre envíos de datos (en milisegundos)

void connectToWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
    display.setCursor(0, 0);
    display.print("Buscando Red");
  }
  Serial.println("Conectado a WiFi!");
}

void updateTime() {
  timeClient.update();
}

void getTimeAndDate(struct tm *timeinfo, char *formattedTime, char *formattedDate) {
  if (!timeinfo) {
    Serial.println("Error al obtener el tiempo");
    return;
  }
  sprintf(formattedDate, "%04d/%02d/%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);
  sprintf(formattedTime, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

void displayTimeAndDate(const char *formattedTime, const char *formattedDate) {
  display.setCursor(0, 0);
  display.print("Hora: ");
  display.println(formattedTime);

  display.setCursor(0, 10);
  display.print("Fecha: ");
  display.println(formattedDate);
}

void readDHTData(DHT &dht, char *tempStr, char *humStr) {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  snprintf(tempStr, 6, "%.1f", temperature);
  snprintf(humStr, 6, "%.1f", humidity);
}

void controlRelay(float humidity, float temperature) {
  if (humidity > 35.0 || temperature > 65.0) {
    // Activa el relé
    digitalWrite(relayPin, HIGH);
    Serial.println("Relé activado");
    // Actualiza la variable global
    estadoRelee = 1;
  } else {
    // Desactiva el relé
    digitalWrite(relayPin, LOW);
    Serial.println("Relé desactivado");
    // Actualiza la variable global
    estadoRelee = 0;
  }
}

double controlPI(float temperature) {
  double setpoint = 60.0;
  double Kp = 2.06;
  double Ki = 0.0692;
  double Kd = 0.0;
  double error = setpoint - temperature;
  double P = Kp * error;
  integral += Ki * error; 
  double D = Kd * (error + prevError);
  double localOutput = P + integral + D;

  prevError = error;  // Actualiza la variable global prevError

  output2 = localOutput;  // Asigna localOutput a la variable global output2

  // Limitar el valor de output para que esté dentro del rango del servo
  if (output2 > 175) {
    output2 = 175;
  } else if (output2 < 1) {
    output2 = 1;
  }

  // Actualizar la variable global output si es necesario
  if (180 - output != output2) { 
    Serial.println(output);
    Serial.println(output2);
    output = 180 - output2;

    // Control del servo
    digitalWrite(SERVO_PIN, HIGH);
    delay(100);
    servo.attach(SERVO_PIN);

    servo.write(output);
    delay(1000);

    digitalWrite(SERVO_PIN, LOW);
    delay(100);
    servo.detach();
  }
  
  Serial.println(output);
  Serial.println(output2);
  return output2;
}

void displayDHTData(float temperature1, float humidity1, float temperature2, float humidity2, float temperature3, float humidity3) {
  // Mostrar los datos del primer sensor DHT22
  display.setCursor(0, 20);
  display.print("T1:");
  display.print(temperature1);
  display.println("C");

  display.setCursor(64, 20);
  display.print("H1:");
  display.print(humidity1);
  display.println("%");

  // Mostrar los datos del segundo sensor DHT22
  display.setCursor(0, 30);
  display.print("T2:");
  display.print(temperature2);
  display.println("C");

  display.setCursor(64, 30);
  display.print("H2:");
  display.print(humidity2);
  display.println("%");

  // Mostrar los datos del tercer sensor DHT22
  display.setCursor(0, 40);
  display.print("T3:");
  display.print(temperature3);
  display.println("C");

  display.setCursor(64, 40);
  display.print("H3:");
  display.print(humidity3);
  display.println("%");
}

void oledGiro(){
  display.setCursor(0, 50);
  display.print("G:");
  display.print(output2);
}

void oledRele(){
  display.setCursor(35, 50);
  display.print("R:");
  display.print(estadoRelee);
}

void writeTemperatureToCSV(float temperature1, float humidity1, float temperature2, float humidity2, float temperature3, float humidity3, struct tm *timeinfo) {
  static int lastDay = -1; // Inicializamos con un valor que no corresponde a un día válido
  char fileName[15]; // Nombre del archivo CSV
  
  // Generar el nombre del archivo con el formato yyyymmdd
  sprintf(fileName, "/%04d%02d%02d.csv", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);

  // Verificar si la fecha ha cambiado desde la última escritura
  if (timeinfo->tm_mday != lastDay) {
    // Si la fecha ha cambiado, cerrar el archivo anterior (si estaba abierto)
    if (dataFile) {
      dataFile.close();
    }
    
    if (!SD.begin()) {
      Serial.println("Error al inicializar la tarjeta SD");
      display.setCursor(87, 50);
      display.print("SD=No");
      return;
    }
    
    // Actualizar el último día registrado
    lastDay = timeinfo->tm_mday;
  }

  // Abrir un nuevo archivo o continuar escribiendo en el existente
  dataFile = SD.open(fileName, FILE_APPEND);
  if (!dataFile) {
    Serial.println("Error al abrir el archivo CSV");
    display.setCursor(87, 50);
    display.print("SD=N");
    return;
  }

  char formattedTime[20];
  char formattedDate[20];
  sprintf(formattedTime, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  sprintf(formattedDate, "%04d/%02d/%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);
  
  // Escribe la fecha, hora, temperatura y humedad en el archivo CSV
  dataFile.print(formattedDate);
  dataFile.print(",");
  dataFile.print(formattedTime);
  dataFile.print(",");
  dataFile.print(temperature1);
  dataFile.print(",");
  dataFile.print(humidity1);
  dataFile.print(",");
  dataFile.print(temperature2);
  dataFile.print(",");
  dataFile.print(humidity2);
  dataFile.print(",");
  dataFile.print(temperature3);
  dataFile.print(",");
  dataFile.print(humidity3);
  dataFile.print(",");
  dataFile.print(output2);
  dataFile.print(",");
  dataFile.println(estadoRelee);
  dataFile.close();
}

bool TS = false;
void sendDataToThingSpeak(float temperature1, float humidity1, float temperature2, float humidity2, float temperature3, float humidity3) {
  WiFiClient client;

  if (!client.connect(server, httpPort)) {
    Serial.println("Error al conectarse a ThingSpeak");
    TS = false;
    return;
  }

  String postStr = apiKey;
  postStr += "&field1=";
  postStr += String(temperature1);
  postStr += "&field2=";
  postStr += String(humidity1);
  postStr += "&field3=";
  postStr += String(temperature2);
  postStr += "&field4=";
  postStr += String(humidity2);
  postStr += "&field5=";
  postStr += String(temperature3);
  postStr += "&field6=";
  postStr += String(humidity3);
  postStr += "&field7=";
  postStr += String(output2);
  postStr += "&field8=";
  postStr += String(estadoRelee);
  postStr += "\r\n\r\n";

  client.print("POST /update HTTP/1.1\n");
  client.print("Host: api.thingspeak.com\n");
  client.print("Connection: close\n");
  client.print("X-THINGSPEAKAPIKEY: " + String(apiKey) + "\n");
  client.print("Content-Type: application/x-www-form-urlencoded\n");
  client.print("Content-Length: ");
  client.print(postStr.length());
  client.print("\n\n");
  client.print(postStr);

  Serial.println("Datos enviados a ThingSpeak");
  TS = true;

  delay(1000); // Espera un momento para la respuesta del servidor

  client.stop();
}

void setup() {
  Serial.begin(115200);

  // Inicializar la pantalla OLED con la dirección I2C específica
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Error al inicializar la pantalla OLED"));
    while (true);
  }

  // Configurar lf a pantalla
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Inicializar la comunicación I2C
  Wire.begin();

  display.clearDisplay();

  // Conectar a WiFi
  connectToWiFi();

  pinMode(relayPin, OUTPUT);

  // Iniciar el cliente NTP
  timeClient.begin();

  // Iniciar los sensores DHT
  dht1.begin();
  dht2.begin();
  dht3.begin();
}

void loop() {
  static int counter = 4; // Contador para controlar el envío a ThingSpeak
  counter++; // Incrementar el contador en cada iteración

  // Actualizar la hora desde el servidor NTP
  updateTime();

  // Obtener la hora y la fecha del servidor NTP
  time_t now = timeClient.getEpochTime();
  struct tm *timeinfo = localtime(&now);
  
  char formattedTime[20];
  char formattedDate[20];
  
  // Iniciar parte de la pantalla

  display.clearDisplay();
  
  // Obtener la hora y la fecha formateadas
  getTimeAndDate(timeinfo, formattedTime, formattedDate);

  // Mostrar la hora y la fecha en la pantalla OLED
  displayTimeAndDate(formattedTime, formattedDate);

  // Leer los datos de los sensores DHT
  readDHTData(dht1, tempStr1, humStr1);
  readDHTData(dht2, tempStr2, humStr2);
  readDHTData(dht3, tempStr3, humStr3);

  controlPI(dht1.readTemperature());

  // Llama a la función para controlar el relé basado en la humedad
  controlRelay(dht2.readHumidity(), dht1.readTemperature());

  oledGiro();
  oledRele();

  // Mostrar los datos de los sensores DHT en la pantalla OLED
  displayDHTData(dht1.readTemperature(), dht1.readHumidity(), dht2.readTemperature(), dht2.readHumidity(), dht3.readTemperature(), dht3.readHumidity());

  // Guardar la temperatura en el archivo CSV
  writeTemperatureToCSV(dht1.readTemperature(), dht1.readHumidity(), dht2.readTemperature(), dht2.readHumidity(), dht3.readTemperature(), dht3.readHumidity(), timeinfo);

  
  // Ejecutar la función de ThingSpeak si el contador alcanza 60
  if (counter >= 5) {
    sendDataToThingSpeak(dht1.readTemperature(), dht1.readHumidity(), dht2.readTemperature(), dht2.readHumidity(), dht3.readTemperature(), dht3.readHumidity());
    counter = 0; // Reiniciar el contador
  }

  // Mostrar estado de ThingSpeak en la pantalla
  display.setCursor(64, 50);
  if (TS == false) {
    display.println("T=N");
  } else {
    display.println("T=Y");
  }

  display.display();

    // Tiempo microsegundos
  uint64_t tiempo_de_espera = 5000000;

  delay(5000);
}
