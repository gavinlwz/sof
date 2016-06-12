/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/dai.h>
#include <reef/ipc.h>
#include <platform/clk.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>

/* convenience component UUIDs and descriptors */
#define SPIPE_MIXER {COMP_TYPE_MIXER, 0, 0}
#define SPIPE_MUX {COMP_TYPE_MUX, 0, 0}
#define SPIPE_VOLUME(xindex) {COMP_TYPE_VOLUME, xindex, 0}
#define SPIPE_SWITCH {COMP_TYPE_SWITCH, 0, 0}
#define SPIPE_DAI_SSP(xindex) {COMP_TYPE_DAI_SSP, xindex, 0}
#define SPIPE_HOST(xindex) {COMP_TYPE_HOST, xindex, 0}

/* convenience buffer descriptors */
#define SPIPE_BUFFER(xsize, xisize, xinum, xiirq, xosize, xonum, xoirq) \
	{.buffer = {.size = xsize, \
	.sink_period = {.size = xisize, .number = xinum, .no_irq = xiirq}, \
	.source_period = {.size = xosize, .number = xonum, .no_irq = xoirq},},}

/* Host facing buffer */
#define SPIPE_HOST_BUF \
	SPIPE_BUFFER(PLAT_HOST_PERSIZE * PLAT_HOST_PERIODS, \
		PLAT_HOST_PERSIZE, PLAT_HOST_PERIODS, 0, \
		PLAT_HOST_PERSIZE, PLAT_HOST_PERIODS, 0)

/* Device facing buffer */
#define SPIPE_DEV_BUF \
	SPIPE_BUFFER(PLAT_DEV_PERSIZE * PLAT_DEV_PERIODS, \
		PLAT_DEV_PERSIZE, PLAT_DEV_PERIODS, 0, \
		PLAT_DEV_PERSIZE, PLAT_DEV_PERIODS, 0)

struct spipe_comp {
	uint32_t type;
	uint32_t index;
	uint32_t id;
};

struct spipe_buffer {
	struct buffer_desc buffer;
	uint32_t id;
};

/* static link between components using UUIDs and IDs */
struct spipe_link {
	struct spipe_comp *source;
	struct spipe_buffer *buffer;
	struct spipe_comp *sink;
};

#if 0
static struct spipe_comp host_p = SPIPE_HOST(0);
static struct spipe_comp host_c = SPIPE_HOST(0);
static struct spipe_comp volume_p = SPIPE_VOLUME(0);
static struct spipe_comp volume_c = SPIPE_VOLUME(0);
static struct spipe_comp ssp_p = SPIPE_DAI_SSP(0);
static struct spipe_comp ssp_c = SPIPE_DAI_SSP(0);

static struct spipe_buffer host_buf_p = SPIPE_HOST_BUF;
static struct spipe_buffer host_buf_c = SPIPE_HOST_BUF;
static struct spipe_buffer dev_buf_p = SPIPE_DEV_BUF;
static struct spipe_buffer dev_buf_c = SPIPE_DEV_BUF;

/*
 * Straight through playback and capture pipes with simple volume.
 * 
 * host PCM0(0) ---> B0 ---> volume(1) ---> B1 ---> SSP0(2)
 * host PCM0(0) <--- B2 <--- volume(3) <--- B3 <--- SSP0(2)
 */

static struct spipe_comp *pipe0_play_comps[] = {
	&host_p,	/* ID = 0 */
	&volume_p,	/* ID = 1 */
	&ssp_p,		/* ID = 2 */
};

static struct spipe_comp *pipe0_capt_comps[] = {
	&ssp_c,		/* ID = 3 */
	&volume_c,	/* ID = 4 */
	&host_c,	/* ID = 5 */
};

static struct spipe_buffer *pipe0_buffers[] = {
	&host_buf_p,	/* ID = 6 */
	&host_buf_c,	/* ID = 7 */
	&dev_buf_p,	/* ID = 8 */
	&dev_buf_c,	/* ID = 9 */
};

