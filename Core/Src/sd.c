#include "sd.h"

/* MMC/SD command */
#define CMD0 (0)           /* GO_IDLE_STATE */
#define CMD1 (1)           /* SEND_OP_COND (MMC) */
#define ACMD41 (0x80 + 41) /* SEND_OP_COND (SDC) */
#define CMD8 (8)           /* SEND_IF_COND */
#define CMD9 (9)           /* SEND_CSD */
#define CMD10 (10)         /* SEND_CID */
#define CMD12 (12)         /* STOP_TRANSMISSION */
#define CMD13 (13)
#define ACMD13 (0x80 + 13) /* SD_STATUS (SDC) */
#define CMD16 (16)         /* SET_BLOCKLEN */
#define CMD17 (17)         /* READ_SINGLE_BLOCK */
#define CMD18 (18)         /* READ_MULTIPLE_BLOCK */
#define CMD23 (23)         /* SET_BLOCK_COUNT (MMC) */
#define ACMD23 (0x80 + 23) /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24 (24)         /* WRITE_BLOCK */
#define CMD25 (25)         /* WRITE_MULTIPLE_BLOCK */
#define CMD32 (32)         /* ERASE_ER_BLK_START */
#define CMD33 (33)         /* ERASE_ER_BLK_END */
#define CMD38 (38)         /* ERASE */
#define CMD55 (55)         /* APP_CMD */
#define CMD58 (58)         /* READ_OCR */

/*Fetch SPI_CS_PIN high or low*/
#define CS_HIGH()                                                    \
    {                                                                \
        HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET); \
    }
#define CS_LOW()                                                       \
    {                                                                  \
        HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET); \
    }
//(Note that the _256 is used as a mask to clear the prescalar bits as it provides binary 111 in the correct position)
#define FCLK_SLOW()                                                                                    \
    {                                                                                                  \
        MODIFY_REG(SD_SPI_HANDLE.Instance->CR1, SPI_BAUDRATEPRESCALER_256, SPI_BAUDRATEPRESCALER_128); \
    } /* Set SCLK = slow, approx 280 KBits/s*/
#define FCLK_FAST()                                                                                  \
    {                                                                                                \
        MODIFY_REG(SD_SPI_HANDLE.Instance->CR1, SPI_BAUDRATEPRESCALER_256, SPI_BAUDRATEPRESCALER_8); \
    } /* Set SCLK = fast, approx 4.5 MBits/s */

/*Begin global variable define*/
SD_Info sd_Info; // Structure that stores sd's information
/*End global variable define*/

/*Exchange a single byte through spi communication */
UINT8 xchg_byte(UINT8 xmitData)
{
    UINT8 rcvData;
    HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &xmitData, &rcvData, 1, 50);
    return rcvData;
}

/*recieve multiple bytes*/
void rcv_mul_byte(UINT8 *rcvData, UINT8 len)
{
    for (int i = 0; i < len; i++)
    {
        rcvData[i] = xchg_byte(SD_DUMMY_BYTE);
    }
}

/*transmit multiple bytes*/
void xmit_mul_byte(UINT8 *xmitData, UINT8 len)
{
    for (int i = 0; i < len; i++)
    {
        xchg_byte(*(++xmitData));
    }
}

/*Wait until the sd card is ready or timeout*/
UINT8 sd_wait_ready(void)
{
    UINT16 timeout = 0xFFF;
    UINT8 data;
    do
    {
        data = xchg_byte(SD_DUMMY_BYTE);
    } while (data != 0xFF && --timeout);
    return (data == 0xFF) ? 1 : 0;
}

/*inactivate spi communication*/
void sd_deselect(void)
{
    CS_HIGH();
    xchg_byte(SD_DUMMY_BYTE);
}

/*activate spi communication*/
UINT8 sd_select(void)
{
    /*pull down the CS to select the sd card*/
    CS_LOW();
    xchg_byte(SD_DUMMY_BYTE);
    if (sd_wait_ready())
    {
        return 1;
    }
    else
    {
        /*select fail with no valid response*/
        /*timeout*/
        sd_deselect();
        return 0;
    }
}

