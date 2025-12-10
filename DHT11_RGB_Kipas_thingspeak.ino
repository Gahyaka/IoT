#include <WiFi.h>
#include <HTTPClient.h> // Library untuk kirim data via URL
#include <DHT.h>

// ==========================================
// 1. KONFIGURASI WIFI & API KEY
// ==========================================
const char* ssid = "fairuz";
const char* password = "12345678";

// Konfigurasi ThingSpeak (Tanpa Library)
const char* server = "api.thingspeak.com";
String apiKey = "48FBCQHJYDA6DMPA"; // API Key Kamu

// ==========================================
// 2. PIN SENSOR & OUTPUT
// ==========================================
#define DHTPIN 18      // Pastikan kabel data DHT di Pin 18
#define DHTTYPE DHT11

// Pin RGB LED
#define RED_PIN   19
#define GREEN_PIN 23   
#define BLUE_PIN  4

// Pin Relay
#define RELAY_PIN 14

// Batas Suhu Pemicu Kipas
const float SUHU_BATAS_KIPAS = 32.0; 

// ==========================================
// VARIABEL GLOBAL
// ==========================================
DHT dht(DHTPIN, DHTTYPE);

float humidity = 0;
float temperature = 0;
int statusKipasAngka = 0; // 0 = Mati, 1 = Nyala (Untuk dikirim ke ThingSpeak)

// Timer (Agar multitasking lancar)
unsigned long lastSensorTime = 0;
unsigned long lastThingSpeakTime = 0;

// Interval Waktu
const long intervalSensor = 2000;      // Baca sensor & atur kipas tiap 2 detik
const long intervalThingSpeak = 16000; // Kirim ke ThingSpeak tiap 16 detik (Agar tidak error limit)

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  
  // Inisialisasi Pin
  pinMode(DHTPIN, INPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // Kondisi Awal: Matikan Relay & LED
  digitalWrite(RELAY_PIN, HIGH); // Asumsi Relay Active Low (HIGH = Mati)
  digitalWrite(RED_PIN, LOW); 
  digitalWrite(GREEN_PIN, LOW); 
  digitalWrite(BLUE_PIN, LOW);

  dht.begin();

  // Koneksi WiFi
  Serial.println();
  Serial.print("Menghubungkan ke WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println("\n✅ Terhubung ke WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// ==========================================
// LOGIKA SENSOR & KIPAS (Berjalan tiap 2 detik)
// ==========================================
void checkSensorAndControl() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Cek jika sensor error
  if (isnan(h) || isnan(t)) {
    Serial.println("❌ Gagal membaca DHT! Cek kabel.");
    return;
  }

  // Update variabel global
  humidity = h;
  temperature = t;

  Serial.print("Suhu: " + String(t) + "°C | Kelembaban: " + String(h) + "% | ");

  // --- LOGIKA KIPAS (RELAY) ---
  // Jika Suhu > 32, Kipas Nyala
  if (t > SUHU_BATAS_KIPAS) {
    digitalWrite(RELAY_PIN, HIGH); // LOW = Nyala (Active Low)
    statusKipasAngka = 1;         // Status 1 untuk ThingSpeak
    Serial.print("Kipas: ON 🔥 | ");
  } else {
    digitalWrite(RELAY_PIN, LOW); // HIGH = Mati
    statusKipasAngka = 0;          // Status 0 untuk ThingSpeak
    Serial.print("Kipas: OFF ❄️ | ");
  }

  // --- LOGIKA LED RGB ---
  if (t <= 30.0) { 
    // Dingin = Biru
    digitalWrite(RED_PIN, LOW); digitalWrite(GREEN_PIN, LOW); digitalWrite(BLUE_PIN, HIGH);
    Serial.println("LED: Biru");
  } else if (t > 30.0 && t <= 32.0) { 
    // Normal = Hijau
    digitalWrite(RED_PIN, LOW); digitalWrite(GREEN_PIN, HIGH); digitalWrite(BLUE_PIN, LOW);
    Serial.println("LED: Hijau");
  } else { 
    // Panas = Merah
    digitalWrite(RED_PIN, HIGH); digitalWrite(GREEN_PIN, LOW); digitalWrite(BLUE_PIN, LOW);
    Serial.println("LED: Merah");
  }
}

// ==========================================
// KIRIM KE THINGSPEAK (Berjalan tiap 16 detik)
// ==========================================
void sendToThingSpeak() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Membuat URL seperti mengetik di browser
    // Format: http://api.thingspeak.com/update?api_key=XXX&field1=XXX&field2=XXX&field3=XXX
    String url = "http://" + String(server) + "/update?api_key=" + apiKey +
                 "&field1=" + String(temperature) +
                 "&field2=" + String(humidity) +
                 "&field3=" + String(statusKipasAngka);

    // Mulai koneksi
    http.begin(url);
    
    // Kirim Request (GET)
    int httpCode = http.GET();
    
    // Cek respon server
    if (httpCode > 0) {
      Serial.print("☁️ Kirim ThingSpeak Berhasil! Kode Respon: ");
      Serial.println(httpCode); // Jika 200 artinya sukses
    } else {
      Serial.print("⚠️ Gagal kirim data. Error HTTP: ");
      Serial.println(httpCode);
    }
    
    // Tutup koneksi
    http.end();
    
  } else {
    Serial.println("⚠️ WiFi Putus, tidak bisa kirim data.");
    WiFi.reconnect(); // Coba sambung ulang
  }
}

// ==========================================
// LOOP UTAMA
// ==========================================
void loop() {
  unsigned long currentMillis = millis();

  // 1. Eksekusi Sensor & Kipas setiap 2 detik
  if (currentMillis - lastSensorTime >= intervalSensor) {
    lastSensorTime = currentMillis;
    checkSensorAndControl();
  }

  // 2. Eksekusi Kirim Data setiap 16 detik
  // ThingSpeak butuh jeda minimal 15 detik antar pengiriman
  if (currentMillis - lastThingSpeakTime >= intervalThingSpeak) {
    lastThingSpeakTime = currentMillis;
    sendToThingSpeak();
  }
}