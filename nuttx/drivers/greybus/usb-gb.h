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

#ifndef __USB_GB_H__
#define __USB_GB_H__

#include <nuttx/greybus/types.h>

#define GB_USB_HOST_DIR_OUT                 0
#define GB_USB_HOST_DIR_IN                  0x80

#define GB_USB_HOST_PIPE_ISOCHRONOUS        0
#define GB_USB_HOST_PIPE_INTERRUPT          1
#define GB_USB_HOST_PIPE_CONTROL            2
#define GB_USB_HOST_PIPE_BULK               3

#define gb_usb_host_pipein(pipe)            ((pipe) & GB_USB_HOST_DIR_IN)
#define gb_usb_host_pipeout(pipe)           (!gb_usb_host_pipein(pipe))

#define gb_usb_host_pipedevice(pipe)        (((pipe) >> 8) & 0x7f)
#define gb_usb_host_pipeendpoint(pipe)      (((pipe) >> 15) & 0xf)

#define gb_usb_host_pipetype(pipe)          (((pipe) >> 30) & 3)
#define gb_usb_host_pipeisoc(pipe) \
    (gb_usb_host_pipetype((pipe)) == GB_USB_HOST_PIPE_ISOCHRONOUS)
#define gb_usb_host_pipeint(pipe) \
    (gb_usb_host_pipetype((pipe)) == GB_USB_HOST_PIPE_INTERRUPT)
#define gb_usb_host_pipecontrol(pipe) \
    (gb_usb_host_pipetype((pipe)) == GB_USB_HOST_PIPE_CONTROL)
#define gb_usb_host_pipebulk(pipe) \
    (gb_usb_host_pipetype((pipe)) == GB_USB_HOST_PIPE_BULK)

#define GB_USB_HOST_URB_SHORT_NOT_OK        0x0001
#define GB_USB_HOST_URB_ISO_ASAP            0x0002
#define GB_USB_HOST_URB_NO_TRANSFER_DMA_MAP 0x0004
#define GB_USB_HOST_URB_NO_FSBR             0x0020
#define GB_USB_HOST_URB_ZERO_PACKET         0x0040
#define GB_USB_HOST_URB_NO_INTERRUPT        0x0080
#define GB_USB_HOST_URB_FREE_BUFFER         0x0100

#define GB_USB_HOST_URB_DIR_IN              0x0200
#define GB_USB_HOST_URB_DIR_OUT             0
#define GB_USB_HOST_URB_DIR_MASK            GB_USB_HOST_URB_DIR_IN

#define GB_USB_HOST_ENDPOINT_XFERTYPE_MASK  0x03
#define GB_USB_HOST_ENDPOINT_XFER_CONTROL   0
#define GB_USB_HOST_ENDPOINT_XFER_ISOC      1
#define GB_USB_HOST_ENDPOINT_XFER_BULK      2
#define GB_USB_HOST_ENDPOINT_XFER_INT       3
#define GB_USB_HOST_ENDPOINT_MAX_ADJUSTABLE 0x80

/* Version of the Greybus USB protocol we support */
#define GB_USB_VERSION_MAJOR		0x00
#define GB_USB_VERSION_MINOR		0x01

/* Greybus USB request types */
#define GB_USB_TYPE_INVALID		0x00
#define GB_USB_TYPE_PROTOCOL_VERSION	0x01
#define GB_USB_TYPE_HCD_STOP		0x02
#define GB_USB_TYPE_HCD_START		0x03
#define GB_USB_TYPE_URB_ENQUEUE		0x04
#define GB_USB_TYPE_HUB_CONTROL		0x07


struct gb_usb_proto_version_response {
	__u8	major;
	__u8	minor;
};

struct gb_usb_urb_enqueue_request {
	__le32 pipe;
	__le32 transfer_flags;
	__le32 transfer_buffer_length;
	__le32 maxpacket;
	__le32 interval;
	__le64 hcpriv_ep;
	__le32 number_of_packets;
	__u8   dev_speed;
	__le32 devnum;
	__le32 dev_ttport;
	u8 setup_packet[8];
	u8 payload[0];
};

struct gb_usb_urb_enqueue_response {
	__le32 status;
	__le32 actual_length;
	u8 payload[0];
};

struct gb_usb_hub_control_request {
	__le16 typeReq;
	__le16 wValue;
	__le16 wIndex;
	__le16 wLength;
};

struct gb_usb_hub_control_response {
	__le32 status;
	__u8 buf[0];
};

#endif /* __USB_GB_H__ */

