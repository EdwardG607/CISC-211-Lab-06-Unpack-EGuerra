/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project. It is intended to
    be used as the starting point for CISC-211 Curiosity Nano Board
    programming projects. After initializing the hardware, it will
    go into a 0.5s loop that calls an assembly function specified in a separate
    .s file. It will print the iteration number and the result of the assembly 
    function call to the serial port.
    As an added bonus, it will toggle the LED on each iteration
    to provide feedback that the code is actually running.
  
    NOTE: PC serial port MUST be set to 115200 rate.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/


// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdio.h>
#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <string.h>
#include <malloc.h>
#include <inttypes.h>   // required to print out pointers using PRIXPTR macro
#include "definitions.h"                // SYS function prototypes

/* RTC Time period match values for input clock of 1 KHz */
#define PERIOD_500MS                            512
#define PERIOD_1S                               1024
#define PERIOD_2S                               2048
#define PERIOD_4S                               4096

#define MAX_PRINT_LEN 1000

static volatile bool isRTCExpired = false;
static volatile bool changeTempSamplingRate = false;
static volatile bool isUSARTTxComplete = true;
static uint8_t uartTxBuffer[MAX_PRINT_LEN] = {0};

// STUDENTS: you can try your own inputs by modifying the lines below
// the following array defines pairs of values (dividend, divisor) for test
// inputs to the assembly function
// tc stands for test case
static uint32_t tc[] = {
    0x80018002, // test case 0: -,-
    0x7EEE7FF0, // test case 1: +,+
    0x00000000, // test case 2: 0,0
    0x700EF00A, // test case 3: +,-
    0xAAAA5555, // test case 4: -,+
    0x00020003  // test case 5: +,+
};

static char * pass = "PASS";
static char * fail = "FAIL";

// VB COMMENT:
// The ARM calling convention permits the use of up to 4 registers, r0-r3
// to pass data into a function. Only one value can be returned to the 
// C caller. The assembly language routine stores the return value
// in r0. The C compiler will automatically use it as the function's return
// value.
//
// Function signature
// for this lab, the function takes one arg (amount), and returns the balance
extern void asmFunc(uint32_t);


extern uint32_t a_value;
extern uint32_t b_value;
// make the variable nameStrPtr available to the C program
extern uint32_t nameStrPtr;

// set this to 0 if using the simulator. BUT note that the simulator
// does NOT support the UART, so there's no way to print output.
#define USING_HW 1

#if USING_HW
static void rtcEventHandler (RTC_TIMER32_INT_MASK intCause, uintptr_t context)
{
    if (intCause & RTC_MODE0_INTENSET_CMP0_Msk)
    {            
        isRTCExpired    = true;
    }
}
static void usartDmaChannelHandler(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle)
{
    if (event == DMAC_TRANSFER_EVENT_COMPLETE)
    {
        isUSARTTxComplete = true;
    }
}
#endif

// print the mem addresses of the global vars at startup
// this is to help the students debug their code
static void printGlobalAddresses(void)
{
    // build the string to be sent out over the serial lines
    snprintf((char*)uartTxBuffer, MAX_PRINT_LEN,
            "========= GLOBAL VARIABLES MEMORY ADDRESS LIST\r\n"
            "global variable \"a_value\" stored at address:  0x%" PRIXPTR "\r\n"
            "global variable \"b_value\" stored at address:  0x%" PRIXPTR "\r\n"
            "========= END -- GLOBAL VARIABLES MEMORY ADDRESS LIST\r\n"
            "\r\n",
            (uintptr_t)(&a_value), 
            (uintptr_t)(&b_value)
            ); 
    isRTCExpired = false;
    isUSARTTxComplete = false;

#if USING_HW 
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, uartTxBuffer, \
        (const void *)&(SERCOM5_REGS->USART_INT.SERCOM_DATA), \
        strlen((const char*)uartTxBuffer));
    // spin here, waiting for timer and UART to complete
    while (isUSARTTxComplete == false); // wait for print to finish
    /* reset it for the next print */
    isUSARTTxComplete = false;
