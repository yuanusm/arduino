volatile uint8_t contador = 0;
volatile uint8_t canal = 0;
volatile uint16_t dato;
volatile uint8_t descarte = 0;
volatile uint8_t nuevoDato = 0;
volatile uint16_t buffer_Shunt_Rx[2][128];
volatile uint8_t canalListo;
uint16_t bufferDSP[2][128];

volatile uint8_t indice = 0;
volatile uint8_t bloqueCompleto = 0;

ISR(ADC_vect){
  if(descarte){
    descarte = 0;
    return;
  }
  buffer_Shunt_Rx[canal][contador] = ADC;

  contador++;

  if(contador==128){

    contador=0;
    canalListo = canal;
    canal ^=1;

    ADMUX = (ADMUX & 0xF0) | canal;
    descarte = 1;
    bloqueCompleto = 1;
  }

}


void setup() {
  Serial.begin(9600);


  ADMUX = (1<<REFS0);      // AVcc como referencia REFS1 REFS0 ADLAR - MUX3 MUX2 MUX1 MUX0
  ADMUX = (ADMUX & 0xF0) | 0;   // ADC0

  ADCSRA =
  (1<<ADEN) |      // habilita ADC
  (1<<ADATE) |     // auto trigger
  (1<<ADIE) |      // interrupción
  (1<<ADSC) |      // comenzar
  (1<<ADPS1) |
  (1<<ADPS0);      // prescaler=8

  ADCSRB = 0;


  sei();

}


void DSP_datos(uint16_t *datos, bool canal){
  if(canal){
    uint32_t suma, promedio;
    suma = 0;
    for(uint8_t i = 0;i<128;i++){
      //suma += datos[i];
      Serial.println(datos[i]);
    }
    promedio = suma >> 7;
    //Serial.println(promedio);
  }
}

void loop() {
  

  if (bloqueCompleto) {

    noInterrupts();

    memcpy(bufferDSP[canalListo],
       buffer_Shunt_Rx[canalListo],
       sizeof(bufferDSP[0]));

    bloqueCompleto = 0;
    interrupts();

    DSP_datos(bufferDSP[canalListo], canalListo);
  }

}
