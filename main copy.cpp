//=====[Libraries]=============================================================
//Aca voy a hacer los cambios del item f, punto 6
#include "mbed.h"
#include "arm_book_lib.h"

//=====[Defines]===============================================================

#define NUMBER_OF_KEYS                           4  //cantidad de pulsadores que se usaran
#define BLINKING_TIME_GAS_ALARM               1000  //Duracion del blink en la alarma si se detecta gas
#define BLINKING_TIME_OVER_TEMP_ALARM          500  //Duracion del blink en la alarma si se detecta sobretemperatura
#define BLINKING_TIME_GAS_AND_OVER_TEMP_ALARM  100  //Duracion del blink en la alarma si se detecta gas y a la vez sobretemperatura
#define NUMBER_OF_AVG_SAMPLES                   100 //Numero de muestras que se usan al trabajar con el potenciometro
#define OVER_TEMP_LEVEL                         50  // Valor a partir del cual se considera sobretemperatura
#define TIME_INCREMENT_MS                       10  //Cantidad de incrementos que habra (evita codigo muy bloqueante)

#define MASK_PORTD                             240  //NUEVO En binario 0000000011110000
#define MASK_VAL_LEIDO                           2  //NUEVO 
//=====[Declaration and initialization of public global objects]===============
//PARA LOS PULSADORES EN VEZ DE DIGITALIN SE PUEDE LEER TODO EL GRUPO JUNTO CON PortIn
//PARA 
DigitalIn enterButton(BUTTON1);
PortIn Buttons(PortD,MASK_PORTD);  //NUEVO
DigitalIn alarmTestButton(D2);
DigitalIn mq2(PE_12);
//LOS LEDS no ESTAN EN EL MISMO PUERTO (PA5,PB0,PB7)?
DigitalOut alarmLed(LED1);
DigitalOut incorrectCodeLed(LED3);
DigitalOut systemBlockedLed(LED2);

DigitalInOut sirenPin(PE_10);

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

AnalogIn potentiometer(A0);
AnalogIn lm35(A1);

//=====[Declaration and initialization of public global variables]=============
//Se inicializa la placa con la alarma apagada, sin deteccion de sobretemperatura y con ningun intento de codigo incorrecto
bool alarmState    = OFF;
bool incorrectCode = false;
bool overTempDetector = OFF;


int numberOfIncorrectCodes = 0;
int buttonBeingCompared    = 0;
int codeSequence[NUMBER_OF_KEYS]   = { 1, 1, 0, 0 }; //Se elige la siguiente secuencia de teclas como clave para apagar la alarma
int code_sequence = 12 //NUEVO  en binario   1  1  0  0
int buttonsPressed[NUMBER_OF_KEYS] = { 0, 0, 0, 0 };
int Val_leido_teclas = 0 // 0 0 0 0 
int accumulatedTimeAlarm = 0;
int aux_mask = 3
bool gasDetectorState          = OFF;
bool overTempDetectorState     = OFF;

float potentiometerReading = 0.0;
float lm35ReadingsAverage  = 0.0;
float lm35ReadingsSum      = 0.0;
float lm35ReadingsArray[NUMBER_OF_AVG_SAMPLES];
float lm35TempC            = 0.0;

//=====[Declarations (prototypes) of public functions]=========================

void inputsInit();
void outputsInit();

void alarmActivationUpdate();
void alarmDeactivationUpdate();

void uartTask();
void availableCommands();
bool areEqual();
float celsiusToFahrenheit( float tempInCelsiusDegrees );
float analogReadingScaledWithTheLM35Formula( float analogReading );

//=====[Main function, the program entry point after power on or reset]========
// En el main se inicializan los puertos de entrada y de salida, en un loop infinito 
int main()
{
    inputsInit(); 
    outputsInit();
    while (true) {
        alarmActivationUpdate();
        alarmDeactivationUpdate();
        uartTask();
        delay(TIME_INCREMENT_MS);
    }
}

