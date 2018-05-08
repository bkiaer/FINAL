/*
 * FINAL_RECEIVER.c
 *
 * Created: 4/30/2018 1:22:08 PM
 * Author : brian
 */ 


#define F_CPU 16000000UL //16MHz
#define BAUD 9600 //Baud Rate
#define MYUBRR F_CPU/16/BAUD-1 //calculate Baud

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <string.h>
#include <util/delay.h>
#include "nrf24l01.h"
#include "nrf24l01-mnemonics.h"

nRF24L01 *setup_rf(void);
void process_message(char *message);
inline void prepare_led_pin(void);
inline void set_led_high(void);

inline void set_led_low(void);
volatile bool rf_interrupt = false;

void USART_init( unsigned int ubrr ); //initialize USART
void USART_tx_string(char *data); //Print String USART
volatile unsigned int adc_temp;
char outs[20]; //array


int main(void) {
	uint8_t address[5] = { 0x20, 0x30, 0x40, 0x51, 0x61 };
	prepare_led_pin();
	char str[80];
		USART_init(MYUBRR); // Initialize the USART (RS232 interface)
		USART_tx_string("Connected!\r\n"); // shows theres a connection with USART
		_delay_ms(125); // wait a bit
		
		
	sei();
	nRF24L01 *rf = setup_rf();
	nRF24L01_listen(rf, 0, address);
	uint8_t addr[5];
	nRF24L01_read_register(rf, CONFIG, addr, 1);
		int i = 0;
	while (true) {
		if (rf_interrupt) {
			rf_interrupt = false;
		
			while (nRF24L01_data_received(rf)) {
				nRF24L01Message msg;
				nRF24L01Message msg2;
				if(i == 0){
				nRF24L01_read_received_data(rf, &msg);
				process_message((char *)msg.data);
				//USART_tx_string("LOOP1");
				//i++;
				}
				 if(i == 1){
				nRF24L01_read_received_data(rf, &msg2);
				
				process_message((char *)msg2.data);
				//USART_tx_string("LOOP2");
			//	i++;
				}
				 if(i==2)
				{
					USART_tx_string(msg.data);
					USART_tx_string(" ");
					USART_tx_string(msg2.data);
					USART_tx_string(" \r\n");
				//	i = 0;
				}
				i++;
				if(i == 3)
				i = 0;
				//USART_tx_string("EXTRA\r\n");
			//	USART_tx_string(" \r\n");
			}
			nRF24L01_listen(rf, 0, address);
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

void process_message(char *message) {
	if (strcmp(message, "ON") == 0)
	set_led_high();
	else if (strcmp(message, "OFF") == 0)
	set_led_low();
}
inline void prepare_led_pin(void) {
	DDRB |= _BV(PB0);
	PORTB &= ~_BV(PB0);
}
inline void set_led_high(void) {
	PORTB |= _BV(PB0);
}
inline void set_led_low(void) {
	PORTB &= ~_BV(PB0);
}



/* INIT USART (RS-232) */
void USART_init( unsigned int ubrr ) {
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)ubrr;
	UCSR0B = (1 << TXEN0); // Enable receiver, transmitter & RX interrupt
	UCSR0C = (3 << UCSZ00); //asynchronous 8 N 1
}

void USART_tx_string( char *data ) {
	while ((*data != '\0')) {
		while (!(UCSR0A & (1 <<UDRE0)));
		UDR0 = *data;
		data++;
	}
}
// nRF24L01 interrupt
ISR(INT0_vect) {
	rf_interrupt = true;
}
