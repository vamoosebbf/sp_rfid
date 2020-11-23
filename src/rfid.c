#include "rfid.h"

#include "fpioa.h"
#include "gpiohs.h"
#include "sleep.h"
#include "printf.h"

/* clang-format off */
#define GPIOHS_OUT_HIGH(io) (*(volatile uint32_t *)0x3800100CU) |= (1 << (io))
#define GPIOHS_OUT_LOWX(io) (*(volatile uint32_t *)0x3800100CU) &= ~(1 << (io))

#define GET_GPIOHS_VALX(io) (((*(volatile uint32_t *)0x38001000U) >> (io)) & 1)
/* clang-format on */

static struct rfid_io_cfg_t spi_io_cfg;
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/**
 * @brief io模拟spi读写
 * 
 * @param [in], data: 写的数据
 * 
 * @return 读到的数据
 */
static uint8_t spi_rw(uint8_t data)
{
    uint8_t temp = 0;

    

    for(uint8_t i = 0; i < 8; i++) {
        GPIOHS_OUT_LOWX(spi_io_cfg.hs_clk);
        usleep(spi_io_cfg.clk_delay_us);

        if(data & 0x80) {
            GPIOHS_OUT_HIGH(spi_io_cfg.hs_mosi);
        } else {
            GPIOHS_OUT_LOWX(spi_io_cfg.hs_mosi);
        }
        data <<= 1;
        temp <<= 1;
        if(GET_GPIOHS_VALX(spi_io_cfg.hs_miso)) { temp++; }

        GPIOHS_OUT_HIGH(spi_io_cfg.hs_clk);
        usleep(spi_io_cfg.clk_delay_us);
    }


    return temp;
}

/**
  * @brief  读RC522寄存器
  * 
  * @param  [in], ucAddress: 寄存器地址
  * 
  * @return 寄存器的当前值
  */
static uint8_t ReadRawRC(uint8_t ucAddress)
{
    uint8_t ret, ucAddr = ((ucAddress << 1) & 0x7E) | 0x80;
    
    GPIOHS_OUT_LOWX(spi_io_cfg.hs_cs);
    usleep(10);

    spi_rw(ucAddr);
    ret = spi_rw(0x00);

    GPIOHS_OUT_HIGH(spi_io_cfg.hs_cs);
    usleep(10);

    return ret;
}

/**
  * @brief  写RC522寄存器
  * 
  * @param  [in], ucAddress: 寄存器地址
  * @param  [in], ucValue:写入寄存器的值
  */
