#include "stdinc.h"
#include "tools.h"

#define RESET_AFTER_TIME 		40  // (in 100 milliseconds * NUMCHANNELS)
#define RESET_DURATION 			5 	// (in 100 milliseconds * NUMCHANNELS)
#define REBOOT_DURATION 		40 	// (in 100 milliseconds * NUMCHANNELS)
#define REBOOT_GIVEUP_THRESH 	3	// give up after x reboots

#define NUMLEDSPERCHANNEL 4
#define NUMCHANNELS 8
#define NUMLEDS (NUMCHANNELS*NUMLEDSPERCHANNEL)

#define MUXADDRESSMASK 0b00000111

struct {
	
	unsigned char enabled;

	unsigned char relais;
	unsigned char signal;
	unsigned int signal_loss_counter;
	int is_resetting;
	int is_rebooting;
	unsigned int reboot_counter;

} channelstate[NUMCHANNELS];

char blink = 0;
char active_channel = 0;
char error_flag = 0;

// Display memory
char display[NUMLEDS];

// dipswitch memory representation
char enabled[NUMCHANNELS];


/**
 * display driver
 * sends commands to the 595 cascade
 * first 4 bits of each 595 are low active, high 4 bits high active
 * because of the maximum current for vcc and gnd lines of the 595
 */
void updateDisplay() {

	unsigned char i;
	for (i=0;i<NUMLEDS;i++) {

		char bit = display[(NUMLEDS-1)-i];

		// invert first 4 bits of each 595
		if ((i%8)>3) {
			if (bit) {
				bit = 0;
			} else {
			    bit = 1;
			}
		}

		// set bit
		if (bit == 1) {
			SBI(PORTB,0);
		} else {
			CBI(PORTB,0);
		}

		// NEXT BIT
		SBI(PORTB,1);
		CBI(PORTB,1);

	}

	// LATCH IT
	SBI(PORTB,2);
	CBI(PORTB,2);
	
}

/**
 * reads dipswitch and stores its value into enabled[] array
 */
void updateRelais() {

	unsigned char i;
	for (i=0;i<NUMCHANNELS;i++) {

		if (channelstate[i].relais == 1) {
			SBI(PORTC,i);
		} else {
			CBI(PORTC,i);
		}

	}

}

/**
 * reads dipswitch and stores its value into enabled[] array
 */
void updateDipswitch() {

	unsigned char i;
	for (i=0;i<NUMCHANNELS;i++) {

		enabled[(NUMCHANNELS-1)-i] = !PIN(PIND,i);	

	}

}


/**
 * writes to the display memory
 */
void setDisplay() {

	unsigned char i;
	for (i=0;i<NUMCHANNELS;i++) {

		char red = 0;
		char yellow = 0;
		char green1 = 0;
		char green2 = enabled[i];

 		if(enabled[i]) {

			// RED (ON=HAD RESET, BLINK=CHANNEL HAS GIVEN UP)
			if (channelstate[i].reboot_counter >= REBOOT_GIVEUP_THRESH) {
				red = blink;         		
			}

			// YELLOW (BLINK=RELAIS ENGAGED, ON=REBOOT IN PROGRESS)
			if (channelstate[i].relais > 0) {				
				yellow = blink;
			} else if (channelstate[i].is_rebooting > 0) {
				yellow = 1;
			}         		
		
			// GREEN 1 (SIGNAL / MEASURING)
			if (i == active_channel) {
				if ((channelstate[i].reboot_counter < REBOOT_GIVEUP_THRESH) && (channelstate[i].is_rebooting == 0)) {
					green1 = 1;          	
				}
			} else {
				green1 = channelstate[i].signal;	
			}

		}


		// RED
		display[i*NUMLEDSPERCHANNEL+0] = red;

		// YELLOW
		display[i*NUMLEDSPERCHANNEL+1] = yellow;

		// GREEN 1
		display[i*NUMLEDSPERCHANNEL+2] = green1;
		
		// GREEN 2 (ENABLED)
		display[i*NUMLEDSPERCHANNEL+3] = green2;

	}

}


void reset() {
	memset(display,0,NUMLEDS);
	memset(enabled,0,NUMCHANNELS);
	memset(channelstate,0,NUMCHANNELS * sizeof(channelstate));
}

