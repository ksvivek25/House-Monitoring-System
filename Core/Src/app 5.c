/*
 * app.c
 *
 *  Created on: Nov 4, 2024
 *      Author: Julian
 */


/* Includes ------------------------------------------------------------------*/
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "app.h"

/* Private define ------------------------------------------------------------*/
#define SENSOR_PIN GPIO_PIN_4
#define LED_PORT GPIOA

#define LED_PORT GPIOA
#define LED_GREEN GPIO_PIN_6
#define LED_RED GPIO_PIN_7
#define BUZZER GPIO_PIN_9

#define LED_PIN GPIO_PIN_5
// keypad input pins
#define PORTA GPIOA
#define KEYPIN1 GPIO_PIN_10
#define KEYPIN2 GPIO_PIN_11
#define KEYPIN3 GPIO_PIN_12
#define KEYPIN4 GPIO_PIN_15

// keypad output pins
#define PORTB GPIOB
#define KEYPIN8 GPIO_PIN_15
#define KEYPIN7 GPIO_PIN_14
#define KEYPIN6 GPIO_PIN_12
#define KEYPIN5 GPIO_PIN_11

/* Private function prototypes -----------------------------------------------*/
void UART_TransmitString(UART_HandleTypeDef *p_huart, char a_string[], int newline);
void ShowCommands(void);
void resetPins();
void scan(void);
void addKey(char c);
char getKey(void);
void printDisplay(void);//// new code

/* Private variables ---------------------------------------------------------*/
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern ADC_HandleTypeDef hadc1;

volatile char rxData;

int HUNDRED_msCount;

// temperature function

#define TEMP_ON_THRESHOLD 24.0f   // Temperature to turn the fan ON (in Celsius)

#define TEMP_OFF_THRESHOLD 22.0f  // Temperature to turn the fan OFF (in Celsius)

void Fan_Init(void);

void Control_Fan(float currentTemp);

// Activating system variables
char password[10] = "\0";
char userInput[10] = "\0";
int pos = 0;
int settingPass;
int countDown;
int count;
int attempts;

// Keypad variables
uint16_t outputPins[4] = {KEYPIN8, KEYPIN7, KEYPIN6, KEYPIN5};
uint16_t inputPins[4] = {KEYPIN4, KEYPIN3, KEYPIN2, KEYPIN1};
char keysPressed[10];
int keyListSize = 0;
int keypadOFF;
int keypadTimer;
char userKey;

// alarm system variables
int sensor_tripped;
int alarmON;
int systemON = 0;

char keys[4][4] = {
		{'1', '2', '3', 'a'},
		{'4', '5', '6', 'b'},
		{'7', '8', '9', 'c'},
		{'*', '0', '#', 'd'}
};

char strBuffer[10];
float volts,temp;
int displayON;
int fan;

void App_Init(void) {
	// initialize keyPressed character list to '.'
	for (int i = 0; i < 10; i++){
		keysPressed[i] = '.';
	}
	settingPass = 1;
	countDown = 0;
	count = 11;
	attempts = 0;
	keypadOFF = 0;
	keypadTimer = 3;
	userKey = ' ';
	HUNDRED_msCount = 0;
	sensor_tripped = alarmON = displayON = fan = 0;

	HAL_GPIO_WritePin(PORTA, LED_PIN, GPIO_PIN_SET);
	UART_TransmitString(&huart2, "||| Enter a 4-digit pin to set password |||", 1); // uart function
	HAL_UART_Receive_IT(&huart2, (uint8_t*) &rxData, 1); //Rx interrupt for uart

	// Initialize output GPIO to low
	resetPins();


	// temperature function
	// Initialize fan control GPIO

	Fan_Init();
	HAL_ADC_Start(&hadc1);


	HAL_TIM_Base_Start_IT(&htim2);
	HAL_TIM_Base_Start_IT(&htim3);
}

