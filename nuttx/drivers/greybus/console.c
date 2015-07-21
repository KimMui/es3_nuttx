/*
 * Copyright (c) 2015 Google, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <nuttx/greybus/greybus.h>
#include <nuttx/fs/fs.h>
#include <arch/byteorder.h>

#include <string.h>
#include <fcntl.h>

#include "uart-gb.h"

static int console_cport = -1;
static int uart_fd = -1;

#define GB_UART_VERSION_MAJOR   0
#define GB_UART_VERSION_MINOR   1

static int gb_console_open(struct file *filep)
{
    return 0;
}

static ssize_t gb_console_read(struct file *filep, char *buffer, size_t buflen)
{
    while (1);
    return 0;
}

static ssize_t gb_console_write(struct file *filep, const char *buffer,
                                 size_t buflen)

{
    int retval;
    struct gb_operation *op;
    struct gb_uart_receive_data_request *req;

    if (uart_fd < 0) {
        uart_fd = open("/dev/ttyS0", O_RDWR);
    }

    write(uart_fd, buffer, buflen);

    if (console_cport < 0) {
        return buflen;
    }

    op = gb_operation_create(console_cport, GB_UART_PROTOCOL_RECEIVE_DATA,
                             sizeof(*req) + buflen);
    if (!op) {
        return 0;
    }

    req = gb_operation_get_request_payload(op);
    req->size = cpu_to_le16(buflen);
    memcpy(req->data, buffer, buflen);

    retval = gb_operation_send_request(op, NULL, false);
    gb_operation_destroy(op);

    return retval ? 0 : buflen;
}

static uint8_t gb_console_protocol_version(struct gb_operation *operation)
{
    struct gb_uart_proto_version_response *response;

    response = gb_operation_alloc_response(operation, sizeof(*response));
    if (!response)
        return GB_OP_NO_MEMORY;

    response->major = GB_UART_VERSION_MAJOR;
    response->minor = GB_UART_VERSION_MINOR;
    return GB_OP_SUCCESS;
}

static uint8_t gb_console_send_data(struct gb_operation *operation)
{
    return GB_OP_SUCCESS;
}

static struct gb_operation_handler gb_console_handlers[] = {
    GB_HANDLER(GB_UART_PROTOCOL_VERSION, gb_console_protocol_version),
    GB_HANDLER(GB_UART_PROTOCOL_SEND_DATA, gb_console_send_data),
};

static const struct file_operations gb_console_ops = {
    .open = gb_console_open,
    .read = gb_console_read,
    .write = gb_console_write,
};

static struct gb_driver gb_console_driver = {
    .op_handlers = (struct gb_operation_handler*) gb_console_handlers,
    .op_handlers_count = ARRAY_SIZE(gb_console_handlers),
};

void *gb_test(void *data)
{
    while (1) {
        sleep(5);
        printf("%s()\n", __func__);
    }
    return NULL;
}

static pthread_t thread;
void greybus_consoleinit(void)
{
    register_driver("/dev/console", &gb_console_ops, 0666, NULL);
}

void gb_remote_console_register(int cport)
{
    console_cport = cport;
    pthread_create(&thread, NULL, gb_test, NULL);
    gb_register_driver(cport, &gb_console_driver);
}

