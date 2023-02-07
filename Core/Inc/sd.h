#include "stm32g4xx_hal.h"
#include "main.h"

typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef unsigned long long UINT64;

#define SD_DUMMY_BYTE (UINT8)(0xFF)

typedef enum
{
    SD_NO_ERROR = 0,
    IDLE_STATE = 1,
    ERASE_RESET = 2,
    ILL_COMMAND = 4,
    COM_CRC_ERROR = 8,
    ERASE_SEQUENCE = 16,
    ADDRESS_ERROR = 32,
    PARAMETER_ERROR = 64,
    WAITING_NO_RESPONSE = 128,
} __R1_Res_Status;

__R1_Res_Status SD_Init(void);