static void WriteRawRC(uint8_t ucAddress, uint8_t ucValue)
{
    uint8_t ucAddr = (ucAddress << 1) & 0x7E;

    GPIOHS_OUT_LOWX(spi_io_cfg.hs_cs);
    usleep(10);

    spi_rw(ucAddr);
    spi_rw(ucValue);

    GPIOHS_OUT_HIGH(spi_io_cfg.hs_cs);
    usleep(10);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/**
  * @brief  对RC522寄存器置位
  * 
  * @param  [in], ucReg: 寄存器地址
  * @param  [in], ucMask: 置位值
  */
static void SetBitMask(uint8_t ucReg, uint8_t ucMask)
{
    uint8_t ucTemp;

    ucTemp = ReadRawRC(ucReg);
    WriteRawRC(ucReg, ucTemp | ucMask);
}

/**
  * @brief  对RC522寄存器清位
  * 
  * @param  [in], ucReg: 寄存器地址
  * @param  [in], ucMask: 清位值
  */
static void ClearBitMask(uint8_t ucReg, uint8_t ucMask)
{
    uint8_t ucTemp;

    ucTemp = ReadRawRC(ucReg);
    WriteRawRC(ucReg, ucTemp & (~ucMask));
}

/**
  * @brief  用RC522计算CRC16
  * 
  * @param  [in], pIndata: 计算CRC16的数组
  * @param  [in], ucLen: 计算CRC16的数组字节长度
  * @param  [out], pOutData: 存放计算结果存放的首地址
  */
static void CalulateCRC(uint8_t *pIndata, uint8_t ucLen, uint8_t *pOutData)
{
    uint8_t uc, ucN;

    ClearBitMask(DivIrqReg, 0x04);

    WriteRawRC(CommandReg, PCD_IDLE);

    SetBitMask(FIFOLevelReg, 0x80);

    for (uc = 0; uc < ucLen; uc++)
        WriteRawRC(FIFODataReg, *(pIndata + uc));

    WriteRawRC(CommandReg, PCD_CALCCRC);

    uc = 0xFF;

    do {
        ucN = ReadRawRC(DivIrqReg);
        uc--;
    } while ((uc != 0) && !(ucN & 0x04));

    pOutData[0] = ReadRawRC(CRCResultRegL);
    pOutData[1] = ReadRawRC(CRCResultRegM);
}

/**
  * @brief  通过RC522和ISO14443卡通讯
  * 
  * @param  [in], ucCommand: RC522命令字
  * @param  [in], pInData: 通过RC522发送到卡片的数据
  * @param  [in], ucInLenByte: 发送数据的字节长度
  * @param  [out], pOutData: 接收到的卡片返回数据
  * @param  [out], pOutLenBit: 返回数据的位长度
  * 
  * @return status
  */
static uint8_t PcdComMF522(uint8_t ucCommand, uint8_t *pInData, uint8_t ucInLenByte,
                            uint8_t *pOutData, uint32_t *pOutLenBit)
{
    uint8_t ucN, cStatus = MI_ERR;
    uint8_t ucIrqEn = 0x00;
    uint8_t ucWaitFor = 0x00;
    uint8_t ucLastBits;
    uint32_t ul;

    switch (ucCommand) {
        case PCD_AUTHENT:     //Mifare认证
            ucIrqEn = 0x12;   //允许错误中断请求ErrIEn  允许空闲中断IdleIEn
            ucWaitFor = 0x10; //认证寻卡等待时候 查询空闲中断标志位
            break;
        case PCD_TRANSCEIVE:  //接收发送 发送接收
            ucIrqEn = 0x77;   //允许TxIEn RxIEn IdleIEn LoAlertIEn ErrIEn TimerIEn
            ucWaitFor = 0x30; //寻卡等待时候 查询接收中断标志位与 空闲中断标志位
            break;
        default:
            break;
    }

    //IRqInv置位管脚IRQ与Status1Reg的IRq位的值相反
    WriteRawRC(ComIEnReg, ucIrqEn | 0x80);
    //Set1该位清零时，CommIRqReg的屏蔽位清零
    ClearBitMask(ComIrqReg, 0x80);
    //写空闲命令
    WriteRawRC(CommandReg, PCD_IDLE);

    //置位FlushBuffer清除内部FIFO的读和写指针以及ErrReg的BufferOvfl标志位被清除
    SetBitMask(FIFOLevelReg, 0x80);

    for (ul = 0; ul < ucInLenByte; ul++)
        WriteRawRC(FIFODataReg, pInData[ul]); //写数据进FIFOdata

    WriteRawRC(CommandReg, ucCommand); //写命令

    if (ucCommand == PCD_TRANSCEIVE) {
        //StartSend置位启动数据发送 该位与收发命令使用时才有效
        SetBitMask(BitFramingReg, 0x80);
    }

    ul = 1000 * 3; //根据时钟频率调整，操作M1卡最大等待时间25ms

    do { //认证 与寻卡等待时间
        ucN = ReadRawRC(ComIrqReg); //查询事件中断
        ul--;
    } while ((ul != 0) && (!(ucN & 0x01)) && (!(ucN & ucWaitFor)));

    ClearBitMask(BitFramingReg, 0x80); //清理允许StartSend位

    if (ul != 0) {
        //读错误标志寄存器BufferOfI CollErr ParityErr ProtocolErr
        if (!(ReadRawRC(ErrorReg) & 0x1B)) {
            cStatus = MI_OK;

            if (ucN & ucIrqEn & 0x01) {
                 //是否发生定时器中断
                cStatus = MI_NOTAGERR;
            }

            if (ucCommand == PCD_TRANSCEIVE) {
                //读FIFO中保存的字节数
                ucN = ReadRawRC(FIFOLevelReg);

                //最后接收到得字节的有效位数
                ucLastBits = ReadRawRC(ControlReg) & 0x07;

                if (ucLastBits) {
                    //N个字节数减去1（最后一个字节）+最后一位的位数 读取到的数据总位数
                    *pOutLenBit = (ucN - 1) * 8 + ucLastBits;
                } else {
                    *pOutLenBit = ucN * 8; //最后接收到的字节整个字节有效
                }

                ucN = (ucN == 0) ? 1 : ucN;
                ucN = (ucN > MAXRLEN) ? MAXRLEN : ucN;

                for (ul = 0; ul < ucN; ul++) {
                    pOutData[ul] = ReadRawRC(FIFODataReg);
                }
            }
        } else {
            cStatus = MI_ERR;
        }
    }

    SetBitMask(ControlReg, 0x80); // stop timer now
    WriteRawRC(CommandReg, PCD_IDLE);

    return cStatus;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void PcdAntennaOn(void)
{
    uint8_t uc;

    uc = ReadRawRC(TxControlReg);
    if (!(uc & 0x03)) {
        SetBitMask(TxControlReg, 0x03);
    }
}

void PcdAntennaOff(void)
{
    ClearBitMask(TxControlReg, 0x03);
}

uint8_t PcdHalt(void)
{
    uint32_t ulLen;
    uint8_t ucComMF522Buf[4] = {PICC_HALT, 0, 0, 0};

    CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);

    return PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &ulLen);
}

