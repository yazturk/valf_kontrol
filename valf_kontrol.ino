
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>

// WiFi erişim noktasının ağ adı ve şifresi
#define ssid TANIMLA
#define password TANIMLA

//röleler Normally Open ise true Normally Close ise false yapın
#define RELAY_NO    false

// Cihaz ekleyip çıkarırken NUM_DEVICES da güncellenmelidir
#define NUM_DEVICES 5
int relayGPIOs[NUM_DEVICES] = {5,4,14,12,13};

enum Mode {AUTO, MANUAL, OFF};
Mode mode;

//Dakika cinsinden otomatik değişim aralığı
int range = 1;
unsigned long lastChange, nextChange;
// Web server port numarası
AsyncWebServer server(80);

// Gönderilecek html cevabı
// Flash bellekte saklanır
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {margin:0px auto; text-align:center; max-width: 600px; }
    h2 {padding: 5px auto; background-color:#e6e6ff; color: #442200;}
    div {margin: 10px;}
    fieldset {display: inline-block; margin: 10px; background-color:lightgrey;}
    legend {color:white; background-color: black;}
    .infoContainer {padding: 5px; border-radius: 5px; margin: 5px; display: inline-block;}
    .container {display:inline-block; padding: 15px;}
    .container:hover {background-color:#eeeecc;}
    .switch {position: relative; display: inline-block; width: 60px; height: 34px} 
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 17px}
    .slider:before {position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 30px}
    input:checked+.slider {background-color: #2196F3}
    input:checked+.slider:before {-webkit-transform: translateX(26px); -ms-transform: translateX(26px); transform: translateX(26px)}
  </style>
</head>
<body>
  <h2>ESP Bahce Sulama</h2>
  <div>
    <div class="infoContainer"><b>Mod:</b><span id="mode">%MODE%</span></div>
    <div class="infoContainer"><b id="nextText"></b><span id="next"></span></div><br>
    <div class="infoContainer"><b>Sure (dk):</b>
      <input type="number" min="1" id="range" value="%RANGE%">
      <input type="button" id="rangeButton" value="Uygula" onclick="submitRange();">
    </div>
  </div>
  <div><input type="button" id="stopButton"></input></div>
  <div id="buttons">
  %BUTTONPLACEHOLDER%
  </div>
<script>
//Arayuz ogelerini sec
const modeBox = document.querySelector("#mode");
const rangeBox = document.querySelector("#range");
const nextText = document.querySelector("#nextText");
const nextBox = document.querySelector("#next");
const stopButton = document.querySelector("#stopButton");

//Bu ogeler sadece sayfa yenilenince guncellenir
var numRelays = %COUNT%;
rangeBox.value = %RANGE%;

//Arayuz degiskenlerini esp'den al
getState();

//Bu fonksiyonu her saniye tekrarla
setInterval(getState, 1000);

//http istegiyle esp'den guncel verileri al
function getState() { 
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/updateall", true);
  xhr.onreadystatechange = () => {
    if (xhr.readyState == 4) {
      if (xhr.status == 200)
      {
        setState(JSON.parse(xhr.responseText));
      } 
      else { //status 200 degil
        console.log("Status: " + xhr.status);
      }
    } 
    else { //readyState 4 degil
      console.log("Ready: " + xhr.readyState + " Status: " + xhr.status);
    }
  }
  xhr.send();
}

//gelen http yanitini HTML ogelerine uygula
function setState(res) {
  if (res == "Error") return;

  var e, i, device;
  for (i = 0; i<numRelays; i++) {
    device = "Cihaz " + i;
    e = document.getElementById(i);
    e.checked = res[device];
  }
  var mode, nowMillis, nextMillis;

  mode = res['mode'];
  nowMillis = res['now'];
  nextMillis = res['next'];
  setNext(nowMillis, nextMillis);

  modeBox.innerHTML = mode;
  
  switch (mode) {
    case "Kapali":
      nextText.innerHTML = "Sonraki:";
      stopButton.value = "Otomatik";
      stopButton.onclick = switchAuto;
      break;
    case "Manuel":
      nextText.innerHTML = "Bitis:";
      stopButton.value = "Durdur";
      stopButton.onclick = stopAll;
      break;
    case "Otomatik":
      nextText.innerHTML = "Sonraki:";
      stopButton.value = "Durdur";
      stopButton.onclick = stopAll;
      break;
  }
}
  
function setNext(nowMillis, nextMillis) {
  var next = new Date();
  next.setTime(next.getTime() + (nextMillis - nowMillis));
  nextBox.innerHTML = next.toLocaleTimeString().slice(0,5);
}

function submitRange() {
  var xhr = new XMLHttpRequest();
  
  xhr.open("GET", "/submit?range=" + rangeBox.value, true);
  xhr.send();
}
//tum cihazlari durdurmak icin sinyal gonder
function stopAll() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/stop", true);
  xhr.send();
  console.log("STOP");
}
//otomatik moda gecmek icin sinyal gonder
function switchAuto() {
  console.log("Otomatik modu ac");
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/auto", true);
  xhr.send(); 
}

//belirli bir cihazi ac/kapa sinyali
function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ 
    xhr.open("GET", "/update?device="+element.id+"&state=true", true); 
  } else { 
    xhr.open("GET", "/update?device=" + element.id + "&state=false", true); 
  }
  xhr.send();
}
</script>
</body>
</html>
)rawliteral";
// html'deki değişkenleri işler
String processor(const String& var){
  String relayStateValue;
  if(var == "BUTTONPLACEHOLDER"){
    String buttons ="";
    for(int i=0; i<NUM_DEVICES; i++){
      relayStateValue = relayState(i) ? "checked" : "";
      buttons+= "<span class='container'><h4>Cihaz#" 
                + String(i) + " - GPIO " + relayGPIOs[i] 
                + "</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"" 
                + String(i) + "\" "+ relayStateValue +" autocomplete='off'><span class=\"slider\"></span></label>"
                + "</span>\n";
    }
    return buttons;
  } 
  if (var == "COUNT") return String(NUM_DEVICES);
  if (var == "RANGE") return String(range);
  return String();
}

