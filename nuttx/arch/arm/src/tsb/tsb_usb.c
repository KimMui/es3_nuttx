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

#include <arch/chip/gpio.h>
#include <nuttx/arch.h>
#include <nuttx/usb.h>
#include <nuttx/gpio.h>

#include <errno.h>
#include <stdio.h>

#include "tsb_scm.h"
#include "up_arch.h"

#include "usb/dwc_otg_hcd_if.h"
#include "usb/dwc_otg_hcd.h"
#include "usb/dwc_otg_driver.h"

#include <arch/arm/semihosting.h> // FIXME: remove once done debugging

#define HUB_LINE_N_RESET 0

#define USB_HOST_DEVEL

#ifndef USB_HOST_DEVEL
#define printf(x...)
#define semihosting_putc(x...)
#endif

static dwc_otg_device_t *g_dev;

#if 0
static dwc_timer_t *hub_timer;
static void hub_status_task(void *data)
{
    int ret;
    char buf[6];

    printf("%s()\n", __func__);

#if 0
    ret = hub_status_data(buf);
    if (ret) {
        hcd_send(RPC_HUB_STATUS, 0xffffffff, buf, sizeof(buf));
    }
#endif
}
#endif

static int _disconnect(dwc_otg_hcd_t *hcd)
{
    return 0;
}

static int _start(dwc_otg_hcd_t *hcd)
{
    hcd_start();
    return 0;
}

static int _hub_info(dwc_otg_hcd_t *hcd, void *urb_handle, uint32_t *hub_addr,
                     uint32_t *port_addr)
{
    struct urb *urb = urb_handle;

    DEBUGASSERT(hub_addr);
    DEBUGASSERT(port_addr);

    if (!urb)
        return -EINVAL;

    *hub_addr = urb->devnum;
    *port_addr = urb->dev_ttport;
    return 0;
}

static int _speed(dwc_otg_hcd_t *hcd, void *urb_handle)
{
    struct urb *urb = urb_handle;
    if (!urb)
        return DWC_OTG_EP_SPEED_HIGH;
    return urb->dev_speed - 1; // FIXME
}

static int _get_b_hnp_enable(dwc_otg_hcd_t *hcd)
{
    return 0;
}

static int _complete(dwc_otg_hcd_t *hcd, void *urb_handle,
             dwc_otg_hcd_urb_t *dwc_urb, int32_t status)
{
    struct urb *urb = urb_handle;

//    semihosting_putc('*');

    DEBUGASSERT(hcd);
    DEBUGASSERT(urb_handle);
    DEBUGASSERT(dwc_urb);

    urb->actual_length = dwc_otg_hcd_urb_get_actual_length(dwc_urb);
    urb->status = status;

    free(dwc_urb);

    DWC_SPINUNLOCK(hcd->lock);
    urb->complete(urb);
//    free(urb); // FIXME unref
    DWC_SPINLOCK(hcd->lock);

    return 0;
}

static struct dwc_otg_hcd_function_ops hcd_fops = {
    .start = _start,
    .disconnect = _disconnect,
    .hub_info = _hub_info,
    .speed = _speed,
    .complete = _complete,
    .get_b_hnp_enable = _get_b_hnp_enable,
};

static int hsic_irq_handler(int irq, void *context)
{
    DEBUGASSERT(g_dev);
    DEBUGASSERT(g_dev->hcd);

//    semihosting_putc('Z');
    dwc_otg_handle_common_intr(g_dev);
    return dwc_otg_hcd_handle_intr(g_dev->hcd);
}

static int hcd_hcd_init(void)
{
    int retval;

    DEBUGASSERT(g_dev);

    g_dev->hcd = dwc_otg_hcd_alloc_hcd();
    if (!g_dev->hcd)
        return -ENOMEM;

    retval = dwc_otg_hcd_init(g_dev->hcd, g_dev->core_if);
    if (retval)
        goto hcd_init_error;

    g_dev->hcd->otg_dev = g_dev;
    dwc_otg_hcd_set_priv_data(g_dev->hcd, NULL);

    return 0;

hcd_init_error:
    dwc_otg_hcd_remove(g_dev->hcd);
    g_dev->hcd = NULL;
    return retval;
}

