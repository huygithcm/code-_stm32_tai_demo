#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

// ESP32 DevKit V1 UART2 defaults. Change these for a different ESP32 board.
static constexpr int SERVO_UART_RX_PIN = 16;
static constexpr int SERVO_UART_TX_PIN = 17;
static constexpr uint32_t SERVO_UART_BAUD = 9600;
static constexpr uint8_t AP_CHANNEL = 6;

HardwareSerial servoUart(2);
DNSServer dnsServer;
WebServer server(80);
String accessPointName;
String accessPointPassword;

static const char INDEX_HTML[] PROGMEM = R"SERVOHTML(
<!doctype html>
<html lang="vi">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <meta name="theme-color" content="#10151b">
  <title>Servo Bench</title>
  <style>
    :root{color-scheme:dark;--bg:#10151b;--panel:#19212b;--soft:#202a36;--line:#2e3a48;--text:#edf3fa;--muted:#9cabbc;--accent:#69d6b0;font-family:system-ui,-apple-system,"Segoe UI",sans-serif}
    *{box-sizing:border-box}body{margin:0;min-height:100vh;color:var(--text);background:radial-gradient(ellipse at top left,#1a2b31 0,transparent 38%),var(--bg)}
    main{width:min(940px,calc(100% - 24px));margin:28px auto 48px}header{display:flex;justify-content:space-between;align-items:center;gap:14px;margin-bottom:18px}
    h1{margin:0 0 5px;font-size:clamp(26px,5vw,36px);letter-spacing:-.8px}.sub,.note{margin:0;color:var(--muted);line-height:1.5}.badge{border:1px solid #347760;color:var(--accent);background:#163229;padding:8px 12px;border-radius:999px;font-size:12px;white-space:nowrap}
    .panel{background:var(--panel);border:1px solid var(--line);border-radius:15px}.toolbar{display:flex;gap:9px;flex-wrap:wrap;margin:16px 0 12px}button{min-height:40px;padding:9px 13px;border:1px solid var(--line);border-radius:9px;background:var(--soft);color:var(--text);font:inherit;font-weight:650;cursor:pointer}button:hover{border-color:#5a7185}button.primary{background:var(--accent);border-color:var(--accent);color:#07160f}
    .grid{display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:11px}.servo{padding:14px;min-width:0}.servo-head{display:flex;justify-content:space-between;gap:4px;align-items:center;margin-bottom:16px}.name{font-weight:750}.actual{font-size:11px;color:var(--muted);white-space:nowrap}.inputs{display:grid;grid-template-columns:1fr 62px;align-items:center;gap:9px}input[type=range]{width:100%;accent-color:#28b889}input[type=number]{width:100%;padding:7px 4px;text-align:center;color:var(--text);background:#111820;border:1px solid var(--line);border-radius:8px;font:inherit}.ticks{display:flex;justify-content:space-between;color:#788797;font-size:10px;margin:4px 2px 13px}.servo button{width:100%;padding:8px 5px}
    .result{margin-top:14px;padding:14px}.result-title{font-size:12px;color:var(--muted);margin-bottom:8px}pre{min-height:42px;max-height:150px;overflow:auto;margin:0;color:#d3e0ea;white-space:pre-wrap;word-break:break-word;font:12px/1.5 ui-monospace,SFMono-Regular,monospace}.note{font-size:12px;margin-top:14px}
    @media(max-width:800px){.grid{grid-template-columns:repeat(3,minmax(0,1fr))}}@media(max-width:520px){main{margin-top:18px}header{align-items:flex-start;flex-direction:column}.grid{grid-template-columns:repeat(2,minmax(0,1fr));gap:8px}.servo{padding:11px}}
  </style>
</head>
<body><main>
  <header><div><h1>Servo Bench</h1><p class="sub">Điều khiển 5 servo qua Wi‑Fi cục bộ.</p></div><span class="badge">ESP32 SoftAP · STM32 UART_1</span></header>
  <div class="toolbar"><button class="primary" id="center">Đưa cả 5 về 90°</button><button id="status">Đọc trạng thái</button><button id="help">HELP</button></div>
  <section class="grid" id="servos" aria-label="Điều khiển từng servo"></section>
  <section class="panel result"><div class="result-title" id="state">Đang chờ phản hồi…</div><pre id="reply">Mỗi servo chỉ di chuyển sau khi nhấn Gửi góc.</pre></section>
  <p class="note">Tham số góc: 0–180°. Kết nối ESP32 GPIO17/TX → J5 pin 2/PA10, GPIO16/RX ← J5 pin 3/PA9, và GND chung.</p>
</main><script>
  const $=s=>document.querySelector(s), reply=$('#reply'), state=$('#state');
  for(let ch=1;ch<=5;ch++){
    const card=document.createElement('article');card.className='panel servo';
    card.innerHTML=`<div class="servo-head"><span class="name">Servo ${ch}</span><span class="actual" id="actual-${ch}">Chưa đọc</span></div><div class="inputs"><input id="range-${ch}" type="range" min="0" max="180" value="90" aria-label="Góc servo ${ch}"><input id="angle-${ch}" type="number" min="0" max="180" step="1" value="90" aria-label="Nhập góc servo ${ch}"></div><div class="ticks"><span>0°</span><span>90°</span><span>180°</span></div><button class="primary" data-channel="${ch}">Gửi góc</button>`;
    $('#servos').append(card);
    const slider=$(`#range-${ch}`),number=$(`#angle-${ch}`);slider.oninput=()=>number.value=slider.value;number.oninput=()=>{if(number.value!=='')slider.value=Math.max(0,Math.min(180,Number(number.value)))};
  }
  function showStatus(text){for(const match of text.matchAll(/S([1-5])=(\d{1,3})/g)){const ch=Number(match[1]),angle=Number(match[2]);if(angle<=180){$(`#actual-${ch}`).textContent=`Hiện tại ${angle}°`; $(`#range-${ch}`).value=angle;$(`#angle-${ch}`).value=angle}}}
  async function api(path,body){const options=body===undefined?{}:{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body)};const response=await fetch(path,options);const data=await response.json();reply.textContent=data.response||data.error||'(không có phản hồi)';state.textContent=data.command?`TX: ${data.command} · ${data.ok?'đã nhận phản hồi':'lỗi UART'}`:(data.ok?'Đã kết nối với server ESP32':'Lỗi');if(data.command==='STATUS'||/^S[1-5]=/.test(data.command||''))showStatus(data.response||'');if(data.command==='ALL=90'){for(let ch=1;ch<=5;ch++)showStatus(`S${ch}=90`)}if(!response.ok||!data.ok)throw new Error(data.error||'Không nhận được phản hồi từ STM32');return data}
  $('#center').onclick=()=>api('/api/center',{}).catch(e=>{state.textContent=e.message});
  $('#status').onclick=()=>api('/api/status').catch(e=>{state.textContent=e.message});
  $('#help').onclick=()=>api('/api/help').catch(e=>{state.textContent=e.message});
  document.querySelectorAll('[data-channel]').forEach(button=>button.onclick=async()=>{const ch=Number(button.dataset.channel),raw=$(`#angle-${ch}`).value,angle=Number(raw);if(raw===''||!Number.isInteger(angle)||angle<0||angle>180){state.textContent='Góc phải là số nguyên từ 0 đến 180.';return}button.disabled=true;try{await api('/api/set',{channel:ch,angle})}catch(e){state.textContent=e.message}finally{button.disabled=false}});
  api('/api/status').catch(e=>{state.textContent=e.message});
</script></body></html>
)SERVOHTML";

String jsonEscape(const String &input) {
    String output;
    output.reserve(input.length() + 8);
    for (size_t i = 0; i < input.length(); ++i) {
        const char value = input[i];
        if (value == '"' || value == '\\') {
            output += '\\';
            output += value;
        } else if (value == '\n') {
            output += "\\n";
        } else if (value != '\r') {
            output += value;
        }
    }
    return output;
}

void sendJson(int statusCode, bool ok, const String &command,
              const String &response, const String &error = "") {
    String body = "{\"ok\":";
    body += ok ? "true" : "false";
    body += ",\"command\":\"" + jsonEscape(command);
    body += "\",\"response\":\"" + jsonEscape(response);
    body += "\",\"error\":\"" + jsonEscape(error) + "\"}";
    server.send(statusCode, "application/json; charset=utf-8", body);
}

String exchangeWithStm32(const String &command, uint32_t timeoutMs = 1200) {
    while (servoUart.available() > 0) {
        servoUart.read();
    }

    servoUart.print(command);
    servoUart.print("\r\n");
    servoUart.flush();

    String response;
    const uint32_t startedAt = millis();
    uint32_t lastByteAt = startedAt;
    bool receivedByte = false;
    while ((uint32_t)(millis() - startedAt) < timeoutMs) {
        while (servoUart.available() > 0) {
            const char value = static_cast<char>(servoUart.read());
            if (value != '\r') {
                response += value;
            }
            receivedByte = true;
            lastByteAt = millis();
        }
        if (receivedByte && (uint32_t)(millis() - lastByteAt) >= 120u) {
            break;
        }
        delay(1);
    }
    response.trim();
    return response;
}

bool parseUnsignedArg(const String &name, int minimum, int maximum, int &value) {
    if (!server.hasArg(name)) {
        return false;
    }
    const String raw = server.arg(name);
    if (raw.length() == 0) {
        return false;
    }
    for (size_t i = 0; i < raw.length(); ++i) {
        if (raw[i] < '0' || raw[i] > '9') {
            return false;
        }
    }
    value = raw.toInt();
    return value >= minimum && value <= maximum;
}

void handleStatus() {
    const String response = exchangeWithStm32("STATUS");
    const bool ok = response.startsWith("STATUS ");
    sendJson(ok ? 200 : 504, ok, "STATUS", response,
             ok ? "" : "Không nhận được STATUS từ STM32; kiểm tra dây UART và nguồn.");
}

void handleSetServo() {
    int channel = 0;
    int angle = 0;
    if (!parseUnsignedArg("channel", 1, 5, channel) ||
        !parseUnsignedArg("angle", 0, 180, angle)) {
        sendJson(400, false, "", "", "Kênh phải từ 1–5, góc phải từ 0–180.");
        return;
    }

    const String command = "S" + String(channel) + "=" + String(angle);
    const String response = exchangeWithStm32(command);
    const String expected = "OK S" + String(channel) + "=" + String(angle) + " deg";
    const bool ok = response == expected;
    sendJson(ok ? 200 : 504, ok, command, response,
             ok ? "" : "STM32 không xác nhận lệnh; kiểm tra UART_1.");
}

void handleCenter() {
    const String response = exchangeWithStm32("ALL=90");
    const bool ok = response == "OK ALL=90 deg";
    sendJson(ok ? 200 : 504, ok, "ALL=90", response,
             ok ? "" : "STM32 không xác nhận lệnh; kiểm tra UART_1.");
}

void handleHelp() {
    const String response = exchangeWithStm32("HELP");
    const bool ok = response.startsWith("Commands");
    sendJson(ok ? 200 : 504, ok, "HELP", response,
             ok ? "" : "Không nhận được HELP từ STM32; kiểm tra UART_1.");
}

void handleNotFound() {
    if (server.uri().startsWith("/api/")) {
        sendJson(404, false, "", "", "API không tồn tại.");
        return;
    }
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void startAccessPoint() {
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);

    String mac = WiFi.softAPmacAddress();
    mac.replace(":", "");
    mac.toUpperCase();
    if (mac.length() != 12 || mac == "000000000000") {
        const uint64_t deviceId = ESP.getEfuseMac();
        char deviceIdText[13];
        snprintf(deviceIdText, sizeof(deviceIdText), "%04X%08X",
                 static_cast<unsigned>((deviceId >> 32) & 0xFFFFu),
                 static_cast<unsigned>(deviceId & 0xFFFFFFFFu));
        mac = deviceIdText;
    }
    accessPointName = "ServoBench-" + mac.substring(mac.length() - 4);
    accessPointPassword = "SB-" + mac;

    const IPAddress address(192, 168, 4, 1);
    WiFi.softAPConfig(address, address, IPAddress(255, 255, 255, 0));
    if (!WiFi.softAP(accessPointName.c_str(), accessPointPassword.c_str(),
                     AP_CHANNEL, false, 4)) {
        Serial.println("ERROR: Could not start the Wi-Fi access point.");
        return;
    }

    dnsServer.start(53, "*", address);
    Serial.println();
    Serial.println("Servo control Wi-Fi started:");
    Serial.print("  SSID: ");
    Serial.println(accessPointName);
    Serial.print("  Password: ");
    Serial.println(accessPointPassword);
    Serial.println("  URL: http://192.168.4.1/");
}

void setup() {
    Serial.begin(115200);
    delay(200);

    servoUart.begin(SERVO_UART_BAUD, SERIAL_8N1,
                    SERVO_UART_RX_PIN, SERVO_UART_TX_PIN);
    startAccessPoint();

    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/set", HTTP_POST, handleSetServo);
    server.on("/api/center", HTTP_POST, handleCenter);
    server.on("/api/help", HTTP_GET, handleHelp);
    server.onNotFound(handleNotFound);
    server.begin();

    Serial.println("HTTP servo server ready.");
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
    delay(1);
}
