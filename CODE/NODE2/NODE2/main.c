/*
 * NODE2.c
 *
 * Created: 5/2/2018 12:16:57 PM
 * Author : kiaer
 */ 

#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "nrf24l01.h"
#include "i2c_master.h"
#define BAUD 9600 //Baud Rate
#define MYUBRR F_CPU/16/BAUD-1 //calculate Baud

#define TSL2591_VISIBLE	(2)
#define TSL2591_INFRARED (1)
#define TSL2591_FULLSPECTRUM (0)

#define TSL2591_ADDR (0x29)
#define TSL2591_READBIT (0x01)

#define TSL2591_COMMAND_BIT (0xA0)
#define TSL2591_CLEAR_INT (0xE7)
#define TSL2591_TEST_INT (0xE4)
#define TSL2591_WORD_BIT (0x20)
#define TSL2591_BLOCK_BIT (0x10)

#define TSL2591_ENABLE_POWEROFF (0x00)
#define TSL2591_ENABLE_POWERON (0x09)
#define TSL2591_ENABLE_AEN (0x02)
#define TSL2591_ENABLE_AIEN (0x10)
#define TSL2591_ENABLE_NPIEN (0x80)

#define TSL2591_LUX_DF (408.0f)
#define TSL2591_LUX_COEFB (1.64f)
#define TSL2591_LUX_COEFC (0.59f)
#define TSL2591_LUX_COEFD (0.86f)

#define TSL2591_ENABLE (0x00)
#define TSL2591_CONFIG (0x01)
#define TSL2591_ID (0x12)
#define TSL2591_C0DATAL (0x14)
#define TSL2591_C0DATAH (0x15)
#define TSL2591_C1DATAL (0x16)
#define TSL2591_C1DATAH (0x17)

#define TSL2591_WRITE 0xE0
#define TSL2591_READ 0xE1

void init_uart(uint16_t baudrate){
	uint16_t UBRR_val = (F_CPU/16)/(baudrate-1);
	
	UBRR0H = UBRR_val >> 8;
	UBRR0L = UBRR_val;
	
	UCSR0B |= (1<<TXEN0) | (1<<RXEN0) | (1<<RXCIE0);
	UCSR0C |= (1<<USBS0) | (3<<UCSZ00);
}

void uart_putc(unsigned char c){
	while(!(UCSR0A & (1<<UDRE0)));
	UDR0 = c;
}


void uart_puts(char *s){
	while(*s){
		uart_putc(*s);
		s++;
	}
}

void init_TSL2591(void){
	i2c_start(TSL2591_WRITE);
	i2c_write(TSL2591_COMMAND_BIT | TSL2591_ENABLE);
	i2c_write(TSL2591_ENABLE_POWERON | TSL2591_ENABLE_AEN |TSL2591_ENABLE_AIEN | TSL2591_ENABLE_NPIEN);
	i2c_stop();
	
	i2c_start(TSL2591_WRITE);
	i2c_write(TSL2591_COMMAND_BIT | TSL2591_CONFIG);
	i2c_write(0x10);
	i2c_stop();
}

float getLux(void){
	float atime = 100.0f, again = 25.0f;
	uint16_t ch0, ch1;
	float cp1;
	float  lux1, lux2, lux;
	lux1 = lux2 = lux = 0;
	uint32_t x = 1;
	
	i2c_start(TSL2591_WRITE);
	i2c_write(TSL2591_COMMAND_BIT | TSL2591_C0DATAL);
	i2c_stop();
	
	i2c_start(TSL2591_READ);
	
	x = ((uint8_t)i2c_read_ack())<<8;
	x |= i2c_read_ack();
	x <<=16;
	x |= ((uint8_t)i2c_read_ack())<<8;
	x |= i2c_read_ack();
	i2c_stop();
	ch1 = x>>8;
	ch0 = x & 0xFFFF;
	
	cp1 = (uint32_t) (atime * again) / TSL2591_LUX_DF;
	lux1 = (uint32_t) ((float) ch0 - (TSL2591_LUX_COEFB * (float) ch1)) / cp1;
	lux2 = (uint32_t) ((TSL2591_LUX_COEFC * (float) ch0) - (TSL2591_LUX_COEFD * (float) ch1)) / cp1;

	lux = (lux1 > lux2) ? lux1: lux2;
	
	return lux;
}

