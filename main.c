#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <util/atomic.h>

#define LED_MASK_B ((1 << PB2) | (1 << PB3) | (1 << PB4) | (1 << PB5))
#define RGB_R_MASK_B ((1 << PB0))
#define ENCODER_MASK_D ((1 << PD2) | (1 << PD3))
#define ENCODER_SW_MASK ((1 << PD4))
#define RGB_GB_MASK_D ((1 << PD6) | (1 << PD7))
#define ENCODER_BUTTON_1 ((1 << PB1))
#define ENCODER_BUTTON_2 ((1 << PD5))
#define DEBOUNCE_TIME_MS 20

typedef enum
{
    OFF = 0,
    RED = 1,
    GREEN = 2,
    BLUE = 3,
    WHITE = 4
}RGB;

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

void adc_init_a0(void) 
{
    //ADMUX = AVcc + channel 0
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

}

uint16_t adc_read_blocking(void) 
{
    ADCSRA |= (1 << ADSC);
    while ((ADCSRA & (1 << ADIF)) == 0) {}
    uint16_t value = ADC;
    ADCSRA |= (1 << ADIF);

    return value;
}

void set_rgb(RGB color) 
{
    PORTB &= ~RGB_R_MASK_B;
    PORTD &= ~RGB_GB_MASK_D;

    switch (color)
    {
        case RED:
            PORTB |= RGB_R_MASK_B;
            break;
        
        case GREEN:
            PORTD |= (1 << PD7);
            break;
        
        case BLUE:
            PORTD |= (1 << PD6);
            break;

        case WHITE:
            PORTB |= RGB_R_MASK_B;
            PORTD |= RGB_GB_MASK_D;
            break;

        case OFF:
        default:
            break;
    }
}

int main(void)
{
    DDRB |= LED_MASK_B | RGB_R_MASK_B;   // Sätter PB2-PB5 samt PB0 som utgång
    DDRD |= RGB_GB_MASK_D; // Sätter PD6-PD7 som utgång

    DDRD &= ~ENCODER_MASK_D; // Sätter PD2-PD3 som ingångar
    DDRD &= ~ENCODER_SW_MASK; // Sätter PD4 som ingång
    DDRB &= ~ENCODER_BUTTON_1; // Sätter PB1 som ingång
    DDRD &= ~ENCODER_BUTTON_2; // Sätter PD5 som ingång

    PORTD |= ENCODER_MASK_D | ENCODER_SW_MASK | ENCODER_BUTTON_2;
    PORTB |= ENCODER_BUTTON_1;

    timer0_init_ms();
    adc_init_a0();

    uint8_t currentCLK = 0;
    uint8_t currentDT = 0;
    uint8_t prevEncState = (((PIND & (1 << PD2)) >> PD2) << 1) | ((PIND & (1 << PD3)) >> PD3);
    uint8_t encState = prevEncState;
    int8_t encoderDelta = 0;
    uint32_t lastToggle = 0;
    uint16_t interval = 250;
    uint8_t ledOn = 0;
    uint32_t lastAdcRead = 0;
    uint16_t adcLatest = 0;
    uint8_t rgbSelected = 0;   // 0 = inaktiv, 1 = aktiv
    uint8_t buttonNow = (PIND & ENCODER_SW_MASK) >> PD4;
    uint8_t buttonPrev = buttonNow;
    uint8_t buttonStable = buttonNow;
    uint32_t lastButtonChange = 0;
    uint32_t lastRgbToggle = 0;
    uint8_t rgbBlinkOn = 0;
    RGB currentColor = OFF;

    
    
    while (1)
    {
        uint32_t now = millis();

        buttonNow = (PIND & ENCODER_SW_MASK) >> PD4;

        if (buttonNow != buttonPrev)
        {
            lastButtonChange = now;
            buttonPrev = buttonNow;
        }

        if (now - lastButtonChange >= DEBOUNCE_TIME_MS) {
            if (buttonStable != buttonNow) {
                buttonStable = buttonNow;

                if (buttonStable == 0) {
                    if (currentColor != OFF) {
                        rgbSelected = !rgbSelected;
                        rgbBlinkOn = 0;
                        lastRgbToggle = now;
                        encoderDelta = 0;
                        prevEncState = (((PIND & (1 << PD2)) >> PD2) << 1) | ((PIND & (1 << PD3)) >> PD3);
                    }
                }
            }
        }

        // Läs encoder som 2-bitars tillstånd och uppdatera vald RGB-färg när ett helt steg registrerats.
        currentCLK = (PIND & (1 << PD2)) >> PD2;
        currentDT  = (PIND & (1 << PD3)) >> PD3;
        encState = (currentCLK << 1) | currentDT;

        if (rgbSelected == 0)
        {
            // Medurs delsteg
            if ((prevEncState == 0b00 && encState == 0b01) ||
                (prevEncState == 0b01 && encState == 0b11) ||
                (prevEncState == 0b11 && encState == 0b10) ||
                (prevEncState == 0b10 && encState == 0b00))
            {
                encoderDelta--;
            }
            // Moturs delsteg
            else if ((prevEncState == 0b00 && encState == 0b10) ||
                    (prevEncState == 0b10 && encState == 0b11) ||
                    (prevEncState == 0b11 && encState == 0b01) ||
                    (prevEncState == 0b01 && encState == 0b00))
            {
                encoderDelta++;
            }

            // Ett helt mekaniskt steg medurs
            if (encoderDelta >= 2)
            {
                currentColor = currentColor + 1;
                if (currentColor > WHITE)
                {
                    currentColor = OFF;
                }
                encoderDelta = 0;
            }
            // Ett helt mekaniskt steg moturs
            else if (encoderDelta <= -2)
            {
                if (currentColor == OFF)
                {
                    currentColor = WHITE;
                }
                else
                {
                    currentColor = currentColor - 1;
                }
                encoderDelta = 0;
            }
        }

        prevEncState = encState;

        if (rgbSelected == 0) {
            set_rgb(currentColor);
        }
        else {
            if (now - lastRgbToggle >= 250) {
                lastRgbToggle = now;
                rgbBlinkOn = !rgbBlinkOn;
            }

            if (rgbBlinkOn == 1) {
                set_rgb(currentColor);
            }
            else {
                set_rgb(OFF);
            }
        }

        //ADC läses periodiskt var 20 ms för att undvika onödiga mätningar och blockering i huvudloopen.
        if (now - lastAdcRead >= 20) 
        {
            adcLatest = adc_read_blocking();
            uint16_t scaled = adcLatest >> 2;
            uint16_t extra_ms = scaled * 10;
            interval = 250 + extra_ms;
            lastAdcRead = now;
        }

        if (now - lastToggle >= interval) 
        {
            lastToggle += interval;
            ledOn = !ledOn;
            if(ledOn == 1) 
            {
                PORTB |= LED_MASK_B;
            }
            else 
            {
                PORTB &= ~LED_MASK_B;
            }
        }
    }

    return 0;
}
