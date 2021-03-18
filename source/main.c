/*	Author: Chandler Mahkorn
 *  Partner(s) Name: 
 *	Lab Section: 21
 *	Assignment: Lab 14  Exercise 1
 *	Exercise Description: [optional - include for your own benefit]
 *
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 */
#include <avr/io.h>
#include <../source/scheduler.h>
#include <../source/timer.h>
#include <stdlib.h>
#ifdef _SIMULATE_
#include "simAVRHeader.h"
#endif

const unsigned char GCD = 1;
const unsigned short UP_THRESH = 250;
const unsigned short DOWN_THRESH = 750;

// Left paddle
const unsigned char LEFT_X = 0x80;
unsigned char leftPos = 0;

// Right paddle
const unsigned char RIGHT_X = 0x01;
unsigned char rightPos = 0;

// Ball
unsigned char up = 0;
unsigned char down = 0;
unsigned char ballTryX = 0x08;
unsigned char ballX = 0x08;
unsigned char ballTryY = 2;
unsigned char ballY = 2;
double ballSpeed = 0;

// General purpose
unsigned char reset = 1;
unsigned char useAI = 0;

// Pins on PORTA are used as input for A2D conversion
// The default channel is 0 (PA0)
// The value of pinNum determines the pin on PORTA used for A2D conversion
// Valid values range between 0 and 7, where the value represents the desired pin for A2D conversion
void Set_A2D_Pin(unsigned char pinNum) {
    ADMUX = (pinNum <= 0x07) ? pinNum : ADMUX;
    // Allow channel to stabilize
    static unsigned char i = 0;
    for ( i=0; i<15; i++ ) { asm("nop"); } 
}

void A2D_init() {
      ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
	// ADEN: Enables analog-to-digital conversion
	// ADSC: Starts analog-to-digital conversion
	// ADATE: Enables auto-triggering, allowing for constant
	//	    analog to digital conversions.
}

void transmit_data(unsigned char data, unsigned char reg) {
    // The data needs to be modified to coincide with the matrix
    // Upper half is swapped with the lower half, then the nibbles are mirrored
    // Outer
    data = (data & 0xF0) >> 4 | (data & 0x0F) << 4;
    // Outer-Middle
    data = (data & 0xCC) >> 2 | (data & 0x33) << 2;
    // Middle
    data = (data & 0xAA) >> 1 | (data & 0x55) << 1;

    int i;
    if (reg == 0) {
        for (i = 0; i < 8 ; ++i) {
        // Sets SRCLR to 1 allowing data to be set
        // Also clears SRCLK in preparation of sending data
        PORTC = 0x08;
            // set SER = next bit of data to be sent.
            PORTC |= ((data >> i) & 0x01);
            // set SRCLK = 1. Rising edge shifts next bit of data into the shift register
            PORTC |= 0x02;  
        }
        // set RCLK = 1. Rising edge copies data from “Shift” register to “Storage” register
        PORTC |= 0x04;
    }

    else if (reg == 1) {
        for (i = 0; i < 8 ; ++i) {
        // Sets SRCLR to 1 allowing data to be set
        // Also clears SRCLK in preparation of sending data
        PORTD = 0x08;
            // set SER = next bit of data to be sent.
            PORTD |= ((data >> i) & 0x01);
            // set SRCLK = 1. Rising edge shifts next bit of data into the shift register
            PORTD |= 0x02;  
        }
        // set RCLK = 1. Rising edge copies data from “Shift” register to “Storage” register
        PORTD |= 0x04;
    }
}

unsigned short get_seed() {
    unsigned short seed = 0;
    unsigned short bit = 0;

    for (int i = 0; i < 16; i++) {
        bit = (ADC & 0x10) >> 4;
        seed = (seed << 1 || bit);
    }

    return seed;
}

const char ballArray[] = {0x1E, 0x1D, 0x1B, 0x17, 0x0F};
const char paddleArray[] = {0x1C, 0x19, 0x13, 0x07};
enum MatrixStates {IDLE, BALL, LPADDLE, RPADDLE};
int MatrixTick(int state) {
    // LED pattern - 0: LED off; 1: LED on
    // Row(s) displaying pattern.
    // 0: display pattern on col
    // 1: do NOT display pattern on col

    // Transitions
    switch(state) {
        case IDLE:
            state = BALL;
            break;

        case BALL:
            state = LPADDLE;
            break;

        case LPADDLE:
            state = RPADDLE;
            break;

        case RPADDLE:
            state = BALL;
            break;
    }

	// State actions
	switch(state) {
        case IDLE:
            // PORTC
            transmit_data(0x00, 0);
            // PORTD
            transmit_data(0x00, 1);
            break;

        case BALL:
            // PORTC
            transmit_data(ballX, 0);
            // PORTD
            transmit_data(ballArray[ballY], 1);
            break;

        case LPADDLE:
            // PORTC
            transmit_data(LEFT_X, 0);
            // PORTD
            transmit_data(paddleArray[leftPos], 1);
            break;

        case RPADDLE:
            // PORTC
            transmit_data(RIGHT_X, 0);
            // PORTD
            transmit_data(paddleArray[rightPos], 1);
            break;

		default:
	        break;
	}

	return state;
}