static int hcd_dev_init(void)
{
    int retval;

    DEBUGASSERT(!g_dev);

    g_dev = malloc(sizeof(*g_dev));
    if (!g_dev)
        return -ENOMEM;

    memset(g_dev, 0, sizeof(*g_dev));
    g_dev->os_dep.reg_offset = 0xFFFFFFFF;
    g_dev->os_dep.base = (void *) HSIC_BASE;

    g_dev->core_if = dwc_otg_cil_init(g_dev->os_dep.base);
    if (!g_dev->core_if) {
        retval = -ENOMEM;
        goto cil_init_error;
    }

    if (set_parameters(g_dev->core_if)) {
        retval = -DWC_E_UNKNOWN;
        goto set_parameter_error;
    }

    dwc_otg_disable_global_interrupts(g_dev->core_if);

    irq_attach(TSB_IRQ_HSIC, hsic_irq_handler);
    g_dev->common_irq_installed = 1;

    dwc_otg_core_init(g_dev->core_if);

    retval = hcd_hcd_init();
    if (retval != 0)
        goto hcd_init_error;

    if (dwc_otg_get_param_adp_enable(g_dev->core_if))
        dwc_otg_adp_start(g_dev->core_if, dwc_otg_is_host_mode(g_dev->core_if));
    else
        dwc_otg_enable_global_interrupts(g_dev->core_if);

    up_enable_irq(TSB_IRQ_HSIC);

    return 0;

hcd_init_error:
    irq_detach(TSB_IRQ_HSIC);
set_parameter_error:
    dwc_otg_cil_remove(g_dev->core_if);
cil_init_error:
    free(g_dev);
    g_dev = NULL;

    return retval;
}

static void turn_off_hsic(void)
{
    up_disable_irq(TSB_IRQ_HSIC);
    irq_detach(TSB_IRQ_HSIC);

    tsb_clk_disable(TSB_CLK_HSIC480);
    tsb_clk_disable(TSB_CLK_HSICBUS);
    tsb_clk_disable(TSB_CLK_HSICREF);

    gpio_direction_out(HUB_LINE_N_RESET, 0);
}

int hcd_init(void)
{
    gpio_activate(HUB_LINE_N_RESET);

    turn_off_hsic();

#if 0
    hub_timer = DWC_TIMER_ALLOC("HUB Status", hub_status_task, NULL);
    DWC_TIMER_SCHEDULE_PERIODIC(hub_timer, 250);    //Time is correct ?
#endif

    return 0;
}

void hcd_exit(void)
{
    DEBUGASSERT(g_dev);
    DEBUGASSERT(g_dev->hcd);
    DEBUGASSERT(g_dev->core_if);

    dwc_otg_hcd_set_priv_data(g_dev->hcd, NULL);
    dwc_otg_hcd_remove(g_dev->hcd);
    dwc_otg_cil_remove(g_dev->core_if);

    free(g_dev);
    g_dev = NULL;

    turn_off_hsic();

    gpio_deactivate(HUB_LINE_N_RESET);
}

int hcd_start(void)
{
    int retval;

#define TSB_SYSCTL_USBOTG_HSIC_CONTROL  (SYSCTL_BASE + 0x500)
#define TSB_HSIC_DPPULLDOWN             (1 << 0)
#define TSB_HSIC_DMPULLDOWN             (1 << 1)
    putreg32(TSB_HSIC_DPPULLDOWN | TSB_HSIC_DMPULLDOWN,
             TSB_SYSCTL_USBOTG_HSIC_CONTROL);

    tsb_clk_enable(TSB_CLK_HSIC480);
    tsb_clk_enable(TSB_CLK_HSICBUS);
    tsb_clk_enable(TSB_CLK_HSICREF);

    tsb_reset(TSB_RST_HSIC);
    tsb_reset(TSB_RST_HSICPHY);
    tsb_reset(TSB_RST_HSICPOR);

    gpio_direction_out(HUB_LINE_N_RESET, 0);

    retval = hcd_dev_init();
    if (retval)
        goto hcd_dev_init_error;

    gpio_direction_out(HUB_LINE_N_RESET, 1);

    retval = dwc_otg_hcd_start(g_dev->hcd, &hcd_fops);
    if (retval)
        goto hcd_start_error;

    dwc_otg_set_hsic_connect(g_dev->hcd->core_if, 1);

    return retval;

hcd_dev_init_error:
hcd_start_error:
    fprintf(stderr, "Could not enable USB HCD\n");

    turn_off_hsic();
    gpio_deactivate(HUB_LINE_N_RESET);
    return retval;
}

void hcd_stop(void)
{
    DEBUGASSERT(g_dev);
    DEBUGASSERT(g_dev->hcd);

    dwc_otg_hcd_stop(g_dev->hcd);
}

int hub_control(uint16_t typeReq, uint16_t wValue, uint16_t wIndex, char *buf,
                uint16_t wLength)
{
    DEBUGASSERT(g_dev);
    DEBUGASSERT(g_dev->hcd);

//    printf("hub_control(0x%hX, 0x%hX, 0x%hX, buf, %hu);\n", typeReq, wValue,
//           wIndex, wLength);

    return dwc_otg_hcd_hub_control(g_dev->hcd, typeReq, wValue, wIndex,
                                   (uint8_t*) buf, wLength);
}

