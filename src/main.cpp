#include <Arduino.h>
#include <math.h>

// --- CONFIGURACAO ---
const int PIN_CHARGE = 4;          // Pino de Saida (Fonte 3.3V)
const int PIN_ADC = 5;             // Pino de Entrada (Leitura)
const double R_KNOWN = 10000.0;    // Resistor de 10k Ohms 
const double VOLTAGE_MAX = 3300.0; // Tensao de referencia (mV)

// --- CONFIGURACOES DE PRECISAO ---
#define MAX_SAMPLES 2000 
double time_samples[MAX_SAMPLES];
double voltage_samples[MAX_SAMPLES];

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_CHARGE, OUTPUT);
  digitalWrite(PIN_CHARGE, LOW);
  
  // Configuracao do ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  
  Serial.println("--- Capacimetro ESP32 Iniciado ---");
}

void loop() {
  // 1. DESCARGA DO CAPACITOR

  digitalWrite(PIN_CHARGE, LOW);
  pinMode(PIN_CHARGE, OUTPUT);
  
  // Aguarda ate o capacitor estar vazio (< 10mV)
  unsigned long start_discharge = millis();
  bool timeout = false;
  
  while(analogReadMilliVolts(PIN_ADC) > 10) {
    delay(10);
    // Timeout de 10s para capacitores grandes
    if(millis() - start_discharge > 10000) {
      timeout = true;
      break; 
    }
  }
  
  if(timeout) {
    Serial.println("Erro: Falha ao descarregar. Verifique o circuito.");
    delay(2000);
    return;
  }
  
  delay(1000); // Estabilizacao final

  // 2. AQUISICAO DE DADOS 
  int sample_count = 0;
  unsigned long start_time = micros();
  
  digitalWrite(PIN_CHARGE, HIGH); // Inicia Carga
  
  while(sample_count < MAX_SAMPLES) {
    unsigned long now = micros();
    double v_read = analogReadMilliVolts(PIN_ADC);
    
    time_samples[sample_count] = (now - start_time) / 1000000.0; // Segundos
    voltage_samples[sample_count] = v_read;
    
    sample_count++;
    
    // Para a leitura ao atingir 95% da carga
    if (v_read >= VOLTAGE_MAX * 0.95) break;
  }
  
  digitalWrite(PIN_CHARGE, LOW); // Desliga imediatamente

  // 3. PROCESSAMENTO MATEMATICO (Regressao Linear Filtrada)
  double sum_xy = 0;
  double sum_x2 = 0;
  int valid_points = 0;
  
  // Limite de calculo: Dados apenas ate 63.2% da carga (1 Tau)
  double voltage_limit = VOLTAGE_MAX * 0.632;
  
  for (int i = 0; i < sample_count; i++) {
    double v = voltage_samples[i];
    
    // Filtros de qualidade
    if (v <= 15) continue;              // Ignora ruido inicial
    if (v >= voltage_limit) continue;   // Ignora a cauda da curva
    
    double normalized = (VOLTAGE_MAX - v) / VOLTAGE_MAX;
    if (normalized <= 0.0001) continue;
    
    double y = log(normalized);
    double x = time_samples[i];
    
    sum_xy += x * y;
    sum_x2 += x * x;
    valid_points++;
  }
  
  // 4. CALCULO DA CAPACITANCIA E RESULTADOS
  if (valid_points > 5) {
    double slope = sum_xy / sum_x2;
    double capacitance_F = -1.0 / (slope * R_KNOWN);
    double capacitance_uF = capacitance_F * 1000000.0;

    Serial.println("--------------------------------");
    Serial.print("Capacitancia Medida: ");
    Serial.print(capacitance_uF, 2); // 2 casas decimais
    Serial.println(" uF");
    Serial.print("Pontos usados no calculo: ");
    Serial.println(valid_points);
    
    // Pequena logica para identificar se esta sem capacitor
    if(capacitance_uF < 0.01) {
       Serial.println("Aviso: Valor muito baixo. Circuito aberto?");
    }
    
  } else {
    Serial.println("--------------------------------");
    Serial.println("Erro: Leitura inconclusiva ou sem capacitor.");
  }
  
  delay(3000); // Aguarda 3 segundos para o proximo teste
}