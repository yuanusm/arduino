#include <Arduino.h>
#include <math.h>
#include <string.h>

// Detector de metales por desfase TX/RX con lock-in digital.
// MCU objetivo: ATmega328P a 16 MHz.
//
// La versión de depuración prioriza que el puerto serie responda y que la RAM
// quede con margen suficiente. Para 8 kHz/128 muestras sería necesaria una ISR
// de 1,024 MHz, demasiado agresiva para depurar comandos en un ATmega328P.

constexpr uint8_t NUM_MUESTRAS = 128;
constexpr uint32_t MCU_CLOCK_HZ = 16000000UL;
constexpr uint16_t F_TX_HZ = 1000;
constexpr uint32_t F_UPDATE_HZ = static_cast<uint32_t>(F_TX_HZ) * NUM_MUESTRAS;
constexpr uint8_t ADC_CANAL_TX = 0;  // ADC0: shunt corriente TX
constexpr uint8_t ADC_CANAL_RX = 1;  // ADC1: bobina RX acondicionada
constexpr uint8_t PIN_PWM_TX = 9;    // OC1A: salida SPWM hacia filtro RC/PAM8610

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

uint16_t bloqueTrabajo[NUM_MUESTRAS];
uint16_t ultimoBloque[2][NUM_MUESTRAS];


struct ResultadoDSP {
  int32_t i;
  int32_t q;
  float amplitud;
  float faseRad;
};

struct EstadoDSP {
  ResultadoDSP tx;
  ResultadoDSP rx;
  float deltaRad;
  float deltaCalibradoRad;
  bool txValido;
  bool rxValido;
  bool calibrado;
};

EstadoDSP estadoDSP = {};
bool txActivo = true;
bool dspActivo = true;
float faseReferenciaRad = 0.0f;
char comandoSerial[16];
uint8_t comandoLen = 0;


