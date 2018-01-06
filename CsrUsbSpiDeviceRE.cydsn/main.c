#include "project.h"
#include "csrspi.h"
#include "string.h"
#include "stdarg.h"
#include "stdio.h"

void debug(const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buff[256];
    vsnprintf(buff, 256, fmt, args);
    
    va_end(args);
    
    UART_PutString(buff);
    UART_PutString("\r\n");
}

#define BUFFER_SIZE	1024

uint8_t transmitBuffer[BUFFER_SIZE];
volatile size_t transmitLength;

uint8_t receiveBuffer[BUFFER_SIZE];
uint8_t *currentRecv;
volatile size_t receiveLength;

#define MODE_SPI 0
#define MODE_JTAG 0xFFFF

uint16_t g_nMode = MODE_SPI;

#define EP_IN 1
#define EP_OUT 2

enum {
    CMD_READ       = 0x0100,
    CMD_WRITE      = 0x0200,
    CMD_SETSPEED   = 0x0300,
    CMD_GETSTOPPED = 0x0400,
    CMD_GETSPEED   = 0x0500,
    CMD_UPDATE     = 0x0600,
    CMD_GETSERIAL  = 0x0700,
    CMD_GETVERSION = 0x0800,
    CMD_SETMODE    = 0x0900,
    CMD_SETBITS    = 0x0F00,
    CMD_BCCMDINIT  = 0x4000,
    CMD_BCCMD      = 0x4100,
};

uint16_t ReadLeftSize() {
    return receiveLength - (currentRecv - receiveBuffer);
}

uint16_t ReadWord() {
    if(ReadLeftSize() < 2)
        return 0;
    
    uint16_t r = (currentRecv[0] << 8) | currentRecv[1];
    currentRecv += 2;
    
    return r;
}

uint8_t ReadByte() {
    if(ReadLeftSize() < 2)
        return 0;

    uint8_t r = currentRecv[0];
    currentRecv += 1;
    
    return r;
}

void TransmitWord(uint16_t n) {
	if (transmitLength + 2 < BUFFER_SIZE) {
        transmitBuffer[transmitLength+1] = n & 0xFF;
        transmitBuffer[transmitLength] = (n >> 8) & 0xFF;
		transmitLength += 2;
	}
}

void TransmitDWord(uint32_t n) {
	TransmitWord((n >> 16) & 0xFFFF);
	TransmitWord(n & 0xFFFF);
}

void PerformInTransfer() {
	size_t nPacket;
    size_t nTransmitOffset = 0;
    
    debug("Will transfer? %d", transmitLength);
    
    if(transmitLength > 0)
        do {
        	nPacket = transmitLength - nTransmitOffset;
            debug("nPacket = %d", nPacket);
        	if (nPacket > 64)
        		nPacket = 64;
        	USB_LoadInEP(EP_IN, transmitBuffer + nTransmitOffset, nPacket);
            nTransmitOffset += nPacket;
        } while(nPacket == 64);
    
    transmitLength = 0;
}

void ClearReceive() {
    receiveLength = 0;
    currentRecv = receiveBuffer;
}

bool PerformOutTransfer() {
    if(USB_OUT_BUFFER_FULL == USB_GetEPState(EP_OUT)) {
        uint16 len = USB_GetEPCount(EP_OUT);
        if(len + receiveLength > BUFFER_SIZE) {
            //overflow
            receiveLength = 0;
            return false;
        }
        
        receiveLength += USB_ReadOutEP(EP_OUT, receiveBuffer + receiveLength, len);
        
        if(len < 64) {
            currentRecv = receiveBuffer;
            return true;
        }
    }
    return false;
}

