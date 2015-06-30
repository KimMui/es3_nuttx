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
 *
 * Author: Fabien Parent <fparent@baylibre.com>
 */

#ifndef __NUTTX_USB_H__
#define __NUTTX_USB_H__

#include <stdint.h>
#include <stddef.h>
#include <semaphore.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>

#include <nuttx/device.h>
#include <arch/atomic.h>

#define DEVICE_TYPE_USB_HCD     "usb-hcd"
#define DEVICE_TYPE_HSIC_DEVICE "hsic-device"

#define USB_URB_GIVEBACK_ASAP           1
#define USB_URB_SEND_ZERO_PACKET        2

#define USB_HOST_DIR_OUT                0
#define USB_HOST_DIR_IN                 0x80

#define USB_HOST_PIPE_ISOCHRONOUS       0
#define USB_HOST_PIPE_INTERRUPT         1
#define USB_HOST_PIPE_CONTROL           2
#define USB_HOST_PIPE_BULK              3

#define usb_host_pipein(pipe)           ((pipe) & USB_HOST_DIR_IN)
#define usb_host_pipeout(pipe)          (!usb_host_pipein(pipe))

#define usb_host_pipedevice(pipe)       (((pipe) >> 8) & 0x7f)
#define usb_host_pipeendpoint(pipe)     (((pipe) >> 15) & 0xf)

#define usb_host_pipetype(pipe)         (((pipe) >> 30) & 3)
#define usb_host_pipeisoc(pipe) \
    (usb_host_pipetype((pipe)) == USB_HOST_PIPE_ISOCHRONOUS)
#define usb_host_pipeint(pipe) \
    (usb_host_pipetype((pipe)) == USB_HOST_PIPE_INTERRUPT)
#define usb_host_pipecontrol(pipe) \
    (usb_host_pipetype((pipe)) == USB_HOST_PIPE_CONTROL)
#define usb_host_pipebulk(pipe) \
    (usb_host_pipetype((pipe)) == USB_HOST_PIPE_BULK)

#define USB_HOST_ENDPOINT_XFER_CONTROL  0
#define USB_HOST_ENDPOINT_XFER_ISOC     1
#define USB_HOST_ENDPOINT_XFER_BULK     2
#define USB_HOST_ENDPOINT_XFER_INT      3

struct urb;
typedef void (*urb_complete_t)(struct urb *urb);

struct urb {
    atomic_t refcount;
    sem_t semaphore;
    urb_complete_t complete;

    void *urb;
    unsigned int pipe;
    unsigned int flags;

    int status;
    int dev_speed;
    int devnum;
    int dev_ttport;

    size_t length;
    size_t actual_length;

    unsigned int maxpacket;

    int interval;

    uint8_t setup_packet[8];
    void *buffer;

    void *hcpriv;
    void *hcpriv_ep;
};

static inline struct urb *urb_create(void) // FIXME
{
    struct urb *urb;

    urb = zalloc(sizeof(*urb));
    if (!urb) {
        return NULL;
    }

    atomic_init(&urb->refcount, 1);
    sem_init(&urb->semaphore, 0, 0);
    return urb;
}

static inline void urb_ref(struct urb *urb)
{
    DEBUGASSERT(urb);
    atomic_inc(&urb->refcount);
}

static inline void urb_unref(struct urb *urb)
{
    if (!urb) {
        return;
    }

    if (atomic_dec(&urb->refcount)) {
        return;
    }

    free(urb);
}

static inline void urb_destroy(struct urb *urb)
{
    urb_unref(urb);
}

struct device_usb_hcd_type_ops {
    int (*start)(struct device *dev);
    void (*stop)(struct device *dev);
    int (*urb_enqueue)(struct device *dev, struct urb *urb);
    int (*urb_dequeue)(struct device *dev, struct urb *urb);
    int (*hub_control)(struct device *dev, uint16_t typeReq, uint16_t wValue,
                       uint16_t wIndex, char *buf, uint16_t wLength);
};

/**
 * Start the USB HCD
 *
 * @param dev HCD device
 * @return 0 if successful
 */
static inline int device_usb_hcd_start(struct device *dev)
{
    DEBUGASSERT(dev);
    DEBUGASSERT(dev->driver && dev->driver->ops &&
                dev->driver->ops->type_ops.usb_hcd);

    if (dev->state != DEVICE_STATE_OPEN) {
        return -ENODEV;
    }

    if (dev->driver->ops->type_ops.usb_hcd->start) {
        return dev->driver->ops->type_ops.usb_hcd->start(dev);
    }

    return -ENOSYS;
}

/**
 * Stop the USB HCD
 *
 * @param dev HCD device
 * @return 0 if successful
 */
