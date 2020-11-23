#ifndef __SPMOD_RFID_H__
#define __SPMOD_RFID_H__

#include <stdint.h>

/* clang-format off */
/////////////////////////////////////////////////////////////////////
//MF522命令字
/////////////////////////////////////////////////////////////////////
#define PCD_IDLE                (0x00)    //取消当前命令
#define PCD_AUTHENT             (0x0E)    //验证密钥
#define PCD_RECEIVE             (0x08)    //接收数据
#define PCD_TRANSMIT            (0x04)    //发送数据
#define PCD_TRANSCEIVE          (0x0C)    //发送并接收数据
#define PCD_RESETPHASE          (0x0F)    //复位
#define PCD_CALCCRC             (0x03)    //CRC计算
/////////////////////////////////////////////////////////////////////
//Mifare_One卡片命令字
/////////////////////////////////////////////////////////////////////
#define PICC_REQIDL             (0x26)    //寻天线区内未进入休眠状态
#define PICC_REQALL             (0x52)    //寻天线区内全部卡
#define PICC_ANTICOLL1          (0x93)    //防冲撞
#define PICC_ANTICOLL2          (0x95)    //防冲撞
#define PICC_AUTHENT1A          (0x60)    //验证A密钥
#define PICC_AUTHENT1B          (0x61)    //验证B密钥
#define PICC_READ               (0x30)    //读块
#define PICC_WRITE              (0xA0)    //写块
#define PICC_DECREMENT          (0xC0)    //扣款
#define PICC_INCREMENT          (0xC1)    //充值
#define PICC_RESTORE            (0xC2)    //调块数据到缓冲区
#define PICC_TRANSFER           (0xB0)    //保存缓冲区中数据
#define PICC_HALT               (0x50)    //休眠
/////////////////////////////////////////////////////////////////////
//MF522 FIFO长度定义
/////////////////////////////////////////////////////////////////////
#define DEF_FIFO_LENGTH         (64)
#define MAXRLEN                 (18)
/////////////////////////////////////////////////////////////////////
//MF522寄存器定义
/////////////////////////////////////////////////////////////////////
// PAGE 0
#define RFU00                   (0x00)
#define CommandReg              (0x01)
#define ComIEnReg               (0x02)
#define DivlEnReg               (0x03)
#define ComIrqReg               (0x04)
#define DivIrqReg               (0x05)
#define ErrorReg                (0x06)
#define Status1Reg              (0x07)
#define Status2Reg              (0x08)
#define FIFODataReg             (0x09)
#define FIFOLevelReg            (0x0A)
#define WaterLevelReg           (0x0B)
#define ControlReg              (0x0C)
#define BitFramingReg           (0x0D)
#define CollReg                 (0x0E)
#define RFU0F                   (0x0F)
// PAGE 1
#define RFU10                   (0x10)
#define ModeReg                 (0x11)
#define TxModeReg               (0x12)
#define RxModeReg               (0x13)
#define TxControlReg            (0x14)
#define TxAutoReg               (0x15)
#define TxSelReg                (0x16)
#define RxSelReg                (0x17)
#define RxThresholdReg          (0x18)
#define DemodReg                (0x19)
#define RFU1A                   (0x1A)
#define RFU1B                   (0x1B)
#define MifareReg               (0x1C)
#define RFU1D                   (0x1D)
#define RFU1E                   (0x1E)
#define SerialSpeedReg          (0x1F)
// PAGE 2
#define RFU20                   (0x20)
#define CRCResultRegM           (0x21)
#define CRCResultRegL           (0x22)
#define RFU23                   (0x23)
#define ModWidthReg             (0x24)
#define RFU25                   (0x25)
#define RFCfgReg                (0x26)
#define GsNReg                  (0x27)
#define CWGsCfgReg              (0x28)
#define ModGsCfgReg             (0x29)
#define TModeReg                (0x2A)
#define TPrescalerReg           (0x2B)
#define TReloadRegH             (0x2C)
#define TReloadRegL             (0x2D)
#define TCounterValueRegH       (0x2E)
#define TCounterValueRegL       (0x2F)
// PAGE 3
#define RFU30                   (0x30)
#define TestSel1Reg             (0x31)
#define TestSel2Reg             (0x32)
#define TestPinEnReg            (0x33)
#define TestPinValueReg         (0x34)
#define TestBusReg              (0x35)
#define AutoTestReg             (0x36)
#define VersionReg              (0x37)
#define AnalogTestReg           (0x38)
#define TestDAC1Reg             (0x39)
#define TestDAC2Reg             (0x3A)
#define TestADCReg              (0x3B)
#define RFU3C                   (0x3C)
#define RFU3D                   (0x3D)
#define RFU3E                   (0x3E)
#define RFU3F                   (0x3F)
/////////////////////////////////////////////////////////////////////
//和MF522通讯时返回的错误代码
/////////////////////////////////////////////////////////////////////
#define MI_OK                   (0x26)
#define MI_NOTAGERR             (0xCC)
#define MI_ERR                  (0xBB)
/////////////////////////////////////////////////////////////////////
/* clang-format on */

