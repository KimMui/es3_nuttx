/*
 * Copyright (c) 2015 Google Inc.
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

#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#define DEBUG_TX
#define INPUT_BUFFER_SIZE 4096
#define TYPE_RESPONSE_FLAG      0x80

static int uart_fd = -1;
static pthread_t thread;
static char in_buffer[INPUT_BUFFER_SIZE];

static void gb_transport_uart_init(void)
{
    if (uart_fd < 0)
        uart_fd = open("/dev/ttyS0", O_RDWR);
}

static int gb_uart_send(unsigned int cport, const void *buf, size_t len)
{
    struct gb_operation_hdr *header = (struct gb_operation_hdr*) buf;

    /* FIXME hack for compatibility with old shim driver */
    header->size = len - sizeof(*header);
    header->type &= ~TYPE_RESPONSE_FLAG;

#ifdef DEBUG_TX
    {
        int i;

        printf("outgoing request - size: %hu, id: %hu, type: %hhu\n",
               header->size, header->id, header->type);
        printf("sending %u bytes\n", len);
        for (i = 0; i < len; i++) {
            printf("%hhX ", ((char*) buf)[i]);
        }
        printf("\n");
    }
#endif

    write(uart_fd, buf, len);
    return 0;
}

static ssize_t gb_uart_recv(void *buf, size_t len)
{
    ssize_t nread = 0;
    size_t length = len;

    while (len) {
        nread = read(uart_fd, buf, len);
        if (nread <= 0)
            continue;

        len -= nread;
        buf = (char*)buf + nread;
    }

    return length;
}

static void *gb_listener(void *data)
{
    ssize_t nread;
    struct gb_operation_hdr header;

    while (1) {
        nread = gb_uart_recv(&header, sizeof(header));
        assert(nread == sizeof(header));
        if (header.size > INPUT_BUFFER_SIZE) {
            printf("incoming request - size: %hu, id: %hu, type: %hhu\n",
               header.size, header.id, header.type);
        }
        assert(header.size <= INPUT_BUFFER_SIZE);

#ifdef DEBUG_RX
        printf("incoming request - size: %hu, id: %hu, type: %hhu\n",
               header.size, header.id, header.type);
#endif

        gb_uart_recv(in_buffer + sizeof(header), header.size);
        header.size += sizeof(header); // FIXME: compat with old shim driver
        memcpy(in_buffer, &header, sizeof(header));

        greybus_rx_handler(0, in_buffer, header.size);
    }
    return NULL;
}

static int gb_unipro_listen(unsigned int cport)
{
    return pthread_create(&thread, NULL, gb_listener, (void*) cport);
}

struct gb_transport_backend gb_uart_backend = {
    .init = gb_transport_uart_init,
    .send = gb_uart_send,
    .listen = gb_unipro_listen,
};

int gb_uart_init(void)
{
    return gb_init(&gb_uart_backend);
}
