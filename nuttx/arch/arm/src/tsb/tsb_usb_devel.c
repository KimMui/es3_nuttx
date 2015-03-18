
void simulate1(void)
{
#if 0
    {
        char buf[50];
        int hub_control(uint16_t typeReq, uint16_t wValue, uint16_t wIndex,
                        char *buf, uint16_t wLength);

        hcd_start();
        SET_DEBUG_LEVEL(DBG_HCDV);

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
        up_mdelay(2000);

        /**
         * Request: 0xa300 = GetPortStatus
         * wValue: 0
         * wIndex: 1
         * wLength: 4
         *
         * buf = PORT_CONNECTION | PORT_POWER | PORT_HIGH_SPEED
         */
        hub_control(0xa300, 0x0000, 0x0001, buf, 0x0004);
        printf("hub_control: %X\r\n", *((uint32_t*) buf));

        /**
         * Request: 0x2301 = ClearPortFeature
         * wValue: 0x10 (C_PORT_CONNECTION)
         * wIndex: 1 (Port 1)
         * wLength: 0
         */
        hub_control(0x2301, 0x0010, 0x0001, buf, 0x0000);
        up_mdelay(2000);

        /**
         * Request: 0xa300 = GetPortStatus
         * wValue: 0
         * wIndex: 1
         * wLength: 4
         *
         * buf = PORT_CONNECTION | PORT_POWER | PORT_HIGH_SPEED
         */
        hub_control(0xa300, 0x0000, 0x0001, buf, 0x0004);
        printf("hub_control: %X\r\n", *((uint32_t*) buf));

        /**
         * Request: 0x2303 = SetPortFeature
         * wValue: 4 (PORT_RESET)
         * wIndex: 1
         * wLength: 0
         */
        hub_control(0x2303, 0x0004, 0x0001, buf, 0x0000);
                    up_mdelay(2000);

        /**
         * Request: 0xa300 = GetPortStatus
         * wValue: 0
         * wIndex: 1
         * wLength: 4
         *
         * buf = PORT_CONNECTION | PORT_ENABLE | PORT_POWER | PORT_HIGH_SPEED
         */
        hub_control(0xa300, 0x0000, 0x0001, buf, 0x0004);
        printf("hub_control: %X\r\n", *((uint32_t*) buf));

        /**
         * Request: 0x2301 = ClearPortFeature
         * wValue: 0x14 (C_PORT_RESET)
         * wIndex: 1 (Port 1)
         * wLength: 0
         */
        hub_control(0x2301, 0x0014, 0x0001, buf, 0x0000);

#if 1
        {
            up_mdelay(2000);
            struct urb *urb = malloc(sizeof(*urb));
            memset(urb, 0, sizeof(*urb));

            sem_init(&urb->semaphore, 0, 0);
            urb->pipe = 0x80000080;
            urb->flags = 0x200;
            urb->length = 0x40;
            urb->maxpacket = 0;
            urb->interval = 1;
            urb->hcpriv_ep = 0;
//            urb->number_of_packets = 0;
            urb->complete = gb_usb_urb_complete;

        /**
         * Request: 0x8006 = GET_DESCRIPTOR
         * wValue:  0x0001 =
         * wIndex:  0x0000 =
         * wLength: 0x4000 =
         *
         * buf = PORT_CONNECTION | PORT_ENABLE | PORT_POWER | PORT_HIGH_SPEED
         */
#if 1
            urb->setup_packet[0] = 0x80;
            urb->setup_packet[1] = 0x06;
            urb->setup_packet[2] = 0x00;
            urb->setup_packet[3] = 0x01;
            urb->setup_packet[4] = 0x00;
            urb->setup_packet[5] = 0x00;
            urb->setup_packet[6] = 0x40;
            urb->setup_packet[7] = 0x00;
#else
            urb->setup_packet[0] = 0x06;
            urb->setup_packet[1] = 0x80;
            urb->setup_packet[2] = 0x01;
            urb->setup_packet[3] = 0x00;
            urb->setup_packet[4] = 0x00;
            urb->setup_packet[5] = 0x00;
            urb->setup_packet[6] = 0x00;
            urb->setup_packet[7] = 0x40;
#endif

            char buffer[] = {0x75, 0x73, 0x62, 0x3a, 0x76, 0x31, 0x44, 0x00,
                             0x42, 0x70, 0x30, 0x30, 0x30, 0x32, 0x64, 0x30,
                             0x33, 0x30, 0x38, 0x64, 0x63, 0x30, 0x39, 0x64,
                             0x73, 0x63, 0x30, 0x30, 0x64, 0x70, 0x30, 0x31,
                             0x69, 0x63, 0x30, 0x39, 0x69, 0x73, 0x63, 0x30,
                             0x30, 0x69, 0x70, 0x30, 0x30, 0x69, 0x6e, 0x30,
                             0x30, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                             0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
            urb->buffer = malloc(urb->length);
            memcpy(urb->buffer, buffer, urb->length);
//            memset(urb->buffer, 0, urb->length);

            printf("enqueue\n");
            urb_enqueue(urb);
        }
#endif

#if 0
        while (1) {
            hub_control(0xa300, 0x0000, 0x0001, buf, 0x0004);
            printf("hub_control: %X\r\n", *((uint32_t*) buf));
            up_mdelay(2000);
        }
#endif
    }


    while (1);
#endif
}

static void urb_complete(struct urb *urb)
{
}

void simulate2(void)
{
    
}
