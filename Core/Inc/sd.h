#include "stm32g4xx_hal.h"
#include "main.h"

typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef unsigned long long UINT64;

#define SD_DUMMY_BYTE (UINT8)(0xFF)
#define SD_READ_TOKEN (UINT8)(0XFE)

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
    // UINT8 csd_struct; //[127:126] csd structure
    // UINT8 rev1;       //[125:120] reserved

    // UINT8 taac;               //[119:112] data read access-time-1
    // UINT8 nsac;               //[111:104] data read access-time-2 in CLK cycles (NSAC * 100)
    // UINT8 tran_speed;         //[103:96] max data transfer rate
    // UINT16 ccc;               //[95:84] card command classes
    // UINT8 read_bl_len;        //[83:80] max read data block length
    // UINT8 read_bl_partial;    //[79:79] partial blocks for read allowed
    // UINT8 write_blk_misalign; //[78:78] write block misalignment
    // UINT8 read_blk_misalign;  //[77:77] read block misalignment
    // UINT8 dsr_imp;            //[76:76] dsr implmented
    // UINT8 rev2;               //[75:74] reserved

    // UINT16 c_size;        //[73:62] device size
    // UINT8 vdd_r_curr_min; //[61:59] max read current vdd min
    // UINT8 vdd_r_curr_max; //[58:56] max read current vdd max
    // UINT8 vdd_w_curr_min; //[55:53] max write current vdd min
    // UINT8 vdd_w_curr_max; //[52:50] max write current vdd max
    // UINT8 c_size_mult;    //[49:47] device size multiplier
    // UINT8 erase_blk_en;   //[46:46] erase single block enable
    // UINT8 sector_size;    //[45:39] erase sector size
    // UINT8 wp_grp_size;    //[38:32] write protect group size
    // UINT8 wp_grp_enable;  //[31:31] write protect group enable
    // UINT8 rev3;           //[30:29] reserved

    // UINT8 r2w_factor;       //[28:26] write speed factor
    // UINT8 write_bl_len;     //[25:22] max write  data block length
    // UINT8 write_bl_partial; //[21:21] partial blocks for write allowed
    // UINT8 rev4;             //[20:16] reserved

    // UINT8 file_format_grp;    //[15:15] file format group
    // UINT8 copy;               //[14:14] copy flag
    // UINT8 perm_write_protect; //[13:13] permanent write protection
    // UINT8 tmp_write_protect;  //[12:12] permanent read protection
    // UINT8 file_format;        //[11:10] file format
    // UINT8 wp_upc;             //[9:9] write protection until power cycle
    // UINT8 rev5;               //[8:8] reserved

    // UINT8 crc; //[7:1] crc
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
        SDC_VER1_VER3 = 1,
        VER_ERROR = 2
    } sd_version;

    enum // sd card type from CMD58
    {
        SDSC = 0,
        SDHC = 1,
        TYPE_ERROR = 2
    } sd_cardType;

    enum
    {
        VOLTAGE_ACC_27_33 = 0x01, // voltage 2.7 - 3.6V
        VOLTAGE_ACC_LOW = 0x02,   // low voltage
        VOLTAGE_ACC_RES1 = 0x04,  // reserved voltage
        VOLTAGE_ACC_RES2 = 0x08   // reserved voltage
    } support_voltage;            // From CMD8
} SD_Info;

extern SD_Info sd_Info;

Res_Status SD_Init(void);
Res_Status SD_getCSDRegister(void);
