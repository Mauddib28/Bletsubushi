// SQLite Includes
#include <sqlite3.h>

// TinyGPS Includes
#include <TinyGPSPlus.h>

// SSD1306/OLED Library Includes
//#include <Adafruit_SSD1306.h>
#define OLED
#define OLED_OFFSET 5
#ifdef OLED
//#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h"        // legacy: #include "SSD1306.h"
#endif

// SD Libraries
#include <SD.h>

// Bluetooth Libraries
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Additional Include
#include "FS.h"
#include "SPI.h"

// Variable Definitions
char sql[1024];
sqlite3 *db1;
sqlite3_stmt *res;
const char *tail;
int rc;
HardwareSerial SerialPort(2);
static const int RXPin = 16, TXPin = 17;
static const uint32_t GPSBaud = 9600;
// GPS data...
double lat;
double lng;
int month;
int day;
int year;
int hour;
int minute;
int second;
int alt;
double acc;
char timestamp_string[] = "%04d-%02d-%02d %02d:%02d:%02d";
int bt_count = 0;
char my_timestamp[23] = {0};
int oled_mode = 0;

// BLE Variables
int scanTime = 5;  //In seconds
BLEScan *pBLEScan;

// The TinyGPSPlus object
TinyGPSPlus gps;

#ifdef OLED
SSD1306Wire display(0x3c, 21, 22, GEOMETRY_128_32);
//SSD1306wire display(0x3c, 21, 22, 0, 0, 128, 32);
//SSD1306wire  display(0x3c, SDA, SCL, SD1306RST);
#endif

// Structure for GPS Data
void dumpInfo() {

  lat = gps.location.lat();
  lng = gps.location.lng();
  alt = gps.altitude.meters();
  month = gps.date.month();
  day = gps.date.day();
  year = gps.date.year();
  hour = gps.time.hour();
  minute = gps.time.minute();
  second = gps.time.second();
  acc = gps.hdop.hdop();

  return;
}

// Function to Trim White Space
void trim(char *str) {
  int i;
  int begin = 0;
  int end = strlen(str) - 1;

  while (isspace((unsigned char)str[begin]))
    begin++;

  while ((end >= begin) && isspace((unsigned char)str[end]))
    end--;

  // Shift all characters back to the start of the string array.
  for (i = begin; i <= end; i++)
    str[i - begin] = str[i];

  str[i - begin] = '\0';  // Null terminate string.
}

// Function to Open Database
int openDb(const char *filename, sqlite3 **db) {
   int rc = sqlite3_open(filename, db);
   if (rc) {
       Serial.printf("Can't open database: %s\n", sqlite3_errmsg(*db));
       return rc;
   } else {
       Serial.printf("Opened database successfully\n");
   }
   return rc;
}

// Setup Function for ESP32 WROOM DA
void setup() {
  Serial.begin(115200);
  SerialPort.begin(GPSBaud, SERIAL_8N1, 16, 17);

  #ifdef OLED
    display.init();
    display.clear();
    display.setFont(ArialMT_Plain_16);
  #endif

  SPI.begin();

  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    #ifdef OLED
      display.clear();
      display.drawString(0, OLED_OFFSET, " BAD SD");
      display.display();
    #endif 
    return;
  }

  #ifdef OLED
    display.clear();
    display.drawString(0, OLED_OFFSET, " SD Okay");
    display.display();
    delay(500);
  #endif 

//  WiFi.mode(WIFI_STA);
//  WiFi.disconnect();
  delay(100);

  // Setup BLE Device for Scanning
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();  //create new scan
  pBLEScan->setActiveScan(true);  //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value

  sqlite3_initialize();

  // Open database 1
  if (openDb("/sd/ble.db", &db1))
    return;

  rc = db_exec(db1, "CREATE TABLE IF NOT EXISTS ble_data (MAC TEXT PRIMARY KEY, Name TEXT, Appearance TEXT, FirstSeen TEXT, LastSeen TEXT, TxPower INTEGER, RSSI INTEGER, CurrentLatitude REAL, CurrentLongitude REAL, AltitudeMeters REAL, AccuracyMeters REAL, AddrType TEXT, ManufData TEXT)");
  if (rc != SQLITE_OK) {
    sqlite3_close(db1);
    return;
  }

}

