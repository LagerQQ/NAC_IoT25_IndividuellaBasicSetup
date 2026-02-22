#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <util/atomic.h>

#define LED_MASK_B ((1 << PB2) | (1 << PB3) | (1 << PB4) | (1 << PB5))

volatile uint32_t g_ms = 0;

uint32_t millis(void) 
{
    uint32_t ms;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) 
    {
        ms = g_ms;
    }
    return ms;
}

void timer0_init_ms(void) 
{
    TCNT0 = 0;
    TCCR0A |= (1 << WGM01);
    OCR0A = 249;
    TCCR0B |= (1 << CS01) | (1 << CS00);
    TIMSK0 |= (1 << OCIE0A);
    sei();
}

ISR(TIMER0_COMPA_vect) 
{
    g_ms++;
}

int main(void)
{
    // Pin 13 på Arduino Uno = PB5 på ATmega328P
    DDRB |= LED_MASK_B;   // Sätt PB2-PB5 som utgång

    timer0_init_ms();

    uint32_t lastToggle = 0;
    uint16_t interval = 250;
    uint8_t ledOn = 0;

    while (1)
    {
        uint32_t now = millis();

        if (now - lastToggle >= interval) {
            lastToggle += interval;
            ledOn = !ledOn;
            if(ledOn == 1) {
                PORTB |= LED_MASK_B;
            }
            else {
                PORTB &= ~LED_MASK_B;
            }
        }
    }

    return 0;
}
