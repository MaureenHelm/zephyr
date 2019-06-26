/*
 * Copyright (c) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/rtio/rtio_spsc.h>
#include <zephyr/rtio/rtio.h>

/*
 * @brief Produce and Consume a single uint32_t in the same execution context
 *
 * @see rtio_spsc_acquire(), rtio_spsc_produce(), rtio_spsc_consume(), rtio_spsc_release()
 *
 * @ingroup rtio_tests
 */
void test_produce_consume_size1(void)
{
	RTIO_SPSC_DEFINE(ezspsc, uint32_t, 1);

	const uint32_t magic = 43219876;

	uint32_t *acq = rtio_spsc_acquire(ezspsc);

	zassert_not_null(acq, "Acquire should succeed");

	*acq = magic;

	uint32_t *acq2 = rtio_spsc_acquire(ezspsc);

	zassert_is_null(acq2, "Acquire should fail");

	uint32_t *cons = rtio_spsc_consume(ezspsc);

	zassert_is_null(cons, "Consume should fail");

	zassert_equal(rtio_spsc_consumable(ezspsc), 0, "Consumables should be 0");

	rtio_spsc_produce(ezspsc);

	zassert_equal(rtio_spsc_consumable(ezspsc), 1, "Consumables should be 1");

	uint32_t *cons2 = rtio_spsc_consume(ezspsc);

	zassert_equal(rtio_spsc_consumable(ezspsc), 0, "Consumables should be 0");

	zassert_not_null(cons2, "Consume should not fail");
	zassert_equal(*cons2, magic, "Consume value should equal magic");

	uint32_t *cons3 = rtio_spsc_consume(ezspsc);

	zassert_is_null(cons3, "Consume should fail");


	uint32_t *acq3 = rtio_spsc_acquire(ezspsc);

	zassert_is_null(acq3, "Acquire should not succeed");

	rtio_spsc_release(ezspsc);

	uint32_t *acq4 = rtio_spsc_acquire(ezspsc);

	zassert_not_null(acq4, "Acquire should succeed");
}

/*&*
 * @brief Produce and Consume 3 items at a time in a spsc of size 4 to validate masking
 * and wrap around reads/writes.
 *
 * @see rtio_spsc_acquire(), rtio_spsc_produce(), rtio_spsc_consume(), rtio_spsc_release()
 *
 * @ingroup rtio_tests
 */
void test_produce_consume_wrap_around(void)
{
	RTIO_SPSC_DEFINE(ezspsc, uint32_t, 4);

	for (int i = 0; i < 10; i++) {
		zassert_equal(rtio_spsc_consumable(ezspsc), 0, "Consumables should be 0");
		for (int j = 0; j < 3; j++) {
			uint32_t *entry = rtio_spsc_acquire(ezspsc);

			zassert_not_null(entry, "Acquire should succeed");
			*entry = i * 3 + j;
			rtio_spsc_produce(ezspsc);
		}
		zassert_equal(rtio_spsc_consumable(ezspsc), 3, "Consumables should be 3");

		for (int k = 0; k < 3; k++) {
			uint32_t *entry = rtio_spsc_consume(ezspsc);

			zassert_not_null(entry, "Consume should succeed");
			zassert_equal(*entry, i * 3 + k, "Consume value should equal i*3+k");
			rtio_spsc_release(ezspsc);
		}

		zassert_equal(rtio_spsc_consumable(ezspsc), 0, "Consumables should be 0");

	}
}

/**
 * @brief Ensure that integer wraps continue to work.
 *
 * Done by setting all values to UINTPTR_MAX - 2 and writing and reading enough
 * to ensure integer wraps occur.
 */
void test_int_wrap_around(void)
{
	RTIO_SPSC_DEFINE(ezspsc, uint32_t, 4);
	ezspsc->_spsc.in = ATOMIC_INIT(UINTPTR_MAX - 2);
	ezspsc->_spsc.out = ATOMIC_INIT(UINTPTR_MAX - 2);

	for (int j = 0; j < 3; j++) {
		uint32_t *entry = rtio_spsc_acquire(ezspsc);

		zassert_not_null(entry, "Acquire should succeed");
		*entry = j;
		rtio_spsc_produce(ezspsc);
	}

	zassert_equal(atomic_get(&ezspsc->_spsc.in), UINTPTR_MAX + 1, "Spsc in should wrap");

	for (int k = 0; k < 3; k++) {
		uint32_t *entry = rtio_spsc_consume(ezspsc);

		zassert_not_null(entry, "Consume should succeed");
		zassert_equal(*entry, k, "Consume value should equal i*3+k");
		rtio_spsc_release(ezspsc);
	}

	zassert_equal(atomic_get(&ezspsc->_spsc.out), UINTPTR_MAX + 1, "Spsc out should wrap");
}