void checkReset() {
	if (((PINA & 0b00010000) >> 4) == 1) {
		// no reset
	} else {
		reset();
	}
}


int main() {

	// ADRESS OUTPUT FOR MULTIPLEXER: Bits 0,1,2
	// ANALOG INPUT: Bit 3
	// RESET SWITCH: Bit 4
	DDRA =  0b00000111;
	PORTA = 0b00010000;

	// 595s
	// 0: SER
	// 1: /SCK
	// 2: /RCK
	DDRB = 0b00001111;
	PORTB = 0b00000000;

	// RELAIS
	DDRC = 0b11111111;
	PORTC = 0b00000000;

	// DIPSWITCH
	DDRD = 0b00000000;
	PORTD = 0b11111111;

	// TIMER1 (Naechste messung)		
	TCCR1B |= (1<<WGM12) | (1<<CS10) | (1<<CS12);
	TCNT1 = 0;
	OCR1A = 2000; // 100ms
	//OCR1A = 1000; // 50ms
	//OCR1A = 10000; // 500ms
	TIMSK1 |= (1<<OCIE1A);

	reset();

	ADC_init();

	sei();

	while(1) {

  		updateDipswitch();
	
		updateRelais();

		setDisplay();
		
  		updateDisplay();

		checkReset();

	}


}

/*
ISR (INT0_vect) {

	error_flag = 0;

}
*/

ISR (TIMER1_COMPA_vect) {

	blink ^= 1;

	uint16_t adcval;

	adcval = ADC_Read_Avg(3, 1);  // Kanal 7, Mittelwert aus 1 Messungen

	if (enabled[active_channel]) {
		
		// CHANNEL IS ENABLED

		if (channelstate[active_channel].reboot_counter >= REBOOT_GIVEUP_THRESH) {

			// TOO MANY REBOOTS, GIVE UP

			channelstate[active_channel].relais = 0;
			channelstate[active_channel].is_rebooting = 0;
			channelstate[active_channel].is_resetting = 0;
			channelstate[active_channel].signal_loss_counter = 0;
			channelstate[active_channel].signal = 0;	

		} else {

			if ((channelstate[active_channel].is_resetting <= 0) && (channelstate[active_channel].is_rebooting <= 0)) {

				// SWITCH RELAIS OFF if channel is not resetting or rebooting
				channelstate[active_channel].relais = 0;
			
				// detect signal
				if (adcval < 20) {
					// SIGNAL LOSS
					channelstate[active_channel].signal = 0;
					channelstate[active_channel].signal_loss_counter++;
				} else {
					// SIGNAL DETECTED
					channelstate[active_channel].signal = 1;
					channelstate[active_channel].signal_loss_counter = 0;
				}

				// if signal is lost for amount of RESET_AFTER_TIME trigger reboot
				if (channelstate[active_channel].signal_loss_counter > RESET_AFTER_TIME) {
					channelstate[active_channel].is_resetting = RESET_DURATION;
					channelstate[active_channel].is_rebooting = REBOOT_DURATION;
					channelstate[active_channel].signal_loss_counter = 0;
					channelstate[active_channel].reboot_counter++;				
				}

			}
	
			if (channelstate[active_channel].is_resetting > 0) {
				channelstate[active_channel].relais = 1;				
				channelstate[active_channel].is_resetting--;
			} else if (channelstate[active_channel].is_rebooting > 0) {
				channelstate[active_channel].relais = 0;				
				channelstate[active_channel].is_rebooting--;		
			}
		}

	} else {

		// CHANNEL IS DISABLED

		channelstate[active_channel].relais = 0;
		channelstate[active_channel].is_rebooting = 0;
		channelstate[active_channel].is_resetting = 0;
		channelstate[active_channel].signal_loss_counter = 0;
		channelstate[active_channel].signal = 0;
		channelstate[active_channel].reboot_counter = 0;

	}
		

	active_channel++;

	if (active_channel >= NUMCHANNELS) {
		active_channel = 0;
	}

	// keep pullup active for reset switch
	PORTA = 0b00010000 | (MUXADDRESSMASK & active_channel);

}
