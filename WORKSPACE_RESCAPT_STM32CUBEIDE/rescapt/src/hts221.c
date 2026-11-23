/* HTS221 humidity / temperature sensor (X-NUCLEO-IKS01A2) over I2C1
 * Linear interpolation with factory calibration (cf. ST TN1218). */
#include "hts221.h"

static float H0_rH, H1_rH, T0_degC, T1_degC;
static int16_t H0_T0_out, H1_T0_out, T0_out, T1_out;

int hts221_Init(void)
{
    uint8_t id = 0, v, c[16];
    if (i2c1_ReadRegBuffer(HTS221_ADDRESS, HTS221_WHO_AM_I, &id, 1) || id != 0xBC)
        return -1;
    v = 0x85;                         /* PD=1, BDU=1, ODR=1 Hz */
    i2c1_WriteRegBuffer(HTS221_ADDRESS, HTS221_CTRL_REG1, &v, 1);
    /* MSB of sub-address = auto-increment */
    if (i2c1_ReadRegBuffer(HTS221_ADDRESS, HTS221_CALIB_0 | 0x80, c, 16)) return -1;
    H0_rH   = c[0] / 2.0f;
    H1_rH   = c[1] / 2.0f;
    T0_degC = (float)(((uint16_t)(c[5] & 0x03) << 8) | c[2]) / 8.0f;
    T1_degC = (float)(((uint16_t)(c[5] & 0x0C) << 6) | c[3]) / 8.0f;
    H0_T0_out = (int16_t)((c[7]  << 8) | c[6]);
    H1_T0_out = (int16_t)((c[11] << 8) | c[10]);
    T0_out    = (int16_t)((c[13] << 8) | c[12]);
    T1_out    = (int16_t)((c[15] << 8) | c[14]);
    return 0;
}

int hts221_Read(float *humidity, float *temp_C)
{
    uint8_t b[4];
    if (i2c1_ReadRegBuffer(HTS221_ADDRESS, HTS221_HUM_OUT_L | 0x80, b, 4)) return -1;
    int16_t h = (int16_t)((b[1] << 8) | b[0]);
    int16_t t = (int16_t)((b[3] << 8) | b[2]);
    float hum = H0_rH + (H1_rH - H0_rH) * (float)(h - H0_T0_out) / (float)(H1_T0_out - H0_T0_out);
    if (hum < 0) hum = 0;
    if (hum > 100) hum = 100;
    *humidity = hum;
    *temp_C = T0_degC + (T1_degC - T0_degC) * (float)(t - T0_out) / (float)(T1_out - T0_out);
    return 0;
}
