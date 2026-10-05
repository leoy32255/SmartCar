#ifndef MOTOR_F407_H
#define MOTOR_F407_H

#include <stdint.h>

/* XY-160D revision 3. Call after Bsp_Init, from the main loop only.
 * Init/Stop disable drive. Enable never restores an earlier command.
 * There is no STBY wire and no dynamic-brake API on this target. */
void Motor_Init(void);
void Motor_Enable(uint8_t enable);
void Motor_Stop(void);
/* Signed permille: 0 and |value|<50 coast; |value|>950 clamps to 950.
 * Positive means IN1/IN2=10 (before board polarity correction), negative=01.
 * Physical wheel direction must be calibrated. Disabled commands are discarded. */
void Motor_SetPWM(int16_t left, int16_t right);
/* Applied signed command after limiting, before board polarity correction. */
void Motor_GetPWM(int16_t *left, int16_t *right);

#endif
