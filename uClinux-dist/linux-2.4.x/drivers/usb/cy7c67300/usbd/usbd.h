/*
 * linux/drivers/usbd/usbd.h 
 *
 * Copyright (c) 2000, 2001, 2002 Lineo
 * Copyright (c) 2001 Hewlett Packard
 *
 * By: 
 *      Stuart Lynne <sl@lineo.com>, 
 *      Tom Rushworth <tbr@lineo.com>, 
 *      Bruce Balden <balden@lineo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


/*
 * USB Device Support structures
 *
 * Descriptors:
 *
 * Driver description:
 * 
 *   struct usb_device_description 
 *
 * Driver instance configuration:
 *
 *   struct usb_device_instance 
 *   struct usb_endpoint_instance;
 *
 *   struct usb_function_instance
 *   struct usb_bus_instance
 *
 * Data transfer:
 *
 *   struct usbd_urb; 
 *
 */

/*
 *
 * Notes...
 *
 * 1. The order of loading must be:
 *
 *        usb-device                    default driver for endpoint 0
 *        usb-function(s)               one or more function drivers
 *        usb-bus-interface(s)          one or more bus interface drivers
 *
 * 2. Loading usb-function modules allows them to register with the 
 * usb-device layer. This makes their usb configuration descriptors 
 * available.
 *
 * 3. Loading usb-bus-interface modules causes them to register with
 * the usb-device layer.
 *
 * 4. The last action from loading a usb-bus-interface driver is to enable
 * their upstream port. This in turn will cause the upstream host to
 * enumerate this device.
 *
 * 5. The usb-device layer provides a default configuration for endpoint 0 for
 * each device. 
 *
 * 6. Additional configurations are added to support the configuration
 * function driver when entering the configured state.
 *
 * 7. The usb-device maintains a list of configurations from the loaded
 * function drivers. It will provide this to the USB HOST as part of the
 * enumeration process and will add someof them as the active configuration
 * as per the USB HOST instructions.
 * 
 *
 */

#ifndef MODULE
#undef __init
#define __init
#undef __exit
#define __exit
#undef GET_USE_COUNT
#define GET_USE_COUNT(module) 0
#undef THIS_MODULE
#define THIS_MODULE 0
#endif


/* Overview
 *
 * USB Descriptors are used to build a configuration database for each USB
 * Function driver.
 *
 * USB Function Drivers register a configuration that may be used by the
 * default endpoint 0 driver to provide a USB HOST with options.
 *
 * USB Bus Interface drivers find and register specific physical bus
 * interfaces.
 *
 * The USB Device layer creates and maintains a device structure for each
 * physical interface that specifies the bus interface driver, shows possible 
 * configurations, specifies the default endpoint 0 driver, active
 * configured function driver and configured endpoints.
 *
 *
 */

struct usbd_urb; 

struct usb_endpoint_instance;
struct usb_configuration_description;
struct usb_function_driver;
struct usb_function_instance;

struct usb_device_instance;

struct usb_bus_driver;
struct usb_bus_instance;


/* USB definitions
 */
#if 0
#define USB_ENDPOINT_CONTROL            0x00
#define USB_ENDPOINT_ISOC               0x01
#define USB_ENDPOINT_BULK               0x02
#define USB_ENDPOINT_INT                0x03

#define USB_CONFIG_REMOTEWAKE           0x20
#define USB_CONFIG_SELFPOWERED          0x40
#define USB_CONFIG_BUSPOWERED           0x80


#define USB_REQUEST_GET_STATUS          0x01
#define USB_REQUEST_XXXXXXXXXXXXE       0x02
#define USB_REQUEST_CLEAR_FEATURE       0x03
#define USB_REQUEST_SET_FEATURE         0x04
#define USB_REQUEST_SET_ADDRESS         0x05
#define USB_REQUEST_GET_DESCRIPTOR      0x06
#define USB_REQUEST_SET_DESCRIPTOR      0x07
#define USB_REQUEST_GET_CONFIGURATION   0x08
#define USB_REQUEST_SET_CONFIGURATION   0x09
#define USB_REQUEST_GET_INTERFACE       0x0A
#define USB_REQUEST_SET_INTERFACE       0x0B

