#include <nuttx/config.h>

#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/fs/fs.h>
#include <nuttx/syslog/ramlog.h>

#include <arch/board/board.h>
#include <arch/chip/gpio.h>

#include "up_arch.h"
#include "up_internal.h"

#include <stdio.h>
#include "usb/dwc_os.h"

void test_callback(void *data)
{
    printf("callback %s\n", __func__);
}

int main(void)
{
    printf("\n\n===== USB Host Dev FW =====\n");

    tsb_gpio_register();

//    int gb_uart_init(void);
//    gb_uart_init();

//    void gb_usb_register(int cport);
//    gb_usb_register(0);

    {
        dwc_timer_t *timer = DWC_TIMER_ALLOC("test", test_callback, NULL);
        DWC_TIMER_SCHEDULE(timer, 10);
        //DWC_TIMER_CANCEL
        //DWC_TIMER_FREE(timer);
    }

    return 0;
}
