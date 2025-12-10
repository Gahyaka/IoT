import time
import network
import urequests
from machine import Pin, reset
import dht

# ==============================
# BLYNK CONFIG
# ==============================
BLYNK_AUTH = "V_39q2N0us9nnC3KyUz3oQDECch9UjPp".strip()
BLYNK_UPDATE_URL = "http://blynk.cloud/external/api/update?"
BLYNK_GET_URL    = "http://blynk.cloud/external/api/get?"

# Mapping virtual pin:
# V1 = Suhu
# V2 = Kelembapan
# V3 = Indikator Kipas (teks)
# V4 = LED indikator PANAS (0-255)
# V5 = LED indikator NORMAL/Hijau (0-255)
# V6 = LED indikator SEJUK/Biru (0-255)
# V0 = Slider Batas Suhu (opsional)

# ==============================
# WI-FI CONFIG
# ==============================
WIFI_SSID = "fairuz"
WIFI_PASS = "12345678"

def wifi_connect():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(WIFI_SSID, WIFI_PASS)

    print("Menyambungkan WiFi...", end="")
    timeout = 20
    while not wlan.isconnected() and timeout > 0:
        time.sleep(1)
        timeout -= 1
        print(".", end="")

    if not wlan.isconnected():
        print("\nWiFi gagal tersambung, reset board...")
        time.sleep(2)
        reset()

    print("\nWiFi tersambung. IP:", wlan.ifconfig()[0])

wifi_connect()

# ==============================
# FUNGSI KIRIM / AMBIL DATA BLYNK
# ==============================
def blynk_send(pin, value):
    """
    Kirim data ke Blynk.
    pin   : string, misal 'V1', 'V2'
    value : angka atau string
    """
    try:
        url = f"{BLYNK_UPDATE_URL}token={BLYNK_AUTH}&{pin}={value}"
        # print("URL:", url)
        urequests.get(url).close()
    except Exception as e:
        print("Gagal kirim ke Blynk:", e)

def blynk_get(pin):
    """
    Ambil data dari Blynk (misal untuk slider).
    pin : string, misal 'V0'
    return: string value atau None jika gagal
    """
    try:
        url = f"{BLYNK_GET_URL}token={BLYNK_AUTH}&{pin}"
        r = urequests.get(url)
        txt = r.text.strip()
        r.close()
        if txt == "" or "Invalid" in txt:
            return None
        return txt
    except Exception as e:
        print("Gagal get dari Blynk:", e)
        return None

# ==============================
# HARDWARE
# ==============================
dht_sensor  = dht.DHT11(Pin(15))
led_r       = Pin(13, Pin.OUT)
led_g       = Pin(12, Pin.OUT)
led_b       = Pin(14, Pin.OUT)
relay_kipas = Pin(17, Pin.OUT)

def rgb(r, g, b):
    led_r.value(r)
    led_g.value(g)
    led_b.value(b)

rgb(0, 0, 0)
relay_kipas.value(0)

# ==============================
# VARIABEL LOGIKA
# ==============================
batas_suhu = 32
last_pub = 0
last_status = ""
last_slider_check = 0  # waktu terakhir cek slider

# ==============================
# LOGIKA KONTROL SUHU
# ==============================
def apply_logic(t, h, sumber="STATUS"):
    """
    Menghitung logika LED + kipas,
    serta mengirimkan ke Blynk via HTTP.
    """
    global batas_suhu, last_status

    # Hitung status
    if t >= batas_suhu:
        rgb(1, 0, 0)
        relay_kipas.value(1)
        status = "PANAS - Kipas ON"
        warna = "MERAH"
        v4 = 255
        v5 = 0
        v6 = 0
    else:
        relay_kipas.value(0)
        if t < 31:
            rgb(0, 0, 1)
            status = "SEJUK - Kipas OFF"
            warna = "BIRU"
            v6 = 255
            v4 = 0
            v5 = 0
        else:
            rgb(0, 1, 0)
            status = "NORMAL - Kipas OFF"
            warna = "HIJAU"
            v5 = 255
            v4 = 0
            v6 = 0

    # Cetak ke shell (hindari spam tapi tetap informatif)
    if sumber == "STATUS" or status != last_status or sumber == "UPDATE":
        last_status = status
        print(
            f"[{sumber}] Suhu: {t} C | Kelembapan: {h}% | "
            f"Batas Suhu: {batas_suhu} C | {status} | LED: {warna}"
        )

    # Kirim ke Blynk
    try:
        # sesuaikan mapping Vx dengan datastream di Blynk kamu
        blynk_send("V1", t)           # Suhu
        blynk_send("V2", h)           # Kelembapan
        blynk_send("V3", status)      # Indikator teks
        blynk_send("V4", v4)          # LED Panas
        blynk_send("V5", v5)          # LED Hijau
        blynk_send("V6", v6)          # LED Biru
    except Exception as e:
        print("[BLYNK SEND ERROR]:", e)

# ==============================
# MAIN PROGRAM
# ==============================
print("=== SISTEM BLYNK HTTP BERJALAN ===\n")

while True:
    now = time.time()

    # 1. Cek slider dari Blynk tiap 3 detik (misal di V0)
    if now - last_slider_check >= 3:
        val = blynk_get("V0")  # pastikan di app slider terhubung ke V0
        if val is not None:
            try:
                new_batas = int(float(val))
                if new_batas != batas_suhu:
                    batas_suhu = new_batas
                    print(f"[UPDATE] Batas suhu baru dari Blynk: {batas_suhu} C")

                    # Baca ulang sensor dan update langsung
                    try:
                        dht_sensor.measure()
                        apply_logic(
                            dht_sensor.temperature(),
                            dht_sensor.humidity(),
                            sumber="UPDATE"
                        )
                    except Exception as e:
                        print("Sensor error saat UPDATE:", e)
            except:
                print("Nilai slider tidak valid dari Blynk:", val)
        last_slider_check = now

    # 2. Baca sensor dan kirim status periodik, misal tiap 8 detik
    if now - last_pub >= 8:
        try:
            dht_sensor.measure()
            suhu = dht_sensor.temperature()
            hum  = dht_sensor.humidity()
            apply_logic(suhu, hum, sumber="STATUS")
            last_pub = now
        except Exception as e:
            print("Sensor Error:", e)

    time.sleep(0.1)