enum LeftStates {READ, MOVE, HOLD};
int LeftTick(int state) {
    unsigned short upDown = (ADC << 2) >> 2;

    unsigned char up = 0;
    unsigned char down = 0;

    // Transitions
    switch(state) {
        case READ:
        
            if (upDown < UP_THRESH) { up = 1; state = MOVE; }
            if (upDown > DOWN_THRESH) { down = 1; state = MOVE; }

            if (up == 0 && down == 0) { state = READ; }
            break;

        case MOVE:
        case HOLD:
            if (upDown < UP_THRESH) { up = 1; state = MOVE; }
            if (upDown > DOWN_THRESH) { down = 1; state = MOVE; }

            if (up != 0 || down != 0) { state = HOLD; }
            else if (up == 0 && down == 0) { state = READ; }
            break;

        default:
            state = READ;
            break;
    }

    // State actions
    switch(state) {
        case MOVE:
            // UP
            if (up) {
                if (leftPos < 3)
                    leftPos++;
            }

            // DOWN
            if (down) {
                if (leftPos > 0)
                    leftPos--;
            }
            break;

        default:
            break;
    }

    return state;
}

void ballUpDown() {
    if (up == 1) {
        if (ballY > 0)
            ballTryY--;
        else {
            up = 0;
            down = 1;
            ballTryY++;
        }
    }
    else if (down == 1) {
        if (ballY < 4)
            ballTryY++;
        else {
            up = 1;
            down = 0;
            ballTryY--;
        }
    }
}

void ballRand(int state) {
// Determine if the ball should spin in the oppposite direction
// Or go straight ahead
    if (up != 0 || down != 0) {
        // Slow down the ball a bit
        ballSpeed += rand() % 5;

        unsigned short val = rand() % 10;
        if (val < 7) {
            ballY = ballTryY;
        }
        else {
            up = 0;
            down = 0;
        }
    }

// Set the ball up after it's bounced from going straight
    else if (up == 0 && down == 0) {
        // Speed up the ball a bit
        ballSpeed -= (rand() % 3) * 0.5;

        unsigned short val = rand() % 10;
        if (val < 5) {
            up = 1;
            down = 0;
        }
        else {
            up = 0;
            down = 1;
        }

        ballUpDown();
        ballY = ballTryY;

        // Left
        if (state == 1) {
            ballX = ballX << 1;
        }
        // Right
        else if (state == 2) {
            ballX = ballX >> 1;
        }
    }
}

enum BallStates {INIT, LEFT, RIGHT};
int BallTick(int state) {
    // Transitions
    switch(state) {
        case INIT:
            // Initial speed
            ballSpeed = 250;

            // Initial up/down direction
            unsigned short dir = rand();
            if ((dir % 2) == 0 ) { up = 1; }
            else { down = 1; }

            // Initial left/right direction
            dir = rand();
            if ((dir % 2) == 0 ) { state = LEFT; }
            else { state = LEFT; }
            break;

        case LEFT:
            // Walls
            if (ballTryX < 0x80) { 
                state = LEFT; 
                ballX = ballTryX; 
                ballY = ballTryY; 
            }
            else { 
                state = RIGHT; 
                ballX = ballTryX; 
                ballY = ballTryY; 
            }

            // Speed up the ball after hitting a wall
            ballSpeed -= (rand() % 2) * 0.5;

            // Left paddle
            if (ballTryX >= 0x80) {
                if (ballTryY == leftPos || ballTryY == (leftPos + 1)) {
                    state = RIGHT;

                    ballX = ballX >> 1;
                    ballRand(state);
                }
            }

            if (ballSpeed <= 20)
                ballSpeed = 40;   
            break;

        case RIGHT:
            // Walls
            if (ballTryX > 0x01) { 
                state = RIGHT; 
                ballX = ballTryX; 
                ballY = ballTryY; 
            }
            else { 
                state = LEFT; 
                ballX = ballTryX; 
                ballY = ballTryY; 
            }

            // Speed up the ball after hitting a wall
            ballSpeed -= (rand() % 2) * 0.5;

            // Right paddle
            if (ballTryX <= 0x01) {
                if (ballTryY == rightPos || ballTryY == (rightPos + 1)) {
                    state = LEFT;

                    ballX = ballX << 1;
                    ballRand(state);
                }
            }

            if (ballSpeed <= 20)
                ballSpeed = 40;
            break;

        default:
            state = INIT;
            break;
    }

    // State actions
    switch(state) {
        case LEFT:
            ballTryX = ballX << 1;
            ballUpDown();
            break;

        case RIGHT:
            ballTryX = ballX >> 1;
            ballUpDown();
            break;

        default:
            break;
    }

    return state;
}

