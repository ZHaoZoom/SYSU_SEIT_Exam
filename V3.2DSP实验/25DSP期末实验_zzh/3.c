#include <mega16.h>
#include <delay.h>
#include <alcd.h>
#include <math.h>
#include <stdio.h>


#define FIR_NSEC      61    
#define SAMPLE_POINT  127   
#define ADC_VREF_TYPE 0x40  

#include "sta_data.c"         

//definition for filter type  
#define LOWPASS     0
#define HIGHPASS    1 
#define BANDPASS    2
#define BANDSTOP    3

bit rd_ad_flag = 0;           
unsigned int count=0;

interrupt [TIM0_OVF] void timer0_ovf_isr(void)
{
    TCNT0 = 0xCF;        // Reload Timer0 counter to adjust the overflow interval (sets the next overflow time)
    count++;             // Increment the overflow counter
    if (count == 4)      // Check if 4 overflows have occurred
    {
        count = 0;       // Reset the counter
        PORTA.5 = ~PORTA.5; // Toggle the state of PORTA.5 (output pin)
    }
}



// Timer1 overflow interrupt service routine
interrupt [TIM1_OVF] void timer1_ovf_isr(void) 
       rd_ad_flag=1;         

//this is used to transmit data to Matlab �����ݷ��͵�MATLAB
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

// Read the AD conversion result        ADC��������ȡADCת���������ADCW
unsigned int read_adc(unsigned char adc_input)
{
ADMUX=adc_input | (ADC_VREF_TYPE & 0xff);
// Delay needed for the stabilization of the ADC input voltage
delay_us(10);
// Start the AD conversion  ���������ΪʲôADC���õ�ʱ����stop���ǻ�����ADC����Ϊ�ֶ���ʼ������
ADCSRA|=0x40;
// Wait for the AD conversion to complete
while ((ADCSRA & 0x10)==0);
ADCSRA|=0x10;
return ADCW;
}


