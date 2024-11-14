#include <Wire.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// define WiFi
#define WIFI_SSID " " // input WiFi Name
#define WIFI_PASSWORD " " // input WiFi password

// Using Firebase
// RTDB API Key
#define API_KEY " " // input API key
#define DATABASE_URL " " // input realtime database firebase URL 

// define DATABASE
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;


// define DHT11
#include  <DHT.h>
#define   DHTPIN  16
#define   DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

float humid, temp;

// define MQ SENSORS
#include <MQUnifiedsensor.h>
#define   Board                   ("ESP-32")
#define   Voltage_Resolution      (5)
#define   ADC_Bit_Resolution      (12)
#define   Type                    ("ESP-32")

#define   Pin                     (34)
#define   Pin1                    (35)
#define   Pin2                    (32) 
#define   Pin3                    (33) 

#define   RatioMQ2CleanAir        (9.83)
#define   RatioMQ135CleanAir      (3.6)
#define   RatioMQ8CleanAir        (70)
#define   RatioMQ7CleanAir        (27.5) 

MQUnifiedsensor MQ2(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin, Type);
MQUnifiedsensor MQ135(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin1, Type);
MQUnifiedsensor MQ8(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin2, Type);
MQUnifiedsensor MQ7(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin3, Type);

float ppmMQ2, ppmMQ135, ppmMQ8, ppmMQ7;

// define LCD
#include <LiquidCrystal_I2C.h>
#define   I2C_SCL   D22
#define   I2C_SDA   D21
LiquidCrystal_I2C lcd (0x27, 16, 2);

void setup() {
  Serial.begin(115200);

  // WiFi SETUP
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print("."); delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // DATABASE SETUP
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if(Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("signUp OK");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);


  Wire.begin();
  dht.begin();

  // SENSOR SETUP
  MQ2.setRegressionMethod(1);
  MQ2.setA(1012.7); MQ2.setB(-2.786);

  MQ135.setRegressionMethod(1);
  MQ135.setA(110.47); MQ135.setB(-2.862); 
  
  MQ8.setRegressionMethod(1);
  MQ8.setA(976.97); MQ8.setB(-0.688);

  MQ7.setRegressionMethod(1);
  MQ7.setA(99.042); MQ7.setB(-1.518);

  MQ2.init();
  MQ135.init();
  MQ8.init();
  MQ7.init();

  Serial.print("Calibrating please wait.");   

  float MQ2calcR0 = 0;
  float MQ135calcR0 = 0;
  float MQ8calcR0 = 0; 
  float MQ7calcR0 = 0;  

  for(int i = 1; i<=10; i ++)   {   
    MQ2.update();  
    MQ135.update();
    MQ8.update();
    MQ7.update();

    MQ2calcR0 += MQ2.calibrate(RatioMQ2CleanAir);
    MQ135calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
    MQ8calcR0 += MQ8.calibrate(RatioMQ8CleanAir);
    MQ7calcR0 += MQ7.calibrate(RatioMQ7CleanAir);
    Serial.print(".");   
  }   

  MQ2.setR0(MQ2calcR0/10);
  MQ135.setR0(MQ135calcR0/10);
  MQ8.setR0(MQ8calcR0/10);
  MQ7.setR0(MQ7calcR0/10);

  Serial.println("  done!.");      
                
  // MQ2.serialDebug(false);
  // MQ135.serialDebug(false);
  // MQ8.serialDebug(false);
  // MQ7.serialDebug(false);

  // SERIAL MONITOR
  Serial.println(F("AirGuard"));

  // LCD SETUP
  lcd.begin(0x27, 16,2);
  lcd.backlight();

  // LCD DISPLAY
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("AirGuard");
  delay(1000);

}

void loop() {
    // MQ SENSOR
    MQ2.update();
    ppmMQ2 = MQ2.readSensor();

    MQ135.update();
    ppmMQ135 = MQ135.readSensor();

    MQ8.update();
    ppmMQ8 = MQ8.readSensor();

    MQ7.update();
    ppmMQ7 = MQ7.readSensor();

    // DHT11
    humid = dht.readHumidity();
    temp = dht.readTemperature();

  if(Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    
    // STORE DATA TO REALTIME DATABASE
    if(Firebase.RTDB.setFloat(&fbdo, "/Sensor/MQ-2", ppmMQ2)) {
      Serial.println(); Serial.print(ppmMQ2);
      Serial.print("- successfully saved to: " + fbdo.dataPath());
      Serial.println(" (" + fbdo.dataType() = ")");
    } else {
      Serial.println("Failed: " +fbdo.errorReason());
    }

    if(Firebase.RTDB.setFloat(&fbdo, "/Sensor/MQ-135", ppmMQ135)) {
      Serial.println(); Serial.print(ppmMQ135);
      Serial.print("- successfully saved to: " + fbdo.dataPath());
      Serial.println(" (" + fbdo.dataType() = ")");
    } else {
      Serial.println("Failed: " +fbdo.errorReason());
    }

    if(Firebase.RTDB.setFloat(&fbdo, "/Sensor/MQ-8", ppmMQ8)) {
      Serial.println(); Serial.print(ppmMQ8);
      Serial.print("- successfully saved to: " + fbdo.dataPath());
      Serial.println(" (" + fbdo.dataType() = ")");
    } else {
      Serial.println("Failed: " +fbdo.errorReason());
    }

    if(Firebase.RTDB.setFloat(&fbdo, "/Sensor/MQ-7", ppmMQ7)) {
      Serial.println(); Serial.print(ppmMQ7);
      Serial.print("- successfully saved to: " + fbdo.dataPath());
      Serial.println(" (" + fbdo.dataType() = ")");
    } else {
      Serial.println("Failed: " +fbdo.errorReason());
    }

    if(Firebase.RTDB.setFloat(&fbdo, "/Sensor/Humidity", humid)) {
      Serial.println(); Serial.print(humid);
      Serial.print("- successfully saved to: " + fbdo.dataPath());
      Serial.println(" (" + fbdo.dataType() = ")");
    } else {
      Serial.println("Failed: " +fbdo.errorReason());
    }

    if(Firebase.RTDB.setFloat(&fbdo, "/Sensor/Temperature", temp)) {
      Serial.println(); Serial.print(temp);
      Serial.print("- successfully saved to: " + fbdo.dataPath());
      Serial.println(" (" + fbdo.dataType() = ")");
    } else {
      Serial.println("Failed: " +fbdo.errorReason());
    }


  }

  // PRINT SENSOR DATA TO LCD
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("CH4: ");
  lcd.print(ppmMQ2);
  lcd.print(" ppm");
  
  lcd.setCursor(0,1);
  lcd.print("CO2: ");
  lcd.print(ppmMQ135);
  lcd.print(" ppm");
  delay(3000);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("H2: ");
  lcd.print(ppmMQ8);
  lcd.print(" ppm");
  
  lcd.setCursor(0,1);
  lcd.print("CO: ");
  lcd.print(ppmMQ7);
  lcd.print(" ppm");
  delay(3000);
 
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Temp: ");
  lcd.print(temp);
  lcd.print(" C");

  lcd.setCursor(0, 1);
  lcd.print("Humidity:");
  lcd.print(humid);
  lcd.print(" %");
  delay(3000);

}
