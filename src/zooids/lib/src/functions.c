#include "functions.h"
#include "colors.h"
#include "bittybuzz/bbzvm.h"
#include <stdlib.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

#define CLAMP(x, min, max) ((x) >= (min) ? ((x) <= (max) ? (x) : (max)) : (min))

/* Private variables ---------------------------------------------------------*/

/* Timer Output Compare Configuration Structure declaration */

/* Peripherals initialized states */

/* Interrupt flags */
volatile bool touchChanged = false;
volatile bool radioEvent = false;
volatile bool reached = false; // used by position control
bool atDestination = false; // true if stopped moving
uint8_t atDestCounter = 0;

bool positionSent = true;
uint8_t currentTouch = 0;

Target currentGoal = {500, 500, 90, true, true};

Motor motorValues = {0, 0, 1.0f, 22, 40, 60};
//Motor motorValues = {0, 0, 1.0f, 15, 25,40};

bool isPositionControl = true;
float targetAngle = 0.0f;
volatile CHARGING_STATE_t chargingStatus = DISCONNECTED;

volatile message_tx_t         message_tx;
volatile message_tx_success_t message_tx_success;
volatile message_rx_t         message_rx;

Position msgp;

extern bool isListening;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/*============================================================================
Name    :   upateRobot
------------------------------------------------------------------------------
Purpose :   updates the position and orientation of the robot and prepare the
            message to be sent
Input   :
Output  :
Return	:
Notes   :
============================================================================*/
void initRobot() {
  HAL_Init();
  SystemClock_Config();

  initRGBLed();

  initMotors();

  if (!initSensors())
    Error_Handler();

  if (!initRadio())
    Error_Handler();

  initPhotoDiodes();

  while(calibrate(&motorValues) == false);
  chargingStatus = (HAL_GPIO_ReadPin(CHARGING_STATUS_GPIO_PORT, CHARGING_STATUS_PIN) == LOW) ? CHARGING : CHARGED;
}

/*============================================================================
Name    :   upateRobot
------------------------------------------------------------------------------
Purpose :   updates the position and orientation of the robot and prepare the
            message to be sent
Input   :
Output  :
Return	:
Notes   :
============================================================================*/
void updateRobot() {
  if(atDestCounter>=3){
    atDestination = true;
    setMotor1(0);
    setMotor2(0);
    //setRedLed(5); //UNCOMMENT TO GET A VISUAL FEEDBACK WHEN THE ROBOT REACHS ITS DESTINATION
    setGreenLed(0);
  }
  if (updateRobotPosition()) {
    setBlueLed(0);
    if (isPositionControl)
    {
      positionControl(currentGoal.x, currentGoal.y, currentGoal.angle, &motorValues, (bool*)&reached, false, currentGoal.finalGoal, currentGoal.ignoreOrientation);
      if(reached){
        setMotor1(0);
        setMotor2(0);
        atDestCounter++;
      }
      else{
        setRedLed(0);
        atDestination = false;
        atDestCounter = 0;
        minimumc(&motorValues.motor1, motorValues.minVelocity);
        minimumc(&motorValues.motor2, motorValues.minVelocity);
        maximumc(&motorValues.motor1, motorValues.maxVelocity);
        maximumc(&motorValues.motor2, motorValues.maxVelocity);

        setMotor1(motorValues.motor1);
        setMotor2(motorValues.motor2);
      }
    }

    prepareMessageToSend(getRobotPosition(), getRobotAngle(), &currentTouch, &atDestination, getBatteryLevel());
  }
  if(isBlinded())
  {
        atDestination = true;
        setBlueLed(5);
        setMotor1(0);
        setMotor2(0);
  }
}

/*============================================================================
Name    :   sleep
------------------------------------------------------------------------------
Purpose :   Sleeping procedure (TODO)
Input   :
Output  :
Return	:
Notes   :
============================================================================*/
void sleep() {
  SleepMode_Measure();
}

void delay(uint16_t ms) {
  /**/delayMicroseconds(ms * 1000);
  /*/for(volatile uint16_t j = 0; j < ms; ++j) {
    for(volatile uint16_t i = 0; i < 700; ++i);
  }//*/
}

/*============================================================================
Name    :   checkRadio
------------------------------------------------------------------------------
Purpose :   acquires new incoming data from the radio
Input   :
Output  :
Return	:
Notes   :
============================================================================*/
void checkRadio() {
  if (radioEvent) {
    bool tx, fail, rx;
    whatHappened(&tx, &fail, &rx);
    clearInterruptFlag(rx, tx, fail);
    radioEvent = false;
    if (rx) {
      handleIncomingRadioMessage();
    }
    if (fail) {
      flush_tx();
    }
  }
}

/*============================================================================
Name    :   handleIncomingMessage
------------------------------------------------------------------------------
Purpose :   acquires new incoming data from the radio
Input   :
Output  :
Return	:
Notes   :
============================================================================*/
void handleIncomingRadioMessage() {
  uint8_t payloadSize = getDynamicPayloadSize();

  Message msg;
  memset(&msg, 0, sizeof(msg));
  readRadio((uint8_t *)&msg, PAYLOAD_MAX_SIZE);
  if (payloadSize > PAYLOAD_MAX_SIZE) {
    flush_rx();
  }
  if (msg.header.id == RECEIVER_ID) {
    switch (msg.header.type) {
    case TYPE_UPDATE:
      break;
    case TYPE_BBZ_MESSAGE:
      if (message_rx != NULL) {
        Position* rbtp = getRobotPosition();
        memcpy_fast(&msgp, msg.payload+1, sizeof(Position));
        float dist = qfp_fsqrt(qfp_uint2float((uint32_t)(msgp.x - rbtp->x)*(uint32_t)(msgp.x - rbtp->x) + (uint32_t)(msgp.y - rbtp->y)*(uint32_t)(msgp.y - rbtp->y)));
        float azimuth = qfp_fatan2(qfp_int2float(msgp.y - rbtp->y), qfp_int2float(msgp.x - rbtp->x));
        message_rx(&msg, dist, azimuth);
      }
      break;
    case TYPE_REBOOT_ROBOT:
      Reboot();
      break;
    default:
      break;
    }
  }
}

