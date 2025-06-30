#include <stdio.h>
#include <mega16.h>
#include <delay.h>
#include <1wire.h>
#include <i2c.h>
#include <ds1820.h>
#include <alcd.h>

// I2C Bus functions
#asm
   .equ __i2c_port=0x15 ;PORTC
   .equ __sda_bit=1
   .equ __scl_bit=0
#endasm


#define EEPROM_BUS_ADDRESS 0xa0

/*
 * @brief  Declare global variables here
 * @note   
*/
int timer_cnt = 0;   
char timer_flag = 0;
unsigned char ds18b20_ls = 0;
unsigned char ds18b20_ms = 0;
unsigned char time_cnt = 0;
unsigned char ee_addr = 0;
float tempature = 0.0;
char temp_buf[16];
char temp_bufF[16];
char temp_ready = 0; // Flag to indicate if temperature is ready
float F = 0.0;
/*
 * @brief  EEPROM read function (EEPROM读取函数)
 * @note
*/
unsigned char eeprom_read(unsigned char address) {
   unsigned char data;

   i2c_start();
   i2c_write(EEPROM_BUS_ADDRESS);
   i2c_write(address);
   i2c_start();
   i2c_write(EEPROM_BUS_ADDRESS | 1);
   data = i2c_read(0);

   i2c_stop();
   return data;
}

/*
 * @brief  EEPROM write function (EEPROM写入函数)
 * @note
*/
void eeprom_write(unsigned char address, unsigned char data) {
   i2c_start();

   i2c_write(EEPROM_BUS_ADDRESS);
   i2c_write(address);
   i2c_write(data);
   i2c_stop();

 
   delay_ms(10);
}

/*
 * @brief  DS18B20 init 12bit
 * @note   
*/
void ds18b20_set_12bit_resolution() {
   w1_init();
   w1_write(0xCC); // 只有一个DS18B20时可以跳过ROM寻找
   w1_write(0x4E); // Write scratchpad
   w1_write(0x00); // TH register
   w1_write(0x00); // TL register
   w1_write(0x7F); // Configuration register (12-bit resolution)
}

/*
 * @brief  DS18B20 start temperature conversion
 * @note
*/
void ds18b20_start_convert() {
   w1_init();
   w1_write(0xCC); 
   w1_write(0x44); // Start temperature conversion
}

/*
 * @brief  DS18B20 read temperature
 * @note   
*/
void ds18b20_read_temp() {
   w1_init();
   w1_write(0xCC);         
   w1_write(0xBE);               // Read start
   ds18b20_ls = w1_read(); // Read low byte
   ds18b20_ms = w1_read(); // Read high byte
}

/*
 * @brief  DS18B20 convert to float
 * @note
*/
float ds18b20_to_float(unsigned char low_byte, unsigned char high_byte) {
   int raw_temp = (high_byte << 8) | low_byte;
   return raw_temp * 0.0625f;
}



// External Interrupt 0 service routine
interrupt [EXT_INT0] void ext_int0_isr(void)
{

}

// External Interrupt 1 service routine
interrupt [EXT_INT1] void ext_int1_isr(void)
{

}

#ifndef RXB8
#define RXB8 1
#endif

#ifndef TXB8
#define TXB8 0
#endif

#ifndef UPE
#define UPE 2
#endif

#ifndef DOR
#define DOR 3
#endif

#ifndef FE
#define FE 4
#endif

#ifndef UDRE
#define UDRE 5
#endif

#ifndef RXC
#define RXC 7
#endif

#define FRAMING_ERROR (1<<FE)
#define PARITY_ERROR (1<<UPE)
#define DATA_OVERRUN (1<<DOR)
#define DATA_REGISTER_EMPTY (1<<UDRE)
#define RX_COMPLETE (1<<RXC)

// USART Receiver buffer
#define RX_BUFFER_SIZE 8
char rx_buffer[RX_BUFFER_SIZE];

#if RX_BUFFER_SIZE <= 256
unsigned char rx_wr_index,rx_rd_index,rx_counter;
#else
unsigned int rx_wr_index,rx_rd_index,rx_counter;
#endif

// This flag is set on USART Receiver buffer overflow
bit rx_buffer_overflow;