void main(void)
{
// Declare your local variables here
long data_out;                    //���ڼ����˲����
int trans[SAMPLE_POINT];              
unsigned int data[SAMPLE_POINT]={0};
int ad_counter=0;
int filter_mode=0;
int i;
int current_num=0;
int a_num=0;
unsigned int sample;

// Timer/Counter 0 initialization
// Clock source: System Clock
// Clock value: 250.000 kHz
// Mode: Normal top=0xFF
// OC0 output: Disconnected
TCCR0 = 0x05;
TCNT0 = 0xB2;
OCR0 = 0x00;


// Timer/Counter 1 initialization
// Clock source: System Clock
// Clock value: 2000.000 kHz
// Mode: Ph. & fr. cor. PWM top=ICR1
// OC1A output: Discon.
// OC1B output: Discon.
// Noise Canceler: Off
// Input Capture on Falling Edge
// Timer1 Overflow Interrupt: On
// Input Capture Interrupt: Off
// Compare A Match Interrupt: Off
// Compare B Match Interrupt: Off
TCCR1A=0x00;                          
TCCR1B=0x12;
TCNT1H=0x00;                         
TCNT1L=0x00;
ICR1H=0x04;                           
ICR1L=0xE2;                           
OCR1AH=0x00;
OCR1AL=0x00;
OCR1BH=0x00;
OCR1BL=0xD0;

// Timer(s)/Counter(s) Interrupt(s) initialization
TIMSK=0x05;

// USART initialization
// Communication Parameters: 8 Data, 1 Stop, No Parity
// USART Receiver: On
// USART Transmitter: On
// USART Mode: Asynchronous
// USART Baud Rate: 56000
UCSRA=0x00;
UCSRB=0x18;                            //��Ϊ0xD8������ ���ս����ж�ʹ�ܣ����ͽ����ж�ʹ��
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
/**********initial configure*********************/
lcd_init(16);
PORTD.2=1;                       //����PD2
DDRD.2=0;
PORTA.5=1;                       //PA5��������ź�
DDRA.5=1;   
lcd_clear();
lcd_gotoxy(0,0);
lcd_putsf("    LOWPASS         fc =50Hz    ");

/**********initial configure*********************/

// Global enable interrupts
#asm("sei")

while (1)
      {
         if(rd_ad_flag)                //��ʱ��1���������ADת��
          {
            rd_ad_flag=0;              //�����״̬��  
            data_out=0;                //data_out���ڼ������1������˲��Ľ��
            sample =(unsigned long)read_adc(1); //��Channel 1 ������(ADCW,16λ��0~9λ��������)��0~1023
            data[a_num]=sample; //�洢����  
            
            switch (filter_mode)
            {
               case LOWPASS:
                        for(i=0; i<FIR_NSEC; i++)
                        {   
                            current_num = a_num - i;
                            if(current_num <0)  current_num = current_num + SAMPLE_POINT;
                            data_out = data_out + (long)(lp[i] * data[current_num]);      
                        }                            
               break;
               
               case HIGHPASS:
                        for(i=0; i<FIR_NSEC; i++)
                        {
                            current_num = a_num - i;
                            if(current_num <0)  current_num = current_num + SAMPLE_POINT;
                            data_out = data_out + (long)(hp[i] * data[current_num]);    
                        }
               break;
               
               case BANDPASS:
                        for(i=0; i<FIR_NSEC; i++)
                        {
                            current_num = a_num - i;
                            if(current_num <0)  current_num = current_num + SAMPLE_POINT;
                            data_out = data_out + (long)(bp[i] * data[current_num]);     
                        }
               break;
               
               case BANDSTOP:
                        for(i=0; i<FIR_NSEC; i++)
                        {
                            current_num = a_num - i;
                            if(current_num <0)  current_num = current_num + SAMPLE_POINT;
                            data_out = data_out + (long)(bs[i] * data[current_num]);   
                        }
               break; 
               
               default:;                          
            } 
            

            a_num = (a_num == SAMPLE_POINT ) ? 0 : (a_num + 1);   // a_num +1 
            
            trans[ad_counter]= data_out/80000 ;                  //��data_out��һ���������Իᳬ��0-1023��Χ
            if(filter_mode==0)                                   //�����˲�ģʽѡ��ֱ��ƫ�ã����ⶪʧ����������
                trans[ad_counter]= trans[ad_counter] + 100;
            if(filter_mode==1)
                trans[ad_counter]= trans[ad_counter] + 500;
            if(filter_mode==2)
                trans[ad_counter]= trans[ad_counter] + 500;
            if(filter_mode==3)
                trans[ad_counter]= trans[ad_counter] + 100;
            if(trans[ad_counter]<0)                              //����������䷶Χ
                trans[ad_counter]=0;
            if(trans[ad_counter]>1023)
                trans[ad_counter]=1023;
            ad_counter  = ad_counter + 1;                        // ��¼ad��ɵĴ���
         }
         
         if(ad_counter> SAMPLE_POINT-1 )    // AD���������ﵽҪ��
         {
               for(i=0;i<SAMPLE_POINT;i++) trans_data((int)data[i]);  //����һ��AD�ռ���������
               for(i=0;i<SAMPLE_POINT;i++) trans_data(trans[i]);      //����һ��FIR�˲��������
               trans_data(filter_mode);                               //����һ����־�����ڱ���˲�ģʽ��,���͵�������֮�ͱ���С�ڵ���256..
               putchar(10);               // stop Matlab transmition, Key value of Matlab
               delay_ms(300);             // delay some time
               ad_counter=0;              // clear adc counter
         }
         
         /**********change the filter mode*********************/            //�ı��˲���ģʽ
         if(!PIND.2)                                                        //���°���PD2
         {
             delay_ms(30);                                                  //������������
             if(!PIND.2)
             {
                filter_mode = (filter_mode==3) ? 0 : (filter_mode+1);       //no more than 4 mode
                
                switch(filter_mode)
                {
                    case 0:
                    lcd_gotoxy(0,0);
                    lcd_putsf("    LOWPASS         fc =50Hz    ");         //�˲���ϵ����sta_data.c��
                    filter_mode=0;
                    break;
                    
                    case 1:
                    lcd_gotoxy(0,0);
                    lcd_putsf("    HIGHPASS        fc =150Hz   ");
                    filter_mode=1;
                    break;
                    
                    case 2:
                    lcd_gotoxy(0,0);
                    lcd_putsf("    BANDPASS        fc =30-80Hz  ");
                    filter_mode=2;
                    break;         
                    
                    case 3:
                    lcd_gotoxy(0,0);
                    lcd_putsf("    BANDSTOP        fc =60-120Hz ");
                    filter_mode=3;
                    break; 
                    
                    default:
                    lcd_gotoxy(0,0);
                    lcd_putsf("     ERROR!                      ");                    
                }
                
                while(!PIND.2);
             }
         }         
         
      }   
}
