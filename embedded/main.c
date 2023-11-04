/*
 * COURSEWORK
 * EMBEDDED SYSTEMS
 * Author : GROUP 2
 */
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define MAX_CARS 100
#define PARK_MAX_CAP 200
#define PRICE_BELOW_10 5000
#define PRICE_ABOVE_10 10000
#define CAR_INFO_SIZE 15

#define F_CPU 8000000UL
#define BAUD_RATE 9600
#define BAUD_PRESCALE ((F_CPU / (16UL * BAUD_RATE)) - 1)

#define rs_fridge PG1
#define rw_fridge PG2
#define enable_fridge PG3
#define rs_gate PA1
#define rw_gate PA2
#define enable_gate PA3
#define dataline PORTH

uint16_t EEPROM_ADDRESS_NUMBER_PLATE = 00;
uint16_t EEPROM_ADDRESS_CHILD_COUNT = 10;
uint16_t EEPROM_ADDRESS_ADULT_COUNT = 20;
uint16_t EEPROM_ADDRESS_MAX_COUNT = 30;
uint16_t EEPROM_ADDRESS_TOTAL_COUNT = 40;
uint16_t EEPROM_ADDRESS_NUMBER_PLATES = 50;
uint16_t WATER_BOTTLES_ADDRESS = 60;
uint16_t WATER_MONEY_ADDRESS = 60;

void latch();
char number_plate[10];
int childCount = 0;
int adultCount = 0;
int carCount = 0;
int waterBottles = 50; // 50 water bottles in the fridge when full.
int bottleNumber;		 // the number of bottles to be dispensed
int waterMoney = 0;
int totalTourists = 0;
int countNumberPlates = 0;
bool loggedIn = false;
bool resetEEPROM = true;

int getChildCount() { return eeprom_read_word((uint16_t *)EEPROM_ADDRESS_CHILD_COUNT); }
int getAdultCount() { return eeprom_read_word((uint16_t *)EEPROM_ADDRESS_ADULT_COUNT); }
int getTotalCount() { return eeprom_read_word((uint16_t *)EEPROM_ADDRESS_TOTAL_COUNT); }

char *removeTrailingNulls(char *str)
{
	int length = strlen(str);

	for (int i = length - 1; i >= 0; i--)
	{
		if (str[i] == '\0')
		{
			str[i] = '\0';
		}
		else
		{
			break;
		}
	}

	return str;
}

void initialiseEEPROM()
{
	updateChildrenInParkCount(0);
	updateAdultsInParkCount(0);
	updateTotalPeopleInParkCount(0);
}

void USART_Init()
{
	// Set baud rate
	UBRR0H = (unsigned char)(BAUD_PRESCALE >> 8);
	UBRR0L = (unsigned char)(BAUD_PRESCALE);

	// Enable receiver and transmitter
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);

	// Set frame format: 8 data bits, 1 stop bit, no parity
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void USART_Transmit(unsigned char data[])
{
	// Wait for empty transmit buffer
	int length = strlen(data);

	for (int k = 0; k < length; k++)
	{
		while (!(UCSR0A & (1 << UDRE0)))
			;

		// Put data into buffer, sends the data
		UDR0 = data[k];
	}
}

void USART_Transmitchar(unsigned char data)
{
	// Wait for empty transmit buffer
	while (!(UCSR0A & (1 << UDRE0)))
		;

	// Put data into buffer, sends the data
	UDR0 = data;
}

unsigned char USART_Receive()
{
	// Wait for data to be received
	while (!(UCSR0A & (1 << RXC0)))
		;

	// Get and return received data from buffer
	return UDR0;
}

void USART_TransmitInt(int num)
{
	char buffer[20];
	sprintf(buffer, "%d", num); // Convert integer to string
	USART_Transmit(buffer);		// Transmit the string
}

void USART_TransmitString(const char *str)
{
	while (*str != '\0')
	{
		USART_Transmitchar(*str);
		str++;
	}
}

void intToString(int number, char *buffer)
{
	sprintf(buffer, "%d", number);
}