static struct spipe_link pipe_play0[] = {
	{&host_p, &host_buf_p, &volume_p},
	{&volume_p, &dev_buf_p, &ssp_p},
};


static struct spipe_link pipe_capture0[] = {
	{&ssp_c, &dev_buf_c, &volume_c},
	{&volume_c, &host_buf_c, &host_c},
};
#endif

#if 1
static struct spipe_comp pipe1_play_comps[] = {
	SPIPE_HOST(0),		/* ID = 0 */
	SPIPE_VOLUME(0),	/* ID = 1 */
	SPIPE_HOST(1),		/* ID = 2 */
	SPIPE_VOLUME(1),	/* ID = 3 */
	SPIPE_MIXER,		/* ID = 4 */
	SPIPE_VOLUME(2),	/* ID = 5 */
	SPIPE_DAI_SSP(0),	/* ID = 6 */
};

static struct spipe_comp pipe1_capt_comps[] = {
	SPIPE_DAI_SSP(0),	/* ID = 7 */
	SPIPE_VOLUME(3),	/* ID = 8 */
	SPIPE_HOST(2),		/* ID = 9 */
};

static struct spipe_buffer pipe1_buffers[] = {
	SPIPE_HOST_BUF,	/* B0 */
	SPIPE_HOST_BUF,	/* B1 */
	SPIPE_HOST_BUF,	/* B2 */
	SPIPE_HOST_BUF,	/* B3 */
	SPIPE_HOST_BUF,	/* B4 */
	SPIPE_DEV_BUF,	/* B5 */
	SPIPE_DEV_BUF,	/* B6 */
	SPIPE_HOST_BUF, /* B7 */
};

/* 
 * Two PCMs mixed into single SSP output.
 *
 * host PCM0(0) ---> volume(1) ---+
 *                                |mixer(4) --> volume(5) ---> SSP0(6)
 * host PCM1(2) ---> volume(3) ---+
 *
 * host PCM0(9) <--- volume(8) <--- SSP0(7)
 */
static struct spipe_link pipe_play1[] = {
	{&pipe1_play_comps[0], &pipe1_buffers[0], &pipe1_play_comps[1]},
	{&pipe1_play_comps[2], &pipe1_buffers[1], &pipe1_play_comps[3]},
	{&pipe1_play_comps[1], &pipe1_buffers[2], &pipe1_play_comps[4]},
	{&pipe1_play_comps[3], &pipe1_buffers[3], &pipe1_play_comps[4]},
	{&pipe1_play_comps[4], &pipe1_buffers[4], &pipe1_play_comps[5]},
	{&pipe1_play_comps[5], &pipe1_buffers[5], &pipe1_play_comps[6]},
};

static struct spipe_link pipe_capture1[] = {
	{&pipe1_capt_comps[0], &pipe1_buffers[6], &pipe1_capt_comps[1]},
	{&pipe1_capt_comps[1], &pipe1_buffers[7], &pipe1_capt_comps[2]},
};
#endif

#if 0
/*
 * Default BDW pipelines with Linux FW Lite
 *
 * host PCM0(0) ---> volume(4) ---+
 *                                |
 * host PCM1(1) ---> volume(5) ---+ mixer(9) -+-> volume(10) ---> SSP0(11)
 *                                |           |
 * host PCM2(2) ---> volume(6) ---+           |
 *                                            |
 * host PCM3(3) <--- volume(7) <--------------+
 *
 * host PCM0(0) <--- volume(8) <--- SSP0(11)
 */
static struct spipe_link pipe_play2[] = {
	{SPIPE_HOST(0), SPIPE_VOLUME(4), SPIPE_MIXER(9)},
	{SPIPE_HOST(1), SPIPE_VOLUME(5), SPIPE_MIXER(9)},
	{SPIPE_HOST(2), SPIPE_VOLUME(6), SPIPE_MIXER(9)},
	{SPIPE_MIXER(9), SPIPE_VOLUME(10), SPIPE_DAI_SSP0(11)},
	{SPIPE_MIXER(9), SPIPE_VOLUME(7), SPIPE_HOST(3)},
};