void PcdReset(void)
{
    uint8_t val = 0, t;

    if(0xFF != spi_io_cfg.hs_rst) {
        gpiohs_set_pin(spi_io_cfg.hs_rst, 0);
        msleep(10);
        gpiohs_set_pin(spi_io_cfg.hs_rst, 1);
    } 

    for (uint8_t i = 0; i< 0x30; i++) {
        val = ReadRawRC(i);
        printk("val: [0x%02X -> 0x%02X]\r\n", i, val);
    }

    WriteRawRC(CommandReg, 0x0f);

    val = 0xFF;
    do {
        val --;
        t = ReadRawRC(CommandReg);
        msleep(1);
    }while((val) && (t & 0x10));

    msleep(1);

    //定义发送和接收常用模式 和Mifare卡通讯，CRC初始值0x6363
    WriteRawRC(ModeReg, 0x3D);

    WriteRawRC(TReloadRegL, 30); //16位定时器低位
    WriteRawRC(TReloadRegH, 0);  //16位定时器高位

    WriteRawRC(TModeReg, 0x8D); //定义内部定时器的设置

    WriteRawRC(TPrescalerReg, 0x3E); //设置定时器分频系数

    WriteRawRC(TxAutoReg, 0x40); //调制发送信号为100%ASK
}

void M500PcdConfigISOType(uint8_t ucType)
{
    if (ucType == 'A') //ISO14443_A
    {
        ClearBitMask(Status2Reg, 0x08);

        WriteRawRC(ModeReg, 0x3D); //3F

        WriteRawRC(RxSelReg, 0x86); //84

        WriteRawRC(RFCfgReg, 0x7F); //4F

        WriteRawRC(TReloadRegL, 30);

        WriteRawRC(TReloadRegH, 0);

        WriteRawRC(TModeReg, 0x8D);

        WriteRawRC(TPrescalerReg, 0x3E);

        msleep(2);

        PcdAntennaOn(); //开天线
    } else {
        printk("unk ISO type\r\n");
    }
}

uint8_t PcdRequest(uint8_t ucReq_code, uint8_t *pTagType)
{
    uint32_t ulLen;
    uint8_t cStatus, ucComMF522Buf[MAXRLEN] = {ucReq_code};

    //清理指示MIFARECyptol单元接通以及所有卡的数据通信被加密的情况
    ClearBitMask(Status2Reg, 0x08);
    //发送的最后一个字节的 七位
    WriteRawRC(BitFramingReg, 0x07);
    //TX1,TX2管脚的输出信号传递经发送调制的13.56的能量载波信号
    SetBitMask(TxControlReg, 0x03);

    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 1, ucComMF522Buf, &ulLen);

    if ((cStatus == MI_OK) && (ulLen == 0x10)) {
        *pTagType = ucComMF522Buf[0];
        *(pTagType + 1) = ucComMF522Buf[1];
    } else {
        cStatus = MI_ERR;
    }

    return cStatus;
}