#define MAX_RETRIES 5
#define SMP_ITERATIONS 100

RTIO_SPSC_DEFINE(spsc, uint32_t, 4);

static void t1_consume(void *p1, void *p2, void *p3)
{
	struct rtio_spsc_spsc *ezspsc = p1;
	uint32_t retries = 0;
	uint32_t *val = NULL;

	for (int i = 0; i < SMP_ITERATIONS; i++) {
		val = NULL;
		retries = 0;
		while (val == NULL && retries < MAX_RETRIES) {
			val = rtio_spsc_consume(ezspsc);
			retries++;
		}
		if (val != NULL) {
			rtio_spsc_release(ezspsc);
		} else {
			printk("consumer yield\n");
			k_yield();
		}
	}
}

static void t2_produce(void *p1, void *p2, void *p3)
{
	struct rtio_spsc_spsc *ezspsc = p1;
	uint32_t retries = 0;
	uint32_t *val = NULL;

	for (int i = 0; i < SMP_ITERATIONS; i++) {
		val = NULL;
		retries = 0;
		printk("producer acquiring\n");
		while (val == NULL && retries < MAX_RETRIES) {
			val = rtio_spsc_acquire(ezspsc);
			retries++;
		}
		if (val != NULL) {
			*val = SMP_ITERATIONS;
			rtio_spsc_produce(ezspsc);
		} else {
			printk("producer yield\n");
			k_yield();
		}
	}
}

#define STACK_SIZE (384 + CONFIG_TEST_EXTRA_STACK_SIZE)
#define THREADS_NUM 2

struct thread_info {
	k_tid_t tid;
	int executed;
	int priority;
	int cpu_id;
};
static ZTEST_BMEM volatile struct thread_info tinfo[THREADS_NUM];
static struct k_thread tthread[THREADS_NUM];
static K_THREAD_STACK_ARRAY_DEFINE(tstack, THREADS_NUM, STACK_SIZE);


/**
 * @brief Test that the producer and consumer are indeed thread safe
 *
 * This can and should be validated on SMP machines where incoherent
 * memory could cause issues.
 */
void test_spsc_threaded(void)
{

	tinfo[0].tid =
		k_thread_create(&tthread[0], tstack[0], STACK_SIZE,
				(k_thread_entry_t)t1_consume,
				spsc, NULL, NULL,
				K_PRIO_PREEMPT(5),
				K_INHERIT_PERMS, K_NO_WAIT);
	tinfo[1].tid =
		k_thread_create(&tthread[1], tstack[1], STACK_SIZE,
				(k_thread_entry_t)t2_produce,
				spsc, NULL, NULL,
				K_PRIO_PREEMPT(5),
				K_INHERIT_PERMS, K_NO_WAIT);

	k_thread_join(tinfo[1].tid, K_FOREVER);
	k_thread_join(tinfo[0].tid, K_FOREVER);
}



static void rtio_iodev_test_submit(const struct rtio_sqe *sqe,
				   struct rtio *r,
				   uint32_t flags);

static const struct rtio_iodev_api rtio_iodev_test_api = {
	.submit = rtio_iodev_test_submit
};

/* Test device submission queue event */
struct rtio_iodev_sqe {
	struct rtio_sqe sqe;
	struct rtio *r;
	uint32_t flags;
};

/**
 * @brief Test device submission queue
 *
 * This is used for reifying the member of the rtio_iodev_test struct
 */
struct rtio_iodev_sq {
	struct rtio_spsc _spsc;
	struct rtio_iodev_sqe buffer[];
};

/*
 * @brief A simple asynchronous testable iodev with a queue and thread
 */
struct rtio_iodev_test {
	/**
	 * io device struct as the first member, makes this an rtio_iodev
	 */
	struct rtio_iodev iodev;

	/**
	 * Request queue,for the device.
	 */
	struct rtio_iodev_sq *sq;

	/**
	 * Semaphore for completions, makes tests cleaner
	 */
	struct k_sem cqe_sem;
};

RTIO_SPSC_DEFINE(_iodev_test_sq, struct rtio_iodev_sqe, 2);
K_SEM_DEFINE(iodev_test_process, 0, 1);
K_SEM_DEFINE(iodev_test_done, 0, 1);
struct rtio_iodev_test iodev_test = {
	.iodev = { .api = &rtio_iodev_test_api },
	.sq = (struct rtio_iodev_sq *const)_iodev_test_sq
};