int CmdRead(unsigned short nAddress, unsigned short nLength);
int CmdWrite(unsigned short nAddress, unsigned short nLength);
int CmdSetSpeed(unsigned short nSpeed);
int CmdGetStopped();
int CmdGetSpeed();
int CmdUpdate();
int CmdGetSerial();
int CmdGetVersion();
int CmdSetMode(unsigned short nMode);
int CmdSetBits(unsigned short nWhich, unsigned short nValue);
void CmdBcCmdInit(unsigned short nA, unsigned short nB);
int CmdBcCmd(unsigned short nLength);

int main(void)
{
    LED_Write(1);
    
    CyGlobalIntEnable; /* Enable global interrupts. */
    
    uint16_t nCommand;
    uint16_t nArgs[2];
    
    UART_Start();
    USB_Start(0, USB_5V_OPERATION);
    CsrInit();
    
    debug("Wait for USB connection...");
    
    while (0u == USB_GetConfiguration()) {}
    
    LED_Write(0);
    
    USB_EnableOutEP(EP_OUT);
    
    debug("USB connected");

    for(;;)
    {
        if (0u != USB_IsConfigurationChanged()) {
            if (0u != USB_GetConfiguration()) {
                USB_EnableOutEP(EP_OUT);
            }
        }
        
        if(PerformOutTransfer()) {
            //parse packet
            if(ReadByte() != '\0') {
                debug("Invalid packet");
                ClearReceive();
                continue;
            }
            
            while(ReadLeftSize() >= 2) {
                nCommand = ReadWord();
                
                switch (nCommand) {
                case CMD_READ:
                    debug("CMD_READ");
					if (ReadLeftSize() < 4) {
						debug("Too few arguments to read");
					} else {
						nArgs[0] = ReadWord();
						nArgs[1] = ReadWord();
						CmdRead(nArgs[0], nArgs[1]);
					}
					break;
        		case CMD_WRITE:
                    debug("CMD_WRITE");
					if (ReadLeftSize() < 4) {
						debug("Too few arguments to write");
					} else {
						nArgs[0] = ReadWord();
						nArgs[1] = ReadWord();
						if (ReadLeftSize() < (nArgs[1] * 2)) {
							debug("Too few arguments to write");
						} else {
							CmdWrite(nArgs[0], nArgs[1]);
						}
					}
					break;
            	case CMD_SETSPEED:
                    debug("CMD_SETSPEED");
					if (ReadLeftSize() < 2) {
						debug("Too few arguments to set speed");
					} else {
						nArgs[0] = ReadWord();
						CmdSetSpeed(nArgs[0]);
					}
					break;
				case CMD_GETSTOPPED:
                    debug("CMD_GETSTOPPED");
					CmdGetStopped();
					break;
				case CMD_GETSPEED:
                    debug("CMD_GETSPEED");
					CmdGetSpeed();
					break;
				case CMD_UPDATE:
                    debug("CMD_UPDATE");
					CmdUpdate();
					break;
				case CMD_GETSERIAL:
                    debug("CMD_GETSERIAL");
					CmdGetSerial();
					break;
				case CMD_GETVERSION:
                    debug("CMD_GETVERSION");
					CmdGetVersion();
					break;
				case CMD_SETMODE:
                    debug("CMD_SETMODE");
					if (ReadLeftSize() < 2) {
						debug("Too few arguments to set mode");
					} else {
						nArgs[0] = ReadWord();
						CmdSetMode(nArgs[0]);
					}
					break;
				case CMD_SETBITS:
                    debug("CMD_SETBITS");
					if (ReadLeftSize() < 4) {
						debug("Too few arguments to set bits");
					} else {
						nArgs[0] = ReadWord();
						nArgs[1] = ReadWord();
						CmdSetBits(nArgs[0], nArgs[1]);
					}
					break;
				case CMD_BCCMDINIT:
                    debug("CMD_BCCMDINIT");
					if (ReadLeftSize() < 4) {
						debug("Too few arguments to init bccmd");
					} else {
						nArgs[0] = ReadWord();
						nArgs[1] = ReadWord();
						CmdBcCmdInit(nArgs[0], nArgs[1]);
					}
					break;
				case CMD_BCCMD:
                    debug("CMD_BCCMD");
					if (ReadLeftSize() < 2) {
						debug("Too few arguments to bccmd");
					} else {
						nArgs[0] = ReadWord();
						if (ReadLeftSize() < (nArgs[0] * 2)) {
							debug("Too few arguments to bccmd");
						} else {
							CmdBcCmd(nArgs[0]);
						}
					}
					break;
				default:
					debug("Unknown command: 0x%04x", nCommand);
				    ClearReceive();
					continue;
				}
                (void) ReadWord(); //Read the two inbetween bytes
            }
            
            ClearReceive();
            PerformInTransfer();
        }
    }
}