uint8_t PcdAnticoll(uint8_t *pSnr)
{
    uint32_t ulLen;
    uint8_t uc, cStatus, ucSnr_check = 0, ucComMF522Buf[MAXRLEN] = {PICC_ANTICOLL1, 0x20};

    //清MFCryptol On位 只有成功执行MFAuthent命令后，该位才能置位
    ClearBitMask(Status2Reg, 0x08);
    //清理寄存器 停止收发
    WriteRawRC(BitFramingReg, 0x00);
    //清ValuesAfterColl所有接收的位在冲突后被清除
    ClearBitMask(CollReg, 0x80);

    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 2, ucComMF522Buf, &ulLen);

    if (cStatus == MI_OK) {
        for (uc = 0; uc < 4; uc++) {
            *(pSnr + uc) = ucComMF522Buf[uc];
            ucSnr_check ^= ucComMF522Buf[uc];
        }

        if (ucSnr_check != ucComMF522Buf[uc]) {
            cStatus = MI_ERR;
        }
    }

    SetBitMask(CollReg, 0x80);

    return cStatus;
}

uint8_t PcdSelect(uint8_t *pSnr)
{
    uint32_t ulLen;
    uint8_t uc, cStatus, ucComMF522Buf[MAXRLEN];;

    ucComMF522Buf[0] = PICC_ANTICOLL1;
    ucComMF522Buf[1] = 0x70;
    ucComMF522Buf[6] = 0;

    for (uc = 0; uc < 4; uc++) {
        ucComMF522Buf[uc + 2] = *(pSnr + uc);
        ucComMF522Buf[6] ^= *(pSnr + uc);
    }

    CalulateCRC(ucComMF522Buf, 7, &ucComMF522Buf[7]);

    ClearBitMask(Status2Reg, 0x08);

    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 9, ucComMF522Buf, &ulLen);

    if((cStatus != MI_OK) || (ulLen != 0x18)) {
        cStatus = MI_ERR;
    }

    return cStatus;
}

uint8_t PcdAuthState(uint8_t ucAuth_mode, uint8_t ucAddr, const uint8_t *pKey, uint8_t *pSnr)
{
    uint32_t ulLen;
    uint8_t uc, cStatus, ucComMF522Buf[MAXRLEN];

    ucComMF522Buf[0] = ucAuth_mode;
    ucComMF522Buf[1] = ucAddr;

    for (uc = 0; uc < 6; uc++) {
        ucComMF522Buf[uc + 2] = *(pKey + uc);
    }

    for (uc = 0; uc < 4; uc++) {
        ucComMF522Buf[uc + 8] = *(pSnr + uc);
    }

    cStatus = PcdComMF522(PCD_AUTHENT, ucComMF522Buf, 12, ucComMF522Buf, &ulLen);

    if ((cStatus != MI_OK) || (!(ReadRawRC(Status2Reg) & 0x08))) {
        cStatus = MI_ERR;
    }

    return cStatus;
}

uint8_t PcdWrite(uint8_t ucAddr, uint8_t *pData)
{
    uint32_t ulLen;
    uint8_t uc, cStatus, ucComMF522Buf[MAXRLEN] = {PICC_WRITE, ucAddr, 0, 0};

    CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);

    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &ulLen);

    if ((cStatus != MI_OK) || (ulLen != 4) ||
        ((ucComMF522Buf[0] & 0x0F) != 0x0A))
    {
            printf("2 ulLen: %d\r\n",ulLen);

            cStatus = MI_ERR;
    }

    if (cStatus == MI_OK) {
        for (uc = 0; uc < 16; uc++) {
            ucComMF522Buf[uc] = *(pData + uc);
        }

        CalulateCRC(ucComMF522Buf, 16, &ucComMF522Buf[16]);

        cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 18, ucComMF522Buf, &ulLen);

        if ((cStatus != MI_OK) || (ulLen != 4) ||
            ((ucComMF522Buf[0] & 0x0F) != 0x0A))
        {
            printf("1 ulLen: %d\r\n",ulLen);
            cStatus = MI_ERR;
        }
    }
    return cStatus;
}

uint8_t PcdRead(uint8_t ucAddr, uint8_t *pData)
{
    uint32_t ulLen;
    uint8_t uc, cStatus, ucComMF522Buf[MAXRLEN] = {PICC_READ, ucAddr, 0, 0};

    CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);

    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &ulLen);

    if ((cStatus == MI_OK) && (ulLen == 0x90)) {
        for (uc = 0; uc < 16; uc++) {
            *(pData + uc) = ucComMF522Buf[uc];
        }
    } else {
        cStatus = MI_ERR;
    }

    return cStatus;
}

