/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_RTIO_RTIO_I2C_H_
#define ZEPHYR_INCLUDE_RTIO_RTIO_I2C_H_

#include <rtio/rtio.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief RTIO API
 * @defgroup rtio_i2c RTIO I2C IODev
 * @ingroup rtio
 * @{
 */

/**
 * @brief An i2c variant of the rtio iodev.
 *
 * Note the iodev struct *must* be the first member to allow for pointer
 * aliasing (everything can look like rtio_iodev)
 */
struct rtio_i2c_iodev {
	struct rtio_iodev iodev;
	struct i2c_dt_spec *i2c;
};

/**
 * @}
 */


#ifdef __cplusplus
}
#endif


#endif /* ZEPHYR_INCLUDE_RTIO_RTIO_I2C_H_ */