#define USB_DESCRIPTOR_DEVICE           0x01
#define USB_DESCRIPTOR_CONFIG           0x02
#define USB_DESCRIPTOR_STRING           0x03
#define USB_DESCRIPTOR_INTERFACE        0x04
#define USB_DESCRIPTOR_ENDPOINT         0x05
#endif


/*
 * Device and/or Interface Class codes
 */
#define USB_CLASS_PER_INTERFACE         0       /* for DeviceClass */
#define USB_CLASS_AUDIO                 1
#define USB_CLASS_COMM                  2
#define USB_CLASS_HID                   3
#define USB_CLASS_PHYSICAL              5
#define USB_CLASS_PRINTER               7
#define USB_CLASS_MASS_STORAGE          8
#define USB_CLASS_HUB                   9
#define USB_CLASS_DATA                  10
#define USB_CLASS_APP_SPEC              0xfe
#define USB_CLASS_VENDOR_SPEC           0xff

/*
 * USB types
 */
#define USB_TYPE_STANDARD               (0x00 << 5)
#define USB_TYPE_CLASS                  (0x01 << 5)
#define USB_TYPE_VENDOR                 (0x02 << 5)
#define USB_TYPE_RESERVED               (0x03 << 5)

/*
 * USB recipients
 */
#define USB_RECIP_DEVICE                0x00
#define USB_RECIP_INTERFACE             0x01
#define USB_RECIP_ENDPOINT              0x02
#define USB_RECIP_OTHER                 0x03

/*
 * USB directions
 */
#define USB_DIR_OUT                     0
#define USB_DIR_IN                      0x80

/*
 * Descriptor types
 */
#define USB_DT_DEVICE                   0x01
#define USB_DT_CONFIG                   0x02
#define USB_DT_STRING                   0x03
#define USB_DT_INTERFACE                0x04
#define USB_DT_ENDPOINT                 0x05
#define USB_DT_OTG                      0x09

#define USB_DT_HID                      (USB_TYPE_CLASS | 0x01)
#define USB_DT_REPORT                   (USB_TYPE_CLASS | 0x02)
#define USB_DT_PHYSICAL                 (USB_TYPE_CLASS | 0x03)
#define USB_DT_HUB                      (USB_TYPE_CLASS | 0x09)

/*
 * Descriptor sizes per descriptor type
 */
#define USB_DT_DEVICE_SIZE              18
#define USB_DT_CONFIG_SIZE              9
#define USB_DT_INTERFACE_SIZE           9
#define USB_DT_ENDPOINT_SIZE            7
#define USB_DT_ENDPOINT_AUDIO_SIZE      9       /* Audio extension */
#define USB_DT_HUB_NONVAR_SIZE          7
#define USB_DT_HID_SIZE                 9

/*
 * Endpoints
 */
#define USB_ENDPOINT_NUMBER_MASK        0x0f    /* in bEndpointAddress */
#define USB_ENDPOINT_DIR_MASK           0x80

#define USB_ENDPOINT_XFERTYPE_MASK      0x03    /* in bmAttributes */
#define USB_ENDPOINT_XFER_CONTROL       0
#define USB_ENDPOINT_XFER_ISOC          1
#define USB_ENDPOINT_XFER_BULK          2
#define USB_ENDPOINT_XFER_INT           3

/*
 * USB Packet IDs (PIDs)
 */
#define USB_PID_UNDEF_0                        0xf0
#define USB_PID_OUT                            0xe1
#define USB_PID_ACK                            0xd2
#define USB_PID_DATA0                          0xc3
#define USB_PID_PING                           0xb4     /* USB 2.0 */
#define USB_PID_SOF                            0xa5
#define USB_PID_NYET                           0x96     /* USB 2.0 */
#define USB_PID_DATA2                          0x87     /* USB 2.0 */
#define USB_PID_SPLIT                          0x78     /* USB 2.0 */
#define USB_PID_IN                             0x69
#define USB_PID_NAK                            0x5a
#define USB_PID_DATA1                          0x4b
#define USB_PID_PREAMBLE                       0x3c     /* Token mode */
#define USB_PID_ERR                            0x3c     /* USB 2.0: handshake mode */
#define USB_PID_SETUP                          0x2d
#define USB_PID_STALL                          0x1e
#define USB_PID_MDATA                          0x0f     /* USB 2.0 */