int urb_enqueue(struct urb *urb)
{
    int retval;
    uint8_t ep_type;
    int number_of_packets = 0;
    bool alloc_bandwidth = false;
    dwc_otg_hcd_urb_t *dwc_urb;

    if (usb_host_pipetype(urb->pipe) == USB_HOST_PIPE_ISOCHRONOUS ||
        usb_host_pipetype(urb->pipe) == USB_HOST_PIPE_INTERRUPT) {
        if (!dwc_otg_hcd_is_bandwidth_allocated(g_dev->hcd, &urb->hcpriv_ep)) {
            alloc_bandwidth = true;
            printf("bandwidth not allocated\n");
        }
    }

    printf("memset(urb, 0, sizeof(*urb));\n");
    printf("urb->pipe = %#X;\n", urb->pipe);
    printf("urb->flags =  %#X;\n", urb->flags);
    printf("urb->length = %#X;\n", urb->length);
    printf("urb->maxpacket = %#X;\n", urb->maxpacket);
    printf("urb->bw = %#X;\n", urb->bw);
    printf("urb->interval = %#X;\n", urb->interval);
    printf("urb->setup_packet[8] = {%#hhX, %#hhX, %#hhX, %#hhX,"
           "                        %#hhX, %#hhX, %#hhX, %#hhX};\n",
           urb->setup_packet[0], urb->setup_packet[1], urb->setup_packet[2],
           urb->setup_packet[3], urb->setup_packet[4], urb->setup_packet[5],
           urb->setup_packet[6], urb->setup_packet[7]);
    printf("urb->complete = urb_complete;\n");
    printf("urb_enqueue(urb);\n");

    //printf("pipe: %u\n", usb_host_pipetype(urb->pipe));

    switch (usb_host_pipetype(urb->pipe)) {
    case USB_HOST_PIPE_CONTROL:
        ep_type = USB_HOST_ENDPOINT_XFER_CONTROL;
        break;

    case USB_HOST_PIPE_ISOCHRONOUS:
        ep_type = USB_HOST_ENDPOINT_XFER_ISOC;
        break;

    case USB_HOST_PIPE_BULK:
        ep_type = USB_HOST_ENDPOINT_XFER_BULK;
        break;

    case USB_HOST_PIPE_INTERRUPT:
        ep_type = USB_HOST_ENDPOINT_XFER_INT;
        break;

    default:
        fprintf(stderr, "Invalid EP Type\n");
        return -EINVAL;
    }

    dwc_urb = dwc_otg_hcd_urb_alloc(g_dev->hcd, number_of_packets, 1);
    if (!dwc_urb)
        return -ENOMEM;

    urb->hcpriv = dwc_urb;

    char *setup = (char*) &urb->setup_packet;;
    char *buffer = urb->buffer;
#if 1
    if (usb_host_pipetype(urb->pipe) == USB_HOST_PIPE_INTERRUPT) {
        buffer = NULL;
        urb->length = 0;
        setup = NULL;
    }
#endif

    dwc_otg_hcd_urb_set_pipeinfo(dwc_urb,
                                 usb_host_pipedevice(urb->pipe),
                                 usb_host_pipeendpoint(urb->pipe), ep_type,
                                 usb_host_pipein(urb->pipe),
                                 urb->maxpacket);

    dwc_otg_hcd_urb_set_params(dwc_urb, urb, buffer, (dwc_dma_t) buffer,
                               urb->length, setup, (dwc_dma_t) setup,
                               urb->flags, urb->interval);

    retval = dwc_otg_hcd_urb_enqueue(g_dev->hcd, dwc_urb, &urb->hcpriv_ep, 1);
    if (retval)
        return retval;

    if (retval && alloc_bandwidth)
        urb->bw = dwc_otg_hcd_get_ep_bandwidth(g_dev->hcd, urb->hcpriv_ep);

    return retval;
}

int urb_dequeue(struct urb *urb)
{
    DEBUGASSERT(g_dev);
    DEBUGASSERT(g_dev->hcd);

    dwc_otg_hcd_urb_dequeue(g_dev->hcd, urb->hcpriv);

    return 0;
}

int endpoint_disable(void *ep)
{
    DEBUGASSERT(g_dev);
    DEBUGASSERT(g_dev->hcd);

    dwc_otg_hcd_endpoint_disable(g_dev->hcd, ep, /* retry = */ 0);
//    ep->hcpriv = NULL;
    return 0;
}