void handleOutgoingRadioMessage(void) {
  if (message_tx == NULL) return;
  Message* msg = message_tx();
  while (msg != NULL) {
    resetCommunicationWatchdog();
    stopListening();
    writeRadio((uint8_t*)msg, sizeof(Header) + sizeof(uint8_t) + sizeof(Position) + 9);
    HAL_Delay(1);
    startListening();

    message_tx_success();
    msg = message_tx();
  }
}

/*============================================================================
Name    :   prepareMessageToSend
------------------------------------------------------------------------------
Purpose :   prepares the new position message and stacks it as
            a new ack packet
Input   :   *position : pointer to the robot's current position
            *orientation : pointer to the robot's current orientation
            which pin has triggered the interrupt
Output  :
Return	:
Notes   :
============================================================================*/
void prepareMessageToSend(Position *position, float *orientation, uint8_t *touch, bool *destination, uint16_t batteryLevel) {
  //setRedLed(0);
  int16_t tmpOrientation = 0;
  Message msg;
  msg.header.id = getRobotId();
  msg.header.type = TYPE_STATUS;

  if (position && orientation && touch) {
    tmpOrientation = (int16_t)(*orientation * 100.0f);

    memcpy_fast(msg.payload, (uint8_t *)position, sizeof(*position));
    memcpy_fast(msg.payload + sizeof(*position), (uint8_t *)&tmpOrientation, sizeof(tmpOrientation));
    memcpy_fast(msg.payload + sizeof(*position) + sizeof(tmpOrientation), touch, sizeof(*touch));
    memcpy_fast(msg.payload + sizeof(*position) + sizeof(tmpOrientation) + sizeof(*touch), destination, sizeof(*destination));
    //            memcpy_fast(msg.payload+sizeof(*position)+sizeof(*tmpOrientation)+sizeof(*touch), &photoDiodesPositions[0], 2*sizeof(Position));
    memcpy_fast(msg.payload + sizeof(*position) + sizeof(tmpOrientation) + sizeof(*touch) + sizeof(*destination), &batteryLevel, sizeof(batteryLevel));
    writeAckPayload(0, (uint8_t *)&msg, sizeof(Header) + sizeof(*position) + sizeof(tmpOrientation) + sizeof(*touch) + sizeof(*destination) + sizeof(batteryLevel)); // + 2*sizeof(Position));

  }
}

/*============================================================================
Name    :   checkTouch
------------------------------------------------------------------------------
Purpose :   acquires touch information when a new event happened and then
            stores it
Input   :
Output  :
Return	:
Notes   :
============================================================================*/
void checkTouch() {
  // if a new touch happened
  if (touchChanged) {
    currentTouch = readQTKeyStatus();
    if (HAL_GPIO_ReadPin(TOUCH_CHANGE_GPIO_PORT, TOUCH_CHANGE_PIN) == HIGH) {
      touchChanged = false;
    }
  }
}

/*============================================================================
Name    :   switchToChargingMode
------------------------------------------------------------------------------
Purpose :   put the robot into sleep mode while charging
Input   :
Output  :
Return	:
Notes   :
============================================================================*/
void switchToChargingMode() {
  setGreenLed(0);
  setBlueLed(0);
  powerDown();
  setMotor1(0);
  setMotor2(0);

  while (chargingStatus == CHARGING) {
    setRedLed(5);
    //glowRedLed();
  }

  powerUp();
}

void __errno() {}

/*============================================================================
Name    :   HAL_GPIO_EXTI_Callback
------------------------------------------------------------------------------
Purpose :   Handles the GPIO interrupt routines
Input   :   GPIO_Pin
            which pin has triggered the interrupt
Output  :
Return	:
Notes   :
============================================================================*/
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  //Touch change IRQ
  if (GPIO_Pin & TOUCH_CHANGE_PIN)
    touchChanged = true;

  if (GPIO_Pin & PHOTODIODE_1_PIN)
    readPhotoDiode(0);
  //
  if (GPIO_Pin & PHOTODIODE_2_PIN)
    readPhotoDiode(1);

  //charger IRQ
  if ((GPIO_Pin & CHARGING_STATUS_PIN) > 0) {
    chargingStatus = (HAL_GPIO_ReadPin(CHARGING_STATUS_GPIO_PORT, CHARGING_STATUS_PIN) == LOW) ? CHARGING : CHARGED;
    if (chargingStatus == CHARGING)
      switchToChargingMode();
  }

  //nRF IRQ
  if ((GPIO_Pin & RADIO_IRQ_PIN) > 0) {
    radioEvent = true;

    if (isListening) {
      bool tx, fail, rx;
      whatHappened(&tx, &fail, &rx);
      if (rx) {
        handleIncomingRadioMessage();
        clearInterruptFlag(true, false, false);
      }
    }
  }

  //IMU IRQ
  //    if(GPIO_Pin && IMU_INT_PIN);
}