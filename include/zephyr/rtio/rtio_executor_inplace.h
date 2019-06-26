/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_RTIO_RTIO_EXECUTOR_INPLACE_H_
#define ZEPHYR_INCLUDE_RTIO_RTIO_EXECUTOR_INPLACE_H_

#include <rtio/rtio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RTIO API
 * @defgroup rtio_api RTIO API
 * @ingroup rtio
 * @{
 */


/**
 * @brief Execte Inplace
 *
 * Iterates over the submission queue executing each requested transfer
 * as needed. Different operation chains are done in order and are not done
 * in parallel using threads or DMA. Each rtio_iodev may still be asynchronous
 * and perhaps even use the platforms DMA, but no specifics are detailed here
 * other than the device is asked to perform an IO operation with a buffer.
 *
 * This is the simplest software executor possible.
 */
int rtio_execute_inplace(struct rtio *r, uint32_t wait_count);

/**
 * @brief In-place Executor
 *
 * Iterates over the submission queue executing each requested transfer
 * as needed. Different operation chains are done in order and are not done
 * in parallel using threads or DMA. Each rtio_iodev may still be asynchronous
 * and perhaps even use the platforms DMA, but no specifics are detailed here
 * other than the device is asked to perform an IO operation with a buffer.
 */
struct rtio_inplace_executor {
	struct rtio_executor ctx;
};

/**
 * @cond INTERNAL_HIDDEN
 */
static const struct rtio_executor_api z_execute_inplace_api = {
	.execute = rtio_execute_inplace
};

/**
 * @endcond INTERNAL_HIDDEN
 */


/**
 * @brief Define a inplace executor with a given name
 *
 * The resulting symbol is typed as struct rtio_executor *
 * which is useful with rtio_submit().
 *
 * @param name Symbol name
 */
#define RTIO_EXECUTOR_INPLACE_DEFINE(name)                                                         \
	struct rtio_inplace_executor _name = { .ctx = { .api = &z_execute_inplace_api } };         \
	struct rtio_executor * const name = (struct rtio_executor * const)&_name;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif


#endif /* ZEPHYR_INCLUDE_RTIO_RTIO_EXECUTOR_INPLACE_H_ */
