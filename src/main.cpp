#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>

const char *ssid = "";
const char *password = "paid4bylinkedin";
const char *FILENAME = "/shots.csv";
WebServer server(80);

// Define pins for HC-SR04
const int trigPin = 13; // Trigger pin connected to digital pin 9
const int echoPin = 12; // Echo pin connected to digital pin 10

// Variables for distance measurement
long duration;
float distance;
float baselineDistance;
const int numBaselineReadings = 10;

// Thresholds
float distanceThreshold;           // Distance indicating a ball is present
const float thresholdOffset = 1.0; // Adjust as needed

// Timing variables
unsigned long shotDetectedTime = 0;
const unsigned long debounceDelay = 500; // Time in milliseconds to prevent multiple detections

void connectWifi()
{
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

String readFile()
{
  File file = SPIFFS.open(FILENAME, "r");
  if (!file)
  {
    server.send(500, "text/plain", "Failed to open file");
    return "";
  }

  // Read file content
  String fileContent = "";
  while (file.available())
  {
    fileContent += (char)file.read();
  }
  file.close();
  return fileContent;
}

void resetCSVFile()
{
  // Open the file in write mode to overwrite its contents
  File file = SPIFFS.open(FILENAME, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for resetting.");
    return;
  }

  // Write the CSV headers (adjust headers as needed)
  file.println("Timestamp,ShotCount");
  file.close();

  Serial.print("File reset: ");
  Serial.println(FILENAME);
}

void handleGetData()
{
  String fileContent = readFile();
  // Send the content with the appropriate header
  server.send(200, "text/csv", fileContent);
}

void recordShot()
{
  // Get current timestamp
  unsigned long timestamp = millis();

  // Read current shot count
  static uint32_t shotCount = 0;
  shotCount++;

  // Open the file in append mode
  File file = SPIFFS.open(FILENAME, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }

  // Write data to the file
  file.printf("%lu,%u\n", timestamp, shotCount);
  file.close();

  Serial.print("Shot recorded: ");
  Serial.print("Timestamp: ");
  Serial.print(timestamp);
  Serial.print(", ShotCount: ");
  Serial.println(shotCount);
}

void initServer()
{
  server.on("/get_data", HTTP_GET, handleGetData);

  // Start the server
  server.begin();
  Serial.println("HTTP server started.");
}

float getDistance()
{
  // Clear the trigger pin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Set the trigger pin HIGH for 10 microseconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the echo pin
  duration = pulseIn(echoPin, HIGH, 30000UL); // Timeout after 30ms (to avoid blocking)

  // Calculate distance in cm
  float dist = duration * 0.0343 / 2;

  // Check for out-of-range readings
  if (dist == 0 || dist > baselineDistance + 100)
  {
    // Invalid reading; return baseline distance
    dist = baselineDistance;
  }

  Serial.print("Distance: ");
  Serial.print(dist);
  Serial.println(" cm");

  return dist;
}

float calculateBaselineDistance()
{
  float totalDistance = 0;
  for (int i = 0; i < numBaselineReadings; i++)
  {
    totalDistance += getDistance();
    delay(100); // Short delay between readings
  }
  return totalDistance / numBaselineReadings;
}

bool detectShot(float currentDistance)
{
  // Check if the distance is less than the threshold and debounce time has passed
  if (currentDistance < distanceThreshold && (millis() - shotDetectedTime > debounceDelay))
  {
    return true;
  }
  return false;
}

void setup()
{
  // Initialize serial communication
  Serial.begin(9600);

  if (!SPIFFS.begin(true))
  {
    Serial.println("An error occurred while mounting SPIFFS");
    return;
  }

  // connectWifi();
  // initServer();

  // Initialize sensor pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  baselineDistance = calculateBaselineDistance();
  distanceThreshold = baselineDistance - thresholdOffset;

  Serial.print("Baseline Distance: ");
  Serial.print(baselineDistance);
  Serial.println(" cm");

  Serial.print("Distance Threshold: ");
  Serial.print(distanceThreshold);
  Serial.println(" cm");
}

void loop()
{
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Remove whitespace or newline characters

    if (command.equalsIgnoreCase("RESET"))
    {
      resetCSVFile();
      Serial.println("CSV file has been reset.");
    }
  }

  server.handleClient();
  distance = getDistance();

  // Check for shot detection
  if (detectShot(distance))
  {
    shotDetectedTime = millis();
    recordShot();
    Serial.println("Shot detected!");
    Serial.print(shotDetectedTime);
  }
  Serial.print(readFile());

  // Short delay before next measurement
  delay(50); // Adjust as needed
}