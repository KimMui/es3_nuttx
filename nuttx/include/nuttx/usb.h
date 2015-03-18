#ifndef __NUTTX_USB_H__
#define __NUTTX_USB_H__

#include <stdint.h>
#include <stddef.h>
#include <semaphore.h>

#define USB_URB_GIVEBACK_ASAP       1
#define USB_URB_SEND_ZERO_PACKET    2

#define USB_HOST_DIR_OUT                 0
#define USB_HOST_DIR_IN                  0x80

#define USB_HOST_PIPE_ISOCHRONOUS        0
#define USB_HOST_PIPE_INTERRUPT          1
#define USB_HOST_PIPE_CONTROL            2
#define USB_HOST_PIPE_BULK               3

#define usb_host_pipein(pipe)            ((pipe) & USB_HOST_DIR_IN)
#define usb_host_pipeout(pipe)           (!usb_host_pipein(pipe))

#define usb_host_pipedevice(pipe)        (((pipe) >> 8) & 0x7f)
#define usb_host_pipeendpoint(pipe)      (((pipe) >> 15) & 0xf)

#define usb_host_pipetype(pipe)          (((pipe) >> 30) & 3)
#define usb_host_pipeisoc(pipe) \
    (usb_host_pipetype((pipe)) == USB_HOST_PIPE_ISOCHRONOUS)
#define usb_host_pipeint(pipe) \
    (usb_host_pipetype((pipe)) == USB_HOST_PIPE_INTERRUPT)
#define usb_host_pipecontrol(pipe) \
    (usb_host_pipetype((pipe)) == USB_HOST_PIPE_CONTROL)
#define usb_host_pipebulk(pipe) \
    (usb_host_pipetype((pipe)) == USB_HOST_PIPE_BULK)

#define USB_HOST_ENDPOINT_XFER_CONTROL   0
#define USB_HOST_ENDPOINT_XFER_ISOC      1
#define USB_HOST_ENDPOINT_XFER_BULK      2
#define USB_HOST_ENDPOINT_XFER_INT       3

struct urb;
typedef void (*urb_complete_t)(struct urb *urb);

struct urb {
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
    void *hcpriv_ep;

    int bw;
    int interval;

    uint8_t setup_packet[8];
    void *buffer;

    urb_complete_t complete;

    sem_t semaphore;
    void *hcpriv;
};

int urb_enqueue(struct urb *urb);
int urb_dequeue(struct urb *urb);
int hub_control(uint16_t typeReq, uint16_t wValue, uint16_t wIndex, char *buf,
                uint16_t wLength);
void hcd_stop(void);
int hcd_start(void);
void hcd_exit(void);
int hcd_init(void);
int endpoint_disable(void *ep);

#endif /* __NUTTX_USB_H__ */

