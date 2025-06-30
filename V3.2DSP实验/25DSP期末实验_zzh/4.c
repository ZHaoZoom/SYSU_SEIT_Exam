#include <mega16.h>
#include <delay.h>
#include <alcd.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

//general I/O definition
#define LED1        PORTA.3
#define LED2        PORTA.4
#define KEY1        PIND.2
#define KEY2        PIND.3

#define SAMPLE_POINT  125

// Declare your global variables here
//global variable for equation: y[n] = -a1* y[n-1] - a2*y[n-2] + b1 * x[n-1]
float g_y   = 0, g_y_1 = 0, g_y_2 = 0;
float g_x   = 0, g_x_1 = 0;
float g_a_1 = 0, g_a_2 = 0, g_b_1 = 0;

float g1_y   = 0, g1_y_1 = 0, g1_y_2 = 0;
float g1_x   = 0, g1_x_1 = 0;
float g1_a_1 = 0, g1_a_2 = 0, g1_b_1 = 0;

float g_y1   = 0, g_y1_1 = 0, g_y1_2 = 0;

float data_out; 
int trans[SAMPLE_POINT];
int counter=0;
char i=0;
bit rd_trans_flag = 0;

//this is used to transmit data to Matlab
void trans_data(int x1)        
{
    unsigned char H5,L5;   
    unsigned int AD_x;
    AD_x=x1&0x3FF;          //only get the data blow 1023  
    H5=AD_x>>5;             //get the high 5 bit value         
    L5=AD_x&0x1F;           //get the low 5 bit value
    putchar(L5+11);         //to avoid the terminator(10) for matlab
    putchar(H5+51);         //to avoid the terminator(10) for matlab
}

void SPI_MasterTransmit(unsigned int VALUE){
	unsigned int Data, Dhigh, Dlow;
	char cData;
	Data = ((unsigned int)VALUE);
	Dhigh = (Data>>8) & (0x00ff);
	Dlow = Data & 0x00ff;
	PORTB.4 = 0;

	cData = Dhigh;
	SPDR = cData;
	while(!(SPSR & (1<<SPIF)));
	delay_us(10);

	cData = Dlow;
	SPDR = cData;
	while(!(SPSR & (1<<SPIF)));
	delay_us(10);

	PORTB.4 = 1;
}

// Timer 0 overflow interrupt service routine
interrupt [TIM0_OVF] void timer0_ovf_isr(void)
{
    TCNT0=0xE0;
    rd_trans_flag = 1;
}

#define ADC_VREF_TYPE 0x40

// Read the AD conversion result
unsigned int read_adc(unsigned char adc_input)
{
ADMUX=adc_input | (ADC_VREF_TYPE & 0xff);
// Delay needed for the stabilization of the ADC input voltage
delay_us(10);
// Start the AD conversion
ADCSRA|=0x40;
// Wait for the AD conversion to complete
while ((ADCSRA & 0x10)==0);
ADCSRA|=0x10;
return ADCW;
}

// SPI functions
#include <spi.h>