#endif
}

// print ints
static void printInts(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    // build the string to be sent out over the serial lines
    snprintf((char*)uartTxBuffer, MAX_PRINT_LEN,
            "========= DEBUG VARIABLE LIST\r\n"
            "\"a1\": %8lu\r\n"
            "\"a2\": %8lu\r\n"
            "\"a3\": %8lu\r\n"
            "\"a4\": %8lu\r\n"
            "========= END -- DEBUG VARIABLE ADDRESS LIST\r\n"
            "\r\n",
            a1,a2,a3,a4 
            ); 
    isRTCExpired = false;
    isUSARTTxComplete = false;

#if USING_HW 
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, uartTxBuffer, \
        (const void *)&(SERCOM5_REGS->USART_INT.SERCOM_DATA), \
        strlen((const char*)uartTxBuffer));
    // spin here, waiting for timer and UART to complete
    while (isUSARTTxComplete == false); // wait for print to finish
    /* reset it for the next print */
    isUSARTTxComplete = false;
#endif
}

// return failure count. A return value of 0 means everything passed.
static int testResult(int testNum, 
                      uint32_t packedValue, 
                      int32_t *passCount,
                      int32_t *failCount)
{
    // for this lab, each test case corresponds to a single pass or fail
    // But for future labs, one test case may have multiple pass/fail criteria
    // So I'm setting it up this way so it'll work for future labs, too --VB
    *failCount = 0;
    *passCount = 0;
    char *aCheck = "OOPS";
    char *bCheck = "OOPS";
    // static char *s2 = "OOPS";
    // static bool firstTime = true;
    uint32_t myA = 0;
    uint32_t myB = 0;
    // unpack A
    myA = packedValue>>16;
    uint32_t aSignBit = myA & 0x8000;
    if (aSignBit != 0)
    {
        myA = myA | 0xFFFF8000;
    }
    
    uint32_t bSignBit = packedValue & 0x8000;
    if (bSignBit != 0)
    {
        myB = packedValue | 0xFFFF8000;
    }
    else
    {
        myB = packedValue & 0x0000FFFF;
    }


    // Check a_value
    if(a_value == myA)
    {
        *passCount += 1;
        aCheck = pass;
    }
    else
    {
        *failCount += 1;
        aCheck = fail;
    }

    // Check b_value
    if(b_value == myB)
    {
        *passCount += 1;
        bCheck = pass;
    }
    else
    {
        *failCount += 1;
        bCheck = fail;
    }
          
    // build the string to be sent out over the serial lines
    snprintf((char*)uartTxBuffer, MAX_PRINT_LEN,
            "========= Test Number: %d =========\r\n"
            "a_value pass/fail:  %s\r\n"
            "b_value pass/fail:  %s\r\n"
            "debug values     expected       actual\r\n"
            "packed_value:..0x%08lx      (NA)\r\n"
            "a_value:.......0x%08lx    0x%08lx\r\n"
            "b_value:.......0x%08lx    0x%08lx\r\n"
            "\r\n",
            testNum,
            aCheck, 
            bCheck,
            packedValue,
            myA, a_value,
            myB, b_value
            );

#if USING_HW 
    // send the string over the serial bus using the UART
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, uartTxBuffer, \
        (const void *)&(SERCOM5_REGS->USART_INT.SERCOM_DATA), \
        strlen((const char*)uartTxBuffer));
#endif

    return *failCount;
    
}



// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************
int main ( void )
{
    
 
#if USING_HW
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, usartDmaChannelHandler, 0);
    RTC_Timer32CallbackRegister(rtcEventHandler, 0);
    RTC_Timer32Compare0Set(PERIOD_500MS);
    RTC_Timer32CounterSet(0);
    RTC_Timer32Start();
#else // using the simulator
    isRTCExpired = true;
    isUSARTTxComplete = true;
