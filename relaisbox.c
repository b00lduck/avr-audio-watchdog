#include "stdinc.h"
#include "tools.h"


#define RESET_AFTER_TIME 200    // (in 100 milliseconds)
#define RESET_DURATION 20 		// (in 100 milliseconds)


// display
char relais = 0;
char power = 1;
char signal = 0;
char signal_loss = 0;
char error_flag = 0;

// status
uint16_t signal_loss_time = 0;
uint16_t reset_timer = 0;

void ADC_init(void) {
 
  uint16_t result;
 
  //  ADMUX = (0<<REFS1) | (1<<REFS0);      // AVcc als Referenz benutzen
  ADMUX = (1<<REFS1) | (1<<REFS0);      // interne Referenzspannung nutzen
  // Bit ADFR ("free running") in ADCSRA steht beim Einschalten
  // schon auf 0, also single conversion
  ADCSRA = (1<<ADPS1) | (1<<ADPS0);     // Frequenzvorteiler
  ADCSRA |= (1<<ADEN);                  // ADC aktivieren
 
  /* nach Aktivieren des ADC wird ein "Dummy-Readout" empfohlen, man liest
     also einen Wert und verwirft diesen, um den ADC "warmlaufen zu lassen" */
 
  ADCSRA |= (1<<ADSC);                  // eine ADC-Wandlung 
  while (ADCSRA & (1<<ADSC) ) {}        // auf Abschluss der Konvertierung warten
  /* ADCW muss einmal gelesen werden, sonst wird Ergebnis der nächsten
     Wandlung nicht übernommen. */
  result = ADCW;
}
 
/* ADC Einzelmessung */
uint16_t ADC_Read( uint8_t channel )
{
  // Kanal waehlen, ohne andere Bits zu beeinflußen
  ADMUX = (ADMUX & ~(0x1F)) | (channel & 0x1F);
  ADCSRA |= (1<<ADSC);            // eine Wandlung "single conversion"
  while (ADCSRA & (1<<ADSC) ) {}  // auf Abschluss der Konvertierung warten
  return ADCW;                    // ADC auslesen und zurückgeben
}
 
/* ADC Mehrfachmessung mit Mittelwertbbildung */
uint16_t ADC_Read_Avg( uint8_t channel, uint8_t average )
{
  uint32_t result = 0;
 
  for (uint8_t i = 0; i < average; ++i )
    result += ADC_Read( channel );
 
  return (uint16_t)( result / average );
}
 


int main() {

	// LEDs	
	DDRC = 0b00111111;
	PORTC = 0b00001000;

	// RESET BUTTON
	DDRD = 0b00000100;
	PORTD = 0b00000100;
	EIMSK |= (1<<INT0);


	// TIMER1 (Naechste messung)		
	TCCR1B |= (1<<WGM12) | (1<<CS10) | (1<<CS12);
	TCNT1 = 0;
	OCR1A = 2000; // 100ms
	TIMSK1 |= (1<<OCIE1A);



	ADC_init();

	sei();

	while(1) {
  
		char data = 0;
		
		if (relais) data += 1;
		if (power) data += 2;

		if (signal) data += 8;
		if (signal_loss) data += 16;
		if (error_flag) data += 32;

		PORTC = data;

	}


}

ISR (INT0_vect) {

	error_flag = 0;

}

ISR (TIMER1_COMPA_vect) {
	

	uint16_t adcval;

	adcval = ADC_Read_Avg(0, 4);  // Kanal 0, Mittelwert aus 4 Messungen

	if (adcval < 500) {
		signal_loss = 1;
		signal = 0;
		signal_loss_time += 1;
	} else {
		signal_loss = 0;
		signal = 1;
		signal_loss_time = 0;
	}

	if (signal_loss_time > (RESET_AFTER_TIME)) {
		error_flag = 1;
		reset_timer = RESET_DURATION;
		signal_loss_time = 0;
	}

	if (reset_timer > 0) {
		relais = 1;
		reset_timer--;
	} else {
		relais = 0;
	}

}