/*
 * Standard requests
 */
#define USB_REQ_GET_STATUS              0x00
#define USB_REQ_CLEAR_FEATURE           0x01
#define USB_REQ_SET_FEATURE             0x03
#define USB_REQ_SET_ADDRESS             0x05
#define USB_REQ_GET_DESCRIPTOR          0x06
#define USB_REQ_SET_DESCRIPTOR          0x07
#define USB_REQ_GET_CONFIGURATION       0x08
#define USB_REQ_SET_CONFIGURATION       0x09
#define USB_REQ_GET_INTERFACE           0x0A
#define USB_REQ_SET_INTERFACE           0x0B
#define USB_REQ_SYNCH_FRAME             0x0C

/*
 * HID requests
 */
#define USB_REQ_GET_REPORT              0x01
#define USB_REQ_GET_IDLE                0x02
#define USB_REQ_GET_PROTOCOL            0x03
#define USB_REQ_SET_REPORT              0x09
#define USB_REQ_SET_IDLE                0x0A
#define USB_REQ_SET_PROTOCOL            0x0B


/*
 * USB Spec Release number
 */

#define USB_BCD_VERSION                 0x0200


/*
 * Device Requests      (c.f Table 9-2)
 */

#define USB_REQ_DIRECTION_MASK          0x80
#define USB_REQ_TYPE_MASK               0x60
#define USB_REQ_RECIPIENT_MASK          0x1f

#define USB_REQ_DEVICE2HOST             0x80
#define USB_REQ_HOST2DEVICE             0x00

#define USB_REQ_TYPE_STANDARD           0x00
#define USB_REQ_TYPE_CLASS              0x20
#define USB_REQ_TYPE_VENDOR             0x40

#define USB_REQ_RECIPIENT_DEVICE        0x00
#define USB_REQ_RECIPIENT_INTERFACE     0x01
#define USB_REQ_RECIPIENT_ENDPOINT      0x02
#define USB_REQ_RECIPIENT_OTHER         0x03

/*
 * get status bits
 */

#define USB_STATUS_SELFPOWERED          0x01
#define USB_STATUS_REMOTEWAKEUP         0x02

#define USB_STATUS_HALT                 0x01

/* 
 * descriptor types
 */

#define USB_DESCRIPTOR_TYPE_DEVICE                      0x01
#define USB_DESCRIPTOR_TYPE_CONFIGURATION               0x02
#define USB_DESCRIPTOR_TYPE_STRING                      0x03
#define USB_DESCRIPTOR_TYPE_INTERFACE                   0x04
#define USB_DESCRIPTOR_TYPE_ENDPOINT                    0x05
#define USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER            0x06
#define USB_DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION   0x07
#define USB_DESCRIPTOR_TYPE_INTERFACE_POWER             0x08


/* USB Requests
 *
 */

struct usb_device_request {
    __u8        bmRequestType;
    __u8        bRequest;
    __u16       wValue;
    __u16       wIndex;
    __u16       wLength;
} __attribute__ ((packed));


/* USB Status
 *
 */
typedef enum urb_send_status {
    SEND_IN_PROGRESS, 
    SEND_FINISHED_OK,
    SEND_FINISHED_ERROR,
    RECV_READY,
    RECV_OK,
    RECV_ERROR
} urb_send_status_t;

/*
 * Structure member address manipulation macros.
 * These are used by client code (code using the urb_link routines), since
 * the urb_link structure is embedded in the client data structures.
 *
 * Note: a macro offsetof equivalent to member_offset is defined in stddef.h
 *       but this is kept here for the sake of portability.
 *
 * p2surround returns a pointer to the surrounding structure given
 * type of the surrounding structure, the name memb of the structure
 * member pointed at by ptr.  For example, if you have:
 * 
 *      struct foo {
 *          int x;
 *          float y;
 *          char z;
 *      } thingy;
 *  
 *      char *cp = &thingy.z;
 *
 * then
 *
 *      &thingy == p2surround(struct foo, z, cp)
 *
 * Clear?
 */
   
#define _cv_(ptr)                 ((char*)(void*)(ptr))
#define member_offset(type,memb)  (_cv_(&(((type*)0)->memb))-(char*)0)
#define p2surround(type,memb,ptr) ((type*)(void*)(_cv_(ptr)-member_offset(type,memb)))