//=====[Implementations of public functions]===================================
//Se configuran los pulsadores como Pull down
void inputsInit()                                       //SE CAMBIO!!!!!
{
    alarmTestButton.mode(PullDown);
    Buttons.mode(PullDown);
    sirenPin.mode(OpenDrain);
    sirenPin.input();
}
//El sistema inicia con la alarma apagada
void outputsInit()                                     //NO SE CAMBIO!!!
{
    alarmLed = OFF;
    incorrectCodeLed = OFF;
    systemBlockedLed = OFF;
}
//Funcion que analiza a los pines conectados a LM35 y al MQ-2, y dependiendo del caso prende o no a la alarma
void alarmActivationUpdate()                          //NO SE CAMBIO!!!!
{
    static int lm35SampleIndex = 0;
    int i = 0;

    lm35ReadingsArray[lm35SampleIndex] = lm35.read();
    lm35SampleIndex++;
    if ( lm35SampleIndex >= NUMBER_OF_AVG_SAMPLES) {
        lm35SampleIndex = 0;
    }
    
       lm35ReadingsSum = 0.0;
    for (i = 0; i < NUMBER_OF_AVG_SAMPLES; i++) {
        lm35ReadingsSum = lm35ReadingsSum + lm35ReadingsArray[i];
    }
    lm35ReadingsAverage = lm35ReadingsSum / NUMBER_OF_AVG_SAMPLES;
       lm35TempC = analogReadingScaledWithTheLM35Formula ( lm35ReadingsAverage );    
    
    if ( lm35TempC > OVER_TEMP_LEVEL ) {
        overTempDetector = ON;
    } else {
        overTempDetector = OFF;
    }

    if( !mq2) {
        gasDetectorState = ON;
        alarmState = ON;
    }
    if( overTempDetector ) {
        overTempDetectorState = ON;
        alarmState = ON;
    }
    if( alarmTestButton ) {             
        overTempDetectorState = ON;
        gasDetectorState = ON;
        alarmState = ON;
    }    
    if( alarmState ) { 
        accumulatedTimeAlarm = accumulatedTimeAlarm + TIME_INCREMENT_MS;
        sirenPin.output();                                     
        sirenPin = LOW;                                        
    
        if( gasDetectorState && overTempDetectorState ) {
            if( accumulatedTimeAlarm >= BLINKING_TIME_GAS_AND_OVER_TEMP_ALARM ) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        } else if( gasDetectorState ) {
            if( accumulatedTimeAlarm >= BLINKING_TIME_GAS_ALARM ) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        } else if ( overTempDetectorState ) {
            if( accumulatedTimeAlarm >= BLINKING_TIME_OVER_TEMP_ALARM  ) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        }
    } else{
        alarmLed = OFF;
        gasDetectorState = OFF;
        overTempDetectorState = OFF;
        sirenPin.input();                                  
    }
}
//Funcion que maneja la entrada de claves para desactivar a la alarma
void alarmDeactivationUpdate()                     //SE CAMBIO
{
    if ( numberOfIncorrectCodes < 5 ) {
        if ( Buttons == code_sequence && !enterButton) {
            incorrectCodeLed = OFF;
        }
        if ( enterButton && !incorrectCodeLed && alarmState ) {
            Buttons_Presionados = Buttons.read() //NUEVO
            if ( areEqual() ) {
                alarmState = OFF;
                numberOfIncorrectCodes = 0;
            } else {
                incorrectCodeLed = ON;
                numberOfIncorrectCodes++;
            }
        }
    } else {
        systemBlockedLed = ON;
    }
}
//Funcion donde se maneja la comunicacion en serie con el teclado de la PC, aqui se mostrará el estado
// de los sensores y se podrá apagar la alarma introduciendo el codigo,
//Ademas de poder cambiar la clave que desactiva la alarma
//Todo dependendiendo de la tecla que el usuario presione
void uartTask()
{
    char receivedChar = '\0';
    char str[100];
    int stringLength;
    if( uartUsb.readable() ) {
        uartUsb.read( &receivedChar, 1 );
        switch (receivedChar) {
        case '1':
            if ( alarmState ) {
                uartUsb.write( "The alarm is activated\r\n", 24);
            } else {
                uartUsb.write( "The alarm is not activated\r\n", 28);
            }
            break;

        case '2':
            if ( !mq2 ) {
                uartUsb.write( "Gas is being detected\r\n", 22);
            } else {
                uartUsb.write( "Gas is not being detected\r\n", 27);
            }
            break;

        case '3':
            if ( overTempDetector ) {
                uartUsb.write( "Temperature is above the maximum level\r\n", 40);
            } else {
                uartUsb.write( "Temperature is below the maximum level\r\n", 40);
            }
            break;
            
        case '4':
            uartUsb.write( "Please enter the code sequence.\r\n", 33 );
            uartUsb.write( "First enter 'A', then 'B', then 'C', and ", 41 ); 
            uartUsb.write( "finally 'D' button\r\n", 20 );
            uartUsb.write( "In each case type 1 for pressed or 0 for ", 41 );
            uartUsb.write( "not pressed\r\n", 13 );
            uartUsb.write( "For example, for 'A' = pressed, ", 32 );
            uartUsb.write( "'B' = pressed, 'C' = not pressed, ", 34);
            uartUsb.write( "'D' = not pressed, enter '1', then '1', ", 40 );
            uartUsb.write( "then '0', and finally '0'\r\n\r\n", 29 );

            incorrectCode = false;
//1  1  0  0

            for ( buttonBeingCompared = 0;                 //CAMBIAR ESTO
                  buttonBeingCompared < NUMBER_OF_KEYS;
                  buttonBeingCompared++) {

                uartUsb.read( &receivedChar, 1 );
                uartUsb.write( "*", 1 );
                Val_leido_teclas = receivedChar << MASK_VAL_LEIDO*aux_mask;
                aux_mask--;
                }
                if(val_leido_teclas != code_sequence){
                    incorrectCode = true;
                } else {
                    incorrectCode = false;
                }
            }

            if ( incorrectCode == false ) {
                uartUsb.write( "\r\nThe code is correct\r\n\r\n", 25 );
                alarmState = OFF;
                incorrectCodeLed = OFF;
                numberOfIncorrectCodes = 0;
            } else {
                uartUsb.write( "\r\nThe code is incorrect\r\n\r\n", 27 );
                incorrectCodeLed = ON;
                numberOfIncorrectCodes++;
            }                
            break;

        case '5':
            uartUsb.write( "Please enter new code sequence\r\n", 32 );
            uartUsb.write( "First enter 'A', then 'B', then 'C', and ", 41 );
            uartUsb.write( "finally 'D' button\r\n", 20 );
            uartUsb.write( "In each case type 1 for pressed or 0 for not ", 45 );
            uartUsb.write( "pressed\r\n", 9 );
            uartUsb.write( "For example, for 'A' = pressed, 'B' = pressed,", 46 );
            uartUsb.write( " 'C' = not pressed,", 19 );
            uartUsb.write( "'D' = not pressed, enter '1', then '1', ", 40 );
            uartUsb.write( "then '0', and finally '0'\r\n\r\n", 29 );

            for ( buttonBeingCompared = 0; 
                  buttonBeingCompared < NUMBER_OF_KEYS; 
                  buttonBeingCompared++) {

                uartUsb.read( &receivedChar, 1 );
                uartUsb.write( "*", 1 );
                if ( receivedChar == '1' ) {
                    code_sequence = 1 << MASK_VAL_LEIDO*aux_mask;
                aux_mask--;
                } else if ( receivedChar == '0' ) {
                    code_sequence = 0 << MASK_VAL_LEIDO*aux_mask;
                aux_mask--;
                }
            }

            uartUsb.write( "\r\nNew code generated\r\n\r\n", 24 );
            break;
 
        case 'p':
        case 'P':
            potentiometerReading = potentiometer.read();
            sprintf ( str, "Potentiometer: %.2f\r\n", potentiometerReading );
            stringLength = strlen(str);
            uartUsb.write( str, stringLength );
            break;

        case 'c':
        case 'C':
            sprintf ( str, "Temperature: %.2f \xB0 C\r\n", lm35TempC );
            stringLength = strlen(str);
            uartUsb.write( str, stringLength );
            break;

        case 'f':
        case 'F':
            sprintf ( str, "Temperature: %.2f \xB0 F\r\n", 
                celsiusToFahrenheit( lm35TempC ) );
            stringLength = strlen(str);
            uartUsb.write( str, stringLength );
            break;

        default:
            availableCommands();
            break;

        }
    }

