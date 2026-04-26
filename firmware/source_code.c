#include <lpc17xx.h>
#include <string.h>
#include <stdio.h>
/* ================= DELAY ================= */
void delay(unsigned int d)
{
volatile int i;
for(i=0;i<d*20;i++);
}
void delay_ms(unsigned int ms)
{
volatile int i,j;
for(i=0;i<ms;i++)
for(j=0;j<2000;j++);
}
/* ================= LCD ================= */
#define LCD_DATA (0xFF<<15)
#define RS (1<<10)
#define EN (1<<11)
void lcd_cmd(char cmd)
{
LPC_GPIO0->FIOCLR = LCD_DATA;
LPC_GPIO0->FIOSET = (cmd<<15);
LPC_GPIO0->FIOCLR = RS;
LPC_GPIO0->FIOSET = EN;
delay(30);
LPC_GPIO0->FIOCLR = EN;
if(cmd==0x01 || cmd==0x02)
delay_ms(5);
}
void lcd_data(char data)
{
LPC_GPIO0->FIOCLR = LCD_DATA;
LPC_GPIO0->FIOSET = (data<<15);
LPC_GPIO0->FIOSET = RS;
LPC_GPIO0->FIOSET = EN;
delay(30);
LPC_GPIO0->FIOCLR = EN;
}
void lcd_str(char *s)
{
while(*s) lcd_data(*s++);
}
void lcd_init()
{
delay_ms(20);
LPC_GPIO0->FIODIR |= LCD_DATA|RS|EN;
lcd_cmd(0x38);
lcd_cmd(0x0C);
lcd_cmd(0x01);
lcd_cmd(0x80);
}
/* ================= 7 SEGMENT ================= */
/* assuming direct GPIO P0.4�P0.11 */
#define SEG_PORT LPC_GPIO0
#define SEG_PINS (0xFF<<4)
void seg_init()
{
SEG_PORT->FIODIR |= SEG_PINS;
}
void seg_off()
{
SEG_PORT->FIODIR |= SEG_PINS;
}
void seg_off()
{
SEG_PORT->FIOCLR = SEG_PINS;
}
/* ================= OUTPUT ================= */
#define RELAY (1<<28)
#define LED1 (1<<19)
#define LED2 (1<<20)
#define LED3 (1<<21)
#define BUZZ (1<<27)
/* ================= GLOBAL ================= */
char correct_pin[]="123456";
char entered[7];
int attempts=3;
/* ================= UART ================= */
void uart_init()
{
LPC_PINCON->PINSEL0 |= (1<<4)|(1<<6);
LPC_UART0->LCR=0x83;
LPC_UART0->DLL=6;
LPC_UART0->DLM=0;
LPC_UART0->FDR=(12<<4)|1;
LPC_UART0->LCR=0x03;
}
void uart_send_char(char c)
{
while(!(LPC_UART0->LSR&(1<<5)));
LPC_UART0->THR=c;
}
void uart_send(char *s)
{
while(*s) uart_send_char(*s++);
}
char uart_get()
{
while(!(LPC_UART0->LSR&1));
return LPC_UART0->RBR;
}
/* ================= ADC LM35 ================= */
void adc_init()
{
LPC_PINCON->PINSEL1 &= ~(3<<18);
LPC_PINCON->PINSEL1 |= (1<<18);
LPC_SC->PCONP |= (1<<12);
LPC_ADC->ADCR=(1<<2)|(4<<8)|(1<<21);
}
float read_temp()
{
unsigned int val;
float volt;
LPC_ADC->ADCR|=(1<<24);
while(!(LPC_ADC->ADGDR&(1UL<<31)));
val=(LPC_ADC->ADGDR>>4)&0xFFF;
volt=(val*3.3)/4095.0;
return volt*100;
}
/* ================= LOCK ================= */
void lock_alarm()
{
uart_send("\r\nSYSTEM LOCKED\r\n");
lcd_cmd(0x01);
lcd_cmd(0x80);
lcd_str("SYSTEM LOCK");
LPC_GPIO1->FIOSET |= BUZZ;
delay_ms(2000);
LPC_GPIO1->FIOCLR |= BUZZ;
attempts=3;
}
/* ================= MAIN ================= */
int main()
{
int i;
float t;
int ok;
int access_done=0;
char buf[40];
uart_init();
adc_init();
lcd_init();
seg_init();
LPC_GPIO1->FIODIR |= RELAY|LED1|LED2|LED3|BUZZ;
LPC_GPIO1->FIOCLR |= RELAY|LED1|LED2|LED3|BUZZ;
lcd_cmd(0x01);
lcd_str("SMART VAULT");
delay_ms(700);
while(!access_done)
{
uart_send("\r\nEnter PIN:\r\n");
lcd_cmd(0x01);
lcd_cmd(0x80);
lcd_str("ENTER PIN:");
for(i=0;i<6;i++)
{
entered[i]=uart_get
lcd_data('*');
}
entered[6]='\0';
/* ===== PIN OK ===== */
if(strcmp(entered,correct_pin)==0)
{
lcd_cmd(0x01);
lcd_str("PIN OK");
LPC_GPIO1->FIOSET |= LED1;
lcd_cmd(0xC0);
lcd_str("CHECK TEMP");
LPC_GPIO1->FIOSET |= LED2;
ok=0;
for(i=0;i<10;i++)
{
t=read_temp();
sprintf(buf,"Temp=%.2f\r\n",t);
uart_send(buf);
if(t>30)
{
ok=1;
break;
}
delay_ms(70);
}
LPC_GPIO1->FIOCLR |= LED2;
/* ===== ACCESS GRANTED ===== */
if(ok)
{
uart_send("ACCESS GRANTED\r\n");
lcd_cmd(0x01);
lcd_str("ACCESS OK");
/* TURN OFF 7 SEGMENT */
seg_off();
LPC_GPIO1->FIOSET |= RELAY;
delay_ms(1200);
LPC_GPIO1->FIOCLR |= RELAY;
access_done=1;
}
else
{
lcd_cmd(0x01);
lcd_str("NO HEAT");
LPC_GPIO1->FIOSET |= LED3;
delay_ms(700);
LPC_GPIO1->FIOCLR |= LED3;
}
LPC_GPIO1->FIOCLR |= LED1;
}
/* ===== WRONG PIN ===== */
else
{
attempts--;
lcd_cmd(0x01);
lcd_str("WRONG PIN");
LPC_GPIO1->FIOSET |= LED3;
delay_ms(350);
LPC_GPIO1->FIOCLR |= LED3;
if(attempts==0)
lock_alarm();
}
}
/* ===== FINAL SAFE STATE ===== */
lcd_cmd(0xC0);
lcd_str("VAULT OPEN");
seg_off();
LPC_GPIO1->FIOCLR |= LED1|LED2|LED3;
while(1);
}