typedef struct urb_link {
    struct urb_link *next;
    struct urb_link *prev;
} urb_link;

struct usbd_urb;

/* 
 * Device Events 
 *
 * These are defined in the USB Spec (c.f USB Spec 2.0 Figure 9-1).
 *
 * There are additional events defined to handle some extra actions we need
 * to have handled.
 *
 */
typedef enum usb_device_event {

    DEVICE_UNKNOWN,             // bi - unknown event
    DEVICE_INIT,                // bi  - initialize
    DEVICE_CREATE,              // bi  - 
    DEVICE_HUB_CONFIGURED,      // bi  - bus has been plugged int
    DEVICE_RESET,               // bi  - hub has powered our port

    DEVICE_ADDRESS_ASSIGNED,    // ep0 - set address setup received
    DEVICE_CONFIGURED,          // ep0 - set configure setup received
    DEVICE_SET_INTERFACE,       // ep0 - set interface setup received

    DEVICE_SET_FEATURE,         // ep0 - set feature setup received
    DEVICE_CLEAR_FEATURE,       // ep0 - clear feature setup received

    DEVICE_DE_CONFIGURED,       // ep0 - set configure setup received for ??

    DEVICE_BUS_INACTIVE,        // bi  - bus in inactive (no SOF packets)
    DEVICE_BUS_ACTIVITY,        // bi  - bus is active again

    DEVICE_POWER_INTERRUPTION,  // bi  - hub has depowered our port
    DEVICE_HUB_RESET,           // bi  - bus has been unplugged
    DEVICE_DESTROY,             // bi  - device instance should be destroyed
    
    DEVICE_BUS_REQUEST,         // function - requests the bus for OTG protocol
    DEVICE_BUS_RELEASE,         // function - cancels request for bus
    
    DEVICE_ACCEPT_HNP,          // function - accept HNP when offered
    DEVICE_REQUEST_SRP,         // function - request SRP signaling

    DEVICE_RCV_URB_RECYCLED,    // function - receive urb has been recycled

    DEVICE_FUNCTION_PRIVATE,    // function - private

} usb_device_event_t;


/* Endpoint configuration
 *
 * Per endpoint configuration data. Used to track which function driver owns
 * an endpoint.
 *
 */
struct usb_endpoint_instance {
    int                 endpoint_address;       // logical endpoint address 

    						// control
						
    struct urb_link     events;                 // events

                                                // receive side
    struct urb_link     rcv;                    // received urbs
    struct urb_link     rdy;                    // empty urbs ready to receive
    struct usbd_urb     *rcv_urb;                // active urb
    int                 rcv_attributes;         // copy of bmAttributes from endpoint descriptor
    int                 rcv_packetSize;         // maximum packet size from endpoint descriptor
    int                 rcv_transferSize;       // maximum transfer size from function driver
    int                 rcv_interrupts;

                                                // transmit side
    struct urb_link     tx;                     // urbs ready to transmit
    struct urb_link     done;                   // transmitted urbs
    struct usbd_urb     *tx_urb;                 // active urb
    int                 tx_attributes;          // copy of bmAttributes from endpoint descriptor
    int                 tx_packetSize;          // maximum packet size from endpoint descriptor
    int                 tx_transferSize;        // maximum transfer size from function driver
    int                 tx_interrupts;

    int                 sent;                   // data already sent
    int                 last;                   // data sent in last packet XXX do we need this
    int                 errors;                 // number of tx errors seen

    int                 ep0_interrupts;
    void                *privdata;              // pointer to private data
};


/* USB Data structure - for passing data around. 
 *
 * This is used for both sending and receiving data. 
 *
 * The callback function is used to let the function driver know when
 * transmitted data has been sent.
 *
 * The callback function is set by the alloc_recv function when an urb is
 * allocated for receiving data for an endpoint and used to call the
 * function driver to inform it that data has arrived.
 */

struct usbd_urb {

    struct usb_endpoint_instance *endpoint;

    struct usb_device_instance *device;
    struct usb_function_instance *function_instance;

    __u8 pid;                                           // PID of received packet (SETUP or OUT)
    struct usb_device_request   device_request;         // contents of received SETUP packet