unsigned char i2c_read(unsigned char isLast)
{
	if(isLast == 0)
	TWCR = ( 1 << TWINT) | (1 << TWEN) | (1 <<TWEA);
	else
	TWCR = ( 1 << TWINT) | ( 1 << TWEN);
	while ((TWCR & ( 1 << TWINT)) == 0);
	return TWDR;
	
}
void setup_timer(void);
nRF24L01 *setup_rf(void);
volatile bool rf_interrupt = false;
volatile bool send_message = false;

void USART_init( unsigned int ubrr ); //initialize USART
void USART_tx_string(char *data); //Print String USART
volatile unsigned int adc_temp;
char outs[256]; //array

volatile char received_data;


int main(void) {
	uint8_t to_address[5] = { 0x20, 0x30, 0x40, 0x51, 0x61 };
	bool on = false;
	char buffer[6];
	float alux;
	char str[80];
	DDRD = (1<<PD5) |(1<<PD6)|(1<<PD7);
	PORTD = 0b11100000;
	
	USART_init(MYUBRR); // Initialize the USART (RS232 interface)
	
	i2c_init();
	init_TSL2591();
		
	USART_tx_string("Connected!\r\n"); // shows theres a connection with USART
	_delay_ms(125); // wait a bit
	
	sei();
	nRF24L01 *rf = setup_rf();
	setup_timer();

	while (true) {
		alux = getLux();
		dtostrf(alux, 6, 2, buffer);

			//	_delay_ms(10000);
		if (rf_interrupt) {
			rf_interrupt = false;
			int success = nRF24L01_transmit_success(rf);
			if (success != 0)
			nRF24L01_flush_transmit_message(rf);
		}
		if (send_message) {
			send_message = false;
			on = !on;
			nRF24L01Message msg;

			if (on){
				strcpy(msg.data, "NODE2: ");
			//	memcpy(msg.data, buffer, 10);
				strcat(msg.data, buffer);
				_delay_ms(1000);
				uart_puts(msg.data);
				uart_puts("  \r\n");
			}
			else memcpy(msg.data, "OFF", 4);
			msg.length = strlen((char *)msg.data) + 1;
			nRF24L01_transmit(rf, to_address, &msg);
		}
	}
	return 0;
}
nRF24L01 *setup_rf(void) {
	nRF24L01 *rf = nRF24L01_init();

rf->ss.port = &PORTB;
rf->ss.pin = PB2;
rf->ce.port = &PORTB;
rf->ce.pin = PB1;
rf->sck.port = &PORTB;
rf->sck.pin = PB5;
rf->mosi.port = &PORTB;
rf->mosi.pin = PB3;
rf->miso.port = &PORTB;
rf->miso.pin = PB4;
// interrupt on falling edge of INT0 (PD2)
EICRA |= _BV(ISC01);
EIMSK |= _BV(INT0);
nRF24L01_begin(rf);
return rf;
}
// setup timer to trigger interrupt every second when at 1MHz
void setup_timer(void) {
	TCCR1B |= _BV(WGM12);
	TIMSK1 |= _BV(OCIE1A);
	OCR1A = 15624;
	TCCR1B |= _BV(CS10) | _BV(CS11);
}

/* INIT USART (RS-232) */
void USART_init( unsigned int ubrr ) {
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)ubrr;
	UCSR0B |= (1 << TXEN0) | (1 << RXEN0)| ( 1 << RXCIE0); // Enable receiver, transmitter & RX interrupt
	UCSR0C |= (1<<UCSZ01) | (1 << UCSZ00);
}

void USART_tx_string( char *data ) {
	while ((*data != '\0')) {
		while (!(UCSR0A & (1 <<UDRE0)));
		UDR0 = *data;
		data++;
	}
}

// each one second interrupt
ISR(TIMER1_COMPA_vect) {
	send_message = true;
}
// nRF24L01 interrupt
ISR(INT0_vect) {
	rf_interrupt = true;
}
