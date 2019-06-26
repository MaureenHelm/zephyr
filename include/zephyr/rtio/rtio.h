/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Real-Time IO device API for moving bytes with low effort
 *
 * RTIO uses a SPSC lock-free queue pair to enable a DMA and ISR friendly I/O API.
 *
 * I/O like operations are setup in a pre-allocated queue with a fixed number of
 * submission requests. Each submission request takes the device to operate on
 * and an operation. The rest is information needed to perform the operation such
 * as a register or mmio address of the device, a source/destination buffers
 * to read from or write to, and other pertinent information.
 *
 * These operations may be chained in a such a way that only when the current
 * operation is complete will the next be executed. If the current request fails
 * all chained requests will also fail.
 *
 * The completion of these requests are pushed into a fixed size completion
 * queue which an application may actively poll for completions.
 *
 * An executor (could use a dma controller!) takes the queues and determines
 * how to perform each requested operation. By default there is a software
 * executor which does all operations in software using software device
 * APIs.
 */

#ifndef ZEPHYR_INCLUDE_RTIO_RTIO_H_
#define ZEPHYR_INCLUDE_RTIO_RTIO_H_

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/rtio/rtio_spsc.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief RTIO
 * @defgroup rtio RTIO
 * @{
 * @}
 */


struct rtio_iodev;

/**
 * @brief RTIO API
 * @defgroup rtio_api RTIO API
 * @ingroup rtio
 * @{
 */

/**
 * @brief RTIO Predefined Priorties
 * @defgroup rtio_sqe_prio RTIO Priorities
 * @ingroup rtio_api
 * @{
 */

/**
 * @brief Low priority
 */
#define RTIO_PRIO_LOW 0

/**
 * @brief Normal priority
 */
#define RTIO_PRIO_NORM 127

/**
 * @brief High priority
 */
#define RTIO_PRIO_HIGH 255

/**
 * @}
 */


/**
 * @brief RTIO SQE Flags
 * @defgroup rtio_sqe_flags RTIO SQE Flags
 * @ingroup rtio_api
 * @{
 */

/**
 * @brief The next request in the queue should wait on this one.
 */
#define RTIO_SQE_CHAINED BIT(0)

/**
 * @}
 */

/**
 * @brief A submission queue event
 */
struct rtio_sqe {
	uint8_t op; /**< Op code */

	uint8_t prio; /**< Op priority */

	uint16_t flags; /**< Op Flags */

	struct rtio_iodev *iodev; /**< Device to operation on */

	/**
	 * User provided pointer to data which is returned upon operation
	 * completion
	 *
	 * If unique identification of completions is desired this should be
	 * unique as well.
	 */
	void *userdata;

	union {
		struct {
			uint32_t buf_len; /**< Length of buffer */

			uint8_t *buf; /**< Buffer to use*/
		};
	};
};

/**
 * @brief Submission queue
 *
 * This is used for typifying the members of an RTIO queue pair
 * but nothing more.
 */
struct rtio_sq {
	struct rtio_spsc _spsc;
	struct rtio_sqe buffer[];
};

/**
 * @brief A completion queue event
 */
struct rtio_cqe {
	int32_t result; /**< Result from operation */
	void *userdata; /**< Associated userdata with operation */
};

/**
 * @brief Completion queue
 *
 * This is used for typifying the members of an RTIO queue pair
 * but nothing more.
 */
struct rtio_cq {
	struct rtio_spsc _spsc;
	struct rtio_cqe buffer[];
};

struct rtio;

struct rtio_executor_api {
	int (*execute)(struct rtio *r, uint32_t wait_count);
};

/**
 * @brief An executor does the work of executing the submissions.
 *
 * This could be a DMA controller backed executor, thread backed,
 * or simple in place executor.
 *
 * A DMA executor might schedule all transfers with priorities
 * and use hardware arbitration.
 *
 * A threaded executor might use a thread pool where each transfer
 * chain is executed across the thread pool and the priority of the
 * transfer is used as the thread priority.
 *
 * A simple in place exector might simply loop over and execute each
 * transfer in the calling threads context. Priority is entirely
 * derived from the calling thread then.
 *
 * An implementation of the executor must place this struct as
 * its first member such that pointer aliasing works.
 */
struct rtio_executor {
	const struct rtio_executor_api *api;
};

/**
 * @brief An RTIO queue pair that both the kernel and application work with
 *
 * The kernel is the consumer of the submission queue, and producer of the completion queue.
 * The application is the consumer of the completion queue and producer of the submission queue.
 *
 * Nothing is done until a call is performed to do the work (rtio_execute).
 */
struct rtio {

	/*
	 * An executor which does the job of working through the submission
	 * queue.
	 */
	struct rtio_executor *executor;

	/* Number of completions that were unable to be submitted with results
	 * due to the cq spsc being full
	 */
	atomic_t xcqcnt;

	/* Submission queue */
	struct rtio_sq *sq;

	/* Completion queue */
	struct rtio_cq *cq;
};

/**
 * @brief RTIO Submission Flags
 * @defgroup rtio_submission_flags RTIO Submission Flags
 * @ingroup rtio_api
 * @{
 */

/**
 * @brief Submit a request to be done while blocking the caller
 *
 * The submitted operation will be done synchronously and the call shall
 * not return until the operation is complete. The default is a non-blocking
 * request.
 */
