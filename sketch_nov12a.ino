#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>


#define WIFI_SSID "WiFi-FF"
#define WIFI_PASSWORD "Character@123"

const unsigned char Passive_buzzer = 14;


const int MPU_addr = 0x68; // I2C address of the MPU-6050
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;
float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
boolean fall = false;     //stores if a fall has occurred
boolean trigger1 = false; //stores if first trigger (lower threshold) has occurred
boolean trigger2 = false; //stores if second trigger (upper threshold) has occurred
boolean trigger3 = false; //stores if third trigger (orientation change) has occurred
byte trigger1count = 0;   //stores the counts past since trigger 1 was set true
byte trigger2count = 0;   //stores the counts past since trigger 2 was set true
byte trigger3count = 0;   //stores the counts past since trigger 3 was set true
int angleChange = 0;
int count_api = 0;
const int trigP = 2; //D4 Or GPIO-2 of nodemcu
const int echoP = 0; //D3 Or GPIO-0 of nodemcu

long duration;
int distance;
int count = 1;

void setup()
{
  pinMode (Passive_buzzer,OUTPUT) ;
  pinMode(trigP, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoP, INPUT);  // Sets the echoPin as an Input
  Serial.begin(9600);     // Open serial channel at 9600 baud rate
  delay(10);

  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0);    // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);
  Serial.println("Wrote to IMU");

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());
}

void loop()
{
  delay(100);
  // delay(5000);
  digitalWrite(trigP, LOW); // Makes trigPin low
  delayMicroseconds(2);     // 2 micro second delay
  digitalWrite(trigP, HIGH);       // tigPin high
  delayMicroseconds(10);           // trigPin high for 10 micro seconds
  digitalWrite(trigP, LOW);        // trigPin low
  duration = pulseIn(echoP, HIGH); //Read echo pin, time in microseconds
  distance = duration * 0.034 / 2; //Calculating actual/real distance

  Serial.print("Distance = "); //Output distance on arduino serial monitor
  Serial.println(distance);
  count_api++;
  if(count_api == 50){
    String fl = "false";
    updateDB_dist(distance, "false");
    count_api = 0;
  }
  // delay(10000);  
  mpu_read();
  ax = (AcX - 2050) / 16384.00;
  ay = (AcY - 77) / 16384.00;
  az = (AcZ - 1947) / 16384.00;
  gx = (GyX + 270) / 131.07;
  gy = (GyY - 351) / 131.07;
  gz = (GyZ + 136) / 131.07;
  // calculating Amplitute vactor for 3 axis
  float Raw_Amp = pow(pow(ax, 2) + pow(ay, 2) + pow(az, 2), 0.5);
  int Amp = Raw_Amp * 10; // Mulitiplied by 10 bcz values are between 0 to 1
  Serial.print("Amp :");
  
  Serial.println(Amp);
  if (Amp <= 9 && trigger2 == false)
  { //if AM breaks lower threshold (0.4g)
    trigger1 = true;
    Serial.println("TRIGGER 1 ACTIVATED");
  }
  if (trigger1 == true)
  {
    trigger1count++;
    if (Amp >= 15)
    { //if AM breaks upper threshold (3g)
      trigger2 = true;
      Serial.println("TRIGGER 2 ACTIVATED");
      trigger1 = false;
      trigger1count = 0;
    }
  }
  if (trigger2 == true)
  {
    trigger2count++;
    angleChange = pow(pow(gx, 2) + pow(gy, 2) + pow(gz, 2), 0.5);
    Serial.print("angleChange : ");

    Serial.println(angleChange);
    if (angleChange >= 5 && angleChange <= 200)
    { //if orientation changes by between 80-100 degrees
      trigger3 = true;
      trigger2 = false;
      trigger2count = 0;
      Serial.println(angleChange);
      Serial.println("TRIGGER 3 ACTIVATED");
    }
  }
  if (trigger3 == true)
  {
    trigger3count++;
    if (trigger3count >= 5)
    {
      angleChange = pow(pow(gx, 2) + pow(gy, 2) + pow(gz, 2), 0.5);
      //delay(10);
      Serial.println(angleChange);
      if ((angleChange >= 0) && (angleChange <= 150))
      { //if orientation changes remains between 0-10 degrees
        fall = true;
        trigger3 = false;
        trigger3count = 0;
        Serial.println(angleChange);
      }
      else
      { //user regained normal orientation
        trigger3 = false;
        trigger3count = 0;
        Serial.println("TRIGGER 3 DEACTIVATED");
      }
    }
  }
  if (fall == true)
  { //in event of a fall detection
    Serial.println("FALL DETECTED");
    String tr = "true";
    updateDB_dist(distance,tr);
    PlayTone();
    fall = false;
    delay(4000);
  }
  if (trigger2count >= 6)
  { //allow 0.5s for orientation change
    trigger2 = false;
    trigger2count = 0;
    Serial.println("TRIGGER 2 DECACTIVATED");
  }
  if (trigger1count >= 6)
  { //allow 0.5s for AM to break upper threshold
    trigger1 = false;
    trigger1count = 0;
    Serial.println("TRIGGER 1 DECACTIVATED");
  }
  
}

bool updateDB_dist(int distance, String trig)
{

  String data = "sensor_id=101&creation_date&height=" + String(distance)+"&fall=" +String(trig);
  Serial.println(data);
  HTTPClient http;
  if (http.begin("http://smartcontainer-rest-api.herokuapp.com/smartcontainer/insert/"))
  {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    auto httpCode = http.POST(data);
    if (httpCode > 0)
    {
      Serial.println(httpCode); //Print HTTP return code
      String payload = http.getString();
      Serial.println(payload); //Print request response payload
      http.end();              //Close connection Serial.println();
      Serial.println();
      Serial.println("closing connection");
    }
    else
      Serial.printf("[HTTP] POST... failed, error: %s", http.errorToString(httpCode).c_str());
  }
  else
  {
    Serial.println("UNABLE TO CONNECT HTTTTTP");
  }
  return true;


}

void mpu_read()
{
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr, 14, true); // request a total of 14 registers
  AcX = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  AcY = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ = Wire.read() << 8 | Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  Tmp = Wire.read() << 8 | Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  GyX = Wire.read() << 8 | Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  GyY = Wire.read() << 8 | Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  GyZ = Wire.read() << 8 | Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
}

void PlayTone(){
Serial.println("Playing");  
tone(Passive_buzzer, 523) ; //DO note 523 Hz
delay (200); 
tone(Passive_buzzer, 587) ; //RE note ...
delay (200); 
tone(Passive_buzzer, 659) ; //MI note ...
delay (200); 
tone(Passive_buzzer, 783) ; //FA note ...
delay (200); 
tone(Passive_buzzer, 880) ; //SOL note ...
delay (200); 
tone(Passive_buzzer, 987) ; //LA note ...
delay (200); 
tone(Passive_buzzer, 1046) ; // SI note ...
delay (200); 
noTone(Passive_buzzer) ; //Turn off the pin attached to the tone()
}