static inline void device_usb_hcd_stop(struct device *dev)
{
    DEBUGASSERT(dev);
    DEBUGASSERT(dev->driver && dev->driver->ops &&
                dev->driver->ops->type_ops.usb_hcd);

    if (dev->state != DEVICE_STATE_OPEN) {
        return;
    }

    if (dev->driver->ops->type_ops.usb_hcd->stop) {
        dev->driver->ops->type_ops.usb_hcd->stop(dev);
    }
}

/**
 * Communicate with the root hub
 *
 * @param dev HCD device
 * @return 0 if successful
 */
static inline int device_usb_hcd_hub_control(struct device *dev,
                                             uint16_t typeReq, uint16_t wValue,
                                             uint16_t wIndex, char *buf,
                                             uint16_t wLength)
{
    DEBUGASSERT(dev);
    DEBUGASSERT(dev->driver && dev->driver->ops &&
                dev->driver->ops->type_ops.usb_hcd);

    if (dev->state != DEVICE_STATE_OPEN) {
        return -ENODEV;
    }

    if (dev->driver->ops->type_ops.usb_hcd->hub_control) {
        return dev->driver->ops->type_ops.usb_hcd->hub_control(dev, typeReq,
                                                               wValue, wIndex,
                                                               buf, wLength);
    }

    return -ENOSYS;
}

/**
 * Send urbs
 *
 * @param dev HCD device
 * @param urb URB to send
 * @return 0 if successful
 */
static inline int device_usb_hcd_urb_enqueue(struct device *dev,
                                             struct urb *urb)
{
    DEBUGASSERT(dev);
    DEBUGASSERT(dev->driver && dev->driver->ops &&
                dev->driver->ops->type_ops.usb_hcd);

    if (dev->state != DEVICE_STATE_OPEN) {
        return -ENODEV;
    }

    if (dev->driver->ops->type_ops.usb_hcd->urb_enqueue) {
        return dev->driver->ops->type_ops.usb_hcd->urb_enqueue(dev, urb);
    }

    return -ENOSYS;
}

/**
 * Enqueue urbs
 *
 * @param dev HCD device
 * @param urb URB to send
 * @return 0 if successful
 */
static inline int device_usb_hcd_urb_dequeue(struct device *dev,
                                             struct urb *urb)
{
    DEBUGASSERT(dev);
    DEBUGASSERT(dev->driver && dev->driver->ops &&
                dev->driver->ops->type_ops.usb_hcd);

    if (dev->state != DEVICE_STATE_OPEN) {
        return -ENODEV;
    }

    if (dev->driver->ops->type_ops.usb_hcd->urb_dequeue) {
        return dev->driver->ops->type_ops.usb_hcd->urb_dequeue(dev, urb);
    }

    return -ENOSYS;
}

struct device_hsic_type_ops {
    int (*reset)(struct device *dev);
    int (*hold_reset)(struct device *dev);
    int (*release_reset)(struct device *dev);
};

/**
 * Reset remote HSIC device
 *
 * @param dev HSIC device
 * @return 0 if successful
 */
static inline int device_hsic_reset(struct device *dev)
{
    DEBUGASSERT(dev);
    DEBUGASSERT(dev->driver && dev->driver->ops &&
                dev->driver->ops->type_ops.hsic);

    if (dev->state != DEVICE_STATE_OPEN) {
        return -ENODEV;
    }

    if (dev->driver->ops->type_ops.hsic->reset) {
        return dev->driver->ops->type_ops.hsic->reset(dev);
    }

    return -ENOSYS;
}

/**
 * Hold under reset the remote HSIC device
 *
 * @param dev HSIC device
 * @return 0 if successful
 */
static inline int device_hsic_hold_reset(struct device *dev)
{
    DEBUGASSERT(dev);
    DEBUGASSERT(dev->driver && dev->driver->ops &&
                dev->driver->ops->type_ops.hsic);

    if (dev->state != DEVICE_STATE_OPEN) {
        return -ENODEV;
    }

    if (dev->driver->ops->type_ops.hsic->hold_reset) {
        return dev->driver->ops->type_ops.hsic->hold_reset(dev);
    }

    return -ENOSYS;
}

/**
 * Release from reset the remote HSIC device
 *
 * @param dev HSIC device
 * @return 0 if successful
 */
static inline int device_hsic_release_reset(struct device *dev)
{
    DEBUGASSERT(dev);
    DEBUGASSERT(dev->driver && dev->driver->ops &&
                dev->driver->ops->type_ops.hsic);

    if (dev->state != DEVICE_STATE_OPEN) {
        return -ENODEV;
    }

    if (dev->driver->ops->type_ops.hsic->release_reset) {
        return dev->driver->ops->type_ops.hsic->release_reset(dev);
    }

    return -ENOSYS;
}

#endif /* __NUTTX_USB_H__ */

