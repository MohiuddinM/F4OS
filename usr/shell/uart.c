/*
 * Copyright (C) 2014 F4OS Authors
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
#include <dev/hw/uart.h>
#include <dev/device.h>
#include <kernel/obj.h>
#include <kernel/sched.h>
#include "app.h"

void uart(int argc, char **argv) {
    struct obj *obj;
    struct uart *uart;
    struct uart_ops *ops;
    int baud;

    if (argc < 2 || argc > 3) {
        printf("Usage: uart device [baud]\r\n");
        return;
    }

    obj = device_get(argv[1]);
    if (!obj) {
        fprintf(stderr, "Unable to find device '%s'\r\n", argv[1]);
        return;
    }

    uart = to_uart(obj);
    ops = uart->obj.ops;

    if (argc == 3) {
        int baud = atoi(argv[2]);
        int ret = ops->set_baud_rate(uart, baud);
        if (ret < 0) {
            fprintf(stderr, "Failed to set baud rate: %d\r\n", ret);
            goto out;
        }
    }

    baud = ops->get_baud_rate(uart);
    if (baud < 0) {
        fprintf(stderr, "Failed to get baud rate: %d\r\n", baud);
        goto out;
    }

    printf("Communicating at baud: %d\r\n", baud);

    while (1) {
        char c;
        int ret;

        ret = ops->read(uart, &c, 1);
        if (ret < 0) {
            fprintf(stderr, "Failed to read: %d\r\n", ret);
            goto out;
        }
        else if (ret == 0) {
            /* Nothing to read */
            yield_if_possible();
            continue;
        }

        putc(c);
    }

out:
    obj_put(obj);
}
DEFINE_APP(uart)
