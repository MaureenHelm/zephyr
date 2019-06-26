/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(rtio_inplace_executor, CONFIG_RTIO_LOG_LEVEL);

#include <kernel.h>
#include <rtio/rtio.h>
#include <rtio/rtio_executor_inplace.h>

int rtio_execute_inplace(struct rtio *r, uint32_t wait_count)
{

	int ret = 0;
	uint32_t completions = 0;
	struct rtio_sqe *sqe = rtio_spsc_consume(r->sq);

	while (sqe != NULL) {
		/* If the current submission chains into the next *or* there is
		 * a wait_count remaining, submit is requested to block.
		 *
		 * Per the rtio_iodev_submit docs, with SYNC is flagged the call
		 * MUST block until the submission is complete in some way.
		 */
		bool blocking = (completions < wait_count) || (sqe->flags & RTIO_SQE_CHAINED);
		uint32_t flags = blocking ? RTIO_SUBMIT_SYNC : 0;

		rtio_iodev_submit(sqe, r, flags);
		if (blocking) {
			completions += 1;
		}
		rtio_spsc_release(r->sq);
		sqe = rtio_spsc_consume(r->sq);
	}
	return ret;
}
