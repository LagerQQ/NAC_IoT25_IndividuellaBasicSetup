#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
    // Pin 13 på Arduino Uno = PB5 på ATmega328P
    DDRB |= (1 << DDB5);   // Sätt PB5 som utgång

    while (1)
    {
        PORTB |= (1 << PORTB5);   // Tänd LED
        _delay_ms(500);

        PORTB &= ~(1 << PORTB5);  // Släck LED
        _delay_ms(500);
    }

    return 0;
}
