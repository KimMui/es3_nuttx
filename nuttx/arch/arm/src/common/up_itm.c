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

#include <nuttx/fs/fs.h>
#include <arch/arm/itm.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "chip.h"
#include "up_arch.h"

#define DEMCR           0xe000edfc
#define DEMCR_TRCENA    (1 << 24)

#define ITM_BASE 0xe0000000
#define ITM_STIMULUS_PORT_BASE  (ITM_BASE + 0x0)
#define ITM_TRACE_ENABLE        (ITM_BASE + 0xe00)
#define ITM_TRACE_PRIVILEGE     (ITM_BASE + 0xe40)
#define ITM_TRACE_CTL           (ITM_BASE + 0xe80)
#define ITM_LOCK_ACCESS         (ITM_BASE + 0xfb0)

#define ITM_ACCESS_CODE         0xc5acce55
#define ITM_TRACE_CTL_ITMENA    (1 << 0)

#define write32(x, y) putreg32((y), (x))
#define read32(x) getreg32((x))

static ssize_t itm_write(struct file *filep, const char *buffer, size_t buflen)
{
    int i = 0;
    struct inode *inode = filep->f_inode;
    int port = (int) inode->i_private;

    for (i = 0; i < buflen; i++)
        itm_putc(port, buffer[i]);

    return buflen;
}

static ssize_t itm_read(struct file *filep, char *buffer, size_t buflen)
{
    return -1;
}

void itm_putc(int port_id, char c)
{
    volatile uint32_t *port = (volatile uint32_t*) ITM_STIMULUS_PORT_BASE;

    while (!port[port_id])
        ;

    volatile uint32_t *port0 = port + port_id;
    volatile char *port0_8b = (char*) port0;
    *port0_8b = c;
}

static const struct file_operations itm_ops = {
    .write = itm_write,
    .read = itm_read,
};

static inline void itm_start_configure(void)
{
    write32(ITM_LOCK_ACCESS, ITM_ACCESS_CODE);
}

static inline void itm_end_configure(void)
{
    write32(ITM_LOCK_ACCESS, 0);
}

void itm_enable_port(int port)
{
    itm_start_configure();
    write32(ITM_TRACE_ENABLE, read32(ITM_TRACE_ENABLE) | (1 << port));
    itm_end_configure();
}

void itm_disable_port(int port)
{
    itm_start_configure();
    write32(ITM_TRACE_ENABLE, read32(ITM_TRACE_ENABLE) & ~(1 << port));
    itm_end_configure();
}

void itm_init(void)
{
    read32(DEMCR) |= DEMCR_TRCENA;

    itm_start_configure();
    read32(ITM_TRACE_CTL) |= ITM_TRACE_CTL_ITMENA;
    itm_end_configure();
}

void itm_consoleinit(void)
{
    itm_init();

    itm_enable_port(0);
    register_driver("/dev/console", &itm_ops, 0666, 0);
}
