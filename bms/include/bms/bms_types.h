/* Common types and fault codes of the BMS library.
 *
 * Units used across the library (chosen so that monitoring is pure integer math):
 *   cell voltage  uint16_t  mV
 *   temperature   int16_t   0.1 degC (deci-degC)
 *   current       int32_t   mA, POSITIVE = CHARGE, negative = discharge
 *   time          uint16_t/uint32_t ms
 * The SoC estimator works in float32 (V, A, s), see bms_soc.h.
 *
 * Library rules (MISRA-minded): C99, no dynamic memory, no recursion, no OS or libc
 * headers except <stdint.h>, <stdbool.h>, <stddef.h>; every array is sized by the
 * compile-time limits below and every index is range checked against the config.
 */
#ifndef BMS_TYPES_H
#define BMS_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BMS_MAX_CELLS 16u /* series cells handled by one BMS instance */
#define BMS_MAX_TEMPS 8u  /* temperature sensors */

/* Latched fault bits (bms_faults_t). Any latched fault forces the FAULT state. */
#define BMS_F_CELL_OV (1UL << 0)       /* cell over-voltage */
#define BMS_F_CELL_UV (1UL << 1)       /* cell under-voltage */
#define BMS_F_CELL_OT (1UL << 2)       /* over-temperature */
#define BMS_F_CELL_UT (1UL << 3)       /* under-temperature */
#define BMS_F_OC_DISCHARGE (1UL << 4)  /* discharge over-current */
#define BMS_F_OC_CHARGE (1UL << 5)     /* charge over-current */
#define BMS_F_OPEN_WIRE (1UL << 6)     /* cell voltage outside the plausible range */
#define BMS_F_SENSOR_STUCK (1UL << 7)  /* cell voltage frozen while current flows */
#define BMS_F_TEMP_SENSOR (1UL << 8)   /* temperature outside the plausible range */
#define BMS_F_PRECHARGE (1UL << 9)     /* precharge did not complete in time */

/* Warning bits (not latched, do not change the state on their own). */
#define BMS_W_COMM_LOSS (1UL << 0)     /* no valid VCU command frame within the timeout */
#define BMS_W_CMD_E2E (1UL << 1)       /* last VCU frame failed the E2E check */

typedef uint32_t bms_faults_t;

#endif