    unsigned char              *buffer;                 // data received (OUT) or being sent (IN)
    unsigned int                buffer_length;
    unsigned int                actual_length;


    struct list_head            urbs;                   // linked list of urbs(s)

    void *                      privdata;

    struct urb_link             link;                   // embedded struct for circular doubly linked list of urbs

    urb_send_status_t           status;
    time_t                      jiffies;
    usb_device_event_t          event;
    int                         data;
};


/* What to do with submitted urb
 *
 */
typedef enum urb_submit {
    SUBMIT_SEND,                // send buffer as DATA (control) or IN (bulk/interrupt)
    SUBMIT_OK,                  // send zero length packet to indicate succes (control) 
    SUBMIT_STALL                // indicate a request error
} urb_submit_t;


/* 
 * Device State (c.f USB Spec 2.0 Figure 9-1)
 *
 * What state the usb device is in. 
 *
 * Note the state does not change if the device is suspended, we simply set a
 * flag to show that it is suspended.
 *
 */
typedef enum usb_device_state {
    STATE_INIT,                 // just initialized
    STATE_CREATED,              // just created
    STATE_ATTACHED,             // we are attached 
    STATE_POWERED,              // we have seen power indication (electrical bus signal)
    STATE_DEFAULT,              // we been reset
    STATE_ADDRESSED,            // we have been addressed (in default configuration)
    STATE_CONFIGURED,           // we have seen a set configuration device command
    STATE_UNKNOWN,              // destroyed
} usb_device_state_t;

/*
 * Device status
 *
 * Overall state
 */
typedef enum usb_device_status {
    USBD_OPENING,		// we are currently opening
    USBD_OK,			// ok to use
    USBD_SUSPENDED,		// we are currently suspended
    USBD_CLOSING,		// we are currently closing
} usb_device_status_t;

#if 0
typedef enum usb_module_status {
    MODULE_OPENED,
    MODULE_ACTIVE,
    MODULE_SUSPENDED,
    MODULE_CLOSING,
    MODULE_CLOSED,
} usb_module_status_t;
#endif


/* BUS Configuration
 *
 * These are used by the ep0 function driver to indicate type of
 * configuratin change that should take place
 */
typedef enum usb_bus_change {
    BUS_RESET,               // error occurred, reset to STATE_UNKNOWN 
    BUS_ADDRESS,             // set address device command received
    BUS_CONFIGURE,           // set configuration device command received
    BUS_INTERFACE,           // set interface device command received
    BUS_CLOSE                // please shutdown 
} usb_bus_change_t;

/* Function Configuration
 *
 * These are used by the ep0 function driver to indicate type of
 * configuratin change that should take place
 */
typedef enum usb_function_change {
    FUNCTION_START,             // open function
    FUNCTION_STOP               // close function
} usb_function_change_t;


/* USB Device Instance
 *
 * For each physical bus interface we create a logical device structure. This
 * tracks all of the required state to track the USB HOST's view of the device.
 *
 * Keep track of the device configuration for a real physical bus interface,
 * this includes the bus interface, multiple function drivers, the current
 * configuration and the current state.
 *
 * This will show:
 *      the specific bus interface driver
 *      the default endpoint 0 driver
 *      the configured function driver
 *      device state
 *      device status
 *      endpoint list
 */

struct usb_device_instance {

    // generic
    char                               *name;
    struct usb_device_descriptor       *device_descriptor;     // per device descriptor
    
    // bus interface 
    struct usb_bus_instance            *bus;            // which bus interface driver

    // function driver(s)
    struct usb_function_instance       *ep0;            // ep0 configuration

    // function configuration descriptors
    unsigned int functions;                             //configurations;
    struct usb_function_instance      *function_instance_array;

    // device state
    usb_device_state_t                  device_state;   // current USB Device state
    usb_device_state_t                  device_previous_state;   // current USB Device state

    __u8                                address;        // current address (zero is default)
    __u8                                configuration;  // current show configuration (zero is default)
    __u8                                interface;      // current interface (zero is default)
    __u8                                alternate;      // alternate flag

    usb_device_status_t                 status;      	// device status

    int                                 urbs_queued;    // number of submitted urbs

    // housekeeping
    struct list_head                    devices;        // linked list usb_device_instance(s)

//    unsigned char                       dev_addr[ETH_ALEN];