enum AIStates {AIREAD, AIMOVE, AIHOLD};
int AITick(int state) {
    if (useAI == 1) {
        
    }

    return state;
}

enum RightStates {READ2, MOVE2, HOLD2};
int RightTick(int state) {
    if (useAI == 0) {
        unsigned char butUp = (~PINA & 0x20);
        unsigned char butDown = (~PINA & 0x40);
        unsigned char up2 = 0;
        unsigned char down2 = 0;

        // Transitions
        switch(state) {
            case READ2:
                if (butUp == 0x20) { up2 = 1; state = MOVE2; }
                else if (butDown == 0x40) { down2 = 1; state = MOVE2; }

                if (up2 == 0 && down2 == 0) { state = READ2; }
                break;

            case MOVE2:
            case HOLD2:
                if (butUp == 0x20) { up2 = 1; state = MOVE2; }
                else if (butDown == 0x40) { down2 = 1; state = MOVE2; }

                if (up2 != 0 || down2 != 0) { state = HOLD2; }
                else if (up2 == 0 && down2 == 0) { state = READ2; }
                break;

            default:
                state = READ2;
                break;
        }

        // State actions
        switch(state) {
            case MOVE2:
                // UP
                if (up2) {
                    if (rightPos < 3)
                        rightPos++;
                }

                // DOWN
                if (down2) {
                    if (rightPos > 0)
                        rightPos--;
                }
                break;

            default:
                break;
        }
    }
    return state;
}

int main(void) {
    DDRA = 0x00;
    DDRB = 0xFF;
    DDRC = 0xFF;
    DDRD = 0xFF;

    PORTA = 0xFF;
    PORTB = 0x00;
    PORTC = 0x00;
    PORTD = 0x00;

    A2D_init();

    // Seed RNG based on photoresistor's value
    Set_A2D_Pin(2);
    srand(get_seed());

    // Joystick up / down only
    Set_A2D_Pin(1);

    // Tasks
    static task task0, task1, task2, task3, task4;
    task *tasks[] = {&task0, &task1, &task2, &task3, &task4};
    const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

    const char start = 0;

    // Task 0 (Matrix Display)
    task0.state = start;
    task0.period = 5;
    task0.elapsedTime = task0.period;
    task0.TickFct = &MatrixTick;

    // Task 1 (Ball)
    task1.state = start;
    task1.period = ballSpeed;
    task1.elapsedTime = task1.period;
    task1.TickFct = &BallTick;

    // Task 2 (Joystick read) -> Player 1
    task2.state = start;
    task2.period = 30;
    task2.elapsedTime = task2.period;
    task2.TickFct = &LeftTick;

    // Task 3 (AI)
    task3.state = start;
    task3.period = 30;
    task3.elapsedTime = task3.period;
    task3.TickFct = &AITick;

    // Task 4 (Button read) -> Player 2 
    task4.state = start;
    task4.period = 30;
    task4.elapsedTime = task4.period;
    task4.TickFct = &RightTick;

    TimerSet(GCD);
    TimerOn();

    while (1) {
        unsigned char butAI = (~PINA & 0x10);
        unsigned char but2P = (~PINA & 0x08);
        unsigned char butReset = (~PINA & 0x01);

        if (reset == 1) {
            if (butAI == 16) {
                useAI = 1;
                reset = 0;
            }
            else if (but2P == 8) {
                useAI = 0;
                reset = 0;
            }
        }
        else {
            if (butReset == 1) {
                reset = 1;
                for (int i = 0; i < numTasks; i++) {
                    tasks[i]->elapsedTime = 0;
                    tasks[i]->state = start;
                }

                // PORTC
                transmit_data(0x00, 0);
                // PORTD
                transmit_data(0x00, 1);
                ballX = 0x08;
                ballY = 2;
            }
            else {
                task1.period = ballSpeed;

                for (int i = 0; i < numTasks; i++) {
                    if (tasks[i]->elapsedTime >= tasks[i]->period) {
                        tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
                        tasks[i]->elapsedTime = 0;
                    }

                    tasks[i]->elapsedTime += GCD;
                }
            }
        }
        
        while(!TimerFlag);
        TimerFlag = 0;
    }
    return 0;
}
