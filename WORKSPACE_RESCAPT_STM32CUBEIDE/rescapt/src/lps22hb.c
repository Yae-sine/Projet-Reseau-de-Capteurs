/* LPS22HB pressure sensor (X-NUCLEO-IKS01A2) over I2C1 */
#include "lps22hb.h"

int lps22hb_Init(void)
{
    uint8_t id = 0, v;
    if (i2c1_ReadRegBuffer(LPS22HB_ADDRESS, LPS22HB_WHO_AM_I, &id, 1) || id != 0xB1)
        return -1;
    v = 0x10;                         /* CTRL_REG2 : IF_ADD_INC */
    i2c1_WriteRegBuffer(LPS22HB_ADDRESS, LPS22HB_CTRL_REG2, &v, 1);
    v = 0x22;                         /* CTRL_REG1 : ODR = 10 Hz, BDU */
    i2c1_WriteRegBuffer(LPS22HB_ADDRESS, LPS22HB_CTRL_REG1, &v, 1);
    return 0;
}

int lps22hb_Read(float *pressure_hPa, float *temp_C)
{
    uint8_t b[5];
    if (i2c1_ReadRegBuffer(LPS22HB_ADDRESS, LPS22HB_PRESS_XL, b, 5)) return -1;
    int32_t p = ((int32_t)b[2] << 16) | ((int32_t)b[1] << 8) | b[0];
    if (p & 0x00800000) p |= 0xFF000000;          /* sign extension 24 bits */
    int16_t t = (int16_t)((b[4] << 8) | b[3]);
    *pressure_hPa = (float)p / 4096.0f;
    *temp_C       = (float)t / 100.0f;
    return 0;
}