/*return command response token*/
/*CS toggle to select or deslect is already included*/
static Res_Status send_cmd(UINT8 cmd, UINT32 arg)
{
    UINT8 rs;
    UINT8 crc;
    if (cmd & 0x80)
    {
        /*ACMD commands*/
        cmd &= 0x7F;
        rs = send_cmd(CMD55, 0);
        if (rs > 1)
        {
            /*Illegal command response not 0*/
            return (Res_Status)rs;
        }
    }

    /*reset communication status*/
    sd_deselect();
    if (!sd_select())
    {
        /*Select fail*/
        return TIMEOUT;
    }

    /*Send command packet*/
    xchg_byte(0x40 | cmd);
    xchg_byte((UINT8)(arg >> 24));
    xchg_byte((UINT8)(arg >> 16));
    xchg_byte((UINT8)(arg >> 8));
    xchg_byte((UINT8)arg);

    /*crc + stop byte*/
    switch (cmd)
    {
    case CMD0:
        crc = 0X95;
        break;
    case CMD8:
        crc = 0x87;
        break;
    /*dummy crc + stop byte*/
    default:
        crc = 0x01;
    }
    /*send crc + stop byte*/
    xchg_byte(crc);

    /*recieve  R1 response*/
    UINT8 cnt;
    cnt = 10;
    do
    {
        /*wait for response by sending dummy byte*/
        rs = xchg_byte(SD_DUMMY_BYTE);
        /*The left bit(MSB) of R1 response is always 0*/
    } while ((rs & 0x80) && --cnt);

    return (Res_Status)rs;
}

Res_Status SD_GoIdleState(void)
{
    static Res_Status res;
    /*Pull down the CS voltage*/
    res = send_cmd(CMD0, 0);
    /*R1 response in idle state*/
    if (res == IDLE_STATE)
    {
        sd_deselect();
    }
    return res;
}

// #define VOLTAGE_ACC_27_33   0b00000001
// #define VOLTAGE_ACC_LOW     0b00000010
// #define VOLTAGE_ACC_RES1    0b00000100
// #define VOLTAGE_ACC_RES2    0b00001000

Res_Status SD_CheckVersion(void)
{
    Res_Status rs;
    /*If receive idle state, it means version 2.00*/
    /*CMD8 will response first byte(MSB) identical to R1; */
    /*The next 4 bytes implies command version[31:28], reserved bit[27:12], voltage accepted[11:8], check-pattern(echo)[7:0]*/
    if ((rs = send_cmd(CMD8, 0x1AA)) == IDLE_STATE)
    {
        sd_Info.sd_version = SDC_VER2;
        UINT8 R7_Response[5];
        R7_Response[0] = rs;
        for (int i = 1; i < 5; i++)
        {
            R7_Response[i] = xchg_byte(SD_DUMMY_BYTE);
        }
        /*Analyze the R7 response data*/
        /*If you want, you can get more data, even output these information*/
        UINT8 cv, vol, echo;
        cv = (R7_Response[1] >> 4);
        vol = (R7_Response[3] & 0x1F);
        echo = R7_Response[4];

        sd_Info.cmd_version = cv;
        sd_Info.support_voltage = vol;

        /*Valid voltage(2.7V - 3.6V)*/
        /*Valid echo 0xAA*/

        if (vol == 0x01 && echo == 0xAA)
        {
            /*suitable sd-status check pass*/
            return rs;
        }
        else
        {
            /*These sd card is not well-defined*/
            /*Or some error happen when receiving R7*/
            sd_deselect();
            return OTHER_ERROR;
        }
    }
    /*Version 1.0*/ /*UNTESTED*/
    else if (rs == (ILL_COMMAND | IDLE_STATE))
    {
        sd_Info.sd_version = SDC_VER1;
        UINT16 timeout;
        timeout = 0XFFF;
        /*do something*/
        do
        {
            rs = send_cmd(CMD1, 0);
        } while (rs == SD_NO_ERROR && --timeout);
        /*Time out*/
        if (timeout < 0)
        {
            sd_deselect();
            return TIMEOUT;
        }
        return rs;
    }
    else
    {
        return SD_NO_ERROR;
    }
}

