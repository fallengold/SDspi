#include "stm32g4xx_hal.h"
#include "main.h"

typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef unsigned long long UINT64;

/*Single token or byte is defined here*/
#define SD_DUMMY_BYTE (UINT8)(0xFF)
#define SD_START_SINGLE_READ_TOKEN (UINT8)(0xFE)
#define SD_START_SINGLE_WRITE_TOKEN (UINT8)(0xFE)
#define SD_START_MULTI_READ_TOKEN (UINT8)(0xFE)
#define SD_START_MULTI_WRITE_TOKEN (UINT8)(0xFC)
#define SD_END_MULTI_WRITE_TOKEN (UINT8)(0xFD)

/*SD'S parameters are defined here*/
#define SDHC_SINGLE_BLOCK_SIZE 512
/*________________________________*/

typedef enum
{
    SD_NO_ERROR = 0x00,
    IDLE_STATE = 0x01,
    ERASE_RESET = 0x02,
    ILL_COMMAND = 0x04,
    COM_CRC_ERROR = 0x08,
    ERASE_SEQUENCE = 0x10,
    ADDRESS_ERROR = 0x20,
    PARAMETER_ERROR = 0x40,

    /*timeout*/
    TIMEOUT = (0x40) + 1,

    /*Data response status*/
    SD_DATA_OK = 0x05,
    SD_DATA_CRC_ERROR = 0x0B,
    SD_DATA_WRITE_ERROR = 0x0D,
    SD_DATA_OTHER_ERROR = (0x0D) + 1,

    OTHER_ERROR = 0xFF + 1
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
Res_Status SD_readSingleBlock(UINT8 *pbuff, UINT64 addr, UINT32 block_size);
Res_Status SD_writeSingleBlock(UINT8 *pbuff, UINT64 addr, UINT32 block_size);