void Pcd_io_init(const struct rfid_io_cfg_t *cfg)
{
    spi_io_cfg.hs_cs = cfg->hs_cs;
    spi_io_cfg.hs_clk = cfg->hs_clk;
    spi_io_cfg.hs_mosi = cfg->hs_mosi;
    spi_io_cfg.hs_miso = cfg->hs_miso;
    spi_io_cfg.hs_rst = cfg->hs_rst;

    spi_io_cfg.clk_delay_us = cfg->clk_delay_us;

    gpiohs_set_drive_mode(spi_io_cfg.hs_cs, GPIO_DM_OUTPUT);
    gpiohs_set_drive_mode(spi_io_cfg.hs_clk, GPIO_DM_OUTPUT);
    gpiohs_set_drive_mode(spi_io_cfg.hs_mosi, GPIO_DM_OUTPUT);
    gpiohs_set_drive_mode(spi_io_cfg.hs_miso, GPIO_DM_INPUT);

    gpiohs_set_pin(spi_io_cfg.hs_cs, 1);
    gpiohs_set_pin(spi_io_cfg.hs_clk, 1);
    gpiohs_set_pin(spi_io_cfg.hs_mosi, 1);

    if(0xFF != spi_io_cfg.hs_rst) {
        gpiohs_set_drive_mode(spi_io_cfg.hs_rst, GPIO_DM_OUTPUT);
        gpiohs_set_pin(spi_io_cfg.hs_rst, 1);
    }
}

/////////////////////////////////////////////////////////////////////
//功    能：写入钱包金额
//参数说明: ucAddr[IN]：块地址
//          pData：写入的金额
//返    回: 成功返回MI_OK
/////////////////////////////////////////////////////////////////////
char WriteAmount(uint8_t ucAddr, uint32_t pData)
{
    uint8_t status;
    uint8_t ucComMF522Buf[16];
    ucComMF522Buf[0] = (pData & ((uint32_t)0x000000ff));
    ucComMF522Buf[1] = (pData & ((uint32_t)0x0000ff00)) >> 8;
    ucComMF522Buf[2] = (pData & ((uint32_t)0x00ff0000)) >> 16;
    ucComMF522Buf[3] = (pData & ((uint32_t)0xff000000)) >> 24;

    ucComMF522Buf[4] = ~(pData & ((uint32_t)0x000000ff));
    ucComMF522Buf[5] = ~(pData & ((uint32_t)0x0000ff00)) >> 8;
    ucComMF522Buf[6] = ~(pData & ((uint32_t)0x00ff0000)) >> 16;
    ucComMF522Buf[7] = ~(pData & ((uint32_t)0xff000000)) >> 24;

    ucComMF522Buf[8] = (pData & ((uint32_t)0x000000ff));
    ucComMF522Buf[9] = (pData & ((uint32_t)0x0000ff00)) >> 8;
    ucComMF522Buf[10] = (pData & ((uint32_t)0x00ff0000)) >> 16;
    ucComMF522Buf[11] = (pData & ((uint32_t)0xff000000)) >> 24;

    ucComMF522Buf[12] = ucAddr;
    ucComMF522Buf[13] = ~ucAddr;
    ucComMF522Buf[14] = ucAddr;
    ucComMF522Buf[15] = ~ucAddr;
    status = PcdWrite(ucAddr, ucComMF522Buf);
    return status;
}

/////////////////////////////////////////////////////////////////////
//功    能：读取钱包金额
//参数说明: ucAddr[IN]：块地址
//          *pData：读出的金额
//返    回: 成功返回MI_OK
/////////////////////////////////////////////////////////////////////
char ReadAmount(uint8_t ucAddr, uint32_t *pData)
{

    uint8_t status = MI_ERR;
    uint8_t j;
    uint8_t ucComMF522Buf[16];
    status = PcdRead(ucAddr, ucComMF522Buf);
    if (status != MI_OK)
        return status;
    for (j = 0; j < 4; j++)
    {
        if ((ucComMF522Buf[j] != ucComMF522Buf[j + 8]) && (ucComMF522Buf[j] != ~ucComMF522Buf[j + 4])) //验证一下是不是钱包的数据
            break;
    }
    if (j == 4)
    {
        status = MI_OK;
        *pData = ucComMF522Buf[0] + (ucComMF522Buf[1] << 8) + (ucComMF522Buf[2] << 16) + (ucComMF522Buf[3] << 24);
    }
    else
    {
        status = MI_ERR;
        *pData = 0;
    }
    return status;
}
