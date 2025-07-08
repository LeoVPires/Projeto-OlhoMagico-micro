#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// Substitua pelas suas credenciais de Wi-Fi
const char* ssid = "ALHN-6349";
const char* password = "AnDALtnmf7";

// Porta HTTP padrão
const int port = 80;
WebServer server(port);

// Definição dos pinos
const int LED_PIN = 25;   // Pino do LED (GPIO2 é comum para o LED onboard)
const int MOTOR_PIN = 26; // Pino para o motor de vibração (escolha um pino GPIO livre)

// Configurações dos pulsos
const int PULSE_ON_DURATION_MS = 600;   // Duração LIGADO do pulso (0.5 segundo)
const int PULSE_OFF_DURATION_MS = 400;  // Duração DESLIGADO entre pulsos (0.5 segundo)
const int BURST_COOLDOWN_MS = 1200;     // Tempo de espera entre cada "rajada" de pulsos (1.5 segundos)
const int NUMBER_OF_BURSTS = 3;         // Número de rajadas de pulsos

// Variáveis de estado do alerta
volatile bool alert_active = false; // Indica se há um alerta em progresso
volatile int pulses_to_do_in_burst = 0; // Quantos pulsos devem ser feitos nesta rajada
volatile int pulses_done_in_burst = 0;  // Quantos pulsos já foram feitos (ligado/desligado)
volatile int current_burst = 0;             // Rajada atual
volatile unsigned long last_pulse_event_time = 0; // Tempo do último evento de pulso (liga/desliga)
volatile unsigned long last_burst_end_time = 0; // <--- NOVO: Tempo do fim da última rajada (para cooldown)

// --- Funções de Handler para o Servidor Web ---

void handleRoot() {
  server.send(200, "text/plain", "Servidor de Alerta ESP32 online! Use /acionar_alerta?pessoas=X");
}

void handleAcionarAlerta() {
  Serial.println("Comando /acionar_alerta recebido!");
  
  if (server.hasArg("pessoas")) {
    int num_pessoas = server.arg("pessoas").toInt();
    if (num_pessoas < 1) num_pessoas = 1; // Garante pelo menos 1 pulso
    if (num_pessoas > 10) num_pessoas = 10; // Limite superior para evitar loops muito longos

    // Inicia o ciclo de alerta apenas se não houver um em andamento
    if (!alert_active) {
      Serial.printf("Alerta ativado para %d pessoas.\n", num_pessoas);
      digitalWrite(LED_PIN, LOW); // Garante que começa desligado
      digitalWrite(MOTOR_PIN, LOW);
      alert_active = true;
      pulses_to_do_in_burst = num_pessoas; // Define o total de pulsos para esta rajada
      pulses_done_in_burst = 0; // Reinicia o contador de pulsos feitos
      current_burst = 1; // Começa na primeira rajada
      last_pulse_event_time = millis(); // Define o tempo inicial para o primeiro pulso
      last_burst_end_time = millis() - BURST_COOLDOWN_MS; // <--- TRUQUE: Faz parecer que o cooldown já passou se for a primeira vez
      server.send(200, "text/plain", "Alerta acionado com base no numero de pessoas!");
    } else {
      Serial.println("Alerta ja esta ativo, ignorando nova requisicao.");
      server.send(200, "text/plain", "Alerta ja esta ativo.");
    }
  } else {
    Serial.println("Parametro 'pessoas' nao encontrado. Use /acionar_alerta?pessoas=X");
    server.send(400, "text/plain", "Erro: Parametro 'pessoas' nao encontrado.");
  }
}

void handleDesligarAlerta() {
  Serial.println("Comando /desligar_alerta recebido!");
  if (alert_active) {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(MOTOR_PIN, LOW);
    alert_active = false;
    pulses_to_do_in_burst = 0;
    pulses_done_in_burst = 0;
    current_burst = 0;
    server.send(200, "text/plain", "Alerta desligado!");
    Serial.println("LED e Motor de vibracao DESLIGADOS.");
  } else {
    server.send(200, "text/plain", "Alerta nao esta ativo.");
    Serial.println("Alerta nao esta ativo.");
  }
}

