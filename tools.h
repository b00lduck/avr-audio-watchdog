#include <avr/io.h>

void delayloop16 (uint16_t count);
void delay_long (uint8_t count);

#define SBI(port,bit) port |= (1<<bit)
#define CBI(port,bit) port &= ~(1<<bit)



