#ifndef INC_HTS221_H_
#define INC_HTS221_H_
#include "main.h"
#define HTS221_ADDRESS    (0x5F<<1)
#define HTS221_WHO_AM_I   0x0F        /* -> 0xBC */
#define HTS221_CTRL_REG1  0x20
#define HTS221_HUM_OUT_L  0x28
#define HTS221_CALIB_0    0x30
int hts221_Init(void);                              /* 0 = OK */
int hts221_Read(float *humidity, float *temp_C);    /* 0 = OK */
#endif
