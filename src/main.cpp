/*
 * =====================================================
 *  SISTEMA DE CONTROLE DE INALADOR - ESP32
 *  Timer de segurança anti-superaquecimento
 * =====================================================
 *
 * PINAGEM:
 *  - GPIO 26 → Relé (controla o inalador ON/OFF)
 *  - GPIO 2  → LED de status integrado (ativo durante sessão)
 *  - GPIO 4  → Buzzer (alertas sonoros)
 *  - GPIO 34 → Botão físico START/STOP (pullup externo)
 *
 * COMUNICAÇÃO:
 *  - Wi-Fi + WebSocket na porta 81
 *  - Interface web servida na porta 80
 *
 * PROTOCOLO WEBSOCKET (JSON):
 *   Recebe:
 *     {"cmd":"start","duration":60}   → inicia sessão de 60s
 *     {"cmd":"stop"}                  → para imediatamente
 *     {"cmd":"status"}                → solicita estado atual
 *
 *   Envia:
 *     {"status":"idle","remaining":0}
 *     {"status":"running","remaining":45}
 *     {"status":"finished","remaining":0}
 *     {"status":"error","msg":"..."}
 *
 * DEPENDÊNCIAS (instale pela IDE Arduino / PlatformIO):
 *   - WiFi.h          (built-in ESP32)
 *   - WebSocketsServer by Markus Sattler
 *   - ArduinoJson by Benoit Blanchon
 */

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// ─── CONFIGURAÇÕES DE REDE ────────────────────────────────────
const char* SSID     = "SEU_WIFI";       // ← altere aqui
const char* PASSWORD = "SUA_SENHA";      // ← altere aqui

// ─── PINOS ───────────────────────────────────────────────────
#define PIN_RELE    26   // Relé: HIGH = inalador LIGADO
#define PIN_LED     2    // LED interno ESP32
#define PIN_BUZZER  4    // Buzzer passivo
#define PIN_BOTAO   34   // Botão físico (pullup externo 10kΩ → 3.3V)

// ─── LIMITES DE SEGURANÇA ─────────────────────────────────────
#define DURACAO_MINIMA_S   5      // mínimo: 5 segundos
#define DURACAO_MAXIMA_S   300    // máximo: 5 minutos
#define ALERTA_RESTANTE_S  10     // bipa quando restar 10s

// ─── ESTADO DO SISTEMA ────────────────────────────────────────
enum EstadoSistema { IDLE, RUNNING, FINISHED, ERROR_STATE };
EstadoSistema estado = IDLE;

unsigned long tempoInicio      = 0;
unsigned long duracaoSessao    = 0;   // em milissegundos
unsigned long ultimoEnvioWS    = 0;
unsigned long ultimoDebounce   = 0;
bool          alertaEmitido    = false;
bool          botaoAnterior    = HIGH;

WebSocketsServer ws(81);

// ─── FUNÇÕES DE HARDWARE ──────────────────────────────────────

void ligarInalador() {
  digitalWrite(PIN_RELE, HIGH);
  digitalWrite(PIN_LED,  HIGH);
}

void desligarInalador() {
  digitalWrite(PIN_RELE, LOW);
  digitalWrite(PIN_LED,  LOW);
}

void beep(int frequencia, int duracao) {
  ledcWriteTone(0, frequencia);
  delay(duracao);
  ledcWriteTone(0, 0);
}

void beepInicio() {
  beep(1000, 100);
  delay(50);
  beep(1500, 150);
}

void beepAlerta() {
  for (int i = 0; i < 3; i++) {
    beep(2000, 80);
    delay(80);
  }
}

void beepFim() {
  beep(1500, 200);
  delay(80);
  beep(1200, 200);
  delay(80);
  beep(800, 400);
}

// ─── WEBSOCKET ────────────────────────────────────────────────

void enviarStatus(uint8_t clienteId = 255) {
  StaticJsonDocument<128> doc;

  long restante = 0;
  if (estado == RUNNING) {
    long decorrido = (millis() - tempoInicio) / 1000;
    restante = max(0L, (long)(duracaoSessao / 1000) - decorrido);
  }

  switch (estado) {
    case IDLE:     doc["status"] = "idle";     break;
    case RUNNING:  doc["status"] = "running";  break;
    case FINISHED: doc["status"] = "finished"; break;
    default:       doc["status"] = "error";    break;
  }
  doc["remaining"] = restante;

  char buf[128];
  serializeJson(doc, buf);

  if (clienteId == 255) {
    ws.broadcastTXT(buf);
  } else {
    ws.sendTXT(clienteId, buf);
  }
}