void main(void)
{
// Declare your local variables here

float analog_frequency1 = 10;
float analog_frequency2 = 15;
float digital_10frequency = analog_frequency1 * 2 * 3.1415926 / 1000;
float digital_15frequency = analog_frequency2 * 2 * 3.1415926 / 1000;
char display_buff[10];
unsigned int DAC_Voltage = 512; // 10bits DAC (0 ~ 1023)
unsigned int sample;
unsigned int data[SAMPLE_POINT] = {0};

// Input/Output Ports initialization
// Port A initialization
// Func7=In Func6=In Func5=In Func4=In Func3=In Func2=In Func1=In Func0=In 
// State7=T State6=T State5=T State4=T State3=T State2=T State1=T State0=T 
PORTA=0x00;
DDRA=0x00;

// Port B initialization
// Func7=Out Func6=In Func5=Out Func4=Out Func3=In Func2=In Func1=In Func0=In 
// State7=0 State6=T State5=0 State4=0 State3=T State2=T State1=T State0=T 
PORTB=0x00;
DDRB=0xB0;

// Port C initialization
// Func7=In Func6=In Func5=In Func4=In Func3=In Func2=In Func1=In Func0=In 
// State7=T State6=T State5=T State4=T State3=T State2=T State1=T State0=T 
PORTC=0x00;
DDRC=0x00;

// Port D initialization
// Func7=In Func6=In Func5=In Func4=In Func3=In Func2=In Func1=In Func0=In 
// State7=T State6=T State5=T State4=T State3=T State2=T State1=T State0=T 
PORTD=0x00;
DDRD=0x00;

// Timer/Counter 0 initialization
// Clock source: System Clock
// Clock value: 15.625 kHz
// Mode: Normal top=0xFF
// OC0 output: Disconnected
TCCR0=0x05;
TCNT0=0xE0;
OCR0=0x00;

// Timer/Counter 1 initialization
// Clock source: System Clock
// Clock value: Timer1 Stopped
// Mode: Normal top=0xFFFF
// OC1A output: Discon.
// OC1B output: Discon.
// Noise Canceler: Off
// Input Capture on Falling Edge
// Timer1 Overflow Interrupt: Off
// Input Capture Interrupt: Off
// Compare A Match Interrupt: Off
// Compare B Match Interrupt: Off
TCCR1A=0x00;
TCCR1B=0x00;
TCNT1H=0x00;
TCNT1L=0x00;
ICR1H=0x00;
ICR1L=0x00;
OCR1AH=0x00;
OCR1AL=0x00;
OCR1BH=0x00;
OCR1BL=0x00;

// Timer/Counter 2 initialization
// Clock source: System Clock
// Clock value: Timer2 Stopped
// Mode: Normal top=0xFF
// OC2 output: Disconnected
ASSR=0x00;
TCCR2=0x00;
TCNT2=0x00;
OCR2=0x00;

// External Interrupt(s) initialization
// INT0: Off
// INT1: Off
// INT2: Off
MCUCR=0x00;
MCUCSR=0x00;

// Timer(s)/Counter(s) Interrupt(s) initialization
TIMSK=0x01;

// USART initialization
// Communication Parameters: 8 Data, 1 Stop, No Parity
// USART Receiver: On
// USART Transmitter: On
// USART Mode: Asynchronous
// USART Baud Rate: 56000
UCSRA=0x00;
UCSRB=0x18;
UCSRC=0x86;
UBRRH=0x00;
UBRRL=0x11;

// Analog Comparator initialization
// Analog Comparator: Off
// Analog Comparator Input Capture by Timer/Counter 1: Off
ACSR=0x80;
SFIOR=0x00;

// ADC initialization
// ADC Clock frequency: 1000.000 kHz
// ADC Voltage Reference: AVCC pin
// ADC Auto Trigger Source: ADC Stopped
ADMUX=ADC_VREF_TYPE & 0xff;
ADCSRA=0x84;

// SPI initialization
// SPI Type: Master
// SPI Clock Rate: 4000.000 kHz
// SPI Clock Phase: Cycle Start
// SPI Clock Polarity: Low
// SPI Data Order: MSB First
SPCR=0x50;
SPSR=0x00;

// TWI initialization
// TWI disabled
TWCR=0x00;

// Alphanumeric LCD initialization
// Connections specified in the
// Project|Configure|C Compiler|Libraries|Alphanumeric LCD menu:
// RS - PORTB Bit 0
// RD - PORTB Bit 1
// EN - PORTB Bit 2
// D4 - PORTD Bit 4
// D5 - PORTD Bit 5
// D6 - PORTD Bit 6
// D7 - PORTD Bit 7
// Characters/line: 16

lcd_init(16);
lcd_gotoxy(0,0); 
lcd_putsf("   Generator       is ready!");

// Global enable interrupts
#asm("sei")

while (1)
      {
        if(rd_trans_flag)
        {   sample =(unsigned long)read_adc(1);  
            SPI_MasterTransmit(DAC_Voltage);
            g1_y = -g1_a_1 * g1_y_1 - g1_a_2 * g1_y_2;
            g1_y_2 = g1_y_1;
            g1_y_1 = g1_y;

            g_y = -g_a_1 * g_y_1 - g_a_2 * g_y_2;
            g_y_2 = g_y_1;
            g_y_1 = g_y;

            data_out = g_y + g1_y;
            trans[counter]= data_out*400+500;
            if(trans[counter]<0)                           
                trans[counter]=0;
            if(trans[counter]>1023)
                trans[counter]=1023; 
            DAC_Voltage = trans[counter];
            trans_data(trans[counter]);
            trans_data(sample);
            counter  = counter + 1;
            if(counter >= SAMPLE_POINT )   
            {   
                putchar(10);               // stop Matlab transmition, Key value of Matlab
                counter=0;              // clear counter
            }
            rd_trans_flag = 0;
        }
        
        if(!KEY1)
        {    
            delay_ms(30);
            if(!KEY1)
            {                 
                #asm("cli");
                
                analog_frequency1 = 15;
                digital_10frequency = analog_frequency1 * 2 * 3.1415926 / 1000;
                lcd_clear();
                lcd_gotoxy(0, 0);
                lcd_putsf("f1=");
                itoa(analog_frequency1, display_buff);
                lcd_gotoxy(6,0);
                lcd_puts(display_buff);
                lcd_puts("Hz   ");

                g_a_1 = -2 * cos(digital_10frequency);
                g_a_2 = 1;
                g_b_1 = sin(digital_10frequency);

                g_x = 1;
                g_x_1 = 0; 
                g_y = 0;
                g_y_1 =0;
                g_y_2 = 0;  
                
                g_y = -g_a_1 * g_y_1 - g_a_2 * g_y_2 + g_b_1 * g_x_1;   
                g_y_2 = g_y_1;
                g_y_1 = g_y;
                g_x_1 = g_x; 
                g_x = 0;        
               
                g_y = -g_a_1 * g_y_1 - g_a_2 * g_y_2 + g_b_1 * g_x_1;   
                g_y_2 = g_y_1;   
                g_y_1 = g_y;
                g_x_1 = g_x; 
                g_x = 0;       
                
                analog_frequency2=25;   
                lcd_gotoxy(0,1);
                lcd_putsf("f2="); 
                itoa(analog_frequency2, display_buff);
                lcd_gotoxy(6,1);
                lcd_puts(display_buff);
                lcd_puts("Hz   ");
                
                g1_a_1 = -2 * cos(digital_15frequency);
                g1_a_2 = 1;
                g1_b_1 = sin(digital_15frequency);
                
                g1_x = 1;
                g1_x_1 = 0; 
                g1_y = 0;
                g1_y_1 =0;
                g1_y_2 = 0;  
                
                g1_y = -g1_a_1 * g1_y_1 - g1_a_2 * g1_y_2 + g1_b_1 * g1_x_1;   
                g1_y_2 = g1_y_1;
                g1_y_1 = g1_y;
                g1_x_1 = g1_x; 
                g1_x = 0;        
               
                g1_y = -g1_a_1 * g1_y_1 - g1_a_2 * g1_y_2 + g1_b_1 * g1_x_1;   
                g1_y_2 = g1_y_1;   
                g1_y_1 = g1_y;
                g1_x_1 = g1_x; 
                g1_x = 0;   
                

                #asm("sei");
                
            }
            while(!KEY1);  
        }    
        
        if(!KEY2)
        {    
            delay_ms(30);
            if(!KEY2)
            {                 
                #asm("cli");   
                
                analog_frequency1 = 15;
                lcd_clear();
                lcd_gotoxy(0, 0);
                lcd_putsf("f1=");
                itoa(analog_frequency1, display_buff);
                lcd_gotoxy(6, 0);
                lcd_puts(display_buff);
                lcd_puts("Hz   ");

                g_a_1 = -2 * cos(digital_10frequency);
                g_a_2 = 1;
                g_b_1 = sin(digital_10frequency);

                g_x = 1;
                g_x_1 = 0; 
                g_y = 0;
                g_y_1 =0;
                g_y_2 = 0;  
                
                g_y = -g_a_1 * g_y_1 - g_a_2 * g_y_2 + g_b_1 * g_x_1;   
                g_y_2 = g_y_1;
                g_y_1 = g_y;
                g_x_1 = g_x; 
                g_x = 0;        
               
                g_y = -g_a_1 * g_y_1 - g_a_2 * g_y_2 + g_b_1 * g_x_1;   
                g_y_2 = g_y_1;   
                g_y_1 = g_y;
                g_x_1 = g_x; 
                g_x = 0;       
                
                analog_frequency2=25;
                digital_15frequency = analog_frequency2 * 2 * 3.1415926 / 1000;   
                lcd_gotoxy(0,1);
                lcd_putsf("f2="); 
                itoa(analog_frequency2, display_buff);
                lcd_gotoxy(6,1);
                lcd_puts(display_buff);
                lcd_puts("Hz   ");
                
                g1_a_1 = -2 * cos(digital_15frequency);
                g1_a_2 = 1;
                g1_b_1 = sin(digital_15frequency);
                
                g1_x = 1;
                g1_x_1 = 0; 
                g1_y = 0;
                g1_y_1 =0;
                g1_y_2 = 0;  
                
                g1_y = -g1_a_1 * g1_y_1 - g1_a_2 * g1_y_2 + g1_b_1 * g1_x_1;   
                g1_y_2 = g1_y_1;
                g1_y_1 = g1_y;
                g1_x_1 = g1_x; 
                g1_x = 0;        
               
                g1_y = -g1_a_1 * g1_y_1 - g1_a_2 * g1_y_2 + g1_b_1 * g1_x_1;   
                g1_y_2 = g1_y_1;   
                g1_y_1 = g1_y;
                g1_x_1 = g1_x; 
                g1_x = 0;   

                #asm("sei");
                
            }
            while(!KEY2);  
        }



      }
}
