# 🔬 Akıllı Metre (All-in-One Akıllı Ölçüm Cihazı)

<div align="center">
    <img src="img/4.jpeg" alt="ESP32-BlueJammer" width="50%" >
</div>

Bu proje, ESP32 tabanlı, web arayüzü desteğine sahip ve bünyesinde 5 farklı ölçüm modunu barındıran gelişmiş bir dijital ölçüm cihazıdır. Tek bir cihazla mesafe ölçebilir, renk analizi yapabilir, eğim (su terazisi) kontrol edebilir ve ortam verilerini takip edebilirsiniz.

## ✨ Öne Çıkan Özellikler

* **📏 Mesafe Ölçümü:** VL53L1X ToF sensörü ile lazer hassasiyetinde ölçüm.
* **🎨 Renk Analizi:** TCS34725 sensörü ile objelerin RGB ve Hex kodlarını tespit etme.
* **⚖️ Dijital Su Terazisi:** MPU6050 ile hassas X ve Y ekseni eğim gösterimi.
* **🌡️ Hava İstasyonu:** SHT31 ile gerçek zamanlı sıcaklık ve nem takibi.
* **💡 Işık Ölçer:** BH1750 ile lüks (lux) cinsinden ışık şiddeti ölçümü.
* **🌐 Web Arayüzü:** Wi-Fi üzerinden bağlanarak verileri canlı izleme ve modu değiştirme.
* **📺 OLED Ekran:** Cihaz üzerinde 128x64 çözünürlükte anlık veri gösterimi.

---

## 📸 Proje Görselleri



### Web Arayüzü Kullanımı


### Sensör Yerleşimi


---

## 🛠️ Kullanılan Donanımlar

| Bileşen | Görevi |
| :--- | :--- |
| **ESP32** | Ana Kontrolcü & Wi-Fi Sunucu |
| **VL53L1X** | Lazer Mesafe Sensörü |
| **TCS34725** | Renk Sensörü |
| **MPU6050** | İvmeölçer ve Jiroskop |
| **SSD1306** | 0.96" OLED Ekran |
| **BH1750** | Işık Şiddeti Sensörü |
| **SHT31** | Sıcaklık ve Nem Sensörü |

---

## 💻 Kurulum ve Kullanım

1.  **Arduino IDE** üzerine ESP32 kart desteğini kurun.
2.  Gerekli kütüphaneleri (Adafruit GFX, SSD1306, VL53L1X, TCS34725 vb.) Library Manager üzerinden indirin.
3.  Kodu ESP32 cihazınıza yükleyin.
4.  Cihaz açıldığında **"Akilli_Metre"** isimli Wi-Fi ağına bağlanın.
5.  Tarayıcınıza `192.168.4.1` yazarak kontrol paneline erişin.

---

## 📄 Lisans
Bu proje [MIT Lisansı](LICENSE) ile korunmaktadır.
