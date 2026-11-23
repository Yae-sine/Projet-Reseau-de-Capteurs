#ifndef MadgwickAHRS_h
#define MadgwickAHRS_h
extern volatile float beta;              /* algorithm gain */
extern volatile float q0, q1, q2, q3;    /* quaternion sensor frame / earth frame */
extern float sampleFreq;                 /* Hz */
void MadgwickAHRSupdateIMU(float gx, float gy, float gz, float ax, float ay, float az);
void MadgwickReset(void);
#endif
