# valf_kontrol
## ESP8266 ile selenoid valflerin web arayüzü üzerinden kontrol edilmesi
+ Valf kullanılarak test edilmedi. 5,4,14,12,13 GPIO numaralarına sahip pinleri kontrol eder
+ Valfleri çalıştırmaya mikrodenetleyicinin gücü yetmeyeceği için röle kullanılmalıdır
+ Arayüzün çalıştığını görmek için ekran kaydı videosuna bakın. Ekran kaydında aynı anda iki ayrı cihazın web tarayısından kontrol edilebildiği gösterilmiştir. 1 dakika beklememek için 1:11'e sarabilirsiniz
+ Derlemeden önce Wifi ağı bilgileri ve valflerin NO (normally open) veya NC (Normally Close) olduğunu belirten değişken düzenlenmelidir. (Varsayılan NC)
+ Yazılım derlenip yüklendiğinde ilgili wifi ağına bağlanır ve wifi ağının verdiği IP adresi üzerinden web arayüzü sunar

[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/eWfWYK3jIr4/0.jpg)](https://www.youtube.com/watch?v=eWfWYK3jIr4)
