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
        HAL_GPIO_WritePin(SD_CS_GPIO_PORT, SD_CS_PIN, GPIO_PIN_SET); \
    }
#define CS_LOW()                                                       \
    {                                                                  \
        HAL_GPIO_WritePin(SD_CS_GPIO_PORT, SD_CS_PIN, GPIO_PIN_RESET); \
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

/*Exchange a single byte through spi communication */
UINT8 xchg_byte(UINT8 xmitData)
{
    UINT8 rcvData;
    rcvData = HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &xmitData, &rcvData, 1, 50);
    return rcvData;
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
        /*select fail with no valid wait-ready response*/
        sd_deselect();
        return 0;
    }
}

/*return command response token*/
static UINT8 send_cmd(UINT8 cmd, UINT32 arg)
{
    UINT8 res;
    UINT8 crc;
    if (cmd & 0x80)
    {
        /*ACMD commands*/
        cmd &= 0x7F;
        res = send_cmd(CMD55, 0);
        if (res > 1)
        {
            /*Illegal command response not 0*/
            return (UINT8)res;
        }
    }

    /*reset communication status*/
    sd_deselect();
    if (!sd_select())
    {
        /*Select fail*/
        return WAITING_NO_RESPONSE;
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
        crc = (UINT8)0x95;
        break;
    case CMD8:
        crc = (UINT8)0x87;
        break;
    /*dummy crc + stop byte*/
    default:
        crc = (UINT8)0x01;
    }
    /*send crc + stop byte*/
    xchg_byte(crc);

    /*recieve  R1 response*/
    UINT8 cnt;
    cnt = 10;
    do
    {
        /*wait for response by sending dummy byte*/
        res = xchg_byte(SD_DUMMY_BYTE);
        /*The left bit(MSB) of R1 response is always 0*/
    } while ((res & (UINT8)0x80) && --cnt);

    return res;
}

UINT8 SD_GoIdleState(void)
{
    UINT8 res;
    /*Pull down the CS voltage*/
    res = send_cmd(CMD0, 0);
    /*R1 response in idle state*/
    if (res == IDLE_STATE)
    {
        CS_HIGH();
        xchg_byte(SD_DUMMY_BYTE);
    }
    return res;
}

UINT8 SD_Init(void)
{
    //__R1_Res_Status res;
    UINT8 res;

    /*Pull up MOSI and CS voltage high in at least 74 clock*/
    CS_HIGH();
    for (int i = 0; i < 10; i++)
    {
        /*send at least 80 dummy byte*/
        xchg_byte(0xFF);
    }
    /*[temp] Test code for sending CMD0*/

    CS_HIGH();
    xchg_byte(0xFF);

    /*send cmd0*/
    /*set CS low*/
    CS_LOW();
    xchg_byte(0xFF);
    /*Wait for sd card status ready*/
    UINT16 timeout = 0XFFF;
    do
    {
        res = xchg_byte(0xFF);
    } while (--timeout && (res != 0xFF));

    if (res != 0xFF)
    {
        return res;
    }

    /*send cmd0*/

    UINT8 cmd_bus[6] = {0};
    cmd_bus[0] = CMD0 | 0x40;
    cmd_bus[1] = 0;
    cmd_bus[2] = 0;
    cmd_bus[3] = 0;
    cmd_bus[4] = 0;
    cmd_bus[5] = 0x95;

    for (int i = 0; i < 6; i++)
    {
        xchg_byte(cmd_bus[i]);
    }
    UINT8 cnt;
    cnt = 10;

    do
    {
        /*Keep setting CS low and MOSI high */
        res = xchg_byte(SD_DUMMY_BYTE);
    } while (--cnt && (res & 0x80));

    return res;
    /*[temp]*/

    // res = SD_GoIdleState();
    // return res;
}
