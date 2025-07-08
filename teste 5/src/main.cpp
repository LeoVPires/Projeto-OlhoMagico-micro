#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>
#include "camera_pins.h" 

// Biblioteca para o Telegram Bot
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h> 

// Substitua pelas suas credenciais de Wi-Fi
#define WIFI_SSID "ALHN-6349"
#define WIFI_PASSWORD "AnDALtnmf7"

// Credenciais do Telegram Bot
#define BOT_TOKEN "7691898679:AAEdlE-BD4rFwxMUczeC8GnjE84UD3z-b_M" 
#define CHAT_ID "7986445882"     

// Porta para o servidor web
#define WEB_SERVER_PORT 80

// Variável global para o servidor
WiFiServer server(WEB_SERVER_PORT);

// Cliente seguro para o Telegram
WiFiClientSecure telegramClient;
UniversalTelegramBot bot(BOT_TOKEN, telegramClient);

// --- NOVAS VARIÁVEIS PARA MÚLTIPLOS CLIENTES ---
#define MAX_STREAM_CLIENTS 2 // Limite de clientes de stream simultâneos (main.py + 1 pessoa)
WiFiClient streamClients[MAX_STREAM_CLIENTS]; // Array para armazenar clientes de stream

volatile int activeStreamClients = 0; 

// --- Variáveis para as páginas HTML ---

// Página HTML para visualização do stream (na raiz /)
const char* HTML_STREAM_VIEWER = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>ESP32-CAM Stream</title>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<style>
body { font-family: Arial, sans-serif; text-align: center; margin-top: 20px; }
h1 { color: #333; }
img { max-width: 90%; height: auto; border: 2px solid #ccc; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
p { font-size: 1.1em; color: #555; }
</style>
</head>
<body>
<h1>ESP32-CAM Live Stream</h1>
<img src="http://%s/stream">
<p>Acesse o stream diretamente em: <a href="http://%s/stream">http://%s/stream</a></p>
<p>Este stream suporta até %d conexões simultâneas.</p>
</body>
</html>
)rawliteral";

// Página HTML de "limite de usuários excedido"
const char* TOO_MANY_CLIENTS_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Limite de Usuários Excedido</title>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<style>
body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; background-color: #f0f0f0; color: #333; }
.message-box {
    background-color: #fff;
    border: 1px solid #ddd;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    padding: 30px;
    margin: 50px auto;
    max-width: 500px;
    border-radius: 8px;
}
h1 { color: #d9534f; }
p { font-size: 1.1em; line-height: 1.6; }
</style>
</head>
<body>
<div class="message-box">
    <h1>Oops!</h1>
    <p>Muitos usuários estão acessando a câmera no momento.</p>
    <p>Por favor, aguarde alguns instantes e recarregue a página.</p>
</div>
</body>
</html>
)rawliteral";


// Função de configuração da câmera (sem alterações)
esp_err_t setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Erro ao inicializar a câmera: 0x%x", err);
    return err;
  }
  return ESP_OK;
}

void startCameraServer() {
  server.begin();
  Serial.println("Servidor de câmera iniciado.");
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  telegramClient.setInsecure();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando-se ao Wi-Fi...");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println("");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi conectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());

    String ipMessage = "ESP32-CAM conectado! IP: " + WiFi.localIP().toString()+"/stream";
    bot.sendMessage(CHAT_ID, ipMessage, "");
    Serial.println("IP enviado para o Telegram.");

  } else {
    Serial.println("Falha ao conectar ao Wi-Fi. Verifique suas credenciais.");
    bot.sendMessage(CHAT_ID, "ESP32-CAM: Falha ao conectar ao Wi-Fi.", "");
    return;
  }

  if (setupCamera() != ESP_OK) {
    bot.sendMessage(CHAT_ID, "ESP32-CAM: Falha ao inicializar a câmera.", "");
    return;
  }

  startCameraServer();
}

