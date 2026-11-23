#include "main.h"
#include "can_protocol.h"
#include "lps22hb.h"
#include "hts221.h"
#include "MadgwickAHRS.h"

//###################################################################
//  BOARD SELECTION : set exactly ONE of these to 1, then build/flash
//  (can also be overridden with -DBOARD_xxx=1 on the command line)
//###################################################################
#ifndef VL6180X_PRESS_HUM_TEMP
#define VL6180X_PRESS_HUM_TEMP	1
#endif
#ifndef MPU9250
#define MPU9250	0
#endif
#ifndef DYN_ANEMO
#define DYN_ANEMO 0
#endif

#if (VL6180X_PRESS_HUM_TEMP + MPU9250 + DYN_ANEMO) != 1
#error "Select exactly one board in main.c"
#endif
//###################################################################

//====================================================================
//			CAN ACCEPTANCE FILTER  (4 standard IDs, list mode)
//====================================================================
#define USE_FILTER	1
#if DYN_ANEMO
#define ID_1	CAN_ID_MOTOR_CMD
#elif VL6180X_PRESS_HUM_TEMP
#define ID_1	CAN_ID_VL_CMD
#else
#define ID_1	CAN_ID_IMU_CMD
#endif
#define ID_2	ID_1
#define ID_3	ID_1
#define ID_4	ID_1

//====================================================================
//			PERIODS (ms)
//====================================================================
#define PERIOD_WIND_MS     500     // anemometer measurement + frame 0x11
#define PERIOD_VL_MS       200     // frame 0x21
#define PERIOD_ENV_MS      500     // frames 0x22 / 0x23
#define PERIOD_IMU_MS      TE      // Madgwick step (10 ms, cf config.h)
#define PERIOD_IMU_TX_MS   20      // quaternion frame
#define PERIOD_RAW_TX_MS   100     // accel / gyro frames

//====================================================================
//			ANEMOMETER / MOTOR SETTINGS
//====================================================================
// Somfy anemometer : wind speed = KMH_PER_HZ * pulse frequency.
// Adjust from the Somfy datasheet / a calibration if needed.
#define WIND_KMH_PER_HZ        2.4f
#define DEFAULT_THRESHOLD_KMH  10.0f
#define AUTO_VELOCITY          200      // dxl units (0.229 rpm) in auto mode
#define DXL_ID                 1

extern void systemClock_Config(void);

void (*rxCompleteCallback) (void);
void can_callback(void);

CAN_Message      rxMsg;
CAN_Message      txMsg;

extern float magCalibration[3];

// ---- Received command (written in CAN IRQ, consumed in main loop)
volatile int          cmd_pending = 0;
volatile CAN_Message  cmd_msg;

void VL6180x_Init(void);
void VL6180x_Step(void);

int status;
int new_switch_state;
int switch_state = -1;

//====================================================================
//			CAN SEND HELPER
//====================================================================
static void can_send(uint32_t id, uint8_t *data, uint8_t len)
{
    CAN_Message m;
    m.id = id;
    m.len = len;
    m.format = CANStandard;
    m.type = CANData;
    for (int i = 0; i < len; i++) m.data[i] = data[i];
    for (int retry = 0; retry < 3; retry++) {
        if (can_Write(m)) return;
        HAL_Delay(1);
    }
}

//====================================================================
//			BOARD 1 : DYNAMIXEL + ANEMOMETER
//====================================================================
#if DYN_ANEMO
static uint8_t  motor_mode = 0;          // 0 stop, 1 manual, 2 auto
static int16_t  manual_velocity = 0;
static float    threshold_kmh = DEFAULT_THRESHOLD_KMH;
static int16_t  applied_velocity = 0x7FFF; // force first write
static float    wind_kmh = 0;
static uint16_t last_pulses = 0;

static void motor_apply(int16_t v)
{
    if (v != applied_velocity) {
        dxl_setGoalVelocity(DXL_ID, v);
        applied_velocity = v;
    }
}

