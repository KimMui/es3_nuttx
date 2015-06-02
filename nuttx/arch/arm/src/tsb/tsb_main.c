#include <nuttx/config.h>

#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/fs/fs.h>
#include <nuttx/syslog/ramlog.h>

#include <arch/board/board.h>
#include <arch/chip/gpio.h>

#include "up_arch.h"
#include "up_internal.h"

#include "usb/dwc_otg_dbg.h"

int main(int argc, char **argv)
{
    printf("\n\n===== USB Host Dev FW =====\n");

//    SET_DEBUG_LEVEL(DBG_ANY);

    tsb_gpio_register(NULL);

    tsb_device_table_register();
    tsb_driver_register();

#if 1
    int gb_uart_init(void);
    gb_uart_init();

    void gb_usb_register(int cport);
    gb_usb_register(0);
#else
    test_hsic_link();
#endif


    gb_replay(0, "./debug.log");
//    while (1);

    return 0;
}
