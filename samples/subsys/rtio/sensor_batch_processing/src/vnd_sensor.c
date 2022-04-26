/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT vnd_sensor

#include <zephyr/zephyr.h>
#include <zephyr/rtio/rtio.h>
#include <zephyr/rtio/rtio_spsc.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(vnd_sensor);

struct vnd_sensor_msg {
	struct rtio_sqe sqe;
	struct rtio *r;
	uint32_t flags;
};

struct vnd_sensor_config {
	uint32_t sample_period;
	struct vnd_sensor_msg *msgs;
	size_t max_msgs;
};

struct vnd_sensor_data {
	struct rtio_iodev iodev;
	struct k_timer timer;
	const struct device *dev;
	struct k_msgq msgq;
};

static int vnd_sensor_iodev_read(const struct device *dev, uint8_t *buf,
		uint32_t buf_len)
{
	LOG_DBG("%s: buf_len = %d, buf = %p", dev->name, buf_len, buf);

	return 0;
}

static void vnd_sensor_iodev_execute(const struct device *dev,
		const struct rtio_sqe *sqe, struct rtio *r, uint32_t flags)
{
	struct rtio_cqe *cqe = rtio_spsc_acquire(r->cq);

	if (cqe == NULL) {
		LOG_ERR("%s: Could not get a cqe", dev->name);
		atomic_add(&r->xcqcnt, (atomic_val_t)1);
		goto out;
	}

	if (sqe->op == RTIO_OP_RX) {
		cqe->result = vnd_sensor_iodev_read(dev, sqe->buf, sqe->buf_len);
		cqe->userdata = sqe->userdata;
	} else {
		LOG_ERR("%s: Invalid op", dev->name);
		cqe->result = -EINVAL;
		cqe->userdata = sqe->userdata;
	}

	rtio_spsc_produce(r->cq);

out:
	return;
}

static void vnd_sensor_iodev_submit(const struct rtio_sqe *sqe, struct rtio *r,
		uint32_t flags)
{
	struct vnd_sensor_data *data = (struct vnd_sensor_data *) sqe->iodev;
	const struct device *dev = data->dev;

	struct vnd_sensor_msg msg = {
		.sqe = *sqe,
		.r = r,
		.flags = flags,
	};

	if (k_msgq_put(&data->msgq, &msg, K_NO_WAIT) != 0) {
		LOG_ERR("%s: Could not put a msg", dev->name);
	}
}

static void vnd_sensor_handle_int(const struct device *dev)
{
	struct vnd_sensor_data *data = dev->data;
	struct vnd_sensor_msg msg;

	if (k_msgq_get(&data->msgq, &msg, K_NO_WAIT) != 0) {
		LOG_ERR("%s: Could not get a msg", dev->name);
	} else {
		vnd_sensor_iodev_execute(dev, &msg.sqe, msg.r, msg.flags);
	}
}

static void vnd_sensor_timer_expiry(struct k_timer *timer)
{
	struct vnd_sensor_data *data =
		CONTAINER_OF(timer, struct vnd_sensor_data, timer);

	vnd_sensor_handle_int(data->dev);
}

static int vnd_sensor_init(const struct device *dev)
{
	const struct vnd_sensor_config *config = dev->config;
	struct vnd_sensor_data *data = dev->data;
	uint32_t sample_period = config->sample_period;

	data->dev = dev;

	k_msgq_init(&data->msgq, (char *) config->msgs,
		    sizeof(struct vnd_sensor_msg), config->max_msgs);

	k_timer_init(&data->timer, vnd_sensor_timer_expiry, NULL);

	k_timer_start(&data->timer, K_MSEC(sample_period),
		      K_MSEC(sample_period));

	return 0;
}

static const struct rtio_iodev_api vnd_sensor_iodev_api = {
	.submit = vnd_sensor_iodev_submit,
};

#define VND_SENSOR_INIT(n)						\
	static struct vnd_sensor_msg					\
		vnd_sensor_msgs_##n[DT_INST_PROP(n, max_msgs)];		\
									\
	static const struct vnd_sensor_config vnd_sensor_config_##n = {	\
		.sample_period = DT_INST_PROP(n, sample_period),	\
		.msgs = vnd_sensor_msgs_##n,				\
		.max_msgs = DT_INST_PROP(n, max_msgs),			\
	};								\
									\
	static struct vnd_sensor_data vnd_sensor_data_##n = {		\
		.iodev = {						\
			.api = &vnd_sensor_iodev_api,			\
		},							\
	};								\
									\
	DEVICE_DT_INST_DEFINE(n,					\
			      vnd_sensor_init,				\
			      NULL,					\
			      &vnd_sensor_data_##n,			\
			      &vnd_sensor_config_##n,			\
			      POST_KERNEL,				\
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE,	\
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(VND_SENSOR_INIT)
