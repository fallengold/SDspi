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

/*Exchange a single byte through spi communication */
UINT8 xchg_byte(UINT8 xmitData)
{
    UINT8 rcvData;
    HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &xmitData, &rcvData, 1, 50);
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
            return (Res_Status)res;
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
        res = xchg_byte(SD_DUMMY_BYTE);
        /*The left bit(MSB) of R1 response is always 0*/
    } while ((res & 0x80) && --cnt);

    return (Res_Status)res;
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
    static Res_Status res;
    /*If receive idle state, it means version 2.00*/
    /*CMD8 will response first byte(MSB) identical to R1; */
    /*The next 4 bytes implies command version[31:28], reserved bit[27:12], voltage accepted[11:8], check-pattern(echo)[7:0]*/
    if ((res = send_cmd(CMD8, 0x1AA)) == IDLE_STATE)
    {
        UINT8 R7_Response[5];
        R7_Response[0] = res;
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

        /*Valid voltage(2.7V - 3.6V)*/
        /*Valid echo 0xAA*/

        if (vol == 0x01 && echo == 0xAA)
        {
            /*suitable sd-status check pass*/
            return res;
        }
        else
        {
            /*These sd card is not well-defined*/
            return OTHER_ERROR;
        }
    }
    /*Version 1.0*/
    else if (res == (ILL_COMMAND | IDLE_STATE))
    {
        UINT16 timeout;
        timeout = 0XFFF;
        /*do something*/
        do
        {
            res = send_cmd(CMD1, 0);
        } while (res == SD_NO_ERROR && --timeout);
        /*Time out*/
        if (timeout < 0)
        {
            return TIMEOUT;
        }
        return res;
    }
    else
    {
        return res;
    }
}

Res_Status SD_Init(void)
{
    static Res_Status res;
    FCLK_SLOW();

    /*Pull up MOSI and CS voltage high in at least 74 clock*/
    HAL_Delay(1); // Delay to give sd card enough time to power up
    CS_HIGH();
    for (int i = 0; i < 10; i++)
    {
        /*send at least 80 dummy byte*/
        xchg_byte(SD_DUMMY_BYTE);
    }

    if ((res = SD_GoIdleState()) == IDLE_STATE) // CMD0 succeed;
    {
        if ((res = SD_CheckVersion()) == IDLE_STATE) // CMD8 check pass
        {
            return res;
        }
    }
    else
    {
        return res;
    }
}