void setup() {
  Serial.begin(115200);

  // Cihazları çıkış olarak ayarla
  for (int i=0; i<NUM_DEVICES; i++) {
    pinMode(relayGPIOs[i], OUTPUT);
  }
  turnOff();
  
  Serial.print("\nBağlanılıyor: ");
  Serial.print(ssid);

  //Wifi bağlantısını başlat
  WiFi.begin(ssid, password);

  //Bağlantı kurulana kadar bekle
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi bağlandı.");
  Serial.print("IP adresi: ");
  Serial.println(WiFi.localIP());

  //Anasayfa istendiğinde5
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  //Güncel bilgiler istendiğinde
  server.on("/updateall", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", stateJSON());
  });
  //Belirli bir cihazı açma kapama
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String device, state;
    bool stateBool;
    
    if (request->hasParam("device") && request->hasParam("state")) {
      device = request->getParam("device")->value();
      state = request->getParam("state")->value();
      stateBool = (state=="true");
      turnDevice(device.toInt(), stateBool);
      //Tüm cihazların kapalı olup olmadığını kontrol et
      int mydevice = -1;
        for (int i=0; i<NUM_DEVICES; i++) {
          if(relayState(i)) {
            mydevice=i;
            break;
          }
        }
        //Açık cihaz bulunamadıysa OFF, bulunduysa MANUAL
        mode = (mydevice == -1) ? OFF : MANUAL;
    }
    else {
      Serial.println("Update isteği anlaşılmadı");
    }
    request->send(200, "text/plain", "OK");
  });
  //Aralık değiştirme isteği
  server.on("/submit", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("range")) {
      int r = request->getParam("range")->value().toInt();
      if (r>=1) {
        range = r;
        //Kapalı modda daha doğru lastchange değeri görünmesi için
        if (mode==OFF) lastChange=millis();
        //İsteğin mevcut işi de etkilemesi için
        nextChange = lastChange + 60000*range;
        Serial.println("range değişti");
        request->send(200, "text/plain", "OK");
      }
      else request->send(200, "text/plain", "Invalid request");
    }
    else request->send(200, "text/plain", "Invalid request");
  });
  //Otomatik moda geçme isteği geldiğinde
  server.on("/auto", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Otomatik moda geçiliyor");
    turnOff();
    mode = AUTO;
    turnDevice(0, true);
    request->send(200, "text/plain", "OK");
  });
  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    turnOff();
    request->send(200, "text/plain", "OK");
  });

  server.begin();
}


String modeToString(Mode m) {
  String result;
  switch (m) {
    case AUTO:
      result = "Otomatik";
      break;
    case MANUAL:
      result = "Manuel";
      break;
    case OFF:
      result = "Kapali";
      break;
  }
  return result;
}

bool relayState(int numRelay){
  if (RELAY_NO) {
    if(digitalRead(relayGPIOs[numRelay])) return false;
    else return true;
  }
  else {
    if (digitalRead(relayGPIOs[numRelay])) return true;
    else return false;
  }
}

String stateJSON() {
  String state;
  String result = "{";
  for (int i=0; i<NUM_DEVICES; i++) {
    state = relayState(i) ? "true" : "false";
    result += "\"Cihaz " + String(i) + "\": " + state + ", ";
  }
  result += "\"mode\": \"" + modeToString(mode) + "\", ";
  result += "\"now\": " + String(millis()) + ", ";
  result += "\"next\": " + String(nextChange) + "}";
  return result;
}

void turnDevice(int d, bool s) {
  int result;
  if (RELAY_NO)
    result = s ? LOW : HIGH;
  else
    result = s ? HIGH : LOW;
  digitalWrite(relayGPIOs[d], result);
  lastChange = millis();
  nextChange = lastChange + 60000*range;
}

void turnOff() {
  mode = OFF;
  for(int i=0; i<NUM_DEVICES; i++) {
    turnDevice(i, false);
  }
  
}

void loop(){
  unsigned long now = millis();
  int device;
  delay(1000); // 1 saniye bekle
  if (now>=nextChange) {
    switch (mode) {
      case AUTO:
        //aktif cihazı bul
        device = -1;
        for (int i=0; i<NUM_DEVICES; i++) {
          if(relayState(i)) {
            device=i;
            break;
          }
        }
        if (device == -1) {
          Serial.println("Otomatik modda aktif cihaz yok");
        }
        else {
          //Aktif cihazı kapat
          turnDevice(device, false);
          // Son cihazdaysa OFF moda geç
          if (device == (NUM_DEVICES - 1)) {
            mode = OFF;
          }
          // Değilse 1 sonrakine geç
          else {
            device += 1;
            // Yeni cihazı aktive et
            turnDevice(device, true);
          }
        }
        break;
      case MANUAL: 
        //Hepsini kapat
        for (int i=0; i<NUM_DEVICES; i++) {
          turnDevice(i, false);
        }
        break;
      case OFF:
        break;
    }
  }
}
