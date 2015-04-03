#include <nuttx/config.h>

#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/fs/fs.h>
#include <nuttx/syslog/ramlog.h>

#include <arch/board/board.h>
#include <arch/chip/gpio.h>

#include "up_arch.h"
#include "up_internal.h"

int main(void)
{
    printf("\n\n===== USB Host Dev FW =====\n");

    tsb_gpio_register();

    int gb_uart_init(void);
    gb_uart_init();

    void gb_usb_register(int cport);
    gb_usb_register(0);

    return 0;
}