unsigned short pCsrBuffer[1024];

int CmdRead(unsigned short nAddress, unsigned short nLength) {
	unsigned short *pCurrent;
    LED_Write(1);
	if (g_nMode == MODE_SPI && nLength < 1024 && CsrSpiRead(nAddress,nLength,pCsrBuffer)) {
		LED_Write(0);
		TransmitWord(CMD_READ);
		TransmitWord(nAddress);
		TransmitWord(nLength);
		pCurrent = pCsrBuffer;
		while (nLength--) {
			TransmitWord(*(pCurrent++));
		}
	} else {
		LED_Write(0);
		TransmitWord(CMD_READ + 1);
		TransmitWord(nAddress);
		TransmitWord(nLength);
		while (nLength--)
			TransmitWord(0);
	}
	return 1;
}
int CmdWrite(unsigned short nAddress, unsigned short nLength) {
	if (nLength > 1024 || g_nMode != MODE_SPI)
		return 0;
	unsigned short i;
	for (i = 0; i < nLength; i++) {
		pCsrBuffer[i] = ReadWord();
	}
    LED_Write(1);
	CsrSpiWrite(nAddress, nLength, pCsrBuffer);
	LED_Write(0);
	return 1;
}
int CmdSetSpeed(unsigned short nSpeed) {
	g_nSpeed = nSpeed;
	return 1;
}
int CmdGetStopped() {
	TransmitWord(CMD_GETSTOPPED);
	TransmitWord(g_nMode != MODE_SPI || CsrSpiIsStopped()); //TODO
	return 1;
}
int CmdGetSpeed() {
	TransmitWord(CMD_GETSPEED);
	TransmitWord(g_nSpeed);
	return 1;
}
int CmdUpdate() {
	return 1;
}
int CmdGetSerial() {
	TransmitWord(CMD_GETSERIAL);
	TransmitDWord(31337);
	return 1;
}
int CmdGetVersion() {
	TransmitWord(CMD_GETVERSION);
	TransmitWord(0x119);
	return 1;
}
int CmdSetMode(unsigned short nMode) {
	g_nMode = nMode;
	return 1;
}
int CmdSetBits(unsigned short nWhich, unsigned short nValue) {
	if (nWhich) g_nWriteBits = nValue;
	else g_nReadBits = nValue;
	return 1;
}

void CmdBcCmdInit(unsigned short nA, unsigned short nB) {
	g_nBcA = nA;
	g_nBcB = nB;
}

int CmdBcCmd(unsigned short nLength) {
	unsigned short i;
	//UARTprintf("BCCMD input:\n");
	for (i = 0; i < nLength; i++) {
		pCsrBuffer[i] = ReadWord();
	}
    LED_Write(1);
	if (CsrSpiBcCmd(nLength, pCsrBuffer)) {
		TransmitWord(CMD_BCCMD);
	} else {
		TransmitWord(CMD_BCCMD + 1);
	}
	LED_Write(0);
	TransmitWord(nLength);
	for (i=0; i<nLength; i++) {
		TransmitWord(pCsrBuffer[i]);
	}
	return 1;
}

/* [] END OF FILE */