ISR(TIMER2_COMPA_vect) {
  OCR1A = pgm_read_byte(&tablaSenoPWM[indicePWM]);
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

void activarTX(bool activar) {
  txActivo = activar;
  if (activar) {
    indicePWM = 0;
    OCR1A = 128;
    TIMSK2 |= _BV(OCIE2A);
  } else {
    TIMSK2 &= ~_BV(OCIE2A);
    OCR1A = 128;
  }
}

void configurarSPWM() {
  pinMode(PIN_PWM_TX, OUTPUT);

  // Timer1 (16 bits): Fast PWM 8-bit no inversor en OC1A, sin prescaler.
  // La portadora PWM queda en 16 MHz / 256 = 62,5 kHz.
  TCCR1A = _BV(COM1A1) | _BV(WGM10);
  TCCR1B = _BV(WGM12) | _BV(CS10);
  OCR1A = 128;

  // Timer2 solo marca la cadencia de actualización de la tabla seno.
  TCCR2A = _BV(WGM21);
  TCCR2B = _BV(CS20);
  const uint32_t ciclosActualizacion = MCU_CLOCK_HZ / F_UPDATE_HZ;
  OCR2A = (ciclosActualizacion > 1 && ciclosActualizacion <= 256)
    ? (ciclosActualizacion - 1)
    : 124;
  TIMSK2 = _BV(OCIE2A);
  activarTX(true);
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

float radAGrados(float rad) {
  return rad * 180.0f / PI;
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

void enviarBloqueCanal(uint8_t canal, const __FlashStringHelper *etiqueta) {
  for (uint8_t n = 0; n < NUM_MUESTRAS; n++) {
    Serial.print(etiqueta);
    Serial.print(',');
    Serial.print(n);
    Serial.print(',');
    Serial.println(ultimoBloque[canal][n]);
  }
}

void enviarBloquesTXRX() {
  for (uint8_t n = 0; n < NUM_MUESTRAS; n++) {
    Serial.print(F("DATA,"));
    Serial.print(n);
    Serial.print(',');
    Serial.print(ultimoBloque[ADC_CANAL_TX][n]);
    Serial.print(',');
    Serial.println(ultimoBloque[ADC_CANAL_RX][n]);
  }
}

void enviarResultadoCanal(const __FlashStringHelper *etiqueta, const ResultadoDSP &r) {
  Serial.print(F("RESULT,"));
  Serial.print(etiqueta);
  Serial.print(',');
  Serial.print(r.i);
  Serial.print(',');
  Serial.print(r.q);
  Serial.print(',');
  Serial.print(r.amplitud, 1);
  Serial.print(',');
  Serial.println(radAGrados(r.faseRad), 3);
}

void enviarResultadosDSP() {
  if (!estadoDSP.txValido || !estadoDSP.rxValido) {
    Serial.println(F("RESULT,WAITING_FOR_BLOCKS"));
    return;
  }

  enviarResultadoCanal(F("TX"), estadoDSP.tx);
  enviarResultadoCanal(F("RX"), estadoDSP.rx);
  Serial.print(F("DELTA_PHASE,"));
  Serial.print(radAGrados(estadoDSP.deltaRad), 3);
  Serial.print(F(",CAL,"));
  Serial.println(radAGrados(estadoDSP.deltaCalibradoRad), 3);
}

void calibrarFaseBase() {
  if (!estadoDSP.txValido || !estadoDSP.rxValido) {
    Serial.println(F("BASE_PHASE,WAITING_FOR_DSP"));
    return;
  }

  faseReferenciaRad = estadoDSP.deltaRad;
  estadoDSP.calibrado = true;
  estadoDSP.deltaCalibradoRad = 0.0f;
  Serial.print(F("BASE_PHASE,"));
  Serial.println(radAGrados(faseReferenciaRad), 3);
}

void enviarEstado() {
  Serial.print(F("STATUS,TX,"));
  Serial.print(txActivo ? F("ON") : F("OFF"));
  Serial.print(F(",DSP,"));
  Serial.print(dspActivo ? F("ON") : F("OFF"));
  Serial.print(F(",CAL,"));
  Serial.print(estadoDSP.calibrado ? F("YES") : F("NO"));
  Serial.print(F(",F_TX,"));
  Serial.print(F_TX_HZ);
  Serial.print(F(",SAMPLES,"));
  Serial.println(NUM_MUESTRAS);
}

bool comandoEs(const char *esperado) {
  return strcmp(comandoSerial, esperado) == 0;
}

void procesarComando() {
  if (comandoEs("TX_ON")) {
    activarTX(true);
    Serial.println(F("OK,TX_ON"));
  } else if (comandoEs("TX_OFF")) {
    activarTX(false);
    Serial.println(F("OK,TX_OFF"));
  } else if (comandoEs("READ_TX") || comandoEs("DEBUG_TX")) {
    enviarBloqueCanal(ADC_CANAL_TX, F("TX"));
  } else if (comandoEs("READ_RX") || comandoEs("DEBUG_RX")) {
    enviarBloqueCanal(ADC_CANAL_RX, F("RX"));
  } else if (comandoEs("READ_ALL")) {
    enviarBloquesTXRX();
  } else if (comandoEs("DSP_ON")) {
    dspActivo = true;
    Serial.println(F("OK,DSP_ON"));
  } else if (comandoEs("DSP_OFF")) {
    dspActivo = false;
    Serial.println(F("OK,DSP_OFF"));
  } else if (comandoEs("RESULT") || comandoEs("READ_DSP")) {
    enviarResultadosDSP();
  } else if (comandoEs("CAL")) {
    calibrarFaseBase();
  } else if (comandoEs("STATUS")) {
    enviarEstado();
  } else if (comandoLen > 0) {
    Serial.print(F("ERR,UNKNOWN_CMD,"));
    Serial.println(comandoSerial);
  }
}

void finalizarComandoSerial() {
  comandoSerial[comandoLen] = '\0';
  procesarComando();
  comandoLen = 0;
  comandoSerial[0] = '\0';
}

void leerComandosSerial() {
  while (Serial.available() > 0) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      finalizarComandoSerial();
    } else if (c >= 'a' && c <= 'z') {
      if (comandoLen < sizeof(comandoSerial) - 1) {
        comandoSerial[comandoLen++] = c - ('a' - 'A');
      }
    } else if (c != ' ' && comandoLen < sizeof(comandoSerial) - 1) {
      comandoSerial[comandoLen++] = c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  configurarSPWM();
  configurarADC();
  sei();
  Serial.println(F("READY,metal_detector_phase_debug,115200"));
  enviarEstado();
}

void loop() {
  leerComandosSerial();

  if (!bloqueCompleto) {
    return;
  }

  noInterrupts();
  const uint8_t canalCopiar = canalListo;
  for (uint8_t n = 0; n < NUM_MUESTRAS; n++) {
    bloqueTrabajo[n] = bufferADC[canalCopiar][n];
    ultimoBloque[canalCopiar][n] = bloqueTrabajo[n];
  }
  bloqueCompleto = false;
  interrupts();

  if (dspActivo) {
    if (canalCopiar == ADC_CANAL_TX) {
      estadoDSP.tx = procesarLockIn(bloqueTrabajo);
      estadoDSP.txValido = true;
    } else {
      estadoDSP.rx = procesarLockIn(bloqueTrabajo);
      estadoDSP.rxValido = true;
    }

    if (estadoDSP.txValido && estadoDSP.rxValido) {
      estadoDSP.deltaRad = normalizarFase(estadoDSP.rx.faseRad - estadoDSP.tx.faseRad);
      estadoDSP.deltaCalibradoRad = estadoDSP.calibrado
        ? normalizarFase(estadoDSP.deltaRad - faseReferenciaRad)
        : estadoDSP.deltaRad;
    }
  }

  leerComandosSerial();
}
