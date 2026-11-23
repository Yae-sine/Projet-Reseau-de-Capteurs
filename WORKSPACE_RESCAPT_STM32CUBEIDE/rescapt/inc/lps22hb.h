#ifndef INC_LPS22HB_H_
#define INC_LPS22HB_H_
#include "main.h"
#define LPS22HB_ADDRESS   (0x5D<<1)   /* IKS01A2 : SA0 = 1 */
#define LPS22HB_WHO_AM_I  0x0F        /* -> 0xB1 */
#define LPS22HB_CTRL_REG1 0x10
#define LPS22HB_CTRL_REG2 0x11
#define LPS22HB_PRESS_XL  0x28
int  lps22hb_Init(void);                               /* 0 = OK */
int  lps22hb_Read(float *pressure_hPa, float *temp_C); /* 0 = OK */
#endif
