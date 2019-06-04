

/* Ejemplo para utilizar la comunicación con thingboards paso a paso con nodeMCU v2
 *  Gastón Mousqués
 *  Basado en varios ejemplos de la documentación de  https://thingsboard.io
 *  
 */

// includes de bibliotecas para comunicación
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

//***************MODIFICAR PARA SU PROYECTO *********************
// includes de bibliotecas sensores, poner las que usen en este caso un DHT11 y un Servo
#include "DHT.h"

//***************MODIFICAR PARA SU PROYECTO *********************
//  configuración datos wifi 
// descomentar el define y poner los valores de su red y de su dispositivo
#define WIFI_AP "NOMBRE RED"
#define WIFI_PASSWORD "PASSWORD RED"


//  configuración datos thingsboard
#define NODE_NAME "Nombre dispositivo THB"   //nombre que le pusieron al dispositivo cuando lo crearon
#define NODE_TOKEN "Token dispositivo THB"   //Token que genera Thingboard para dispositivo cuando lo crearon


//***************NO MODIFICAR *********************
char thingsboardServer[] = "demo.thingsboard.io";

/*definir topicos.
 * telemetry - para enviar datos de los sensores
 * request - para recibir una solicitud y enviar datos 
 * attributes - para recibir comandos en baes a atributtos shared definidos en el dispositivo
 */
char telemetryTopic[] = "v1/devices/me/telemetry";
char requestTopic[] = "v1/devices/me/rpc/request/+";  //RPC - El Servidor usa este topico para enviar rquests, cliente response
char attributesTopic[] = "v1/devices/me/attributes";  //El Servidor usa este topico para enviar atributos

// declarar cliente Wifi y PubSus
WiFiClient wifiClient;
PubSubClient client(wifiClient);

// declarar variables control loop (para no usar delay() en loop
unsigned long lastSend;
int elapsedTime = 1000; // tiempo transcurrido entre envios al servidor


//***************MODIFICAR PARA SU PROYECTO *********************
// configuración sensores que utilizan
#define DHTPIN D1
#define DHTTYPE DHT11

// Declarar e Inicializar sensores.
DHT dht(DHTPIN, DHTTYPE);

const int ledPIN = D0;

//
//************************* Funciones Setup y loop *********************
// función setup micro
void setup()
{
  Serial.begin(9600);

  // inicializar wifi y pubsus
  connectToWiFi();
  client.setServer( thingsboardServer, 1883 );

  // agregado para recibir callbacks
  client.setCallback(on_message);
   
  lastSend = 0; // para controlar cada cuanto tiempo se envian datos


  // ******** AGREGAR INICIALZICION DE SENSORES PARA SU PROYECTO *********
  
  dht.begin(); //inicaliza el DHT

  pinMode(ledPIN , OUTPUT);  //definir pin como salida
  delay(10);
}

// función loop micro
void loop()
{
  if ( !client.connected() ) {
    reconnect();
  }

  if ( millis() - lastSend > elapsedTime ) { // Update and send only after 1 seconds
    
    // FUNCION DE TELEMETRIA para enviar datos a thingsboard
    getAndSendData();   // FUNCION QUE ENVIA INFORMACIÓN DE TELEMETRIA
    
    lastSend = millis();
  }

  client.loop();
}

//***************MODIFICAR PARA SU PROYECTO *********************
/*
 * función para leer datos de sensores y enviar telementria al servidor
 * Se debe sensar y armar el JSON especifico para su proyecto
 * Esta función es llamada desde el loop()
 */
void getAndSendData()
{

  // Lectura de sensores 
  Serial.println("Collecting temperature data.");

  // Reading temperature or humidity takes about 250 milliseconds!
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  String temperature = String(t);
  String humidity = String(h);

  //Debug messages
  Serial.print( "Sending temperature and humidity : [" );
  Serial.print( temperature ); Serial.print( "," );
  Serial.print( humidity );
  Serial.print( "]   -> " );

  // Preparar el payload, a modo de ejemplo el mensaje se arma utilizando la clase String. esto se puede hacer con
  // la biblioteca ArduinoJson
  // el formato del mensaje es {key"value, Key: value, .....}
  // en este caso seria {"temperature:"valor, "humidity:"valor}
  //
  String payload = "{";
  payload += "\"temperature\":"; payload += temperature; payload += ",";
  payload += "\"humidity\":"; payload += humidity;
  payload += "}";

  // Enviar el payload al servidor usando el topico para telemetria
  char attributes[100];
  payload.toCharArray( attributes, 100 );
  if (client.publish( telemetryTopic, attributes ) == true)
    Serial.println("publicado ok");
     
  Serial.println( attributes );

}


