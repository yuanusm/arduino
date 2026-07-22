#include <Arduino.h>
#include <math.h>

// Detector de metales por desfase TX/RX con lock-in digital.
// MCU objetivo: ATmega328P a 16 MHz.
//
// NOTA práctica: 8 kHz con 128 muestras/ciclo requiere actualizar SPWM a
// 1,024 MHz y muestrear dos canales ADC a ese ritmo de bloque, lo cual deja
// muy poco margen en un ATmega328P. Las constantes quedan centralizadas para
// ajustar la frecuencia de trabajo durante la puesta en marcha.

constexpr uint8_t NUM_MUESTRAS = 128;
constexpr uint32_t MCU_CLOCK_HZ = 16000000UL;
constexpr uint16_t F_TX_HZ = 8000;
constexpr uint32_t F_UPDATE_HZ = static_cast<uint32_t>(F_TX_HZ) * NUM_MUESTRAS;
constexpr uint8_t ADC_CANAL_TX = 0;  // ADC0: shunt corriente TX
constexpr uint8_t ADC_CANAL_RX = 1;  // ADC1: bobina RX acondicionada
constexpr uint8_t PIN_PWM_TX = 3;    // OC2B: salida SPWM hacia filtro RC/PAM8610

// Tabla seno centrada para SPWM: 0..255, media 128.
const uint8_t tablaSenoPWM[NUM_MUESTRAS] PROGMEM = {
  128, 134, 140, 146, 152, 158, 165, 170,
  176, 182, 188, 193, 198, 203, 208, 213,
  218, 222, 226, 230, 234, 237, 240, 243,
  245, 248, 250, 251, 253, 254, 254, 255,
  255, 255, 254, 254, 253, 251, 250, 248,
  245, 243, 240, 237, 234, 230, 226, 222,
  218, 213, 208, 203, 198, 193, 188, 182,
  176, 170, 165, 158, 152, 146, 140, 134,
  128, 121, 115, 109, 103, 97, 90, 85,
  79, 73, 67, 62, 57, 52, 47, 42,
  37, 33, 29, 25, 21, 18, 15, 12,
  10, 7, 5, 4, 2, 1, 1, 0,
  0, 0, 1, 1, 2, 4, 5, 7,
  10, 12, 15, 18, 21, 25, 29, 33,
  37, 42, 47, 52, 57, 62, 67, 73,
  79, 85, 90, 97, 103, 109, 115, 121
};

// Referencias digitales seno/coseno sincronizadas en int8_t para lock-in síncrono.
const int8_t refSin[NUM_MUESTRAS] PROGMEM = {
  0, 6, 12, 19, 25, 31, 37, 43,
  49, 55, 60, 66, 71, 76, 81, 86,
  90, 95, 99, 103, 106, 110, 113, 115,
  118, 120, 122, 124, 125, 126, 127, 127,
  127, 127, 127, 126, 125, 124, 122, 120,
  118, 115, 113, 110, 106, 103, 99, 95,
  90, 86, 81, 76, 71, 66, 60, 55,
  49, 43, 37, 31, 25, 19, 12, 6,
  0, -6, -12, -19, -25, -31, -37, -43,
  -49, -55, -60, -66, -71, -76, -81, -86,
  -90, -95, -99, -103, -106, -110, -113, -115,
  -118, -120, -122, -124, -125, -126, -127, -127,
  -127, -127, -127, -126, -125, -124, -122, -120,
  -118, -115, -113, -110, -106, -103, -99, -95,
  -90, -86, -81, -76, -71, -66, -60, -55,
  -49, -43, -37, -31, -25, -19, -12, -6
};

const int8_t refCos[NUM_MUESTRAS] PROGMEM = {
  127, 127, 127, 126, 125, 124, 122, 120,
  118, 115, 113, 110, 106, 103, 99, 95,
  90, 86, 81, 76, 71, 66, 60, 55,
  49, 43, 37, 31, 25, 19, 12, 6,
  0, -6, -12, -19, -25, -31, -37, -43,
  -49, -55, -60, -66, -71, -76, -81, -86,
  -90, -95, -99, -103, -106, -110, -113, -115,
  -118, -120, -122, -124, -125, -126, -127, -127,
  -127, -127, -127, -126, -125, -124, -122, -120,
  -118, -115, -113, -110, -106, -103, -99, -95,
  -90, -86, -81, -76, -71, -66, -60, -55,
  -49, -43, -37, -31, -25, -19, -12, -6,
  0, 6, 12, 19, 25, 31, 37, 43,
  49, 55, 60, 66, 71, 76, 81, 86,
  90, 95, 99, 103, 106, 110, 113, 115,
  118, 120, 122, 124, 125, 126, 127, 127
};

volatile uint8_t indicePWM = 0;
volatile uint8_t canalADC = ADC_CANAL_TX;
volatile uint8_t indiceADC = 0;
volatile uint8_t canalListo = ADC_CANAL_TX;
volatile bool bloqueCompleto = false;
volatile bool descartarConversion = false;
volatile uint16_t bufferADC[2][NUM_MUESTRAS];