static void wind_motor_task(uint32_t now)
{
    static uint32_t t_wind = 0;
    if (now - t_wind < PERIOD_WIND_MS) return;
    float dt = (now - t_wind) / 1000.0f;
    t_wind = now;

    last_pulses = (uint16_t)anemo_GetCount();
    anemo_ResetCount();
    wind_kmh = WIND_KMH_PER_HZ * (last_pulses / dt);

    // ---- motor control law
    int16_t v = 0;
    if (motor_mode == 1) v = manual_velocity;
    else if (motor_mode == 2) {
        static int running = 0;                        // hysteresis 10 %
        if (wind_kmh >= threshold_kmh) running = 1;
        else if (wind_kmh < 0.9f * threshold_kmh) running = 0;
        v = running ? AUTO_VELOCITY : 0;
    }
    motor_apply(v);

    uint8_t d[8];
    PUT16(d, 0, (uint16_t)(wind_kmh * 100.0f));
    PUT16(d, 2, last_pulses);
    d[4] = motor_mode;
    d[5] = (applied_velocity != 0);
    PUT16(d, 6, applied_velocity);
    can_send(CAN_ID_WIND_STATUS, d, 8);
    term_printf("wind=%d.%02d km/h pulses=%d mode=%d v=%d\n\r",
                (int)wind_kmh, (int)(wind_kmh * 100) % 100, last_pulses, motor_mode, applied_velocity);
}

static void handle_command(CAN_Message *m)
{
    if (m->id != CAN_ID_MOTOR_CMD || m->len < 5) return;
    motor_mode = m->data[0] > 2 ? 0 : m->data[0];
    manual_velocity = (int16_t)GET16(m->data, 1);
    if (manual_velocity > 1000) manual_velocity = 1000;
    if (manual_velocity < -1000) manual_velocity = -1000;
    threshold_kmh = GET16(m->data, 3) / 100.0f;
}
#endif

//====================================================================
//			BOARD 2 : VL6180X + LPS22HB + HTS221
//====================================================================
#if VL6180X_PRESS_HUM_TEMP
static int vl_mode = 0;              // 0 distance, 1 luminosity (requested)
static int last_phys_switch = -1;
static int lps_ok = 0, hts_ok = 0;

static void env_task(uint32_t now)
{
    static uint32_t t_vl = 0, t_env = 0;
    uint8_t d[8];

    if (now - t_vl >= PERIOD_VL_MS) {
        t_vl = now;
        uint32_t value = (switch_state == SWITCH_VAL_RANGING) ? (uint32_t)range : (uint32_t)Als.lux;
        d[0] = (switch_state == SWITCH_VAL_RANGING) ? 0 : 1;
        PUT32(d, 1, value);
        can_send(CAN_ID_VL6180X, d, 5);
    }
    if (now - t_env >= PERIOD_ENV_MS) {
        t_env = now;
        float p, t, h, t2;
        if (lps_ok && lps22hb_Read(&p, &t) == 0) {
            PUT32(d, 0, (uint32_t)(p * 100.0f));
            PUT16(d, 4, (int16_t)(t * 100.0f));
            can_send(CAN_ID_PRESSURE, d, 6);
        }
        if (hts_ok && hts221_Read(&h, &t2) == 0) {
            PUT16(d, 0, (uint16_t)(h * 100.0f));
            PUT16(d, 2, (int16_t)(t2 * 100.0f));
            can_send(CAN_ID_HUMIDITY, d, 4);
        }
    }
}

static void handle_command(CAN_Message *m)
{
    if (m->id != CAN_ID_VL_CMD || m->len < 1) return;
    vl_mode = m->data[0] ? 1 : 0;
}
#endif

//====================================================================
//			BOARD 3 : MPU9250 + MADGWICK
//====================================================================
#if MPU9250
#define ACC_LSB_PER_G   8192.0f                                  // +-4 G
#define GYR_RAD_PER_LSB ((1000.0f / 32768.0f) * (3.14159265f / 180.0f)) // +-1000 dps

