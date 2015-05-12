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

#include <arch/byteorder.h>
#include <nuttx/greybus/greybus.h>
#include <nuttx/usb.h>
#include "usb-gb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(GB_USB_DEBUG)
#define gb_usb_debug(x...) printf(x)
#else
#define gb_usb_debug(x...)
#endif

static struct device *usbdev;
static unsigned int usb_cport;

static uint8_t gb_usb_protocol_version(struct gb_operation *operation)
{
    struct gb_usb_proto_version_response *response;

    response = gb_operation_alloc_response(operation, sizeof(*response));
    if (!response) {
        return GB_OP_NO_MEMORY;
    }

    response->major = GB_USB_VERSION_MAJOR;
    response->minor = GB_USB_VERSION_MINOR;
    return GB_OP_SUCCESS;
}

static uint8_t gb_usb_hcd_stop(struct gb_operation *operation)
{
    gb_usb_debug("%s()\n", __func__);

    device_usb_hcd_stop(usbdev);

    return GB_OP_SUCCESS;
}

static uint8_t gb_usb_hcd_start(struct gb_operation *operation)
{
    int retval;

    gb_usb_debug("%s()\n", __func__);

    retval = device_usb_hcd_start(usbdev);
    if (retval) {
        return GB_OP_UNKNOWN_ERROR;
    }

    return GB_OP_SUCCESS;
}


static void gb_usb_urb_complete(struct urb *urb)
{
#if defined(ASYNC_URB_ENQUEUE)
    struct gb_operation *operation;
    struct gb_usb_urb_completion_request *comp;

    operation = gb_operation_create(usb_cport, GB_USB_TYPE_URB_COMPLETION,
                                    sizeof(*comp) + urb->actual_length);
    if (!operation) {
        goto end;
    }

    comp = gb_operation_get_request_payload(operation);
    comp->actual_length = cpu_to_le32(urb->actual_length);
    comp->status = cpu_to_le32(urb->status);
    memcpy(comp->payload, urb->buffer, urb->actual_length);

    gb_operation_send_request(operation, NULL, false);

end:
    free(urb->buffer);
    free(urb);
#else
    sem_post(&urb->semaphore);
#endif
}

static uint8_t gb_usb_urb_enqueue(struct gb_operation *operation)
{
    int retval;
    int op_status = GB_OP_SUCCESS;
    struct urb *urb;
    struct gb_usb_urb_enqueue_response *response;
    struct gb_usb_urb_enqueue_request *request =
        gb_operation_get_request_payload(operation);
    uint32_t transfer_buffer_length;
    uint32_t transfer_flags;

    gb_usb_debug("device_usb_hcd_urb_enqueue(usbdev, urb);\n");

    if (gb_operation_get_request_payload_size(operation) < sizeof(*request)) {
        return GB_OP_INVALID;
    }

    urb = urb_create();
    if (!urb) {
        return GB_OP_NO_MEMORY;
    }

    urb->pipe = le32_to_cpu(request->pipe);
    urb->length = le32_to_cpu(request->transfer_buffer_length);
    urb->maxpacket = le32_to_cpu(request->maxpacket);
    urb->interval = le32_to_cpu(request->interval);
    urb->dev_speed = request->dev_speed;
    urb->dev_ttport = le32_to_cpu(request->dev_ttport);
    urb->devnum = le32_to_cpu(request->devnum);
    urb->complete = gb_usb_urb_complete;
    memcpy(urb->setup_packet, request->setup_packet, sizeof(urb->setup_packet));

    transfer_buffer_length = le32_to_cpu(request->transfer_buffer_length);
    transfer_flags = le32_to_cpu(request->transfer_flags);

    if (transfer_buffer_length) {
        urb->buffer = malloc(transfer_buffer_length);
        if (!urb->buffer) {
            op_status = GB_OP_NO_MEMORY;
            goto error;
        }

        memcpy(urb->buffer, request->payload, transfer_buffer_length);
    }

    if (!(transfer_flags & GB_USB_HOST_URB_NO_INTERRUPT)) {
        urb->flags |= USB_URB_GIVEBACK_ASAP;
    }

    if (transfer_flags & GB_USB_HOST_URB_ZERO_PACKET) {
        urb->flags |= USB_URB_SEND_ZERO_PACKET;
    }

    retval = device_usb_hcd_urb_enqueue(usbdev, urb);
    if (retval) {
        op_status = GB_OP_UNKNOWN_ERROR;
        goto error;
    }

#if defined(ASYNC_URB_ENQUEUE)
    return op_status;
#else
    sem_wait(&urb->semaphore);

    response =
        gb_operation_alloc_response(operation,
                                    sizeof(*response) + urb->actual_length);
    if (!response) {
        op_status = GB_OP_NO_MEMORY;
        goto error;
    }

    memset(response, 0, sizeof(*response) + urb->actual_length);

    response->actual_length = urb->actual_length;
    response->status = urb->status;
    memcpy(response->payload, urb->buffer, urb->actual_length);
#endif

error:
    free(urb->buffer);
    urb_destroy(urb);

    return op_status;
}

static uint8_t gb_usb_hub_control(struct gb_operation *operation)
{
    struct gb_usb_hub_control_response *response;
    struct gb_usb_hub_control_request *request =
        gb_operation_get_request_payload(operation);
    uint16_t typeReq;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
    int status;

    if (gb_operation_get_request_payload_size(operation) < sizeof(*request)) {
        return GB_OP_INVALID;
    }

    typeReq = le16_to_cpu(request->typeReq);
    wValue = le16_to_cpu(request->wValue);
    wIndex = le16_to_cpu(request->wIndex);
    wLength = le16_to_cpu(request->wLength);

    response =
        gb_operation_alloc_response(operation, sizeof(*response) + wLength);
    if (!response) {
        return GB_OP_NO_MEMORY;
    }

    gb_usb_debug("%s(%hX, %hX, %hX, %hX)\n", __func__, typeReq, wValue, wIndex,
                 wLength);

    status = device_usb_hcd_hub_control(usbdev, typeReq, wValue,  wIndex,
                                        (char*) response->buf, wLength);
    if (status) {
        return GB_OP_UNKNOWN_ERROR;
    }

    return GB_OP_SUCCESS;
}

static int gb_usb_init(unsigned int cport)
{
    usbdev = device_open(DEVICE_TYPE_USB_HCD, 0);
    if (!usbdev) {
        return -ENODEV;
    }

    return 0;
}

static void gb_usb_exit(unsigned int cport)
{
    if (usbdev) {
        device_close(usbdev);
    }
}

static struct gb_operation_handler gb_usb_handlers[] = {
    GB_HANDLER(GB_USB_TYPE_PROTOCOL_VERSION, gb_usb_protocol_version),
    GB_HANDLER(GB_USB_TYPE_HCD_STOP, gb_usb_hcd_stop),
    GB_HANDLER(GB_USB_TYPE_HCD_START, gb_usb_hcd_start),
    GB_HANDLER(GB_USB_TYPE_URB_ENQUEUE, gb_usb_urb_enqueue),
    GB_HANDLER(GB_USB_TYPE_HUB_CONTROL, gb_usb_hub_control),
};

struct gb_driver usb_driver = {
    .op_handlers = (struct gb_operation_handler*) gb_usb_handlers,
    .op_handlers_count = ARRAY_SIZE(gb_usb_handlers),

    .init = gb_usb_init,
    .exit = gb_usb_exit,
};

void gb_usb_register(int cport)
{
    usb_cport = cport;
    gb_register_driver(cport, &usb_driver);
}
