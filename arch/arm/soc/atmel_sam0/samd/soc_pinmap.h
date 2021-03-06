/*
 * Copyright (c) 2017 Google LLC.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ATMEL_SAMD_SOC_PINMAP_H_
#define _ATMEL_SAMD_SOC_PINMAP_H_

/* SERCOM in UART mode */

/* SERCOM0 on RX=PA11, TX=PA10 */
#define PIN_UART_SAM0_SERCOM0_RX {0, 11, PORT_PMUX_PMUXE_C_Val}
#define PIN_UART_SAM0_SERCOM0_TX {0, 10, PORT_PMUX_PMUXE_C_Val}

/* SERCOM1 on RX=PA19, TX=PA18 */
#define PIN_UART_SAM0_SERCOM1_RX {0, 19, PORT_PMUX_PMUXE_C_Val}
#define PIN_UART_SAM0_SERCOM1_TX {0, 18, PORT_PMUX_PMUXE_C_Val}

/* SERCOM2 on RX=PA15, TX=PA14 */
#define PIN_UART_SAM0_SERCOM2_RX {0, 15, PORT_PMUX_PMUXE_C_Val}
#define PIN_UART_SAM0_SERCOM2_TX {0, 14, PORT_PMUX_PMUXE_C_Val}

/* SERCOM3 on RX=PA21, TX=PA20 */
#define PIN_UART_SAM0_SERCOM3_RX {0, 21, PORT_PMUX_PMUXE_D_Val}
#define PIN_UART_SAM0_SERCOM3_TX {0, 20, PORT_PMUX_PMUXE_D_Val}

/* SERCOM4 on RX=PB11, TX=PB10 */
#define PIN_UART_SAM0_SERCOM4_RX {0, 21, PORT_PMUX_PMUXE_C_Val}
#define PIN_UART_SAM0_SERCOM4_TX {0, 20, PORT_PMUX_PMUXE_C_Val}

/* SERCOM5 on RX=PB23, TX=PB22 */
#define PIN_UART_SAM0_SERCOM5_RX {1, 23, PORT_PMUX_PMUXE_D_Val}
#define PIN_UART_SAM0_SERCOM5_TX {1, 22, PORT_PMUX_PMUXE_D_Val}

#endif /* _ATMEL_SAMD_SOC_PINMAP_H_ */
