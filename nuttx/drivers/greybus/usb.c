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
    gb_usb_debug("device_usb_hcd_stop(usbdev);\n");

    device_usb_hcd_stop(usbdev);

    return GB_OP_SUCCESS;
}

static uint8_t gb_usb_hcd_start(struct gb_operation *operation)
{
    int retval;

    gb_usb_debug("device_usb_hcd_start(usbdev, &usb4624);\n");

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
    if (!operation)
        goto end;

    comp = gb_operation_get_request_payload(operation);
    comp->actual_length = urb->actual_length;
    comp->status = urb->status;
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
    int i;
    int retval;
    int op_status = GB_OP_SUCCESS;
    struct urb *urb;
    struct gb_usb_urb_enqueue_response *response;
    struct gb_usb_urb_enqueue_request *request =
        gb_operation_get_request_payload(operation);

    gb_usb_debug("device_usb_hcd_urb_enqueue(usbdev, urb);\n");

    urb = urb_create();
    if (!urb) {
        return GB_OP_NO_MEMORY;
    }

    urb->urb = NULL;
    urb->pipe = request->pipe;
    urb->length = request->transfer_buffer_length;
    urb->maxpacket = request->maxpacket;
    //urb->hcpriv_ep = request->hcpriv_ep;
    urb->interval = request->interval;
    urb->dev_speed = request->dev_speed;
    urb->dev_ttport = request->dev_ttport;
    urb->devnum = request->devnum;
    urb->complete = gb_usb_urb_complete;
    memcpy(urb->setup_packet, request->setup_packet, sizeof(urb->setup_packet));

    if (usb_host_pipeint(urb->pipe)) {
        printf("INT PIPE@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    }

#if 1
    printf("urb->devnum = %d\n", urb->devnum);
    printf("urb->dev_ttport = %d\n", urb->dev_ttport);
    printf("urb->dev_speed = %hhu\n", urb->dev_speed);
#endif

    if (request->transfer_buffer_length) {
        urb->buffer = malloc(request->transfer_buffer_length);
        if (!urb->buffer) {
            op_status = GB_OP_NO_MEMORY;
            goto error;
        }

        memcpy(urb->buffer, request->payload, request->transfer_buffer_length);
    }

    if (!(request->transfer_flags & GB_USB_HOST_URB_NO_INTERRUPT)) {
        urb->flags |= USB_URB_GIVEBACK_ASAP;
    }

    if (request->transfer_flags & GB_USB_HOST_URB_ZERO_PACKET) {
        urb->flags |= USB_URB_SEND_ZERO_PACKET;
    }

    retval = device_usb_hcd_urb_enqueue(usbdev, urb);
    if (retval) {
        printf("%s() : %d = %d\n", __func__, __LINE__, retval);
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

    printf("\nGot a response back from device: %d %d\n",
           urb->actual_length,
           urb->status);

    response->actual_length = urb->actual_length;
    response->status = urb->status;
    memcpy(response->payload, urb->buffer, urb->actual_length);

#if TX_DEBUG
    {
        int i;
        if (urb->actual_length) {
            printf("buffer:\n");
            for (i = 0; i < urb->actual_length; i++) {
                printf("%hhx ", ((char*) urb->buffer)[i]);
            }
            printf("\n");
        }
    }
#endif
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

    if (gb_operation_get_request_payload_size(operation) < sizeof(*request)) {
        return GB_OP_INVALID;
    }

    response =
        gb_operation_alloc_response(operation,
                                    sizeof(*response) + request->wLength);
    if (!response) {
        return GB_OP_NO_MEMORY;
    }

    gb_usb_debug("%s(%hX, %hX, %hX, %hX)\n", __func__, request->typeReq,
                 request->wValue, request->wIndex, request->wLength);

    response->status = device_usb_hcd_hub_control(usbdev, request->typeReq,
                                                  request->wValue,
                                                  request->wIndex,
                                                  (char*) response->buf,
                                                  request->wLength);

    return GB_OP_SUCCESS;
}

static int gb_usb_init(unsigned int cport)
{
    ssize_t nwrote;
    char buf[64];
    int fd;

    usbdev = device_open(DEVICE_TYPE_USB_HCD, 0);
    if (!usbdev) {
        return -ENODEV;
    }

#if 0
    device_usb_hcd_start(usbdev, &usb4624);
    device_usb_hcd_hub_control(usbdev, 0xA006, 0x2900, 0x0, buf, 0xF);
    device_usb_hcd_hub_control(usbdev, 0xA000, 0x0, 0x0, buf, 0x4);
    device_usb_hcd_hub_control(usbdev, 0x2303, 0x8, 0x1, buf, 0x0);
    device_usb_hcd_hub_control(usbdev, 0xA300, 0x0, 0x1, buf, 0x4);
    device_usb_hcd_hub_control(usbdev, 0x2301, 0x10, 0x1, buf, 0x0);
    device_usb_hcd_hub_control(usbdev, 0xA300, 0x0, 0x1, buf, 0x4);
    device_usb_hcd_hub_control(usbdev, 0x2303, 0x4, 0x1, buf, 0x0);
    device_usb_hcd_hub_control(usbdev, 0xA300, 0x0, 0x1, buf, 0x4);
    device_usb_hcd_hub_control(usbdev, 0x2301, 0x14, 0x1, buf, 0x0);
#endif

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