// Additional Function Definitions
const char* data = "Callback function called";
static int callback(void *data, int argc, char **argv, char **azColName){
   int i;
   Serial.printf("%s: ", (const char*)data);
   for (i = 0; i<argc; i++){
       Serial.printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   Serial.printf("\n");
   return 0;
}

// Definitions for Executing on Database
char *zErrMsg = 0;
int db_exec(sqlite3 *db, const char *sql) {
   Serial.println(sql);
   long start = micros();
   int rc = sqlite3_exec(db, sql, callback, (void*)data, &zErrMsg);
   if (rc != SQLITE_OK) {
       Serial.printf("SQL error: %s\n", zErrMsg);
       sqlite3_free(zErrMsg);
   } else {
       Serial.printf("Operation done successfully\n");
   }
   //Serial.print(F("Time taken:"));
   //Serial.println(micros()-start);
   return rc;
}

// Main Function
void loop() {
  // put your main code here, to run repeatedly:
  int n = 0;
  const char *name;
  char mac[19] = { 0 };
  long my_bssid = 0;
  sqlite3_stmt *stmt;
  int count = 0;

  while (SerialPort.available() > 0) {
    if (gps.encode(SerialPort.read())) {
      dumpInfo();
    }
  }

  // BLE Device Scanning
  BLEScanResults *foundDevices = pBLEScan->start(scanTime, false);
  BLEAdvertisedDevice currentDevice;
  // Parse the Data from the Found Devices
  n = foundDevices->getCount();
  for (int i = 0; i < n;  ++i) {
    currentDevice = foundDevices->getDevice(i);

    if ((gps.location.isValid()) && (year > 2023)) {
      // Gather Data
      name = currentDevice.getName().c_str();
      if (sqlite3_prepare_v2(db1, "SELECT * FROM ble_data WHERE MAC=?", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, mac, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
          int rssi = currentDevice.getRSSI();
          int existingRssi = sqlite3_column_int(stmt, 6);
          // If RSSI better, update location data
          if (rssi > existingRssi) {
            sqlite3_finalize(stmt);
            sqlite3_prepare_v2(db1, "UPDATE ble_data SET Name=?, Appearance=?, LastSeen=?, TxPower=?, RSSI=?, CurrentLatitude=?, CurrentLongitude=?, AltitudeMeters=?, AccuracyMeters=?, AddrType=?, ManufData=? WHERE MAC=?", -1, &stmt, NULL);
            // Write in each pieces of the above '?' in the string
            sqlite3_bind_text(stmt, 1, name, strlen(name), SQLITE_STATIC);  // Name
            sqlite3_bind_int(stmt, 2, currentDevice.getAppearance()); // Appearance
            sqlite3_bind_text(stmt, 3, my_timestamp, strlen(my_timestamp), SQLITE_STATIC); // LastSeen
            sqlite3_bind_int(stmt, 4, currentDevice.getTXPower()); // TX Power
            sqlite3_bind_int(stmt, 5, currentDevice.getRSSI()); // RSSI
            sqlite3_bind_double(stmt, 6, lat); // Current Latitude
            sqlite3_bind_double(stmt, 7, lng); // Current Longitude
            sqlite3_bind_double(stmt, 8, alt); // Altitude Meters
            sqlite3_bind_double(stmt, 9, acc); // Accuracy Meters
            sqlite3_bind_int(stmt, 10, currentDevice.getAddressType()); // Address Type
            sqlite3_bind_text(stmt, 11, currentDevice.getManufacturerData().c_str(), strlen(currentDevice.getManufacturerData().c_str()), SQLITE_STATIC); // Manufacturer Data
            if (sqlite3_step(stmt) != SQLITE_DONE) {
              Serial.println(F("Failed to execute update statement"));
            }
          } else {
            // Update data but not location
            sqlite3_finalize(stmt);
            sqlite3_prepare_v2(db1, "UPDATE ble_data SET Name=?, Appearance=?, LastSeen=?, TxPower=?, RSSI=?, AddrType=?, ManufData=? WHERE MAC=?", -1, &stmt, NULL);
            sqlite3_bind_text(stmt, 1, name, strlen(name), SQLITE_STATIC);  // Name
            sqlite3_bind_int(stmt, 2, currentDevice.getAppearance()); // Appearance
            sqlite3_bind_text(stmt, 3, my_timestamp, strlen(my_timestamp), SQLITE_STATIC); // LastSeen
            sqlite3_bind_int(stmt, 4, currentDevice.getTXPower()); // TX Power
            sqlite3_bind_int(stmt, 5, currentDevice.getRSSI()); // RSSI
            sqlite3_bind_int(stmt, 6, currentDevice.getAddressType()); // Address Type
            sqlite3_bind_text(stmt, 7, currentDevice.getManufacturerData().c_str(), strlen(currentDevice.getManufacturerData().c_str()), SQLITE_STATIC); // Manufacturer Data
            if (sqlite3_step(stmt) != SQLITE_DONE) {
              Serial.println(F("Failed to execute update statement"));
            }
          }
        } else {
          sqlite3_finalize(stmt);
          sqlite3_prepare_v2(db1, "INSERT INTO ble_data (MAC, Name, Appearance, FirstSeen, LastSeen, TxPower, RSSI, CurrentLatitude, CurrentLongitude, AltitudeMeters, AccuracyMeters, AddrType, ManufData) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,)", -1, &stmt, NULL);
          sqlite3_bind_text(stmt, 1, currentDevice.getAddress().toString().c_str(), strlen(currentDevice.getAddress().toString().c_str()), SQLITE_STATIC);  // BT Address MAC
          sqlite3_bind_text(stmt, 2, name, strlen(name), SQLITE_STATIC);  // Name
          sqlite3_bind_int(stmt, 3, currentDevice.getAppearance()); // Appearance
          sqlite3_bind_text(stmt, 4, my_timestamp, strlen(my_timestamp), SQLITE_STATIC); // First Seen
          sqlite3_bind_text(stmt, 5, my_timestamp, strlen(my_timestamp), SQLITE_STATIC); // LastSeen
          sqlite3_bind_int(stmt, 6, currentDevice.getTXPower()); // TX Power
          sqlite3_bind_int(stmt, 7, currentDevice.getRSSI()); // RSSI
          sqlite3_bind_double(stmt, 8, lat); // Current Latitude
          sqlite3_bind_double(stmt, 9, lng); // Current Longitude
          sqlite3_bind_double(stmt, 10, alt); // Altitude Meters
          sqlite3_bind_double(stmt, 11, acc); // Accuracy Meters
          sqlite3_bind_int(stmt, 12, currentDevice.getAddressType()); // Address Type
          sqlite3_bind_text(stmt, 13, currentDevice.getManufacturerData().c_str(), strlen(currentDevice.getManufacturerData().c_str()), SQLITE_STATIC); // Manufacturer Data
          if (sqlite3_step(stmt) != SQLITE_DONE) {
            Serial.println(F("Failed to execute insert statement"));
          }
        }
        sqlite3_finalize(stmt);
      }
    }
    SerialPort.flush();
  }

  // Cleaning
  pBLEScan->clearResults();  // delete results fromBLEScan buffer to release memory
  delay(2000);

  char my_count[40] = { 0 };

#ifdef OLED
  if(oled_mode == 5)
    oled_mode = 0;

  switch(oled_mode)
  {
    case 0: // Display GPS Lat and Lng
      if(gps.location.isValid()) {
        snprintf(my_count, sizeof(my_count) - 1, "%f\n%f alt: %d", lat, lng, alt);
        display.setFont(ArialMT_Plain_10);
        display.clear();
        display.drawString(0, 0, my_count);
        display.display();
        display.setFont(ArialMT_Plain_16);
      } else {
        display.clear();
        display.drawString(0, OLED_OFFSET, " NO GPS LOCK");
        display.display();
      }
      break;
    case 1: // Display last seen count
      snprintf(my_count, sizeof(my_count) - 1, "Last: %d", n);
      display.clear();
      display.drawString(0, OLED_OFFSET, my_count);
      display.display();
      break;
    case 2: // Display total from DB
      // rc = db_exec(db1, "SELECT * from wifi_data");
      rc = sqlite3_prepare_v2(db1, "SELECT COUNT(*) from ble_data", -1, &stmt, 0);
      if (rc == SQLITE_OK) {
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
          count = sqlite3_column_int(stmt, 0);
        }
      }
      sqlite3_finalize(stmt);
      snprintf(my_count, sizeof(my_count) - 1, "%d", count);
      display.clear();
      display.drawString(0, OLED_OFFSET, my_count);
      display.display();
      break;
    case 3: // Display total OPEN AuthMode from DB
      rc = sqlite3_prepare_v2(db1, "SELECT COUNT(*) from ble_data WHERE Appearance = '0'", -1, &stmt, 0);
      if (rc == SQLITE_OK) {
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
          count = sqlite3_column_int(stmt, 0);
        }
      }
      sqlite3_finalize(stmt);
      snprintf(my_count, sizeof(my_count) - 1, "HID?: %d", count);
      display.clear();
      display.drawString(0, OLED_OFFSET, my_count);
      display.display();
      break;
    case 4: // Display UTC Date and Time...
      if(year > 2023) 
      {
        display.setFont(ArialMT_Plain_10);
        snprintf(my_count, sizeof(my_count) - 1, "%04d-%02d-%02d\n%02d:%02d:%02d", year, month, day, hour, minute, second);
        display.clear();
        display.drawString(0, 0, my_count);
        display.display();
        display.setFont(ArialMT_Plain_16);
      } else {
        display.clear();
        display.drawString(0, OLED_OFFSET, " NO GPS TIME");
        display.display();
      }
      break;
  }

  oled_mode++;
#endif
}