void iniciarSessao(uint8_t clienteId, int duracaoSegundos) {
  if (estado == RUNNING) {
    ws.sendTXT(clienteId, "{\"status\":\"error\",\"msg\":\"Sessao ja em andamento\"}");
    return;
  }

  if (duracaoSegundos < DURACAO_MINIMA_S || duracaoSegundos > DURACAO_MAXIMA_S) {
    String err = "{\"status\":\"error\",\"msg\":\"Duracao invalida. Use entre ";
    err += DURACAO_MINIMA_S;
    err += " e ";
    err += DURACAO_MAXIMA_S;
    err += " segundos\"}";
    ws.sendTXT(clienteId, err.c_str());
    return;
  }

  duracaoSessao = (unsigned long)duracaoSegundos * 1000UL;
  tempoInicio   = millis();
  alertaEmitido = false;
  estado        = RUNNING;

  ligarInalador();
  beepInicio();

  Serial.printf("[SESSAO] Iniciada: %d segundos\n", duracaoSegundos);
  enviarStatus();
}

void pararSessao(bool porTimer = false) {
  desligarInalador();
  estado = porTimer ? FINISHED : IDLE;

  if (porTimer) {
    beepFim();
    Serial.println("[SESSAO] Finalizada por timer.");
  } else {
    beep(800, 200);
    Serial.println("[SESSAO] Parada manualmente.");
  }

  enviarStatus();
}

void onWebSocketEvent(uint8_t clienteId, WStype_t tipo, uint8_t* payload, size_t length) {
  switch (tipo) {
    case WStype_CONNECTED:
      Serial.printf("[WS] Cliente %u conectado\n", clienteId);
      enviarStatus(clienteId);
      break;

    case WStype_DISCONNECTED:
      Serial.printf("[WS] Cliente %u desconectado\n", clienteId);
      break;

    case WStype_TEXT: {
      StaticJsonDocument<128> doc;
      DeserializationError err = deserializeJson(doc, payload, length);
      if (err) {
        ws.sendTXT(clienteId, "{\"status\":\"error\",\"msg\":\"JSON invalido\"}");
        return;
      }

      const char* cmd = doc["cmd"];

      if (strcmp(cmd, "start") == 0) {
        int dur = doc["duration"] | 0;
        iniciarSessao(clienteId, dur);

      } else if (strcmp(cmd, "stop") == 0) {
        if (estado == RUNNING) pararSessao(false);
        else enviarStatus(clienteId);

      } else if (strcmp(cmd, "status") == 0) {
        enviarStatus(clienteId);
      }
      break;
    }

    default:
      break;
  }
}

// ─── SETUP ────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  // Pinos
  pinMode(PIN_RELE,  OUTPUT);
  pinMode(PIN_LED,   OUTPUT);
  pinMode(PIN_BOTAO, INPUT);   // pullup externo

  digitalWrite(PIN_RELE, LOW);
  digitalWrite(PIN_LED,  LOW);

  // Buzzer via LEDC (canal 0)
  ledcSetup(0, 2000, 8);
  ledcAttachPin(PIN_BUZZER, 0);

  // Wi-Fi
  Serial.printf("\n[WIFI] Conectando a %s", SSID);
  WiFi.begin(SSID, PASSWORD);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Conectado!");
    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());
    beep(1200, 200);
  } else {
    Serial.println("\n[WIFI] Falha na conexão. Verifique as credenciais.");
    // Opera em modo offline (controle apenas pelo botão físico)
  }

  // WebSocket
  ws.begin();
  ws.onEvent(onWebSocketEvent);
  Serial.println("[WS] Servidor WebSocket iniciado na porta 81");
}

// ─── LOOP ─────────────────────────────────────────────────────

void loop() {
  ws.loop();

  // ── Verificar timer ──────────────────────────────────────────
  if (estado == RUNNING) {
    unsigned long decorrido = millis() - tempoInicio;

    // Alerta de 10 segundos finais
    long restante = (long)(duracaoSessao / 1000) - (long)(decorrido / 1000);
    if (restante <= ALERTA_RESTANTE_S && !alertaEmitido) {
      alertaEmitido = true;
      beepAlerta();
    }

    // Fim da sessão
    if (decorrido >= duracaoSessao) {
      pararSessao(true);
    }
  }

  // ── Enviar status periódico (a cada 1s quando rodando) ───────
  if (estado == RUNNING && millis() - ultimoEnvioWS >= 1000) {
    ultimoEnvioWS = millis();
    enviarStatus();
  }

  // ── Botão físico START/STOP ───────────────────────────────────
  bool leituraBotao = digitalRead(PIN_BOTAO);
  if (leituraBotao != botaoAnterior && millis() - ultimoDebounce > 50) {
    ultimoDebounce = millis();
    if (leituraBotao == LOW) {   // botão pressionado (pullup → LOW ao apertar)
      if (estado == IDLE || estado == FINISHED) {
        // Inicia sessão padrão de 60s pelo botão físico
        iniciarSessao(255, 60);
      } else if (estado == RUNNING) {
        pararSessao(false);
      }
    }
    botaoAnterior = leituraBotao;
  }
}
