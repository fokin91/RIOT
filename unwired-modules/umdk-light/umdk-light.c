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
 * @file		umdk-light.c
 * @brief       umdk-light module implementation
 * @author      Oleg Artamonov <info@unwds.com>
 */

#ifdef __cplusplus
extern "C" {
#endif

/* define is autogenerated, do not change */
#undef _UMDK_MID_
#define _UMDK_MID_ UNWDS_LIGHT_MODULE_ID

/* define is autogenerated, do not change */
#undef _UMDK_NAME_
#define _UMDK_NAME_ "light"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "periph/gpio.h"
#include "periph/i2c.h"

#include "board.h"

#include "opt3001.h"

#include "unwds-common.h"
#include "umdk-light.h"

#include "thread.h"
#include "rtctimers-millis.h"

static opt3001_t dev_opt3001;

static uwnds_cb_t *callback;

static kernel_pid_t timer_pid;

static msg_t timer_msg = {};
static rtctimers_millis_t timer;

static bool is_polled = false;

typedef enum {
    UMDK_LIGHT_OPT3001  = 1,
} umdk_light_active_sensors_t;

static uint8_t active_sensors = 0;

static struct {
	uint8_t publish_period_min;
	uint8_t i2c_dev;
} light_config;

static bool init_sensor(void) {
	dev_opt3001.i2c = UMDK_LIGHT_I2C;

	printf("[umdk-" _UMDK_NAME_ "] Initializing LIGHT on I2C #%d\n", dev_opt3001.i2c);

    if (opt3001_init(&dev_opt3001) == 0) {
        puts("[umdk-" _UMDK_NAME_ "] TI OPT3001 sensor found");
        active_sensors |= UMDK_LIGHT_OPT3001;
    }
    
	return (active_sensors != 0);
}

static void prepare_result(module_data_t *data) {
    uint16_t luminocity;
    
    if (active_sensors & UMDK_LIGHT_OPT3001) {
        opt3001_measure_t measure = {};
        opt3001_measure(&dev_opt3001, &measure);
        
        /* OPT3001 reports luminocity as 4-bit value, 83865 lux maximum */
        /* let's limit it to 2-bit and 65535 lux for practical purposes */
        /* no need to precisely measure direct sunlight */
        if (measure.luminocity > UINT16_MAX)
        {
           luminocity = UINT16_MAX; 
        } else {
           luminocity = measure.luminocity;
        }
    }

	printf("[umdk-" _UMDK_NAME_ "] Luminocity %u lux\n", luminocity);
    
    if (data) {
        data->data[0] = _UMDK_MID_;
        data->data[1] = UMDK_LIGHT_DATA;
        data->length = 2;
        
        /* Copy measurements into response */
        convert_to_be_sam((void *)&luminocity, sizeof(luminocity));
        memcpy(&data->data[data->length], (uint8_t *) &luminocity, sizeof(luminocity));
        data->length += sizeof(luminocity);
    }
}

static void *timer_thread(void *arg) {
    (void)arg;
    
    msg_t msg;
    msg_t msg_queue[4];
    msg_init_queue(msg_queue, 4);
    
    puts("[umdk-" _UMDK_NAME_ "] Periodic publisher thread started");

    while (1) {
        msg_receive(&msg);

        module_data_t data = {};
        data.as_ack = is_polled;
        is_polled = false;

        prepare_result(&data);

        /* Notify the application */
        callback(&data);

        /* Restart after delay */
        rtctimers_millis_set_msg(&timer, 60000 * light_config.publish_period_min, &timer_msg, timer_pid);
    }

    return NULL;
}

static void reset_config(void) {
	light_config.publish_period_min = UMDK_LIGHT_PUBLISH_PERIOD_MIN;
	light_config.i2c_dev = UMDK_LIGHT_I2C;
}

static void init_config(void) {
	reset_config();

	if (!unwds_read_nvram_config(_UMDK_MID_, (uint8_t *) &light_config, sizeof(light_config)))
		reset_config();

	if (light_config.i2c_dev >= I2C_NUMOF) {
		reset_config();
		return;
	}
}

static inline void save_config(void) {
	unwds_write_nvram_config(_UMDK_MID_, (uint8_t *) &light_config, sizeof(light_config));
}

static void set_period (int period) {
    rtctimers_millis_remove(&timer);

    light_config.publish_period_min = period;
	save_config();

	/* Don't restart timer if new period is zero */
	if (light_config.publish_period_min) {
        rtctimers_millis_set_msg(&timer, 60000 * light_config.publish_period_min, &timer_msg, timer_pid);
		printf("[umdk-" _UMDK_NAME_ "] Period set to %d minute (s)\n", light_config.publish_period_min);
    } else {
        puts("[umdk-" _UMDK_NAME_ "] Timer stopped");
    }
}

int umdk_light_shell_cmd(int argc, char **argv) {
    if (argc == 1) {
        puts (_UMDK_NAME_ " get - get results now");
        puts (_UMDK_NAME_ " send - get and send results now");
        puts (_UMDK_NAME_ " period <N> - set period to N minutes");
        puts (_UMDK_NAME_ " reset - reset settings to default");
        return 0;
    }
    
    char *cmd = argv[1];
	
    if (strcmp(cmd, "get") == 0) {
        prepare_result(NULL);
    }
    
    if (strcmp(cmd, "send") == 0) {
		/* Send signal to publisher thread */
		msg_send(&timer_msg, timer_pid);
    }
    
    if (strcmp(cmd, "period") == 0) {
        char *val = argv[2];
        set_period(atoi(val));
    }
    
    if (strcmp(cmd, "reset") == 0) {
        reset_config();
        save_config();
    }
    
    return 1;
}

static void btn_connect(void *arg) {
    (void)arg;
    
    is_polled = false;
    msg_send(&timer_msg, timer_pid);
}

void umdk_light_init(uint32_t *non_gpio_pin_map, uwnds_cb_t *event_callback) {
	(void) non_gpio_pin_map;

	callback = event_callback;

	init_config();
	printf("[umdk-" _UMDK_NAME_ "] Publish period: %d min\n", light_config.publish_period_min);

	if (!init_sensor()) {
		puts("[umdk-" _UMDK_NAME_ "] Unable to init sensor!");
        return;
	}

	/* Create handler thread */
	char *stack = (char *) allocate_stack(UMDK_LIGHT_STACK_SIZE);
	if (!stack) {
		return;
	}
    
    unwds_add_shell_command( _UMDK_NAME_, "type '" _UMDK_NAME_ "' for commands list", umdk_light_shell_cmd);

#ifdef UNWD_CONNECT_BTN
    if (UNWD_USE_CONNECT_BTN) {
        gpio_init_int(UNWD_CONNECT_BTN, GPIO_IN_PU, GPIO_FALLING, btn_connect, NULL);
    }
#endif
    
	timer_pid = thread_create(stack, UMDK_LIGHT_STACK_SIZE, THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST, timer_thread, NULL, "opt3001 thread");

    /* Start publishing timer */
	rtctimers_millis_set_msg(&timer, 60000 * light_config.publish_period_min, &timer_msg, timer_pid);
}

static void reply_fail(module_data_t *reply) {
	reply->length = 2;
	reply->data[0] = _UMDK_MID_;
	reply->data[1] = UMDK_LIGHT_CMD_FAIL;
}

static void reply_ok(module_data_t *reply) {
	reply->length = 2;
	reply->data[0] = _UMDK_MID_;
	reply->data[1] = UMDK_LIGHT_CMD_COMMAND;
}

bool umdk_light_cmd(module_data_t *cmd, module_data_t *reply) {
	if (cmd->length < 1) {
		reply_fail(reply);
		return true;
	}

	umdk_light_cmd_t c = cmd->data[0];
	switch (c) {
	case UMDK_LIGHT_CMD_COMMAND: {
		if (cmd->length != 2) {
			reply_fail(reply);
			break;
		}

		uint8_t period = cmd->data[1];
		set_period(period);

		reply_ok(reply);
		break;
	}

	case UMDK_LIGHT_CMD_POLL:
		is_polled = true;

		/* Send signal to publisher thread */
		msg_send(&timer_msg, timer_pid);

		return false; /* Don't reply */

	default:
		reply_fail(reply);
		break;
	}

	return true;
}

#ifdef __cplusplus
}
#endif