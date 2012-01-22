#include "stdinc.h"
#include "tools.h"


#define RESET_AFTER_TIME 200    // (in 100 milliseconds)
#define RESET_DURATION 20 		// (in 100 milliseconds)


#define NUMLEDS 6

#define NUMCHANNELS 2
#define MUXADDRESSMASK 0b00000011

struct {

//	char enabled;
//	char relais;
	char signal;
	char signal_loss;
//	char resetted;

} channelstate[8];



char active_channel = 0;


// display
char display[NUMLEDS];



/**
char relais = 0;
char power = 1;
char signal = 0;
char signal_loss = 0;
*/

char error_flag = 0;


// status
uint16_t signal_loss_time = 0;
uint16_t reset_timer = 0;

void ADC_init(void) {
 
  uint16_t result;
 
  ADMUX = (1<<REFS0) | (1<<REFS1);      // VC2,5V
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
uint16_t ADC_Read( uint8_t channel ) {
  // Kanal waehlen, ohne andere Bits zu beeinflußen
  ADMUX = (ADMUX & ~(0x1F)) | (channel & 0x1F);
  ADCSRA |= (1<<ADSC);            // eine Wandlung "single conversion"
  while (ADCSRA & (1<<ADSC) ) {}  // auf Abschluss der Konvertierung warten
  return ADCW;                    // ADC auslesen und zurückgeben
}
 
/* ADC Mehrfachmessung mit Mittelwertbbildung */
uint16_t ADC_Read_Avg( uint8_t channel, uint8_t average ) {
  uint32_t result = 0;
 
  for (uint8_t i = 0; i < average; ++i ) {
    result += ADC_Read( channel );
  }
 
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

	// ADRESS OUTPUT FOR MULTIPLEXER
	DDRA = 0b00000011;
	PORTA = 0b00000000;


	// TIMER1 (Naechste messung)		
	TCCR1B |= (1<<WGM12) | (1<<CS10) | (1<<CS12);
	TCNT1 = 0;
	OCR1A = 2000; // 100ms
	//OCR1A = 1000; // 50ms
	//OCR1A = 10000; // 500ms
	TIMSK1 |= (1<<OCIE1A);


	// reset LEDs
	char i;
	for (i=0;i<NUMLEDS;i++) {
		display[i] = 0;
	}

	ADC_init();

	sei();

	while(1) {
  
		char data = 0;

		char i;
		char mask = 1;
		
		for (i=0;i<NUMLEDS;i++) {
			data += display[i] * mask;
			mask <<= 1;
		}
		
		PORTC = data;

	}


}

ISR (INT0_vect) {

	error_flag = 0;

}

ISR (TIMER1_COMPA_vect) {

	display[0] = active_channel & 1;
	display[1] = (active_channel & 2) >> 1;
	

	uint16_t adcval;

	adcval = ADC_Read_Avg(7, 1);  // Kanal 7, Mittelwert aus 4 Messungen

	if (adcval < 20) {
		channelstate[active_channel].signal_loss = 1;
		if (active_channel == 0) {
			display[2] = 1;
		}
		if (active_channel == 1) {
			display[4] = 1;
		}

	} else {
		channelstate[active_channel].signal_loss = 0;

		if (active_channel == 0) {
			display[2] = 0;
		}
		if (active_channel == 1) {
			display[4] = 0;
		}

	}


	active_channel++;

	if (active_channel >= NUMCHANNELS) {
		active_channel = 0;
	}

	PORTA = active_channel & MUXADDRESSMASK;



/*
	uint16_t adcval;

	adcval = ADC_Read_Avg(7, 4);  // Kanal 7, Mittelwert aus 4 Messungen

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

*/

}