// --- Setup ---
void setup() {
  Serial.begin(9600);
  Serial.println("Iniciando ESP32 de Alerta...");

  pinMode(LED_PIN, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); 
  digitalWrite(MOTOR_PIN, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi...");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println("");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi conectado!");
    Serial.print("Endereço IP do Servidor de Alerta: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Falha ao conectar ao WiFi!");
    return;
  }

  server.on("/", handleRoot);
  server.on("/acionar_alerta", handleAcionarAlerta);
  server.on("/desligar_alerta", handleDesligarAlerta); 
  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

// --- Loop ---
void loop() {
  server.handleClient(); // Processa as requisições HTTP recebidas

  if (alert_active) {
    // Lógica para controle dos pulsos e rajadas
    if (pulses_done_in_burst < pulses_to_do_in_burst) { // Ainda há pulsos para fazer na rajada atual
      // Se o LED/Motor está desligado (pronto para o próximo pulso ON) e o tempo de espera OFF passou
      if (digitalRead(LED_PIN) == LOW && (millis() - last_pulse_event_time >= PULSE_OFF_DURATION_MS)) {
        digitalWrite(LED_PIN, HIGH); // Liga
        digitalWrite(MOTOR_PIN, HIGH);
        last_pulse_event_time = millis();
        Serial.println("Alerta: PULSE ON");
      } 
      // Se o LED/Motor está ligado e o tempo de pulso ON passou
      else if (digitalRead(LED_PIN) == HIGH && (millis() - last_pulse_event_time >= PULSE_ON_DURATION_MS)) {
        digitalWrite(LED_PIN, LOW); // Desliga
        digitalWrite(MOTOR_PIN, LOW);
        last_pulse_event_time = millis();
        pulses_done_in_burst++; // Incrementa pulsos feitos
        Serial.printf("Alerta: PULSE OFF. Pulsos feitos nesta rajada: %d de %d\n", pulses_done_in_burst, pulses_to_do_in_burst);

        // Se este foi o ÚLTIMO pulso da rajada, registre o tempo final para o cooldown do burst
        if (pulses_done_in_burst == pulses_to_do_in_burst) {
          last_burst_end_time = millis(); // <--- CHAVE: Atualiza o fim da rajada aqui!
          Serial.println("Rajada de pulsos concluída. Aguardando cooldown.");
        }
      }
    } else { // Rajada atual terminou, ou estamos esperando o cooldown entre rajadas
      // Se ainda não atingimos o número total de rajadas E o cooldown da rajada passou
      if (current_burst < NUMBER_OF_BURSTS && (millis() - last_burst_end_time >= BURST_COOLDOWN_MS)) { // <--- Usa last_burst_end_time
        current_burst++;
        pulses_to_do_in_burst = server.arg("pessoas").toInt(); // Reinicia os pulsos para a próxima rajada
        if (pulses_to_do_in_burst < 1) pulses_to_do_in_burst = 1; // Garante min 1
        pulses_done_in_burst = 0; // Zera o contador de pulsos feitos para a nova rajada
        Serial.printf("Iniciando Rajada %d. Pulsos: %d\n", current_burst, pulses_to_do_in_burst);
        last_pulse_event_time = millis(); // Reseta o tempo para o primeiro pulso da nova rajada
      } 
      // Todas as rajadas foram concluídas
      else if (current_burst >= NUMBER_OF_BURSTS) {
        digitalWrite(LED_PIN, LOW); // Garante que tudo está desligado
        digitalWrite(MOTOR_PIN, LOW);
        alert_active = false; // Desativa o alerta
        Serial.println("Alerta pulsante completo. Alerta desativado.");
      }
    }
  }
}