int checkCredentials(char *username, char *password)
{
	// Perform authentication logic here
	// Return 1 if authentication is successful, 0 otherwise
	// Example:
	if (strcmp(username, "Silver") == 0 && strcmp(password, "123") == 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void USART_ReceiveString(char *buffer, int maxLength)
{
	int i = 0;
	char receivedChar;

	// Receive characters until a newline or until maxLength is reached
	while (i < maxLength - 1)
	{
		receivedChar = USART_Receive();

		// Echo the received character back to the user
		USART_Transmit(receivedChar);

		// Check for newline character (end of input)
		if (receivedChar == '\n' || receivedChar == '\r')
		{
			break;
		}

		// Store the received character in the buffer
		buffer[i++] = receivedChar;
	}

	// Null-terminate the string
	buffer[i] = '\0';
}

int USART_ReadInteger()
{
	char buffer[20];
	int num;
	USART_ReceiveString(buffer, sizeof(buffer));
	sscanf(buffer, "%d", &num);
	return num;
}

char getOption()
{
	char option;
	option = USART_Receive();
	USART_Transmit("\r\n"); // Print newline to move to the next line
	return option;
}

void replenishFridge()
{
	char inputBuffer[20];
	int bottlesToAdd;
	int desiredBottles = 50;  // Desired number of bottles in the fridge

	USART_Transmit("Number of bottles in the fridge: ");
	USART_TransmitInt(waterBottles);
	USART_Transmit("\r\n");

	// number of bottles to add 
	bottlesToAdd = desiredBottles - waterBottles;

	if (bottlesToAdd > 0) {
		waterBottles += bottlesToAdd;
		USART_Transmit("Replenishing done.\r\n");
		USART_Transmit("Number of bottles in the fridge: ");
		USART_TransmitInt(waterBottles);
		USART_Transmit("\r\n");
		} else {
		USART_Transmit("Fridge is already stocked with enough bottles.\r\n");
	}
}
void displayMenu()
{
	char attendantname[20];
	char password[20];
	char choice;

	USART_Transmit("\t Welcome to Queen Elizabeth National park.\r\n");
	USART_Transmit("Please Login to continue.\r\n");
	bool loggedIn = false;

	while (!loggedIn)
	{
		USART_Transmit("Enter your username: ");
		USART_ReceiveString(attendantname, sizeof(attendantname));
		USART_Transmit("\r\n"); // Print newline to move to the next line
		USART_Transmit("Enter your password: ");
		USART_ReceiveString(password, sizeof(password));
		USART_Transmit("\r\n"); // Print newline to move to the next line

		if (checkCredentials(attendantname, password))
		{
			USART_Transmit("Login successful.\r\n");
			loggedIn = true; // Set loggedIn to true to exit the loop
		}
		else
		{
			USART_Transmit("Invalid username or password. Please try again.\r\n");
		}
	}

	do
	{
		USART_Transmit("Choose an option:\r\n");
		USART_Transmit("1. Register Tourists.\r\n");
		USART_Transmit("2. Tourists in the park.\r\n");
		USART_Transmit("3. Cars in the park.\r\n");
		USART_Transmit("4. Total money collected by the park.\r\n");
		USART_Transmit("5. Number of drivers in the park.\r\n");
		USART_Transmit("6. Number of bottles in the fridge.\r\n");
		USART_Transmit("7. Replenish fridge.\r\n");
		USART_Transmit("8. Check if park is full.\r\n");
		USART_Transmit("9. Car exiting park.\r\n");
		USART_Transmit("Q. Logout.\r\n");
		USART_Transmit("Enter an option.\r\n");
		choice = getOption();
	
		handleMenuChoice(choice);
	} while (choice != 'Q');

}

bool isParkFull()
{
	totalTourists = eeprom_read_word((uint16_t *)EEPROM_ADDRESS_TOTAL_COUNT);
	if (totalTourists > PARK_MAX_CAP)
		return true;
	return false;
}

void saveCarInfo(char numberPlate[CAR_INFO_SIZE])
{
	if (countNumberPlates >= 1)
	{
		eeprom_write_block(numberPlate, (void *)EEPROM_ADDRESS_NUMBER_PLATES, CAR_INFO_SIZE);
		countNumberPlates += 1;
		EEPROM_ADDRESS_NUMBER_PLATES += CAR_INFO_SIZE;
		return;
	}

	eeprom_write_block(numberPlate, (void *)EEPROM_ADDRESS_NUMBER_PLATES, CAR_INFO_SIZE);
	countNumberPlates += 1;
	EEPROM_ADDRESS_NUMBER_PLATES += CAR_INFO_SIZE;
}

uint16_t getMaxAddress()
{
	uint16_t maxAddress = EEPROM_ADDRESS_NUMBER_PLATES;
	if (EEPROM_ADDRESS_NUMBER_PLATES > 40)
		maxAddress = EEPROM_ADDRESS_NUMBER_PLATES - CAR_INFO_SIZE;

	return maxAddress;
}

void displayCarsInsidePark()
{
	USART_Transmit("Number of cars: ");
	USART_TransmitInt(countNumberPlates);
	USART_Transmit("\r\n");

	USART_Transmit("Number plate - Children - Adults");
	USART_Transmit("\r\n");

	int count = 1;
	char carInfo[15];
	uint16_t maxAddress = getMaxAddress();

	for (uint16_t i = (EEPROM_ADDRESS_NUMBER_PLATES - (countNumberPlates * CAR_INFO_SIZE)); i <= maxAddress; i += CAR_INFO_SIZE)
	{
		eeprom_read_block(carInfo, (void *)i, CAR_INFO_SIZE);
		USART_TransmitInt(count);
		USART_Transmit(": ");
		USART_Transmit(carInfo);
		USART_Transmit("\r\n");
		count++;
	}
}

void displayMoneyCollected()
{
	eeprom_write_block((void*)&waterMoney, (void*)WATER_MONEY_ADDRESS, sizeof(waterMoney));
	waterMoney = MoneySlot();
	int totalCollected = 0;
	totalCollected = (getChildCount() * PRICE_BELOW_10) + ((getAdultCount() - 1) * PRICE_ABOVE_10);
	int collectedMoney = waterMoney + totalCollected;
	USART_Transmit("Total amount collected: ");
	USART_TransmitInt(collectedMoney);
	USART_Transmit("\r\n");
}

void clearEEPROMMemoryRange(uint16_t start, uint16_t end)
{
	uint8_t clearValue = 0xFF;
	for (uint16_t addr = start; addr < end; addr++)
	{
		eeprom_write_byte((uint8_t *)addr, clearValue);
	}
}

void resetControllerEEPROM()
{
	uint16_t eepromAddress = 0;

	// we choose to use the getMaxAddress() since it only resets those addresses that
	// we wrote to instead of having to do all the memory(using E2END).
	// This saves us a lot of time.
	clearEEPROMMemoryRange(0, getMaxAddress());

	USART_Transmit("Resetting controller successful.");
	USART_Transmit("\r\n");
}

void handleMenuChoice(int choice)
{
	switch (choice)
	{
	case '1':
		if (isParkFull())
		{
			lcd_print_gate("park full");
			return;
		}
		lcd_print_gate("Registration.");
		USART_Transmit("Number plate: ");
		USART_ReceiveString(number_plate, sizeof(number_plate));
		USART_Transmit(" You entered: ");
		USART_Transmit(number_plate);
		USART_Transmit("\r\n");

		USART_Transmit("Below 10yrs: ");
		childCount = USART_ReadInteger();
		USART_Transmit(" You entered: ");
		USART_TransmitInt(childCount);
		USART_Transmit("\r\n");

		USART_Transmit("Above 10yrs: ");
		adultCount = USART_ReadInteger();
		USART_Transmit(" You entered: ");
		USART_TransmitInt(adultCount);
		USART_Transmit("\r\n");

		saveCar(number_plate, childCount, adultCount);

		USART_Transmit("Tourists registered successfully. Data stored in EEPROM.\r\n");

		lcd_print_gate("Car entering.");
		_delay_ms(2000);
		lcd_print_gate("Gate closing");
		_delay_ms(3000);
		lcd_print_gate("Queen Elizabeth N.P.");

		break;
	case '2':
		USART_Transmit("Tourists in the park:\r\n");

		USART_Transmit("Below 10yrs: ");
		USART_TransmitInt(getChildCount());
		USART_Transmit("\r\n");

		USART_Transmit("Above 10yrs: ");
		USART_TransmitInt(getAdultCount());
		USART_Transmit("\r\n");

		USART_Transmit("Total: ");
		USART_TransmitInt(getTotalCount());
		USART_Transmit("\r\n");
		break;
	case '3':
		displayCarsInsidePark(); 
		break;
	case '4':
		displayMoneyCollected();
		break;
	case '5':
		USART_Transmit("Number of drivers inside the park: ");
		USART_TransmitInt(countNumberPlates);
		USART_Transmit("\r\n");
		break;
	case '6':
		eeprom_write_block((void*)&waterBottles, (void*)WATER_BOTTLES_ADDRESS, sizeof(waterBottles));
		MoneySlot();
		USART_Transmit("Number of bottles in the fridge: ");
		USART_TransmitInt(waterBottles);
		USART_Transmit("\r\n");
		break;
	case '7':
		replenishFridge();
		break;
	case '8':
		USART_Transmit("Is park full: ");
		USART_Transmit(isParkFull() ? "yes" : "no");
		USART_Transmit("\r\n");
		break;
	case '9':
		carLeavingPark();
		break;
	case 'Q':
		loggedIn = false;
		USART_Transmit("Logged out.\r\n");
		break;
	default:
		USART_Transmit("Invalid choice. Please try again.\n");
	}
}

void carLeavingPark()
{
	char numberPlate[CAR_INFO_SIZE];
	char carInfo[CAR_INFO_SIZE];
	char *token;
	char *carInfoParts[3];
	int pos = 0;

	USART_Transmit("Exiting car number plate: ");
	USART_ReceiveString(numberPlate, sizeof(numberPlate));
	USART_Transmit("\r\n");

	for (uint16_t address = (EEPROM_ADDRESS_NUMBER_PLATES - (countNumberPlates * CAR_INFO_SIZE)); address <= getMaxAddress(); address += CAR_INFO_SIZE)
	{
		eeprom_read_block(carInfo, (void *)address, CAR_INFO_SIZE);
		token = strtok(carInfo, "-");
		carInfoParts[pos] = token;
		pos++;
		while (token != NULL)
		{
			token = strtok(NULL, "-");
			carInfoParts[pos] = token;
			pos++;
		}

		if (!strcmp(removeTrailingNulls(numberPlate), carInfoParts[0]))
		{
			countNumberPlates--;
			clearEEPROMMemoryRange(address, address + CAR_INFO_SIZE); // delete that car from memory
			updateChildrenInParkCount(getChildCount() - atoi(carInfoParts[1]));
			updateAdultsInParkCount(getAdultCount() - atoi(carInfoParts[2]));
			updateTotalPeopleInParkCount(atoi(carInfoParts[1]) + atoi(carInfoParts[2]));
			lcd_print_gate("Car exiting the park.");
			_delay_ms(2000);
			lcd_print_gate();
			return;
		}
		else
		{
			USART_Transmit("The entry of car with number plate: ");
			USART_Transmit(numberPlate);
			USART_Transmit(" was not recorded.");

			USART_Transmit("\r\n");
			return;
		}
	}
}

void updateChildrenInParkCount(int count)
{
	eeprom_write_word((uint16_t *)EEPROM_ADDRESS_CHILD_COUNT, count);
}

void updateAdultsInParkCount(int count)
{
	eeprom_write_word((uint16_t *)EEPROM_ADDRESS_ADULT_COUNT, count);
}

void updateTotalPeopleInParkCount(int count)
{
	eeprom_write_word((uint16_t *)EEPROM_ADDRESS_TOTAL_COUNT, count);
}

void saveCar(char numberPlate[10], int occupantsBelowTen, int occupantsAboveTen)
{
	char formattedCarInfo[15];
	sprintf(formattedCarInfo, "%s-%d-%d", numberPlate, occupantsBelowTen, occupantsAboveTen);
	saveCarInfo(formattedCarInfo);

	updateChildrenInParkCount(getChildCount() + occupantsBelowTen);
	updateAdultsInParkCount(getAdultCount() + occupantsAboveTen);
	updateTotalPeopleInParkCount(getTotalCount() + occupantsAboveTen + occupantsBelowTen);
}


void latch_lcd_fridge()
{
	PORTG |= (1 << enable_fridge);
	_delay_ms(1);
	PORTG &= ~(1 << enable_fridge);
	_delay_ms(1);
}

void latch_lcd_gate()
{
	PORTA |= (1 << enable_gate);
	_delay_ms(1);
	PORTA &= ~(1 << enable_gate);
	_delay_ms(1);
}

void lcd_data_fridge(unsigned char data)
{
	PORTH = data;
	PORTG |= (1 << rs_fridge);	// Data mode   rs= 1
	PORTG &= ~(1 << rw_fridge); // WRITE
	latch_lcd_fridge();
}

void lcd_data_gate(unsigned char data)
{
	PORTF = data;
	PORTA |= (1 << rs_gate);  // Data mode   rs= 1
	PORTA &= ~(1 << rw_gate); // WRITE
	latch_lcd_gate();
}

void lcd_command_fridge(unsigned char command)
{
	PORTH = command;
	PORTG &= ~(1 << rs_fridge); // COMMAND MODE
	PORTG &= ~(1 << rw_fridge); // WRITE
	latch_lcd_fridge();
}

void lcd_command_gate(unsigned char command)
{
	PORTF = command;
	PORTA &= ~(1 << rs_gate); // COMMAND MODE
	PORTA &= ~(1 << rw_gate); // WRITE
	latch_lcd_gate();
}

void lcd_init_fridge()
{
	lcd_command_fridge(0x0f);
	lcd_command_fridge(0x3f);
	lcd_command_fridge(0x01);
}

void lcd_init_gate()
{
	lcd_command_gate(0x0f);
	lcd_command_gate(0x3f);
	lcd_command_gate(0x01);
}

void lcd_print_fridge(char *content)
{
	int len = strlen(content);
	for (int i = 0; i < len; i++)
	{
		lcd_data_fridge(content[i]);
	}
}

void lcd_print_gate_clear(char *content)
{
	int len = strlen(content);
	for (int i = 0; i < len; i++)
	{
		lcd_data_gate(content[i]);
	}
}

void lcd_print_gate(char *content)
{
	lcd_command_gate(0x01);
	int len = strlen(content);
	for (int i = 0; i < len; i++)
	{
		lcd_data_gate(content[i]);
	}
}

void MoneySlot()
{
	lcd_command_fridge(0x01); 
	lcd_command_fridge(0x80);
	lcd_print_fridge("Bottle costs");
	lcd_command_fridge(0xC9);
	lcd_print_fridge("UGX1500");
	lcd_command_fridge(0x90);
	lcd_print_fridge("Bottles: ");
	int bottleNumber = 0;
	while (bottleNumber == 0)
	{
		bottleNumber = bottle_keypad();
	}
	lcd_command_fridge(0x99);
	lcd_data_fridge((char)(bottleNumber + '0'));
	_delay_ms(500);

	int cash = bottleNumber * 1500;
	char bottleCash[8];
	sprintf(bottleCash, "%d", cash);
	
	lcd_command_fridge(0x01);
	lcd_command_fridge(0x80);
	_delay_ms(50);
	lcd_print_fridge("Total Cost:");
	lcd_command_fridge(0xC5);
	lcd_print_fridge(bottleCash);
	lcd_command_fridge(0xCA);
	lcd_print_fridge("UGX.");
	_delay_ms(50);

	lcd_command_fridge(0x90);
	lcd_print_fridge("Press 9:Accept");
	lcd_command_fridge(0xD0);
	lcd_print_fridge("Press 0: Quit");

	int choice = 0;
	while (choice == 0)
	{
		choice = bottle_keypad();

		if (choice == 9)
		{
			lcd_command_fridge(0x01);
			lcd_command_fridge(0x80);
			lcd_print_fridge("Accepted.");
			for (int i = 0; i < 3; i++) // motor - open money slot
			{
				PORTK |= (1 << 0);
				_delay_ms(1000);
				PORTK &= ~(1 << 0);
				_delay_ms(1000);
			}

			_delay_ms(2000);

			// dispense bottle
			for (int b = 0; b < bottleNumber; b++)
			{
				PORTK |= (1 << 1);
				_delay_ms(2000);
				PORTK &= ~(1 << 1);
				_delay_ms(2000);
			}

			waterMoney += cash;
			waterBottles -= bottleNumber;
			return waterMoney;
		}
		else if (choice == 0)
		{
			lcd_command_fridge(0x01);
			lcd_command_fridge(0x80);
			lcd_print_fridge("Bye Bye. Thanks.");
			_delay_ms(2000);
			return;
		}
		else
		{
			choice = 0; // Reset choice if an invalid key is pressed
		}

		 
	}
	MoneySlot();
}

int bottle_keypad()
{
	PORTL = 0b11111011;
	if ((PINL & 0x8) == 0)
	{
		bottleNumber = 1;
	}
	else if ((PINL & 0x10) == 0)
	{
		bottleNumber = 4;
	}
	else if ((PINL & 0x20) == 0)
	{
		bottleNumber = 7;
	}

	PORTL = 0b11111101; // second column
	if ((PINL & 0x8) == 0)
	{
		bottleNumber = 2;
	}
	else if ((PINL & 0x10) == 0)
	{
		bottleNumber = 5;
	}
	else if ((PINL & 0x20) == 0)
	{
		bottleNumber = 8;
	}
	else if ((PINL & 0x40) == 0)
	{
		bottleNumber = 0;
	}

	PORTL = 0b11111110;
	if ((PINL & 0x8) == 0)
	{
		bottleNumber = 3;
	}
	else if ((PINL & 0x10) == 0)
	{
		bottleNumber = 6;
	}
	else if ((PINL & 0x20) == 0)
	{
		bottleNumber = 9;
	}
	return bottleNumber;
}

int main(void)
{
	DDRL = 0x87;
	DDRH = 0xff; // fridge LCD
	DDRG = 0xff; // fridge LCD
	DDRK = 0xff;
	DDRF = 0xff; // gate LCD
	DDRA = 0xff; // gate LCD

	lcd_init_gate();
	lcd_init_fridge();
	lcd_print_gate_clear("WELCOME TO");
	lcd_command_gate(0xC0);
	lcd_print_gate_clear("QUEEN ELIZABETH");
	lcd_command_gate(0x90);
	lcd_print_gate_clear("NATIONAL PARK.");
	_delay_ms(2000);
	USART_Init();
	initialiseEEPROM();

	// setup interrupts
	sei();
	EIMSK |= (1 << INT0);
	EICRA |= (1 << ISC01);

	// set data direction registers
	DDRD = 0x00;
	DDRB = 0xFF;

	// Set pull up resistor on incoming car switch
	PORTD |= (1 << PD0);

	if (resetEEPROM)
	{
		resetControllerEEPROM();
		initialiseEEPROM();
	}

	while (1)
	{
		displayMenu();
	}
}

ISR(INT0_vect)
{
	if (PIND & ~(1 << PD0)) {
		PORTB |= (1 << PB7);
		_delay_ms(500);
		PORTB &= ~(1 << PB7);
		lcd_print_gate("Car at Gate.");
	}
		
}