    struct tq_struct                    device_bh;	// runs as bottom half, equivalent to interrupt time
    struct tq_struct			function_bh;	// runs as process

    // Shouldn't need to make this atomic, all we need is a change indicator
    unsigned long                       usbd_rxtx_timestamp;
    unsigned long                       usbd_last_rxtx_timestamp;
    void                                *privdata; 
};



/* Function configuration structure
 *
 * This is allocated for each configured instance of a function driver.
 *
 * It stores pointers to the usb_function_driver for the appropriate function,
 * and pointers to the USB HOST requested usb_configuration_description and
 * usb_interface_description.
 *
 * The privdata pointer may be used by the function driver to store private
 * per instance state information.
 *
 */
struct usb_function_instance {

    //struct usb_device_instance *device;
    
    struct usb_function_driver *function_driver;
    void                       *privdata;       // private data for the function
};


/* Bus Interface configuration structure
 *
 * This is allocated for each configured instance of a bus interface driver.
 *
 * It contains a pointer to the appropriate bus interface driver.
 *
 * The privdata pointer may be used by the bus interface driver to store private
 * per instance state information.
 */
struct usb_bus_instance {

    struct usb_bus_driver              *driver;
    struct usb_device_instance         *device;
    struct usb_endpoint_instance       *endpoint_array; // array of available configured endpoints

    unsigned int                        serial_number;
    char *                              serial_number_str;
    void                               *privdata;       // private data for the bus interface

};


/**
 * usbd_hotplug - call a hotplug script
 * @interface: hotplug action
 * @action: hotplug action
 */
int usbd_hotplug(char *, char *);


/* USB-Device 
 *
 * The USB Device layer has three components. 
 *
 *      - maintain a list of function drivers
 *      - create and manage each device instance based on bus interface registrations
 *      - implement the default endpoint 0 control driver
 *
 * Function drivers register speculatively. Their use depends in the end on
 * have one of their configurations being selected by the USB HOST.
 *
 * Bus Interface drivers register specifically. For each physical device
 * they find they register, thereby creating a usb_device_instance.
 *
 * For each bus interface configuration registered the USB Device layer will
 * create device configuration instance. The default endpoint 0 driver will
 * be automatically installed to handle SETUP packets.
 *
 * Bus Interface drivers should not enable their upstream port until the bus
 * interface registration has completed.
 *
 */

extern char *usbd_device_events[];
extern char *usbd_device_states[];

/* function driver registration
 *
 * Called by function drivers to register themselves when loaded
 * or de-register when unloading.
 */
//int usbd_strings_init(void);
int usbd_register_function(struct usb_function_driver *);
void usbd_deregister_function(struct usb_function_driver *);

/* bus driver registration
 *
 * Called by bus interface drivers to register themselves when loaded
 * or de-register when unloading.
 */
struct usb_bus_instance *usbd_register_bus(struct usb_bus_driver *);
void usbd_deregister_bus(struct usb_bus_instance *);

/* device
 *
 * Called by bus interface drivers to register a virtual device for a
 * physical interface that has been detected. Typically when actually
 * plugged into a usb bus. The device can be de-register, typically
 * when unplugged from the usb bus.
 */
struct usb_device_instance *usbd_register_device(char *, struct usb_bus_instance *, int);
void usbd_deregister_device(struct usb_device_instance *);

/* 
 * usbd_configure_device is used by function drivers (usually the control endpoint)
 * to change the device configuration.
 *
 * usbd_device_event is used by bus interface drivers to tell the higher layers that
 * certain events have taken place.
 */
//int usbd_configure_device(usb_bus_change_t, struct usb_device_instance *);
void usbd_device_event_irq(struct usb_device_instance *conf, usb_device_event_t, int );
void usbd_device_event(struct usb_device_instance *conf, usb_device_event_t, int );


int usbd_endpoint_halted(struct usb_device_instance *, int);
int usbd_device_feature(struct usb_device_instance *, int, int);

/* urb allocation
 *
 * Used to allocate and de-allocate urb structures.
 */
int usbd_alloc_urb_data(struct usbd_urb * , int );
struct usbd_urb * usbd_alloc_urb(struct usb_device_instance *, struct usb_function_instance *, __u8, int length );
//static __inline__ void usbd_dealloc_urb(struct usbd_urb *);

