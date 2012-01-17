#include "tools.h"

inline void delayloop16 (uint16_t count) {

	asm volatile ("cp  %A0, __zero_reg__ \n\t"
                  "cpc %B0, __zero_reg__ \n\t"
                  "breq 2f               \n\t"
                  "1:                    \n\t"
                  "sbiw %0,1             \n\t"
                  "brne 1b               \n\t"
                  "2:" : "=w" (count) : "0"  (count)
        );                            
}


inline void delay_long (uint8_t count) {

	while(count--) delayloop16(60000);

}