static void
rtio_iodev_test_execute(struct rtio_iodev_test *iodev, const struct rtio_sqe *sqe, struct rtio *r,
			uint32_t flags)
{
	struct rtio_cqe *cqe = rtio_spsc_acquire(r->cq);

	if (cqe == NULL) {
		atomic_add(&r->xcqcnt, (atomic_val_t)1);
		goto out;
	}

	if (sqe->op == RTIO_OP_NOP) {
		cqe->result = 0;
		cqe->userdata = sqe->userdata;
	} else {
		cqe->result = -EINVAL;
		cqe->userdata = sqe->userdata;
	}

	rtio_spsc_produce(r->cq);

out:
	return;
}


static void rtio_iodev_exec(void *p0, void *p1, void *p2)
{
	struct rtio_iodev_test *iodev = (struct rtio_iodev_test *)p0;
	struct k_sem *process = (struct k_sem *)p1;
	struct k_sem *done = (struct k_sem *)p2;

	while (true) {
		k_sem_take(process, K_FOREVER);

		struct rtio_iodev_sqe *iodev_sqe = rtio_spsc_consume(iodev->sq);

		if (iodev_sqe != NULL) {
			rtio_iodev_test_execute(iodev, &iodev_sqe->sqe, iodev_sqe->r,
						iodev_sqe->flags);
			rtio_spsc_release(iodev->sq);

			/* Simulate a task that consumes time */
			k_sleep(K_MSEC(10));
		}

		k_sem_give(done);
	}
}

K_THREAD_DEFINE(iodev_test_thr, 1024, rtio_iodev_exec,
		&iodev_test, &iodev_test_process, &iodev_test_done,
		1, 0, 0);

static void rtio_iodev_test_submit(
	const struct rtio_sqe *sqe,
	struct rtio *r,
	uint32_t flags)
{
	struct rtio_iodev_test *iodev = (struct rtio_iodev_test *)sqe->iodev;

	TC_PRINT("acquire: acquire %lu, in %lu, consume %lu, out %lu, mask %lx\n",
		 iodev->sq->_spsc.acquire, atomic_get(&iodev->sq->_spsc.in),
		 iodev->sq->_spsc.consume, atomic_get(&iodev->sq->_spsc.out),
		 iodev->sq->_spsc.mask);

	/* Emulate non-blocking requests with a queue
	 * and k_thread
	 */
	struct rtio_iodev_sqe *iodev_sqe = rtio_spsc_acquire(iodev->sq);

	TC_PRINT("iodev_sqe is %p\n", iodev_sqe);

	if (iodev_sqe != NULL) {
		memcpy(&iodev_sqe->sqe, sqe, sizeof(struct rtio_sqe));
		iodev_sqe->r = r;
		iodev_sqe->flags = flags;
		rtio_spsc_produce(iodev->sq);
		k_sem_give(&iodev_test_process);
		TC_PRINT("produced: acquire %lu, in %lu, consume %lu, out %lu, mask %lx\n",
			 iodev->sq->_spsc.acquire, atomic_get(&iodev->sq->_spsc.in),
			 iodev->sq->_spsc.consume, atomic_get(&iodev->sq->_spsc.out),
			 iodev->sq->_spsc.mask);

		if (flags & RTIO_SUBMIT_SYNC) {
			k_sem_take(&iodev_test_done, K_FOREVER);
		}
	} else {
		TC_PRINT("iodev_sqe is NULL (%p), return -EWOULDBLOCK\n", iodev_sqe);
		struct rtio_cqe *cqe = rtio_spsc_acquire(r->cq);

		if (cqe == NULL) {
			atomic_add(&r->xcqcnt, (atomic_val_t)1);
			return;
		}

		cqe->userdata = sqe->userdata;
		cqe->result = -EWOULDBLOCK;

		rtio_spsc_produce(r->cq);
	}
}

#include <rtio/rtio_executor_inplace.h>
RTIO_EXECUTOR_INPLACE_DEFINE(inplace_exec);
RTIO_DEFINE(r, inplace_exec, 4, 4);


/**
 * @brief Test the basics of the RTIO API
 *
 * Ensures that we can setup an RTIO context, enqueue a request, and receive
 * a completion event.
 */
void test_rtio_simple(void)
{
	int res;
	uintptr_t userdata[2] = {0, 1};
	struct rtio_sqe *sqe;
	struct rtio_cqe *cqe;

	rtio_spsc_reset(r.sq);
	rtio_spsc_reset(r.cq);
	rtio_spsc_reset(iodev_test.sq);

	sqe = rtio_spsc_acquire(r.sq);
	zassert_not_null(sqe, "Expected a valid sqe");
	rtio_sqe_prep_nop(sqe, (struct rtio_iodev *)&iodev_test, &userdata[0]);
	rtio_spsc_produce(r.sq);

	res = rtio_submit(&r, 1);
	zassert_ok(res, "Should return ok from rtio_execute");

	cqe = rtio_spsc_consume(r.cq);
	zassert_not_null(cqe, "Expected a valid cqe");
	zassert_ok(cqe->result, "Result should be ok");
	zassert_equal_ptr(cqe->userdata, &userdata[0], "Expected userdata back");
	rtio_spsc_release(r.cq);
}