/*
 * Detach and return the first urb in a list with a distinguished
 * head "hd", or NULL if the list is empty.
 */
//static __inline__ struct usbd_urb *first_urb_detached_irq(urb_link *);

/*
 * Detach and return the first urb in a list with a distinguished
 * head "hd", or NULL if the list is empty.
 */
// XXX XXX need to implement
//static __inline__ struct usbd_urb *usbd_next_rcv_urb(struct usb_endpoint_instance *);



/*
 * usb_send_urb is used to submit a data urb for sending.
 */
//static __inline__ int usbd_send_urb(struct usbd_urb *);

/*
 * usb_cancel_urb is used to cancel a previously submitted data urb for sending.
 */
int usbd_cancel_urb(struct usb_device_instance *, struct usbd_urb *);

void usbd_request_bus(struct usb_device_instance *, int);


/*
 * usb_receive_setup is used by the bus interface driver to pass a SETUP 
 * packet into the the system.
 */
//static __inline__ int usbd_recv_setup(struct usbd_urb *);

/**
 * usbd_rcv_complete_irq - complete a receive
 * @endpoint:
 * @len:
 * @status:
 *
 * Called from rcv interrupt to complete.
 */
//static __inline__ void usbd_rcv_complete_irq(struct usb_endpoint_instance *, int, int );

/**
 * usbd_tx_complete_irq - complete a transmit
 * @endpoint:
 *
 * Called from tx interrupt to complete.
 */
//static __inline__ void usbd_tx_complete_irq(struct usb_endpoint_instance *, int );

/*
 * usb_receive_urb is used by the bus interface driver to pass a 
 * DATA packet into the the system.
 */
//static __inline__ void usbd_recv_urb_irq(struct usbd_urb *);

/*
 * usb_urb_sent is used by the bus interface driver to signal that
 * an urb has been sent
 */
//static void usb_urb_sent_irq(struct usbd_urb *, int);


/* descriptors
 *
 * Various ways of finding descriptors based on the current device and any
 * possible configuration / interface / endpoint for it.
 */
struct usb_configuration_descriptor *
usbd_device_configuration_descriptor( struct usb_device_instance *, int, int );

struct usb_function_instance *
usbd_device_function_instance(struct usb_device_instance *, unsigned int );

struct usb_interface_instance *
usbd_device_interface_instance(struct usb_device_instance *, int , int , int );

struct usb_alternate_instance *
usbd_device_alternate_instance(struct usb_device_instance *, int , int , int , int );

struct usb_interface_descriptor *
usbd_device_interface_descriptor(struct usb_device_instance *, int , int, int, int );

struct usb_endpoint_descriptor *
usbd_device_endpoint_descriptor_index(struct usb_device_instance *, int , int , int, int, int );

struct usb_class_descriptor *
usbd_device_class_descriptor_index( struct usb_device_instance *, int , int , int , int , int );

struct usb_endpoint_descriptor *
usbd_device_endpoint_descriptor(struct usb_device_instance *, int , int , int, int, int );

struct usb_otg_descriptor *
usbd_device_otg_descriptor(struct usb_device_instance *, int , int , int , int );


int 
usbd_device_endpoint_transfersize(struct usb_device_instance *, int , int , int , int, int );

struct usb_string_descriptor *
usbd_get_string(__u8 );

struct usb_device_descriptor *
usbd_device_device_descriptor(struct usb_device_instance *, int);


void usbd_flush_tx(struct usb_endpoint_instance *);
void usbd_fill_rcv(struct usb_device_instance *, struct usb_endpoint_instance *, int );

void usbd_flush_rcv(struct usb_endpoint_instance *);
void usbd_flush_ep(struct usb_endpoint_instance *);



static __inline__ void * ckmalloc(int n, int f)
{
    void *p;
    if ((p = kmalloc(n, f))==NULL) {
        return NULL;
    }
    memset(p, 0, n);
    return p;
}

static __inline__ char * strdup(char *str)
{
    int n;
    char *s;
    if (str && (n = strlen(str)+1) && (s = kmalloc(n, GFP_ATOMIC))) {
        return strcpy(s, str);
    }
    return NULL;
}


