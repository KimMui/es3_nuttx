/**
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
 *
 * Author: Fabien Parent <fparent@baylibre.com>
 */

#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>

#include <nuttx/config.h>
#include <nuttx/device.h>
#include <nuttx/device_table.h>
#include <nuttx/util.h>
#include <nuttx/usb.h>
#include <nuttx/i2c.h>
#include <nuttx/gpio/tca64xx.h>

#include <arch/tsb/gpio.h>
#include <arch/tsb/device_table.h>
#include <arch/tsb/driver.h>


#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include <arch/tsb/unipro.h>
#include <apps/greybus-utils/utils.h>
#include <apps/ara/service_mgr.h>
#include <apps/ara/gb_loopback.h>

static struct srvmgr_service services[] = {
#if defined(CONFIG_ARA_GB_LOOPBACK)
    {
        .name = "gb_loopback",
        .func = gb_loopback_service,
    },
#endif
    { NULL, NULL }
};

static pthread_t enable_cports_thread;
void *enable_cports_fn(void *data)
{
    enable_cports();
    return NULL;
}

void board_initialize(void)
{
    int ret;

    tsb_gpio_register(NULL);

    tsb_device_table_register();
    tsb_driver_register();

    enable_manifest("IID-1", NULL, 0);
    gb_unipro_init();
    srvmgr_start(services);

    ret = pthread_create(&enable_cports_thread, NULL, enable_cports_fn, NULL);
    if (ret) {
        printf("Can't create enable_cport thread: error %d. Exiting!\n", ret);
        return;
    }

    void module_init(void);
    module_init();
}