void App_MainLoop(void) {
	/* adc variables */
	uint32_t TEMP;
	/* ------------- */

	scan();

	// Poll ADC for a new temperature reading

	HAL_ADC_PollForConversion(&hadc1, 100);

	TEMP = HAL_ADC_GetValue(&hadc1);

	volts = ((float)TEMP * 3.3f) / 4095.0f;  // Assuming 12-bit ADC and 3.3V reference

	temp = volts * 100.0f; // LM35: 10mV/°C (Convert voltage to Celsius)

	Control_Fan(temp);

	if(HAL_GPIO_ReadPin(LED_PORT, SENSOR_PIN)==GPIO_PIN_SET){
		sensor_tripped = 1;
		//UART_TransmitString(&huart2, "Sensor Tripped!!", 1); // uart
	}

	if (keyListSize != 0){
		displayON = 0; // stop displaying if you begin entering password
		userKey = getKey();

		if (settingPass == 1){
			password[pos] = userKey;
			pos++;
			UART_TransmitString(&huart2, &userKey, 0); // uart -- print the key the user pressed

			if (pos == 4){
				pos = 0;

				UART_TransmitString(&huart2, "\n\rYour password is: ", 0); // uart -- print complete password
				UART_TransmitString(&huart2, password, 1); // uart -- print complete password

				settingPass = 0;
				displayON = 1;

				ShowCommands(); // uart
				// turn on green led to indicate working program
				HAL_GPIO_WritePin(LED_PORT, LED_GREEN, GPIO_PIN_SET);
			}
		} else {
//			if (userKey == '#'){ // used to get the value for adc
//
//				sprintf(strBuffer, "%7.2f", temp);
//				UART_TransmitString(&huart2, "TEMP (C): ", 0); // adc -- print adc value
//				UART_TransmitString(&huart2, strBuffer, 1); // adc -- print adc value
//			}
			//else {
				userInput[pos] = userKey;
				pos++;
				UART_TransmitString(&huart2, &userKey, 0); // uart -- print the key the user pressed
			//}

			// turn on security system
			if (pos == 4){
				if (!strcmp(userInput, password)){ // if password matches
					if (alarmON || sensor_tripped){ //  turn off alarm or countdown if password matches
						alarmON = 0;
						systemON = 0;
						sensor_tripped = 0; // reset sensor tripped
						countDown = 0;
						count = 11;

						UART_TransmitString(&huart2, "\n\rDeactived alarm.", 1); // uart

						HAL_GPIO_WritePin(LED_PORT, LED_GREEN, GPIO_PIN_SET);
						HAL_GPIO_WritePin(LED_PORT, LED_RED, GPIO_PIN_RESET);
						HAL_GPIO_WritePin(LED_PORT, BUZZER, GPIO_PIN_RESET);
					} else {
						countDown = 1;
						//count = 15;

						//systemON = 1;
						UART_TransmitString(&huart2, "\n\rSystem will activate in 11 seconds.", 1); // uart
					}
					attempts = 0;
				} else {
					attempts++;
					UART_TransmitString(&huart2, "\n\rIncorrect password.", 1); // uart
				}
				userInput[0]='\0';
				pos=0;
				displayON = 1; // begin displaying
			}

			if (attempts == 3){
				if (alarmON){
					//keyListSize = 0;
				} else if (sensor_tripped){
					attempts = 0;
					alarmON = 1;
					UART_TransmitString(&huart2, "Exceeded 3 attempts.", 1); // uart
					UART_TransmitString(&huart2, "Activate alarm!!!", 1); // uart

					countDown = 0;
				}
			}
		}
	}

	// sensor tripped
	if (sensor_tripped){
		if (systemON && !alarmON){
			// system to indicate that alarm was tripped
			//alarmON = 1;
			countDown = 1;
		} else if (!systemON){
			sensor_tripped = 0;
		}
	}
}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *p_htim) {
	if (p_htim == &htim2) { // 1 second timer
		if (displayON){
			sprintf(strBuffer, "%7.2f", temp);
			printDisplay();
		}
		HAL_GPIO_TogglePin(PORTA, LED_PIN);

		if (countDown == 1){
			if (count == 11 && sensor_tripped == 1){
				UART_TransmitString(&huart2, "Alarm will activate in 11 seconds.", 1);
			}
			// UART_TransmitString(&huart2, itoa(count, num, 10), 1);
			count--;
			HAL_GPIO_TogglePin(LED_PORT, LED_RED);
			if (count == 0){
				if (sensor_tripped){
					alarmON = 1;
				} else if (!systemON){
					systemON = 1;
					UART_TransmitString(&huart2, "\n\rSystem On\n", 1);
				}
				countDown = 0;
				count = 11;
				// attempts = 0;
				HAL_GPIO_WritePin(LED_PORT, LED_GREEN, GPIO_PIN_RESET);
			}
		}
	}

	// tim3 set for 100ms
	if (p_htim == &htim3){
		HUNDRED_msCount++;
		// every 200 ms for keypad delay
		if (HUNDRED_msCount == 2){
			HUNDRED_msCount = 0;
			if (keypadOFF == 1){
				keypadTimer--;
				if (keypadTimer == 0){
					keypadTimer = 2;
					keypadOFF = 0;
				}
			}
		}

		if (alarmON){
			HAL_GPIO_TogglePin(LED_PORT, LED_RED);
			HAL_GPIO_TogglePin(LED_PORT, BUZZER);
		}
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if(HAL_GPIO_ReadPin(LED_PORT, SENSOR_PIN)==GPIO_PIN_SET){
		UART_TransmitString(&huart2, "Sensor Tripped!!", 1); // uart
	}
}

void scan(void){
	// if there is no key pressed, keep on scanning
	if (!keypadOFF){
		for (int i = 0; i < 4; i++){ // rows
			HAL_GPIO_WritePin(PORTB, outputPins[i], GPIO_PIN_SET);
			for (int j = 0; j < 4; j++){ // columns
				if (HAL_GPIO_ReadPin(PORTA, inputPins[j]) == 1){
					keypadOFF = 1;
					addKey(keys[i][j]);
				}
			}
			HAL_GPIO_WritePin(PORTB, outputPins[i], GPIO_PIN_RESET);
		}
	}
}

void addKey(char c){
	if (keyListSize == 0){
		keysPressed[0] = c;
	} else if (keyListSize > 9){
	} else {
		keysPressed[keyListSize] = c;
	}
	keyListSize++;
}
char getKey(void){
	char c;

	keyListSize--;
	c = keysPressed[keyListSize];
	keysPressed[keyListSize] = '.';

	return c;
}

void resetPins(){
	HAL_GPIO_WritePin(PORTB, KEYPIN8, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(PORTB, KEYPIN7, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(PORTB, KEYPIN6, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(PORTB, KEYPIN5, GPIO_PIN_RESET);
}

void ShowCommands(void) {
	UART_TransmitString(&huart2, "\nTo activate alarm system, enter password.\n", 1);
}

// new code
void printDisplay(void){
	UART_TransmitString(&huart2, "Security system status: ", 0);
	if (systemON){
		UART_TransmitString(&huart2, "ENABLED", 0);
	} else {
		UART_TransmitString(&huart2, "DISABLED", 0);
	}
	UART_TransmitString(&huart2, "      Temp: ", 0);
	UART_TransmitString(&huart2, strBuffer, 0);
	UART_TransmitString(&huart2, "      Fan: ", 0);
	if (fan == 1){
		UART_TransmitString(&huart2, "ON", 1);
	} else {
		UART_TransmitString(&huart2, "OFF", 1);
	}

}

void UART_TransmitString(UART_HandleTypeDef *p_huart,char a_string[], int newline)
{
	HAL_UART_Transmit(p_huart, (uint8_t*) a_string, strlen(a_string), HAL_MAX_DELAY);
	if (newline != 0) {
		HAL_UART_Transmit(p_huart, (uint8_t*) "\n\r", 2, HAL_MAX_DELAY);
	}
}

/* GPIO Initialization for Fan ------------------------------------------------*/

void Fan_Init(void) {

    __HAL_RCC_GPIOB_CLK_ENABLE(); // Enable clock for GPIO port B


    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = GPIO_PIN_0; // Example: PB0 for fan control

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;

    GPIO_InitStruct.Pull = GPIO_NOPULL;

    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;


    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); // Ensure fan is OFF initially

}

/* Fan Control Logic ----------------------------------------------------------*/

void Control_Fan(float currentTemp) {

    static uint8_t fanState = 0; // 0: Fan OFF, 1: Fan ON


    if (currentTemp >= TEMP_ON_THRESHOLD && fanState == 0) {

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); // Turn fan ON

        fanState = 1;

        fan = 1;

    } else if (currentTemp <= TEMP_OFF_THRESHOLD && fanState == 1) {

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); // Turn fan OFF

        fanState = 0;

        fan = 0;

    }

}