static struct spipe_link pipe_capture2[] = {
	{SPIPE_DAI_SSP0(11), SPIPE_VOLUME(8), SPIPE_HOST(0)},
};
#endif

/* static pipeline ID */
struct pipeline *pipeline_static;
uint32_t pipeline_id = 0;

struct pipeline *init_static_pipeline(void)
{
	int i, err;

	/* init system pipeline core */
	err = pipeline_init();
	if (err < 0)
		return NULL;

	/* create the pipeline */
	pipeline_static = pipeline_new(pipeline_id);
	if (pipeline_static < 0)
		return NULL;

	// TODO: clock and DAI should come from topology
	// TODO: rate should come from platform.h
	pipeline_set_work_freq(pipeline_static, 1000, CLK_SSP);

#if 0
	/* create playback components in the pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe0_play_comps); i++) {
		pipe0_play_comps[i]->id = ipc_comp_new(pipeline_id,
			pipe0_play_comps[i]->type, pipe0_play_comps[i]->index,
			STREAM_DIRECTION_PLAYBACK);
		if (pipe0_play_comps[i]->id < 0)
			goto error;
	}

	/* create capture components in the pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe0_capt_comps); i++) {
		pipe0_capt_comps[i]->id = ipc_comp_new(pipeline_id,
			pipe0_capt_comps[i]->type, pipe0_capt_comps[i]->index,
			STREAM_DIRECTION_CAPTURE);
		if (pipe0_capt_comps[i]->id < 0)
			goto error;
	}

	/* create buffers in the pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe0_buffers); i++) {
		pipe0_buffers[i]->id = ipc_buffer_new(pipeline_id,
			&pipe0_buffers[i]->buffer);
		if (pipe0_buffers[i]->id < 0)
			goto error;
	}

	/* create components on playback pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_play0); i++) {
		/* add source -> sink */
		err = ipc_comp_connect(pipe_play0[i].source->id,
			pipe_play0[i].sink->id,
			pipe_play0[i].buffer->id);
		if (err < 0)
			goto error;
	}

	/* create components on capture pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_capture0); i++) {
		/* add source -> sink */
		err = ipc_comp_connect(pipe_capture0[i].source->id,
			pipe_capture0[i].sink->id,
			pipe_capture0[i].buffer->id);
		if (err < 0)
			goto error;
	}
#else
	/* create playback components in the pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe1_play_comps); i++) {
		pipe1_play_comps[i].id = ipc_comp_new(pipeline_id,
			pipe1_play_comps[i].type, pipe1_play_comps[i].index,
			STREAM_DIRECTION_PLAYBACK);
		if (pipe1_play_comps[i].id < 0)
			goto error;
	}

	/* create capture components in the pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe1_capt_comps); i++) {
		pipe1_capt_comps[i].id = ipc_comp_new(pipeline_id,
			pipe1_capt_comps[i].type, pipe1_capt_comps[i].index,
			STREAM_DIRECTION_CAPTURE);
		if (pipe1_capt_comps[i].id < 0)
			goto error;
	}

	/* create buffers in the pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe1_buffers); i++) {
		pipe1_buffers[i].id = ipc_buffer_new(pipeline_id,
			&pipe1_buffers[i].buffer);
		if (pipe1_buffers[i].id < 0)
			goto error;
	}

	/* connect components on playback pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_play1); i++) {
		/* add source -> sink */
		err = ipc_comp_connect(pipe_play1[i].source->id,
			pipe_play1[i].sink->id,
			pipe_play1[i].buffer->id);
		if (err < 0)
			goto error;
	}

	/* create components on capture pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe_capture1); i++) {
		/* add source -> sink */
		err = ipc_comp_connect(pipe_capture1[i].source->id,
			pipe_capture1[i].sink->id,
			pipe_capture1[i].buffer->id);
		if (err < 0)
			goto error;
	}
#endif

	/* pipeline now ready for params, prepare and cmds */
	return pipeline_static;

error:
	trace_pipe_error("ePS");
	pipeline_free(pipeline_static);
	return NULL;
}
