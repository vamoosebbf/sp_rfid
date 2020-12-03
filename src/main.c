#include "rfid.h"
#include "fpioa.h"
#include "gpiohs.h"
#include "sleep.h"
#include "printf.h"
#include "sysctl.h"
#include "board_config.h"

int main(int argc, char const *argv[])
{
    uint8_t w_buf[16];
    uint8_t r_buf[16];
    uint8_t type[2];
    uint8_t uid[4];
    uint32_t w_val = 110;
    uint32_t r_val;
    uint8_t key[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    uint32_t freq = 0;
    freq = sysctl_pll_set_freq(SYSCTL_PLL0, 800000000);
    uint64_t core = current_coreid();
    printf("pll freq: %dhz\r\n", freq);
    const struct rfid_io_cfg_t io_cfg =
        {.hs_cs = RFID_CS_HSNUM,
         .hs_clk = RFID_CK_HSNUM,
         .hs_mosi = RFID_MO_HSNUM,
         .hs_miso = RFID_MI_HSNUM,
         .hs_rst = 0xFF,
         .clk_delay_us = 3};

    Pcd_io_init(&io_cfg);
    PcdReset();
    PcdAntennaOn();
    M500PcdConfigISOType('A');

    for (int i = 0; i < 16; i++)
    {
        w_buf[i] = i;
    }

    while (1)
    {
        // find card
        if (PcdRequest(0x52, type) == MI_OK)
            printf("find card success Tagtype: 0x%x\r\n", type[0] << 8 | type[1]);
        msleep(100);

        // get uid
        if (PcdAnticoll(uid) != MI_OK)
            continue;
        printf("uid: %x,%x,%x,%x\r\n", uid[0], uid[1], uid[2], uid[3]);
        msleep(100);

        // select card
        if (PcdSelect(uid) == MI_OK)
            printf("pcd select success\r\n");
        msleep(100);

        // auth key
        if (PcdAuthState(0x60, 0x11, key, uid) != MI_OK)
            continue;
        printf("auth success\r\n");

        // write
        if (PcdWrite(0x11, w_buf) == MI_OK)
            printf("write success\r\n");
        msleep(100);

        // read
        if (PcdRead(0x11, &r_buf) == MI_OK)
        {
            printf("read success: %d\r\n", r_buf[1]);
            break;
        }
        sleep(2);
    }
    return 0;
}