void loop() {
    // 1. Aceitar novas conexões
    WiFiClient newClient = server.available();
    if (newClient) {
        String req = newClient.readStringUntil('\n'); // Leia a primeira linha da requisição
        Serial.println(req);
        // Consome o restante do cabeçalho
        while (newClient.connected() && newClient.available()) {
            String line = newClient.readStringUntil('\n');
            if (line.length() == 0) {
                break;
            }
        }

        if (req.indexOf("GET /stream HTTP/1.1") != -1) { // Requisição ESPECÍFICA para o stream
            // Tenta encontrar um slot livre APENAS para clientes de stream
            int freeSlot = -1;
            for (int i = 0; i < MAX_STREAM_CLIENTS; i++) {
                if (!streamClients[i] || !streamClients[i].connected()) {
                    freeSlot = i;
                    break;
                }
            }

            if (freeSlot != -1) {
                streamClients[freeSlot] = newClient; // Atribui o novo cliente ao slot
                activeStreamClients++;
                Serial.printf("Novo cliente de stream conectado! Total: %d\n", activeStreamClients);
                // Envia os cabeçalhos MJPEG para o cliente de stream
                streamClients[freeSlot].println("HTTP/1.1 200 OK");
                streamClients[freeSlot].println("Content-Type: multipart/x-mixed-replace; boundary=--frame");
                streamClients[freeSlot].println();
            } else {
                // Limite de clientes de stream atingido, rejeita esta conexão
                Serial.println("Limite de clientes de stream excedido! Rejeitando.");
                newClient.println("HTTP/1.1 503 Service Unavailable");
                newClient.println("Content-Type: text/plain"); // Não é um stream, então text/plain
                newClient.println("Retry-After: 60");
                newClient.println();
                newClient.println("Limite de conexoes de stream excedido. Por favor, tente novamente mais tarde.");
                newClient.stop(); // Fecha a conexão
            }
        } else { // Requisição para a raiz (ou qualquer outra URL não-stream)
            // Serve a página HTML de visualização ou a de limite excedido
            newClient.println("HTTP/1.1 200 OK");
            newClient.println("Content-Type: text/html");
            newClient.println();
            
            // Aqui, a lógica é que a página HTML não precisa de um slot de cliente persistente
            // Ela é servida e a conexão é fechada.
            // A mensagem de "limite excedido" nesta página HTML é apenas informativa,
            // não impede que o usuário tente acessar o /stream novamente.
            char buffer[512]; // Buffer para formatar a string HTML
            snprintf(buffer, sizeof(buffer), HTML_STREAM_VIEWER, 
                     WiFi.localIP().toString().c_str(), 
                     WiFi.localIP().toString().c_str(), 
                     WiFi.localIP().toString().c_str(), 
                     MAX_STREAM_CLIENTS);
            newClient.print(buffer); // Envia o HTML formatado
            newClient.stop(); // Fecha a conexão para requisições não-stream
        }
    }

    // 2. Enviar frames para todos os clientes de stream ativos
    // Só tenta capturar um frame se houver pelo menos um cliente de stream ativo.
    // Isso economiza CPU da câmera se ninguém estiver assistindo.
    if (activeStreamClients > 0) {
        camera_fb_t *fb = esp_camera_fb_get(); 
        if (!fb) {
            Serial.println("Falha ao capturar frame!");
            delay(1); 
            return; 
        }

        for (int i = 0; i < MAX_STREAM_CLIENTS; i++) {
            if (streamClients[i] && streamClients[i].connected()) { 
                streamClients[i].println("--frame");
                streamClients[i].println("Content-Type: image/jpeg");
                streamClients[i].print("Content-Length: ");
                streamClients[i].println(fb->len);
                streamClients[i].println();

                streamClients[i].write(fb->buf, fb->len);
                streamClients[i].println();
            } else if (streamClients[i]) { // Se o cliente existia mas desconectou
                streamClients[i].stop(); 
                streamClients[i] = WiFiClient(); // Limpa o slot
                activeStreamClients--;
                Serial.printf("Cliente %d desconectado. Total: %d\n", i, activeStreamClients);
            }
        }
        esp_camera_fb_return(fb); 
    } else {
        // Se não há clientes de stream ativos, não captura frames da câmera.
        // Isso é importante para economizar energia e CPU.
        delay(100); // Pequeno atraso para não lotar o loop principal
    }
}