/**
 * @brief Test chained requests
 *
 * Ensures that we can setup an RTIO context, enqueue a chained requests,
 * and receive completion events in the correct order given the chained
 * flag.
 */
void test_rtio_chain(void)
{
	int res;
	uintptr_t userdata[4] = {0, 1, 2, 3};
	struct rtio_sqe *sqe;
	struct rtio_cqe *cqe;

	rtio_spsc_reset(r.sq);
	rtio_spsc_reset(r.cq);
	rtio_spsc_reset(iodev_test.sq);

	for (int i = 0; i < 4; i++) {
		sqe = rtio_spsc_acquire(r.sq);
		zassert_not_null(sqe, "Expected a valid sqe");
		rtio_sqe_prep_nop(sqe, (struct rtio_iodev *)&iodev_test, &userdata[i]);
		sqe->flags |= RTIO_SQE_CHAINED;
		rtio_spsc_produce(r.sq);
	}
	zassert_equal(rtio_spsc_consumable(r.sq), 4, "Should have 4 pending ops");

	res = rtio_submit(&r, 4);
	zassert_ok(res, "Should return ok from rtio_execute");
	zassert_equal(rtio_spsc_consumable(r.cq), 4, "Should have 4 pending completions");

	for (int i = 0; i < 4; i++) {
		TC_PRINT("consume %d\n", i);
		cqe = rtio_spsc_consume(r.cq);
		zassert_not_null(cqe, "Expected a valid cqe");
		zassert_ok(cqe->result, "Result should be ok");
		zassert_equal_ptr(cqe->userdata, &userdata[i], "Expected in order completions");
		rtio_spsc_release(r.cq);
	}
}

/**
 * @brief Test multiple asynchronous chains against one iodev
 */
void test_rtio_multiple_chains(void)
{
	int res;
	uintptr_t userdata[4] = {0, 1, 2, 3};
	struct rtio_sqe *sqe;
	struct rtio_cqe *cqe;

	rtio_spsc_reset(r.sq);
	rtio_spsc_reset(r.cq);
	rtio_spsc_reset(iodev_test.sq);

	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
			sqe = rtio_spsc_acquire(r.sq);
			zassert_not_null(sqe, "Expected a valid sqe");
			rtio_sqe_prep_nop(sqe, (struct rtio_iodev *)&iodev_test,
					  (void *)userdata[i*2 + j]);
			if (j == 0) {
				sqe->flags |= RTIO_SQE_CHAINED;
			} else {
				sqe->flags |= 0;
			}
			rtio_spsc_produce(r.sq);
		}
	}
	zassert_equal(rtio_spsc_consumable(r.sq), 4, "Should have 4 pending ops");

	res = rtio_submit(&r, 0);
	zassert_ok(res, "Should return ok from rtio_execute");

	bool seen[4] = { 0 };

	for (int i = 0; i < 4; i++) {
		cqe = rtio_spsc_consume(r.cq);

		while (cqe == NULL) {
			k_sleep(K_MSEC(1));
			cqe = rtio_spsc_consume(r.cq);
		}

		zassert_not_null(cqe, "Expected a valid cqe");
		TC_PRINT("result %d, would block is %d, inval is %d\n",
			 cqe->result, -EWOULDBLOCK, -EINVAL);
		zassert_ok(cqe->result, "Result should be ok");
		seen[(uintptr_t)cqe->userdata] = true;
		if (seen[1]) {
			zassert_true(seen[0], "Should see 0 before 1");
		}
		if (seen[3]) {
			zassert_true(seen[2], "Should see 2 before 3");
		}
		rtio_spsc_release(r.cq);
	}
}

void test_main(void)
{
	ztest_test_suite(rtio_spsc_test,
			 ztest_1cpu_unit_test(test_produce_consume_size1),
			 ztest_1cpu_unit_test(test_produce_consume_wrap_around),
			 ztest_1cpu_unit_test(test_int_wrap_around),
			 ztest_unit_test(test_spsc_threaded),
			 ztest_unit_test(test_rtio_simple),
			 ztest_unit_test(test_rtio_chain),
			 ztest_unit_test(test_rtio_multiple_chains)
			 );

	ztest_run_test_suite(rtio_spsc_test);
}