uint16_t bufferDSP[2][NUM_MUESTRAS];

struct ResultadoDSP {
  int32_t i;
  int32_t q;
  float amplitud;
  float faseRad;
};

ISR(TIMER1_COMPA_vect) {
  OCR2B = pgm_read_byte(&tablaSenoPWM[indicePWM]);
  indicePWM++;
  if (indicePWM >= NUM_MUESTRAS) {
    indicePWM = 0;
  }
}

ISR(ADC_vect) {
  const uint16_t muestra = ADC;

  if (descartarConversion) {
    descartarConversion = false;
    return;
  }

  bufferADC[canalADC][indiceADC] = muestra;
  indiceADC++;

  if (indiceADC >= NUM_MUESTRAS) {
    indiceADC = 0;
    canalListo = canalADC;
    canalADC = (canalADC == ADC_CANAL_TX) ? ADC_CANAL_RX : ADC_CANAL_TX;
    ADMUX = (ADMUX & 0xF0) | canalADC;
    descartarConversion = true;
    bloqueCompleto = true;
  }
}

void configurarSPWM() {
  pinMode(PIN_PWM_TX, OUTPUT);

  // Timer2: Fast PWM 8-bit no inversor en OC2B, sin prescaler.
  // Portadora PWM aproximada: 16 MHz / 256 = 62,5 kHz.
  TCCR2A = _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS20);
  OCR2B = 128;

  // Timer1: CTC para avanzar la tabla seno.
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS10);
  const uint32_t ciclosActualizacion = MCU_CLOCK_HZ / F_UPDATE_HZ;
  OCR1A = (ciclosActualizacion > 1) ? (ciclosActualizacion - 1) : 1;
  TIMSK1 = _BV(OCIE1A);
}

void configurarADC() {
  ADMUX = _BV(REFS0) | ADC_CANAL_TX;  // AVcc como referencia, ADC0 inicial.
  ADCSRA = _BV(ADEN) | _BV(ADATE) | _BV(ADIE) | _BV(ADSC) | _BV(ADPS2);
  ADCSRB = 0;  // free running.
  DIDR0 = _BV(ADC0D) | _BV(ADC1D);
}

ResultadoDSP procesarLockIn(const uint16_t *datos) {
  uint32_t suma = 0;
  for (uint8_t n = 0; n < NUM_MUESTRAS; n++) {
    suma += datos[n];
  }

  const int16_t promedio = suma / NUM_MUESTRAS;
  int32_t iAcum = 0;
  int32_t qAcum = 0;

  for (uint8_t n = 0; n < NUM_MUESTRAS; n++) {
    const int16_t ac = static_cast<int16_t>(datos[n]) - promedio;
    iAcum += static_cast<int32_t>(ac) * static_cast<int8_t>(pgm_read_byte(&refCos[n]));
    qAcum += static_cast<int32_t>(ac) * static_cast<int8_t>(pgm_read_byte(&refSin[n]));
  }

  ResultadoDSP r;
  r.i = iAcum;
  r.q = qAcum;
  r.amplitud = sqrt(static_cast<float>(iAcum) * iAcum + static_cast<float>(qAcum) * qAcum);
  r.faseRad = atan2(static_cast<float>(qAcum), static_cast<float>(iAcum));
  return r;
}

float normalizarFase(float fase) {
  while (fase > PI) {
    fase -= TWO_PI;
  }
  while (fase < -PI) {
    fase += TWO_PI;
  }
  return fase;
}

void setup() {
  Serial.begin(115200);
  configurarSPWM();
  configurarADC();
  sei();
}

void loop() {
  if (!bloqueCompleto) {
    return;
  }

  noInterrupts();
  const uint8_t canalCopiar = canalListo;
  for (uint8_t n = 0; n < NUM_MUESTRAS; n++) {
    bufferDSP[canalCopiar][n] = bufferADC[canalCopiar][n];
  }
  bloqueCompleto = false;
  interrupts();

  static bool txValido = false;
  static bool rxValido = false;
  static ResultadoDSP tx = {};
  static ResultadoDSP rx = {};

  if (canalCopiar == ADC_CANAL_TX) {
    tx = procesarLockIn(bufferDSP[ADC_CANAL_TX]);
    txValido = true;
  } else {
    rx = procesarLockIn(bufferDSP[ADC_CANAL_RX]);
    rxValido = true;
  }

  if (txValido && rxValido) {
    const float deltaPhi = normalizarFase(rx.faseRad - tx.faseRad);

    Serial.print(F("A_TX="));
    Serial.print(tx.amplitud, 1);
    Serial.print(F(" A_RX="));
    Serial.print(rx.amplitud, 1);
    Serial.print(F(" Phi_TX="));
    Serial.print(tx.faseRad, 5);
    Serial.print(F(" Phi_RX="));
    Serial.print(rx.faseRad, 5);
    Serial.print(F(" DeltaPhi="));
    Serial.println(deltaPhi, 5);
  }
}
