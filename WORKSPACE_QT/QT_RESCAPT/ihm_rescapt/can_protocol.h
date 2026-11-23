/*
 * can_protocol.h  --  CAN identifiers & payload layout shared with the IHM
 * All multi-byte fields are BIG-ENDIAN (MSB first).  Bus speed: 500 kbit/s.
 *
 *  Board 1 : DYNAMIXEL + ANEMOMETRE   (base 0x10)
 *    0x11  TX  wind/motor  [0-1] wind speed  km/h x100 (uint16)
 *                          [2-3] pulses during last period (uint16)
 *                          [4]   motor mode (0 stop, 1 manual, 2 auto)
 *                          [5]   motor running (0/1)
 *                          [6-7] current goal velocity (int16, dxl units 0.229 rpm)
 *    0x12  RX  command     [0]   motor mode (0 stop, 1 manual, 2 auto)
 *                          [1-2] manual velocity (int16, dxl units)
 *                          [3-4] wind threshold for auto mode km/h x100 (uint16)
 *
 *  Board 2 : VL6180X + LPS22HB + HTS221   (base 0x20)
 *    0x21  TX  VL6180X     [0]   mode (0 distance, 1 luminosity)
 *                          [1-4] value (uint32 : mm or lux)
 *    0x22  TX  LPS22HB     [0-3] pressure hPa x100 (uint32)  [4-5] temp degC x100 (int16)
 *    0x23  TX  HTS221      [0-1] humidity %RH x100 (uint16)  [2-3] temp degC x100 (int16)
 *    0x24  RX  command     [0]   VL6180X mode (0 distance, 1 luminosity)
 *
 *  Board 3 : MPU9250   (base 0x30)
 *    0x31  TX  quaternion  q0..q3 (4 x int16, x10000)  -- Madgwick output
 *    0x32  TX  accel       ax,ay,az (3 x int16, mg)
 *    0x33  TX  gyro        gx,gy,gz (3 x int16, 0.1 deg/s)
 *    0x34  RX  command     [0] = 1 : reset orientation (quaternion = identity)
 */
#ifndef INC_CAN_PROTOCOL_H_
#define INC_CAN_PROTOCOL_H_

#define CAN_ID_WIND_STATUS   0x11
#define CAN_ID_MOTOR_CMD     0x12

#define CAN_ID_VL6180X       0x21
#define CAN_ID_PRESSURE      0x22
#define CAN_ID_HUMIDITY      0x23
#define CAN_ID_VL_CMD        0x24

#define CAN_ID_IMU_QUAT      0x31
#define CAN_ID_IMU_ACCEL     0x32
#define CAN_ID_IMU_GYRO      0x33
#define CAN_ID_IMU_CMD       0x34

#define PUT16(b,i,v)  do{ (b)[i]=(uint8_t)(((uint16_t)(v))>>8); (b)[(i)+1]=(uint8_t)(v); }while(0)
#define PUT32(b,i,v)  do{ (b)[i]=(uint8_t)(((uint32_t)(v))>>24); (b)[(i)+1]=(uint8_t)(((uint32_t)(v))>>16); \
                          (b)[(i)+2]=(uint8_t)(((uint32_t)(v))>>8); (b)[(i)+3]=(uint8_t)(v); }while(0)
#define GET16(b,i)    ((uint16_t)(((uint16_t)(b)[i]<<8)|(b)[(i)+1]))

#endif