// USART Receiver interrupt service routine
interrupt [USART_RXC] void usart_rx_isr(void)
{
char status,data;
status=UCSRA;
data=UDR;
if ((status & (FRAMING_ERROR | PARITY_ERROR | DATA_OVERRUN))==0)
   {
   rx_buffer[rx_wr_index++]=data;
#if RX_BUFFER_SIZE == 256
   // special case for receiver buffer size=256
   if (++rx_counter == 0)
      {
#else
   if (rx_wr_index == RX_BUFFER_SIZE) rx_wr_index=0;
   if (++rx_counter == RX_BUFFER_SIZE)
      {
      rx_counter=0;
#endif
      rx_buffer_overflow=1;
      }
   }
}

#ifndef _DEBUG_TERMINAL_IO_
// Get a character from the USART Receiver buffer
#define _ALTERNATE_GETCHAR_
#pragma used+
char getchar(void)
{
char data;
while (rx_counter==0);
data=rx_buffer[rx_rd_index++];
#if RX_BUFFER_SIZE != 256
if (rx_rd_index == RX_BUFFER_SIZE) rx_rd_index=0;
#endif
#asm("cli")
--rx_counter;
#asm("sei")
return data;
}
#pragma used-
#endif

// USART Transmitter buffer
#define TX_BUFFER_SIZE 8
char tx_buffer[TX_BUFFER_SIZE];

#if TX_BUFFER_SIZE <= 256
unsigned char tx_wr_index,tx_rd_index,tx_counter;
#else
unsigned int tx_wr_index,tx_rd_index,tx_counter;
#endif

// USART Transmitter interrupt service routine
interrupt [USART_TXC] void usart_tx_isr(void)
{
if (tx_counter)
   {
   --tx_counter;
   UDR=tx_buffer[tx_rd_index++];
#if TX_BUFFER_SIZE != 256
   if (tx_rd_index == TX_BUFFER_SIZE) tx_rd_index=0;
#endif
   }
}

#ifndef _DEBUG_TERMINAL_IO_
// Write a character to the USART Transmitter buffer
#define _ALTERNATE_PUTCHAR_
#pragma used+
void putchar(char c)
{
while (tx_counter == TX_BUFFER_SIZE);
#asm("cli")
if (tx_counter || ((UCSRA & DATA_REGISTER_EMPTY)==0))
   {
   tx_buffer[tx_wr_index++]=c;
#if TX_BUFFER_SIZE != 256
   if (tx_wr_index == TX_BUFFER_SIZE) tx_wr_index=0;
#endif
   ++tx_counter;
   }
else
   UDR=c;
#asm("sei")
}
#pragma used-
#endif


// Timer 0 output compare interrupt service routine
/*
 * @brief  每达到一次compare值就会触发一次中断，然后计数1000次就是1s
*/
interrupt[TIM0_COMP] void timer0_comp_isr(void)
{
   if (timer_cnt >= 1000) // 1s
   {
      timer_flag = 1; 
      timer_cnt = 0; 
   }
   else
   timer_cnt++;
}




void main(void)
{
   // Declare your local variables here

   // Input/Output Ports initialization
   // Port A initialization
   // Func7=In Func6=In Func5=In Func4=In Func3=In Func2=In Func1=In Func0=In 
   // State7=T State6=T State5=T State4=T State3=T State2=T State1=T State0=T 
   PORTA = 0x00;
   DDRA = 0x00;

   // Port B initialization
   // Func7=In Func6=In Func5=In Func4=In Func3=In Func2=In Func1=In Func0=In 
   // State7=T State6=T State5=T State4=T State3=T State2=T State1=T State0=T 
   PORTB = 0x00;
   DDRB = 0x00;

   // Port C initialization
   // Func7=In Func6=In Func5=In Func4=In Func3=In Func2=In Func1=In Func0=In 
   // State7=T State6=T State5=T State4=T State3=T State2=T State1=T State0=T 
   PORTC = 0x00;
   DDRC = 0x00;

   // Port D initialization
   // Func7=In Func6=In Func5=In Func4=In Func3=In Func2=In Func1=In Func0=In 
   // State7=T State6=T State5=T State4=T State3=T State2=T State1=T State0=T 
   PORTD = 0x00;
   DDRD = 0x00;

   // Timer/Counter 0 initialization
   // Clock source: System Clock
   // Clock value: 250.000 kHz
   // Mode: CTC top=OCR0
   // OC0 output: Disconnected
   /*
    *    @brief Timer/Counter 0 initialization
    *    @param
    *    @brief 为了让timer0能够测到1s，而其为8bit，我这里选取了250kHz的时钟，比较容易计算
    *           计算可以得到1/250000Hz = 0.000004s
    *           0.000004s * 250 = 0.001s   那么compare值就为250
    *           再重复个1000次的flag就可以

   */
   TCCR0 = 0x0B;
   TCNT0 = 0x00;
   OCR0 = 0xF9;

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
   TCCR1A = 0x00;
   TCCR1B = 0x00;
   TCNT1H = 0x00;
   TCNT1L = 0x00;
   ICR1H = 0x00;
   ICR1L = 0x00;
   OCR1AH = 0x00;
   OCR1AL = 0x00;
   OCR1BH = 0x00;
   OCR1BL = 0x00;

   // Timer/Counter 2 initialization
   // Clock source: System Clock
   // Clock value: Timer2 Stopped
   // Mode: Normal top=0xFF
   // OC2 output: Disconnected
   ASSR = 0x00;
   TCCR2 = 0x00;
   TCNT2 = 0x00;
   OCR2 = 0x00;

   // External Interrupt(s) initialization
   // INT0: On
   // INT0 Mode: Falling Edge
   // INT1: On
   // INT1 Mode: Falling Edge
   // INT2: Off
   GICR |= 0xC0;
   MCUCR = 0x0A;
   MCUCSR = 0x00;
   GIFR = 0xC0;

   // Timer(s)/Counter(s) Interrupt(s) initialization
   TIMSK = 0x02;

   // USART initialization
   // Communication Parameters: 8 Data, 1 Stop, No Parity
   // USART Receiver: On
   // USART Transmitter: On
   // USART Mode: Asynchronous
   // USART Baud Rate: 9600
   UCSRA = 0x00;
   UCSRB = 0xD8;
   UCSRC = 0x86;
   UBRRH = 0x00;
   UBRRL = 0x67;

   // Analog Comparator initialization
   // Analog Comparator: Off
   // Analog Comparator Input Capture by Timer/Counter 1: Off
   ACSR = 0x80;
   SFIOR = 0x00;

   // ADC initialization
   // ADC disabled
   ADCSRA = 0x00;

   // SPI initialization
   // SPI disabled
   SPCR = 0x00;

   // TWI initialization
   // TWI disabled
   TWCR = 0x00;

   // I2C Bus initialization
   i2c_init();

   // 1 Wire Bus initialization
   // 1 Wire Data port: PORTC
   // 1 Wire Data bit: 7
   // Note: 1 Wire port settings must be specified in the
   // Project|Configure|C Compiler|Libraries|1 Wire IDE menu.
   w1_init();

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
   lcd_clear();
   ds18b20_set_12bit_resolution();
   // Global enable interrupts
   #asm("sei")


   while (1)
   {
      ds18b20_start_convert();   //convert是在DS18B20上进行的，不是cpu

      while (!timer_flag);
      timer_flag = 0;

      // sleep_enable();
      // idle();
      // sleep_disable();


      ds18b20_read_temp();
      eeprom_write(ee_addr * 2, ds18b20_ls);
      eeprom_write(ee_addr * 2 + 1, ds18b20_ms);
      tempature = ds18b20_to_float(ds18b20_ls, ds18b20_ms);
      F = ((long)ds18b20_ms << 8 | ds18b20_ls) * 0.0625 * 1.8 + 32;
      sprintf(temp_buf, "T=%.1f C", tempature);   //%.4f说明tempature是一个浮点数 \xdf表示了° C指示了温度的符号
      sprintf(temp_bufF, "T=%.1f F", F);
      ee_addr++; // 指针移动到下一个位置
      if (ee_addr >= 5)
      {
         ee_addr = 0; // 如果到了末尾，就绕回到开头
      }

      lcd_gotoxy(0, 0);
      lcd_puts(temp_buf);
      lcd_gotoxy(0, 1);
      lcd_puts(temp_bufF);




      lcd_gotoxy(10, 0);
      if (time_cnt >= 9) // 10s
      {
         time_cnt = 0;
      }
      else
         time_cnt++;
      lcd_putchar(time_cnt + '0');
      lcd_putchar('S');

      if (time_cnt >= 5)
      {
         temp_ready = 1;
      }



      if (temp_ready)
      {
         unsigned char i = 0;
         float sum = 0;
         float sum_F = 0;
         for (i = 0; i < 5; ++i)
         {
            ds18b20_ls = eeprom_read(i * 2);
            ds18b20_ms = eeprom_read(i * 2 + 1);
            sum += ((long)ds18b20_ms << 8 | ds18b20_ls) * 0.0625;
            sum_F += ((long)ds18b20_ms << 8 | ds18b20_ls) * 0.0625 * 1.8 + 32;
         }

         sprintf(temp_buf, "%.1f C", sum / 5);
         sprintf(temp_bufF, "%.1f C", sum_F / 5);

         lcd_gotoxy(9, 0);
         lcd_puts(temp_buf);
         lcd_gotoxy(9, 1);
         lcd_puts(temp_bufF);

         printf("%.4f\n", sum / 5);
         printf("%.4f\n", sum_F / 5);

      }
   }
}
