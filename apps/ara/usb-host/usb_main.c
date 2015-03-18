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
 */

#include <nuttx/config.h>
#include <nuttx/usb.h>
#include <nuttx/arch.h>
#include <arch/chip/gpio.h>

#include <stdio.h>

static bool test_hub_communication(void)
{
    char buf[64];

    hcd_init();
    hcd_start();

    /**
     * Request: 0xa006 = GetHubDescriptor
     * wValue: 0x2900
     * wIndex: 0
     * wLength: 0xf
     */
    hub_control(0xa006, 0x2900, 0x0000, buf, 0x000f);

    /**
     * Request: 0xa000 = GetHubStatus
     * wValue: 0
     * wIndex: 0
     * wLength: 4
     */
    hub_control(0xa000, 0x0000, 0x0000, buf, 0x0004);

    /**
     * Request: 0x2303 = SetPortFeature
     * wValue: 8 (PORT_POWER)
     * wIndex: 1 (Port 1)
     * wLength: 0
     */
    hub_control(0x2303, 0x0008, 0x0001, buf, 0x0000);
    up_mdelay(1000);

    /**
     * Request: 0xa300 = GetPortStatus
     * wValue: 0
     * wIndex: 1
     * wLength: 4
     *
     * buf = PORT_CONNECTION | PORT_POWER | PORT_HIGH_SPEED
     */
    hub_control(0xa300, 0x0000, 0x0001, buf, 0x0004);

    hcd_exit();

    return *((uint32_t*) buf) != 0;
}

static void print_test_result(const char *name, bool result)
{
    printf("%s: %s\e[m\n", name, result ? "\e[0;32mPASS" : "\e[1;31mFAIL");
}

int usb_host_main(int argc, char **argv)
{
    bool retval;

    tsb_gpio_register();

    printf("Tests:\n");

    retval = test_hub_communication();
    print_test_result("Communication with HUB", retval);
    if (!retval) {
        fprintf(stderr, "ERROR: Cannot communicate with Hub.\n");
        goto end;
    }

    retval = test_hub_communication();
    print_test_result("Reset of the HUB", retval);
    if (!retval) {
        fprintf(stderr, "ERROR: Cannot communicate with Hub "
                        "a second time (RESET_N broken?).\n");
        goto end;
    }

end:
    return 0;
}
