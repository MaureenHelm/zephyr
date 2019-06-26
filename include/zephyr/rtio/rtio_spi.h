/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_RTIO_RTIO_SPI_H_
#define ZEPHYR_INCLUDE_RTIO_RTIO_SPI_H_

#include <rtio/rtio.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief RTIO API
 * @defgroup rtio_spi RTIO SPI IODev
 * @ingroup rtio
 * @{
 */

/**
 * @brief A spi variant of the rtio iodev.
 *
 * Note the iodev struct *must* be the first member to allow for pointer
 * aliasing (everything can look like rtio_iodev)
 */
struct rtio_spi_iodev {
	struct rtio_iodev iodev;
	struct spi_dt_spec *spi;
};

/**
 * @}
 */


#ifdef __cplusplus
}
#endif


#endif /* ZEPHYR_INCLUDE_RTIO_RTIO_SPI_H_ */