static void imu_task(uint32_t now)
{
    static uint32_t t_imu = 0, t_q = 0, t_raw = 0;
    uint8_t d[8];

    if (now - t_imu >= PERIOD_IMU_MS) {
        t_imu = now;
        mpu9250_Step();
        MadgwickAHRSupdateIMU(imu_gyr[0] * GYR_RAD_PER_LSB, imu_gyr[1] * GYR_RAD_PER_LSB,
                              imu_gyr[2] * GYR_RAD_PER_LSB,
                              (float)imu_acc[0], (float)imu_acc[1], (float)imu_acc[2]);
    }
    if (now - t_q >= PERIOD_IMU_TX_MS) {
        t_q = now;
        PUT16(d, 0, (int16_t)(q0 * 10000.0f));
        PUT16(d, 2, (int16_t)(q1 * 10000.0f));
        PUT16(d, 4, (int16_t)(q2 * 10000.0f));
        PUT16(d, 6, (int16_t)(q3 * 10000.0f));
        can_send(CAN_ID_IMU_QUAT, d, 8);
    }
    if (now - t_raw >= PERIOD_RAW_TX_MS) {
        t_raw = now;
        for (int i = 0; i < 3; i++) PUT16(d, 2 * i, (int16_t)(imu_acc[i] * 1000.0f / ACC_LSB_PER_G));
        can_send(CAN_ID_IMU_ACCEL, d, 6);
        for (int i = 0; i < 3; i++) PUT16(d, 2 * i, (int16_t)(imu_gyr[i] * (1000.0f / 32768.0f) * 10.0f));
        can_send(CAN_ID_IMU_GYRO, d, 6);
    }
}

static void handle_command(CAN_Message *m)
{
    if (m->id == CAN_ID_IMU_CMD && m->len >= 1 && m->data[0] == 1) MadgwickReset();
}
#endif

//====================================================================
// >>>>>>>>>>>>>>>>>>>>>>>>>> MAIN <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//====================================================================
int main(void)
{
	HAL_Init();
	systemClock_Config();
    SysTick_Config(HAL_RCC_GetHCLKFreq() / 1000); //SysTick end of count event each 1ms
	uart2_Init();
	uart1_Init();
	i2c1_Init();

#if DYN_ANEMO
    anemo_Timer1Init();
#endif

	HAL_Delay(1000); // Wait

#if VL6180X_PRESS_HUM_TEMP
    VL6180x_Init();
    lps_ok = (lps22hb_Init() == 0);
    hts_ok = (hts221_Init() == 0);
    term_printf("LPS22HB %s, HTS221 %s\n\r", lps_ok ? "OK" : "NOT FOUND", hts_ok ? "OK" : "NOT FOUND");
#endif

#if MPU9250
    mpu9250_InitMPU9250();
    mpu9250_CalibrateMPU9250();      // keep the board still during ~2 s
#if USE_MAGNETOMETER
    mpu9250_InitAK8963(magCalibration);
#endif
	term_printf("MPU9250 WhoAmI = 0x%x (0x71 expected)\n\r", mpu9250_WhoAmI());
    sampleFreq = 1000.0f / PERIOD_IMU_MS;
#endif

    can_Init();
    can_SetFreq(CAN_BAUDRATE); // CAN BAUDRATE : 500 kbit/s -- cf inc/config.h
#if USE_FILTER
    can_Filter_list((ID_1<<21)|(ID_2<<5) , (ID_3<<21)|(ID_4<<5) , CANStandard, 0);
#else
    can_Filter_disable(); // Accept everybody
#endif
    can_IrqInit();
    can_IrqSet(&can_callback);

#if DYN_ANEMO
    dxl_LED(DXL_ID, LED_ON);
    HAL_Delay(300);
    dxl_LED(DXL_ID, LED_OFF);
    dxl_torque(DXL_ID, TORQUE_OFF);
    dxl_setOperatingMode(DXL_ID, VELOCITY_MODE);
    dxl_torque(DXL_ID, TORQUE_ON);
    motor_apply(0);
#endif

    while (1) {
        uint32_t now = HAL_GetTick();

        if (cmd_pending) {
            CAN_Message m;
            __disable_irq();
            m = *(CAN_Message *)&cmd_msg;
            cmd_pending = 0;
            __enable_irq();
            handle_command(&m);
        }

#if DYN_ANEMO
        wind_motor_task(now);
#endif
#if VL6180X_PRESS_HUM_TEMP
        VL6180x_Step();
        env_task(now);
#endif
#if MPU9250
        imu_task(now);
#endif
    }
	return 0;
}

