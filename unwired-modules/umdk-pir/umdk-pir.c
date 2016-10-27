/*
 * Copyright (C) 2016 Unwired Devices [info@unwds.com]
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    
 * @ingroup     
 * @brief       
 * @{
 * @file		umdk-pir.c
 * @brief       umdk-pir module implementation
 * @author      MC
 * @author		Evgeniy Ponomarev
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "periph/gpio.h"

#include "board.h"

#include "unwds-common.h"
#include "umdk-pir.h"

#include "thread.h"
#include "xtimer.h"

static kernel_pid_t handler_pid;

static msg_t pir;

static int last_pressed[4] = { 0, };

static uwnds_cb_t *callback;

static void *handler(void *arg) {
    msg_t msg;
    msg_t msg_queue[128];
    msg_init_queue(msg_queue, 128);

    while (1) {
        msg_receive(&msg);
        int val = msg.content.value;

        module_data_t data;
        data.length = 2;
        data.data[0] = UNWDS_PIR_MODULE_ID;
        data.data[1] = val;

        callback(&data);
    }

	return NULL;
}

static void pir_rising_cb(void *arg) {
	(void) arg;

    int now = xtimer_now();
    /* Don't accept a press of current button if it did occur earlier than last press plus debouncing time */
    if (now - last_pressed[0] <= UMDK_PIR_DEBOUNCE_TIME_MS * 1000) {
    	last_pressed[0] = now;
    	return;
	}
    last_pressed[0] = now;

	msg_send_int(&pir, handler_pid);
}

void umdk_pir_init(uint32_t *non_gpio_pin_map, uwnds_cb_t *event_callback) {
	(void) non_gpio_pin_map;

	callback = event_callback;

	/* Prepare event messages */
	pir.content.value = 1;

	/* Initialize interrupts */
	gpio_init_int(UMDK_PIR, GPIO_IN_PD, GPIO_RISING, pir_rising_cb, NULL);

	/* Create handler thread */
	char *stack = (char *) allocate_stack();
	if (!stack) {
		puts("umdk-pir: unable to allocate memory. Is too many modules enabled?");
		return;
	}

	handler_pid = thread_create(stack, UNWDS_STACK_SIZE_BYTES, THREAD_PRIORITY_MAIN - 1, 0, handler, NULL, "PIR thread");
}

bool umdk_pir_cmd(module_data_t *data, module_data_t *reply) {
	return false;
}

#ifdef __cplusplus
}
#endif