//Funcion que muestra por consola todas las opciones que tiene el usuario

void availableCommands()
{
    uartUsb.write( "Available commands:\r\n", 21 );
    uartUsb.write( "Press '1' to get the alarm state\r\n", 34 );
    uartUsb.write( "Press '2' to get the gas detector state\r\n", 41 );
    uartUsb.write( "Press '3' to get the over temperature detector state\r\n", 54 );
    uartUsb.write( "Press '4' to enter the code sequence\r\n", 38 );
    uartUsb.write( "Press '5' to enter a new code\r\n", 31 );
    uartUsb.write( "Press 'P' or 'p' to get potentiometer reading\r\n", 47 );
    uartUsb.write( "Press 'f' or 'F' to get lm35 reading in Fahrenheit\r\n", 52 );
    uartUsb.write( "Press 'c' or 'C' to get lm35 reading in Celsius\r\n\r\n", 51 );
}
//Funcion para ver si el codigo entrante es la clave correcta para apagar la alarma
bool areEqual()              //SE CAMBIO
{
        if (Buttons_Presionados != code_sequence) {
            return false;
        }
    return true;
}
//Funciones para convertir el valor leido del potenciometro en unidades Grados o Farenheit
float analogReadingScaledWithTheLM35Formula( float analogReading )
{
    return ( analogReading * 3.3 / 0.01 );
}

float celsiusToFahrenheit( float tempInCelsiusDegrees )
{
    return ( tempInCelsiusDegrees * 9.0 / 5.0 + 32.0 );
}