//====================================================================
//			CAN CALLBACK RECEPT (IRQ context : copy only)
//====================================================================
void can_callback(void)
{
	CAN_Message msg_rcv;
	can_Read(&msg_rcv);
	*(CAN_Message *)&cmd_msg = msg_rcv;
	cmd_pending = 1;
}
//====================================================================
//			TIMER CALLBACK PERIOD
//====================================================================
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
}
//====================================================================

#if VL6180X_PRESS_HUM_TEMP
void VL6180x_Init(void)
{
	uint8_t id;
	State.mode = 1;

    XNUCLEO6180XA1_Init();
    HAL_Delay(500); // Wait
    XNUCLEO6180XA1_Reset(0);
    HAL_Delay(10);
    XNUCLEO6180XA1_Reset(1);
    HAL_Delay(1);

    HAL_Delay(10);
    VL6180x_WaitDeviceBooted(theVL6180xDev);
    id=VL6180x_Identification(theVL6180xDev);
    term_printf("id=%d, should be 180 (0xB4) \n\r", id);
    VL6180x_InitData(theVL6180xDev);

    State.InitScale=VL6180x_UpscaleGetScaling(theVL6180xDev);
    State.FilterEn=VL6180x_FilterGetState(theVL6180xDev);
    VL6180x_DMaxSetState(theVL6180xDev, DMaxDispTime>0);

    switch_state=-1 ; // force re-init of the working mode
    State.mode = AlrmStart;
}
//====================================================================
void VL6180x_Step(void)
{
    DISP_ExecLoopBody();

    // The red switch of the shield still works ; the IHM (CAN 0x24) can
    // override it. The last event (switch toggle or IHM order) wins.
    int phys = XNUCLEO6180XA1_GetSwitch() ? 1 : 0;
    if (phys != last_phys_switch) {
        if (last_phys_switch != -1) vl_mode = (phys == SWITCH_VAL_RANGING) ? 0 : 1;
        last_phys_switch = phys;
    }
    new_switch_state = (vl_mode == 0) ? SWITCH_VAL_RANGING : !SWITCH_VAL_RANGING;

    if (new_switch_state != switch_state) {
        switch_state=new_switch_state;
        status = VL6180x_Prepare(theVL6180xDev);
        VL6180x_RangeSetMaxConvergenceTime(theVL6180xDev, 63);
        if (status) {
            AbortErr("ErIn");
        }
        else{
            if (switch_state == SWITCH_VAL_RANGING) {
                VL6180x_SetupGPIO1(theVL6180xDev, GPIOx_SELECT_GPIO_INTERRUPT_OUTPUT, INTR_POL_HIGH);
                VL6180x_ClearAllInterrupt(theVL6180xDev);
                State.ScaleSwapCnt=0;
                DoScalingSwap( State.InitScale);
            } else {
                 State.mode = RunAlsPoll;
                 InitAlsMode();
            }
        }
    }

    switch (State.mode) {
    case RunRangePoll:
        RangeState();
        break;
    case RunAlsPoll:
        AlsState();
        break;
    case InitErr:
        TimeStarted = g_TickCnt;
        State.mode = WaitForReset;
        break;
    case AlrmStart:
       GoToAlaramState();
       break;
    case AlrmRun:
        AlarmState();
        break;
    case FromSwitch:
        switch_state = -1;   // re-init selected mode
        break;
    case ScaleSwap:
        if (g_TickCnt - TimeStarted >= ScaleDispTime) {
            State.mode = RunRangePoll;
            TimeStarted=g_TickCnt;
        }
        else
        	DISP_ExecLoopBody();
        break;
    default: {
    	 DISP_ExecLoopBody();
          if (g_TickCnt - TimeStarted >= 5000) {
              NVIC_SystemReset();
          }
    }
    }
}
#endif
//====================================================================
