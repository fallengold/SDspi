#include "stm32g4xx_hal.h"
#include "main.h"

typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef unsigned long long UINT64;

/*Single token or byte is defined here*/
#define SD_DUMMY_BYTE (UINT8)(0xFF)
#define SD_READ_TOKEN (UINT8)(0XFE)

/*SD'S parameters are defined here*/
#define SDHC_SINGLE_BLOCK_SIZE 512

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
    TIMEOUT = 128,
    OTHER_ERROR = 256,
} Res_Status;

typedef struct
{

    UINT8 read_bl_len;
    UINT16 c_size;
    UINT8 c_size_mult;

} CSD_Info;

typedef struct
{
    // UINT8 mid;  //[127:120] manufacturer id
    // UINT16 oid; //[119:104] oem/application id
    // UINT64 pnm; //[103:64] product name
    // UINT8 prv;  //[63:56] product revision
    // UINT32 psn; //[55:24] product serial number
    // UINT8 rev;  //[23:20] reserved

    // UINT16 mdt; //[19:8] manufacturing date
    // UINT8 crc;  // [7:1] crc7 checksum

} CID_Info;

typedef struct
{
    CID_Info CID;
    CSD_Info CSD;
    UINT8 cmd_version; // command version from CMD8
    enum               // sd card version from CMD8
    {
        SDC_VER2 = 0,
        SDC_VER1 = 1,
        VER_ERROR = 2
    } sd_version;

    enum // sd card type from CMD58
    {
        SDSC = 0,
        SDHC = 1,
        TYPE_ERROR = 2
    } sd_V2cardType;

    enum
    {
        VOLTAGE_ACC_27_33 = 0x01, // voltage 2.7 - 3.6V
        VOLTAGE_ACC_LOW = 0x02,   // low voltage
        VOLTAGE_ACC_RES1 = 0x04,  // reserved voltage
        VOLTAGE_ACC_RES2 = 0x08   // reserved voltage
    } support_voltage;            // From CMD8

    UINT32 sd_block_size;
    UINT64 sd_capacity; // Byte

} SD_Info;

extern SD_Info sd_Info;

Res_Status SD_Init(void);
Res_Status SD_getCSDRegister(void);