#endif //SIMULATOR
    
    printGlobalAddresses();

    // initialize all the variables
    int32_t passCount = 0;
    int32_t failCount = 0;
    int32_t totalPassCount = 0;
    int32_t totalFailCount = 0;
    // int32_t x1 = sizeof(tc);
    // int32_t x2 = sizeof(tc[0]);
    uint32_t x1 = sizeof(tc);
    uint32_t x2 = sizeof(tc[0]);
    uint32_t numTestCases = x1/x2;
    
    // set to true to execute for debug
    if(false)
    {
        printInts(x1,x2,numTestCases,0);
    }
            
    // Loop forever
    while ( true )
    {
        // Do the tests
        for (int testCase = 0; testCase < numTestCases; ++testCase)
        {
            // Toggle the LED to show we're running a new test case
            LED0_Toggle();

            // reset the state variables for the timer and serial port funcs
            isRTCExpired = false;
            isUSARTTxComplete = false;
            
            // STUDENTS:
            // !!!! THIS IS WHERE YOUR ASSEMBLY LANGUAGE PROGRAM GETS CALLED!!!!
            // Call our assembly function defined in file asmFunc.s
            uint32_t input = tc[testCase];
            asmFunc(input);
            
            // test the result and see if it passed
            failCount = testResult(testCase,input,
                                   &passCount,&failCount);
            totalPassCount = totalPassCount + passCount;
            totalFailCount = totalFailCount + failCount;

#if USING_HW
            // spin here until the UART has completed transmission
            // and the timer has expired
            //while  (false == isUSARTTxComplete ); 
            while ((isRTCExpired == false) ||
                   (isUSARTTxComplete == false));
#endif

        } // for each test case
        
        // When all test cases are complete, print the pass/fail statistics
        // Keep looping so that students can see code is still running.
        // We do this in case there are very few tests and they don't have the
        // terminal hooked up in time.
        uint32_t idleCount = 1;
        uint32_t totalTests = totalPassCount + totalFailCount;
#if USING_HW 
        bool firstTime = true;
#endif
        while(true)      // post-test forever loop
        {
            isRTCExpired = false;
            isUSARTTxComplete = false;
            
            uint32_t numPointsMax = 40;
            uint32_t pointsScored = numPointsMax * totalPassCount / totalTests;
            
            snprintf((char*)uartTxBuffer, MAX_PRINT_LEN,
                    "========= %s: Lab 6 Unpack: ALL TESTS COMPLETE, Idle Cycle Number: %ld\r\n"
                    "Summary of tests: %ld of %ld tests passed\r\n"
                    "Final score for test cases: %ld of %ld points\r\n"
                    "\r\n",
                    (char *) nameStrPtr, idleCount, 
                    totalPassCount, totalTests,
                    pointsScored, numPointsMax); 


#if USING_HW 
            DMAC_ChannelTransfer(DMAC_CHANNEL_0, uartTxBuffer, \
                (const void *)&(SERCOM5_REGS->USART_INT.SERCOM_DATA), \
                strlen((const char*)uartTxBuffer));
            // spin here, waiting for timer and UART to complete
            LED0_Toggle();
            ++idleCount;

            while ((isRTCExpired == false) ||
                   (isUSARTTxComplete == false));

            // STUDENTS:
            // UNCOMMENT THE NEXT LINE IF YOU WANT YOUR CODE TO STOP AFTER THE LAST TEST CASE!
            // exit(0);
            
            // slow down the blink rate after the tests have been executed
            if (firstTime == true)
            {
                firstTime = false; // only execute this section once
                RTC_Timer32Compare0Set(PERIOD_4S); // set blink period to 4sec
                RTC_Timer32CounterSet(0); // reset timer to start at 0
            }
#endif
        } // end - post-test forever loop
        
        // Should never get here...
        break;
    } // while ...
            
    /* Execution should not come here during normal operation */
    return ( EXIT_FAILURE );
}
/*******************************************************************************
 End of File
*/

