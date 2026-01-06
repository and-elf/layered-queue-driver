/*
 * AUTO-GENERATED FILE - DO NOT EDIT
 * Generated from devicetree by scripts/dts_gen.py
 */

#ifndef LQ_GENERATED_H_
#define LQ_GENERATED_H_

#include "lq_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Engine instance */
extern struct lq_engine g_lq_engine;

/* Initialization function */
int lq_generated_init(void);

/* Hardware ISR handlers */
void lq_adc_isr_rpm_adc(uint16_t value);
void lq_spi_isr_rpm_spi(int32_t value);
void lq_adc_isr_temp_adc(uint16_t value);
void lq_adc_isr_oil_adc(uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* LQ_GENERATED_H_ */