//***************MODIFICAR PARA SU PROYECTO *********************
/* 
 *  Este callback se llama cuando se utilizan widgets de control que envian mensajes por el topico requestTopic
 *  Notar que en la función de reconnect se realiza la suscripción al topico de request
 *  
 *  El formato del string que llega es "v1/devices/me/rpc/request/{id}/Operación. donde operación es el método get declarado en el  
 *  widget que usan para mandar el comando e {id} es el indetificador del mensaje generado por el servidor
 */

 
void on_message(const char* topic, byte* payload, unsigned int length) 
{
    // Mostrar datos recibidos del servidor
  Serial.println("On message");
  char message[length + 1];
  strncpy ( message, (char*)payload, length);
   message[length] = '\0';
  
  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println( message);

  // Veridifcar el topico por el cual llegó el mensaje
  if (strcmp(topic, "v1/devices/me/attributes") ==0) { //es un cambio en atributos compoartidos
    processAttributeRequestCommand(message);
  } else {
    // es un request
    Serial.println("----> PREQUEST");
    procesarRequest(message);
  }

}

//Metodo para procesar requests
void procesarRequest(char *message)
{

  // Decode JSON request with ArduinoJson 6
  // Notar que a modo de ejemplo este mensaje se arma utilizando la librería ArduinoJson en lugar de desarmar el string a "mano"
  const int capacity = JSON_OBJECT_SIZE(4);
  StaticJsonDocument<capacity> doc;
  DeserializationError err = deserializeJson(doc, message);
  if (err) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.c_str());\
    return;
  }

  String method = doc["method"];
  bool state = doc["params"];
  if (state == true) {
    digitalWrite(ledPIN , HIGH);   // poner el Pin en HIGH
  } else {
    digitalWrite(ledPIN , LOW);    // poner el Pin en LOW
  }
}

// Metodo para procesar los cambios en atributos
void processAttributeRequestCommand(char* message)
{  // Decode JSON request with ArduinoJson 6 https://arduinojson.org/v6/doc/deserialization/
  // Notar que a modo de ejemplo este mensaje se arma utilizando la librería ArduinoJson en lugar de desarmar el string a "mano"
  const int capacity = JSON_OBJECT_SIZE(4);
  StaticJsonDocument<capacity> doc;
  DeserializationError err = deserializeJson(doc, message);
  if (err) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.c_str());\
    return;
  }

  JsonObject obj = doc.as<JsonObject>();
  if (obj.containsKey("frecuenciaEnvioMs")) { //ver si es frecuencia
    elapsedTime = doc["frecuenciaEnvioMs"];

    Serial.print("valor frecuencia:");
    Serial.println(elapsedTime);
  }
  
 
}

//***************NO MODIFICAR - Conexion con Wifi y ThingsBoard *********************
/*
 * funcion para reconectarse al servidor de thingsboard y suscribirse a los topicos de RPC y Atributos
 */
void reconnect() {
  int statusWifi = WL_IDLE_STATUS;
  // Loop until we're reconnected
  while (!client.connected()) {
    statusWifi = WiFi.status();
    connectToWiFi();
    
    Serial.print("Connecting to ThingsBoard node ...");
    // Attempt to connect (clientId, username, password)
    if ( client.connect(NODE_NAME, NODE_TOKEN, NULL) ) {
      Serial.println( "[DONE]" );
      
      // Suscribir al Topico de request
      client.subscribe(requestTopic); 
      client.subscribe(attributesTopic); 
            
    } else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.print( client.state() );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}

/*
 * función para conectarse a wifi
 */
void connectToWiFi()
{
  Serial.println("Connecting to WiFi ...");
  // attempt to connect to WiFi network

  WiFi.begin(WIFI_AP, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
}