#define ACMD41_HCS 0x40000000
#define ACMD41_SDXC_POWER 0x10000000
#define ACMD41_S18R 0x04000000
#define ACMD41_VOLTAGE 0x00ff8000
#define ACMD41_ARG_HC (ACMD41_HCS | ACMD41_SDXC_POWER | ACMD41_VOLTAGE)
#define ACMD41_ARG_SC (ACMD41_VOLTAGE)

/*To check SDHC or SDSC*/
Res_Status SD_getCardType(void)
{
    Res_Status rs;
    /*We should first send ACMD41 to activate sd card's initialization*/
    UINT16 timeout = 0xFFF;
    while ((rs = send_cmd(ACMD41, ACMD41_HCS)) != SD_NO_ERROR && --timeout)
    {
    };
    if (!timeout)
    {
        timeout = 0xFF;
        // try with CMD1 command
        while ((rs = send_cmd(CMD1, ACMD41_HCS)) != SD_NO_ERROR && --timeout)
            ;
        if (!timeout)
        {
            sd_deselect();
            goto cmd58test;
            return TIMEOUT;
        } // timeout
    }
    if (rs != SD_NO_ERROR)
    {
        sd_deselect();
        return rs; // get other illegal response
    }

/*send CMD58 to get OCR info*/
cmd58test:;
    if ((rs = send_cmd(CMD58, 0)) != SD_NO_ERROR)
    {
        goto readocrtest;
        sd_deselect();
        return rs;
    }
readocrtest:;
    /*Send succeed, getting R3 response*/

    UINT8 R3_res[5];
    R3_res[0] = (UINT8)rs;
    for (int i = 1; i < 5; i++)
    {
        R3_res[i] = xchg_byte(SD_DUMMY_BYTE);
    }
    /*Get ocr CCS(bit30)*/
    /*1 -> SDHC; 0-> SDSC*/
    if (R3_res[1] & 0x80)
    {
        sd_Info.sd_V2cardType = SDHC;
        return SD_NO_ERROR;
    }
    else
    {
        sd_Info.sd_V2cardType = SDSC;
        return SD_NO_ERROR;
    }
}

Res_Status SD_getCSDRegister(void)
{
    Res_Status rs;
    /*Send cmd9 to get CSD Register's information*/
    /*wait for the R1 response from CMD9*/
    if ((rs = send_cmd(CMD9, 0)) != SD_NO_ERROR)
    {
        sd_deselect();
        return rs;
    }

    /*Get read-block token*/
    UINT8 read_token, timeout;
    timeout = 0xFF;
    while ((read_token = xchg_byte(SD_DUMMY_BYTE)) != SD_START_SINGLE_READ_TOKEN && --timeout)
        ;
    if (!timeout)
    {
        sd_deselect();
        return TIMEOUT;
    }

    /*token valid, then try to get 16bits bytes from the sd card */
    UINT8 csd[16] = {0};
    rcv_mul_byte(csd, 16);

    /*read two dummy crc check bit*/
    xchg_byte(SD_DUMMY_BYTE);
    xchg_byte(SD_DUMMY_BYTE);

    /*Calculate card's block's size, card's capacity*/
    /*SD Ver2.0 (SDHC)*/
    if (sd_Info.sd_version == SDC_VER2 && sd_Info.sd_V2cardType == SDHC)
    {
        sd_Info.CSD.c_size = (UINT32)csd[9] + (UINT32)(csd[8] << 8) + ((UINT32)(csd[7] & 0x3F) << 16);
        sd_Info.sd_block_size = SDHC_SINGLE_BLOCK_SIZE;
        sd_Info.sd_capacity = (UINT64)(sd_Info.CSD.c_size + 1) * 512 * 1024; // capacity is measure in Byte
    }
    /*SDSC*/
    else
    {
        sd_Info.CSD.read_bl_len = csd[5] & 0x1F;
        sd_Info.CSD.c_size = ((UINT16)(csd[6] & 0x02) << 10) + (csd[7] << 2) + ((csd[8] & 0xC0) >> 6);
        sd_Info.CSD.c_size_mult = (csd[9] & 0x02) + ((csd[10] & 0x80) >> 7);
        sd_Info.sd_capacity = (sd_Info.CSD.c_size + 1); // block base count

        sd_Info.sd_capacity *= (1 << (sd_Info.CSD.c_size_mult + 2)); // block count = block base count * 2^(multplier + 2)
        sd_Info.sd_block_size = 1 << (sd_Info.CSD.read_bl_len);      // single block size = 2 ^ (read_block_length)
        sd_Info.sd_capacity *= sd_Info.sd_block_size;                // card size = block count * single block size
    }

    sd_deselect();
    return SD_NO_ERROR;
}