struct rfid_io_cfg_t
{
    uint8_t hs_cs;
    uint8_t hs_clk;
    uint8_t hs_mosi;
    uint8_t hs_miso;
    uint8_t hs_rst; /* 0xFF: disable */
    uint8_t clk_delay_us;
};

/**
  * @brief  开启天线 
  */
void PcdAntennaOn(void);

/**
  * @brief  关闭天线
  */
void PcdAntennaOff(void);

/**
  * @brief  命令卡片进入休眠状态
  * 
  * @return status
  */
uint8_t PcdHalt(void);

/**
  * @brief  复位RC522 
  */
void PcdReset(void);

/**
  * @brief  设置RC522的工作方式
  * 
  * @param  [in], ucType: 工作方式
  */
void M500PcdConfigISOType(uint8_t ucType);

/**
  * @brief 寻卡
  * 
  * @param  [in], ucReq_code: 寻卡方式 = 0x52，寻感应区内所有符合14443A标准的卡；
  *         寻卡方式= 0x26，寻未进入休眠状态的卡
  * @param  [in], pTagType: 卡片类型代码
  *             = 0x4400，Mifare_UltraLight
  *             = 0x0400，Mifare_One(S50)
  *             = 0x0200，Mifare_One(S70)
  *             = 0x0800，Mifare_Pro(X))
  *             = 0x4403，Mifare_DESFire
  *
  * @return status
  */
uint8_t PcdRequest(uint8_t ucReq_code, uint8_t *pTagType);

/**
  * @brief  防冲撞
  * 
  * @param  [in], pSnr: 卡片序列号，4字节
  * 
  * @return status
  */
uint8_t PcdAnticoll(uint8_t *pSnr);

/**
  * @brief  选定卡片
  * 
  * @param  [in], pSnr: 卡片序列号，4字节
  * 
  * @return status
  */
uint8_t PcdSelect(uint8_t *pSnr);

/**
  * @brief  验证卡片密码
  * 
  * @param  [in], ucAuth_mode: 密码验证模式= 0x60，验证A密钥，
            密码验证模式= 0x61，验证B密钥
  * @param  [in], ucAddr: 块地址
  * @param  [in], pKey: 密码 
  * @param  [in], pSnr: 卡片序列号，4字节
  * 
  * @return status
  */
uint8_t PcdAuthState(uint8_t ucAuth_mode, uint8_t ucAddr, const uint8_t *pKey, uint8_t *pSnr);

/**
  * @brief  写数据到M1卡一块
  * 
  * @param  [in], ucAddr: 块地址
  * @param  [in], pData: 写入的数据，16字节
  * 
  * @return status
  */
uint8_t PcdWrite(uint8_t ucAddr, uint8_t *pData);

/**
  * @brief  读取M1卡一块数据
  * 
  * @param  [in], ucAddr: 块地址
  * @param  [out], pData: 读出的数据，16字节
  * 
  * @return status
  */
uint8_t PcdRead(uint8_t ucAddr, uint8_t *pData);

/**
 * @brief 初始化spi io配置
 * 
 * @param [in], cfg: io配置，并输出默认电平
 */
void Pcd_io_init(const struct rfid_io_cfg_t *cfg);

char WriteAmount(uint8_t ucAddr, uint32_t pData);
char ReadAmount(uint8_t ucAddr, uint32_t *pData);

#endif /* __SPMOD_RFID_H__ */