#define RTIO_SUBMIT_SYNC BIT(0)

/**
 * @}
 */


/**
 * @brief API that an RTIO IO device should implement
 */
struct rtio_iodev_api {
	/**
	 * @brief Submission function for a request to the iodev
	 *
	 * By default with no flags provided the function should
	 * return as soon as possible. It is acceptable but not
	 * ideal if the function must poll on hardware for completion
	 * of the request.
	 *
	 * If the RTIO_SUBMIT_SYNC flag is set the function MUST
	 * complete the request before returning.
	 *
	 * Since the entire context (including the executor) is
	 * passed in the executor may be used in conjunction with the
	 * requested operation in a implementation defined way. For
	 * example to setup a DMA transfer.
	 */
	void (*submit)(const struct rtio_sqe *sqe,
			struct rtio *r,
			uint32_t flags);
};

/**
 * @brief An IO device with a function table for submitting requests
 *
 * This is required to be the first member of every iodev. There's a strong
 * possibility this will be extended with some common data fields (statistics)
 * in the future.
 */
struct rtio_iodev {
	const struct rtio_iodev_api * const api;
};


/** An operation that does nothing and will complete immediately */
#define RTIO_OP_NOP 0

/** An operation that receives (reads) */
#define RTIO_OP_RX 1

/** An operation that transmits (writes) */
#define RTIO_OP_TX 2

/**
 * @brief Define a fixed length submission queue
 *
 * @param name Variable name
 * @param len Queue length, power of 2 required (2, 4, 8)
 */
#define RTIO_SQ_DEFINE(name, len)			\
	RTIO_SPSC_DEFINE(name, struct rtio_sqe, len)

/**
 * @brief Acquire and prepare a nop (no op) request
 */
static inline void rtio_sqe_prep_nop(struct rtio_sqe *sqe,
				     struct rtio_iodev *iodev, void *userdata)
{
	sqe->op = RTIO_OP_NOP;
	sqe->iodev = iodev;
	sqe->userdata = userdata;
}

/**
 * @brief Acquire and prepare a read request
 */
static inline void rtio_sqe_prep_read(struct rtio_sqe *sqe,
				      struct rtio_iodev *iodev,
				      int8_t prio,
				      uint8_t *buf,
				      uint32_t len)
{
	sqe->op = RTIO_OP_RX;
	sqe->prio = prio;
	sqe->iodev = iodev;
	sqe->buf_len = len;
	sqe->buf = buf;
}

/**
 * @brief Acquire and prepare a write request
 */
static inline void rtio_sqe_prep_write(struct rtio_sqe *sqe,
				       struct rtio_iodev *iodev,
				       int8_t prio,
				       uint8_t *buf,
				       uint32_t len)
{
	sqe->op = RTIO_OP_TX;
	sqe->prio = prio;
	sqe->iodev = iodev;
	sqe->buf_len = len;
	sqe->buf = buf;
}

/**
 * @brief Define a fixed length completion queue
 *
 * @param name Variable name
 * @param len Queue length, power of 2 required (2, 4, 8)
 */
#define RTIO_CQ_DEFINE(name, len)			\
	RTIO_SPSC_DEFINE(name, struct rtio_cqe, len)

/**
 * @brief Define an RTIO instance with a given queue size
 *
 * @param name Name of the RTIO
 * @param exec Symbol for rtio_executor (pointer)
 * @param sq_sz Size of the submission queue, must be power of 2
 * @param cq_sz Size of the completion queue, must be power of 2
 */
#define RTIO_DEFINE(name, exec, sq_sz, cq_sz)				\
	RTIO_SQ_DEFINE(_sq_##name, sq_sz);				\
	RTIO_CQ_DEFINE(_cq_##name, cq_sz);				\
	struct rtio name = {						\
		.executor = (exec),					\
		.xcqcnt = ATOMIC_INIT(0),				\
		.sq = (struct rtio_sq * const)_sq_##name,			\
		.cq = (struct rtio_cq * const)_cq_##name,			\
	}

/**
 * @brief Set the executor of the rtio context
 */
static inline void rtio_set_executor(struct rtio *r, struct rtio_executor *exc)
{
	r->executor = exc;
}

/**
 * @brief Perform an submitted operation, maybe block or not based on the blocking parameter
 */
static inline void rtio_iodev_submit(struct rtio_sqe *sqe, struct rtio *r,
				     uint32_t submit_flags)
{
	sqe->iodev->api->submit(sqe, r, submit_flags);
}

/**
 * @brief Submit I/O requests to the underlying executor
 *
 * Submits the queue of requested IO operation chains to
 * the underlying executor. The underlying executor will decide
 * on which hardware and with what sort of parallelism the execution
 * of IO chains is performed.
 *
 * @param r RTIO context
 * @param wait_count Count of completions to wait for
 * If wait_count is for completions flag is set, the syscall will not
 * return until the desired number of completions are done.
 *
 * @retval 0 On success
 */
static inline int rtio_submit(struct rtio *r, uint32_t wait_count)
{
	__ASSERT(r->executor != NULL, "rtio needs an executor!");

	return r->executor->api->execute(r, wait_count);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_RTIO_RTIO_H_ */
