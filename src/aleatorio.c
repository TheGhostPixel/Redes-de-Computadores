// RCOM 2026/2027
//
// Link layer protocol implementation

#define _POSIX_SOURCE 1 // POSIX compliant source (must come before the includes)

#include "link_layer.h"
#include "serial_port.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/// MISC
#define BUF_SIZE 256

// Supervision frames: FLAG | A | C | BCC1 (= A ^ C) | FLAG
// SET: sent by the Transmitter (A = 0x03, C = 0x03, BCC1 = 0x03 ^ 0x03 = 0x00)
static const unsigned char SET_FRAME[5] = {0x7E, 0x03, 0x03, 0x00, 0x7E};
// UA: sent by the Receiver as the reply (A = 0x03, C = 0x07, BCC1 = 0x03 ^ 0x07 = 0x04)
static const unsigned char UA_FRAME[5] = {0x7E, 0x03, 0x07, 0x04, 0x7E};

// States of the supervision frame state machine
typedef enum { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP } State;

// Set to 1 while waiting for a frame, cleared to 0 by the alarm handler on timeout
static volatile sig_atomic_t alarmEnabled = 0;

// Alarm handler: called when SIGALRM is triggered
static void alarmHandler(int sig) { alarmEnabled = 0; }

// Reads bytes from the serial port and, using a state machine, waits for the
// supervision frame `expected` (5 bytes).
// Returns 0 if the frame was received, -1 if the alarm fired first.
static int waitFrame(const unsigned char *expected)
{
    State state = START;
    unsigned char byte;

    while (state != STOP && alarmEnabled)
    {
        // Read one byte (a read interrupted by the alarm returns -1)
        if (readByteSerialPort(&byte) <= 0)
            continue;

        printf("byte = 0x%02X\n", byte); // debug

        switch (state)
        {
        case START:
            // Waiting for the initial FLAG
            if (byte == expected[0]) state = FLAG_RCV;
            break;
        case FLAG_RCV:
            // FLAG received, expecting the A field
            if (byte == expected[1]) state = A_RCV;
            else if (byte != expected[0]) state = START;
            break;
        case A_RCV:
            // A received, expecting the C field
            if (byte == expected[2]) state = C_RCV;
            else if (byte == expected[0]) state = FLAG_RCV;
            else state = START;
            break;
        case C_RCV:
            // C received, expecting BCC1 (A ^ C)
            if (byte == expected[3]) state = BCC_OK;
            else if (byte == expected[0]) state = FLAG_RCV;
            else state = START;
            break;
        case BCC_OK:
            // BCC1 is correct, expecting the final FLAG
            if (byte == expected[4]) state = STOP;
            else state = START;
            break;
        default:
            break;
        }
    }
    return state == STOP ? 0 : -1;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llOpenTx(LinkLayer llParameters)
{
    // Install the alarm handler
    struct sigaction sa = {0};
    sa.sa_handler = alarmHandler; // no SA_RESTART: the alarm interrupts a blocked read
    if (sigaction(SIGALRM, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    // Open and configure the serial port
    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    // Send SET and retransmit it on timeout, up to nRetransmissions attempts
    for (int attempt = 1; attempt <= llParameters.nRetransmissions; attempt++)
    {
        // Send SET
        int bytes = writeBytesSerialPort(SET_FRAME, 5);
        printf("SET sent (%d bytes), attempt #%d\n", bytes, attempt);

        // Wait for UA until the timeout expires
        alarmEnabled = 1;
        alarm(llParameters.timeout);
        int r = waitFrame(UA_FRAME);
        alarm(0); // disable any pending alarm

        if (r == 0)
        {
            printf("UA received. Connection established.\n");
            return 0;
        }
    }

    // All attempts failed
    printf("No UA received. Connection failed.\n");
    closeSerialPort();
    return -1;
}

int llOpenRx(LinkLayer llParameters)
{
    // Open and configure the serial port
    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);
    printf("Waiting for SET...\n");

    // Wait for SET (no alarm: waits indefinitely)
    alarmEnabled = 1;
    if (waitFrame(SET_FRAME) < 0)
    {
        closeSerialPort();
        return -1;
    }

    // Reply with UA
    int bytes = writeBytesSerialPort(UA_FRAME, 5);
    printf("SET received. UA sent (%d bytes). Connection established.\n", bytes);

    return 0;
}

////////////////////////////////////////////////
// LLSEND
////////////////////////////////////////////////
int llSend(const unsigned char *buf, int bufSize)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////
int llReceive(unsigned char *packet)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llCloseTx()
{
    // TODO: Implement this function

    return 0;
}

int llCloseRx()
{
    // TODO: Implement this function

    return 0;
}