/*Read a single block given the address*/
Res_Status SD_readSingleBlock(UINT8 *pbuff, UINT64 addr, UINT32 size)
{
    /*Sector count*/
    Res_Status rs;
    /*SDHC and the above sd cards use sector unit to search for the data*/
    if (sd_Info.sd_V2cardType == SDHC && sd_Info.sd_version == SDC_VER2)
    {
        addr /= SDHC_SINGLE_BLOCK_SIZE;
        size = SDHC_SINGLE_BLOCK_SIZE;
    }

    /*send cmd17 to read*/
    if ((rs = send_cmd(CMD17, addr)) != SD_NO_ERROR)
    {
        sd_deselect();
        return rs;
    }

    /*Get block read token*/
    UINT8 timeout = 0xFF;
    UINT8 read_token;
    while ((read_token = xchg_byte(SD_DUMMY_BYTE)) != SD_START_SINGLE_READ_TOKEN && --timeout)
        ;
    if (!timeout)
    { // timeout
        sd_deselect();
        return TIMEOUT;
    }
    /*Read data*/
    rcv_mul_byte(pbuff, size);
    /*2 crc bit*/
    xchg_byte(SD_DUMMY_BYTE);
    xchg_byte(SD_DUMMY_BYTE);

    sd_deselect();
    return SD_NO_ERROR;
}

/*Write a single block given the address*/
Res_Status SD_writeSingleBlock(UINT8 *pbuff, UINT64 addr, UINT32 size)
{
    Res_Status rs;

    if (sd_Info.sd_V2cardType == SDHC && sd_Info.sd_version == SDC_VER2)
    {
        addr /= SDHC_SINGLE_BLOCK_SIZE;
        size = SDHC_SINGLE_BLOCK_SIZE;
    }

    /*send CMD24*/
    if ((rs = send_cmd(CMD24, addr)) != SD_NO_ERROR)
    {
        sd_deselect();
        return rs;
    }

    /*Transmit block write token*/
    // sd_select();
    xchg_byte(SD_DUMMY_BYTE);
    xchg_byte(SD_START_SINGLE_WRITE_TOKEN);

    /*Transmit data*/
    xmit_mul_byte(pbuff, size);
    xchg_byte(SD_DUMMY_BYTE);
    xchg_byte(SD_DUMMY_BYTE); // CRC

    /*get data response token*/
    UINT8 timeout = 0xFF;
    while ((rs = xchg_byte(SD_DUMMY_BYTE)) == 0xFF && --timeout)
        ;
    if (!timeout)
    {
        sd_deselect();
        return TIMEOUT;
    }

    /*error handling function */

    return rs;
}

Res_Status SD_Init(void)
{
    Res_Status res;

    /*Pull up MOSI and CS voltage high in at least 74 clock*/
    // HAL_Delay(10); // Delay to give sd card enough time to power up
    CS_HIGH();
    for (int i = 0; i < 10; i++)
    {
        /*send at least 80 dummy byte*/
        xchg_byte(SD_DUMMY_BYTE);
    }

    if ((res = SD_GoIdleState()) != IDLE_STATE) // CMD0 check pass;
    {
        return res;
    }
    if ((res = SD_CheckVersion()) != IDLE_STATE) // CMD8 check pass
    {
        return res;
    }

    if ((res = SD_getCardType()) != SD_NO_ERROR) // CMD58 check pass
    {
        return res;
    }

    sd_deselect();
    return SD_NO_ERROR;
}
