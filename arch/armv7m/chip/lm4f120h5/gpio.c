/*
 * Copyright (C) 2013, 2014 F4OS Authors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <atomic.h>
#include <arch/chip/gpio.h>
#include <arch/chip/registers.h>
#include <dev/hw/gpio.h>
#include <dev/raw_mem.h>
#include <kernel/class.h>

static struct lm4f_gpio lm4f_gpio_decode(uint32_t num) {
    struct lm4f_gpio gpio;

    /* 8 GPIOs/port */
    if (num < LM4F_GPIO_PE0) {
        gpio.port = num/8;
        gpio.pin = num % 8;
    }
    else if (num < LM4F_GPIO_PF0) {
        gpio.port = LM4F_GPIO_PORTE;
        gpio.pin = num - LM4F_GPIO_PE0;
    }
    else {
        gpio.port = LM4F_GPIO_PORTF;
        gpio.pin = num - LM4F_GPIO_PF0;
    }

    return gpio;
}

/*
 * Set to standard reset values:
 * input, digital IO disabled
 */
static int lm4f_gpio_reset(struct gpio *g) {
    struct lm4f_gpio gpio = lm4f_gpio_decode(g->num);

    lm4f_gpio_dir(&gpio, LM4F_GPIO_DIR_IN);
    lm4f_gpio_den(&gpio, 0);

    g->active_low = 0;

    return 0;
}

static int lm4f_gpio_active_low(struct gpio *g, int active_low) {
    /* Force active_low to a binary value */
    g->active_low = !!active_low;

    return 0;
}

static int lm4f_gpio_direction(struct gpio *g, int flags) {
    struct lm4f_gpio gpio = lm4f_gpio_decode(g->num);
    int direction;
    int ret = 0;

    switch (flags & GPIO_DIRECTION_MASK) {
    case GPIO_INPUT:
        direction = LM4F_GPIO_DIR_IN;
        break;
    case GPIO_OUTPUT:
        direction = LM4F_GPIO_DIR_OUT;
        break;
    default:
        ret = -1;
        goto out;
    }

    lm4f_gpio_dir(&gpio, direction);

out:
    return ret;
}

static unsigned int lm4f_gpio_get_io_value(struct gpio *g) {
    struct lm4f_gpio gpio = lm4f_gpio_decode(g->num);
    struct lm4f_gpio_regs *regs = gpio_get_regs(gpio.port);

    /* Bits [9:2] of DATA register address used as bitmask */
    uint32_t reg = raw_mem_read(&regs->DATA + (1 << gpio.pin));

    /* Flip value if active low */
    return ((reg >> gpio.pin) & 1) ^ g->active_low;
}

static int lm4f_gpio_set_output_value(struct gpio *g, unsigned int val) {
    struct lm4f_gpio gpio = lm4f_gpio_decode(g->num);
    struct lm4f_gpio_regs *regs = gpio_get_regs(gpio.port);

    /* Force val to 0 or 1, flip if active low */
    val = !!val ^ g->active_low;

    /* Bits [9:2] of DATA register address used as bitmask */
    raw_mem_write(&regs->DATA + (1 << gpio.pin), val << gpio.pin);

    return 0;
}

/*
 * Enable the approriate clock for gpio.
 */
static inline void lm4f_gpio_clock_enable(struct lm4f_gpio gpio) {
    uint32_t reg;

    /* Enable GPIO Port */
    do {
        reg = load_link(&SYSCTL_RCGCGPIO_R);
        reg |= 1 << gpio.port;
    } while (store_conditional(&SYSCTL_RCGCGPIO_R, reg));

    /* Access with AHB instead of APB */
    do {
        reg = load_link(&SYSCTL_GPIOHBCTL_R);
        reg |= 1 << gpio.port;
    } while (store_conditional(&SYSCTL_GPIOHBCTL_R, reg));
}

/* No custom flags currently supported */
static int lm4f_gpio_set_flags(struct gpio *g, unsigned int flag, int val) {
    return GPIO_ERR_FLG_UNSUPPT;
}

static int lm4f_gpio_dtor(struct gpio *gpio) {
    struct gpio_ops *ops = (struct gpio_ops *)gpio->obj.ops;

    ops->reset(gpio);

    return 0;
}

static struct gpio_ops lm4f_gpio_ops = {
    .reset = lm4f_gpio_reset,
    .active_low = lm4f_gpio_active_low,
    .direction = lm4f_gpio_direction,
    .get_input_value = lm4f_gpio_get_io_value,
    .set_output_value = lm4f_gpio_set_output_value,
    .get_output_value = lm4f_gpio_get_io_value,
    .set_flags = lm4f_gpio_set_flags,
    .dtor = lm4f_gpio_dtor,
};

/*
 * Build name of GPIO in buf.
 * buf must be at least 5 bytes long.
 * Name is in form "PB2"
 */
static void lm4f_gpio_build_name(struct lm4f_gpio gpio, char *buf) {
    buf[0] = 'P';
    buf[1] = 'A' + gpio.port;   /* Ports start at A */

    /* Set pin number.  Always <8, fits in 2 characters */
    itoa(gpio.pin, &buf[2], 2, 10);
}

/* Implement exported interface defined in dev/hw/gpio.h */
int gpio_valid(uint32_t gpio) {
    if (gpio < LM4F_NUM_GPIOS) {
        return 0;
    }

    return GPIO_ERR_INVAL;
}

struct obj *_gpio_instantiate(uint32_t gpio) {
    struct lm4f_gpio g = lm4f_gpio_decode(gpio);

    char name[5] = {'\0'};
    lm4f_gpio_build_name(g, name);

    struct obj *o = instantiate(name, &gpio_class, &lm4f_gpio_ops, struct gpio);
    if (!o) {
        return NULL;
    }

    lm4f_gpio_clock_enable(g);

    /* Enable digital IO */
    lm4f_gpio_den(&g, 1);

    /* Export to the OS */
    class_export_member(o);

    return o;
}
