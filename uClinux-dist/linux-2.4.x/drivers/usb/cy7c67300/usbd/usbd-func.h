/*
 * linux/drivers/usbd/usb-function.h - USB Function 
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
 * USB Function Driver structures
 *
 * Descriptors:
 *   struct usb_endpoint_description
 *   struct usb_interface_description 
 *   struct usb_configuration_description
 *
 * Driver description:
 *   struct usb_function_driver
 *   struct usb_function_operations
 *
 */


/* USB Descriptors - Create a complete description of all of the
 * function driver capabilities. These map directly to the USB descriptors.
 *
 * This heirarchy is created by the functions drivers and is passed to the
 * usb-device driver when the function driver is registered.
 *
 *  device
 *      configuration
 *           interface
 *              alternate
 *                   class
 *                   class
 *              alternate
 *                   endpoint
 *                   endpoint
 *           interface
 *              alternate
 *                   endpoint
 *                   endpoint
 *      configuration
 *           interface
 *              alternate
 *                   endpoint
 *                   endpoint
 *
 *
 * The configuration structures refer to the USB Configurations that will be
 * made available to a USB HOST during the enumeration process. 
 *
 * The USB HOST will select a configuration and optionally an interface with
 * the usb set configuration and set interface commands.
 *
 * The selected interface (or the default interface if not specifically
 * selected) will define the list of endpoints that will be used.
 *
 * The configuration and interfaces are stored in an array that is indexed
 * by the specified configuratin or interface number minus one. 
 *
 * A configuration number of zero is used to specify a return to the unconfigured
 * state.
 *
 */


/* Mass Storage types */

#define MS_INTERFACE_CLASS				0x08
#define MS_INTERFACE_SUBCLASS_RBC		0x01
#define MS_INTERFACE_SUBCLASS_CDROM		0x02
#define MS_INTERFACE_SUBCLASS_SCSI		0x06
#define MS_INTERFACE_PROTOCOL_BULK_ONLY 0x50

// Local defines from the mass storage class spec
#define SC_MASS_STORAGE_RESET       0xff
#define SC_GET_MAX_LUN              0xfe
#define CBW_TAG                     4
#define CBW_DATA_TRANSFER_LEN_LSB   8
#define CBW_DATA_TRANSFER_LEN_MSB   9
#define CBW_FLAGS                   12
#define CBW_FLAGS_DIR_BIT           0x80
#define CBW_LUN                     13
#define CBW_CBW_LEN                 14
#define CBW_CBW_LEN_MASK            0xf
#define CBW_DATA_START              15

/* HID class types */

#define HID_INTERFACE_CLASS				0x03
#define HID_INTERFACE_SUBCLASS_NONE		0x00
#define HID_INTERFACE_SUBCLASS_BOOT		0x01
#define HID_INTERFACE_PROTOCOL_NONE		0x00
#define HID_INTERFACE_PROTOCOL_KEYBOARD	0x01
#define HID_INTERFACE_PROTOCOL_MOUSE	0x02

/*
 * communications class types
 *
 * c.f. CDC  USB Class Definitions for Communications Devices 
 * c.f. WMCD USB CDC Subclass Specification for Wireless Mobile Communications Devices
 *
 */

#define CLASS_BCD_VERSION               0x0110

// c.f. CDC 4.1 Table 14
#define COMMUNICATIONS_DEVICE_CLASS     0x02

// c.f. CDC 4.2 Table 15
#define COMMUNICATIONS_INTERFACE_CLASS  0x02

// c.f. CDC 4.3 Table 16
#define COMMUNICATIONS_NO_SUBCLASS      0x00
#define COMMUNICATIONS_DLCM_SUBCLASS    0x01
#define COMMUNICATIONS_ACM_SUBCLASS     0x02
#define COMMUNICATIONS_TCM_SUBCLASS     0x03
#define COMMUNICATIONS_MCCM_SUBCLASS    0x04
#define COMMUNICATIONS_CCM_SUBCLASS     0x05
#define COMMUNICATIONS_ENCM_SUBCLASS    0x06
#define COMMUNICATIONS_ANCM_SUBCLASS    0x07

// c.f. WMCD 5.1
#define COMMUNICATIONS_WHCM_SUBCLASS    0x08
#define COMMUNICATIONS_DMM_SUBCLASS     0x09
#define COMMUNICATIONS_MDLM_SUBCLASS    0x0a
#define COMMUNICATIONS_OBEX_SUBCLASS    0x0b

// c.f. CDC 4.6 Table 18
#define DATA_INTERFACE_CLASS            0x0a

// c.f. CDC 4.7 Table 19
#define COMMUNICATIONS_NO_PROTOCOL      0x00


// c.f. CDC 5.2.3 Table 24
#define CS_INTERFACE    		0x24
#define CS_ENDPOINT     		0x25

/*
 * bDescriptorSubtypes
 *
 * c.f. CDC 5.2.3 Table 25
 * c.f. WMCD 5.3 Table 5.3
 */

#define USB_ST_HEADER                   0x00
#define USB_ST_CMF                      0x01
#define USB_ST_ACMF                     0x02
#define USB_ST_DLMF                     0x03
#define USB_ST_TRF                      0x04
#define USB_ST_TCLF                     0x05
#define USB_ST_UF                       0x06
#define USB_ST_CSF                      0x07
#define USB_ST_TOMF                     0x08
#define USB_ST_USBTF                    0x09
#define USB_ST_NCT                      0x0a
#define USB_ST_PUF                      0x0b
#define USB_ST_EUF                      0x0c
#define USB_ST_MCMF                     0x0d
#define USB_ST_CCMF                     0x0e
#define USB_ST_ENF                      0x0f
#define USB_ST_ATMNF                    0x10

#define	USB_ST_WHCM			0x11
#define	USB_ST_MDLM			0x12
#define	USB_ST_MDLMD			0x13
#define	USB_ST_DMM			0x14
#define USB_ST_OBEX			0x15
#define	USB_ST_CS			0x16
#define	USB_ST_CSD			0x17
#define	USB_ST_TCM			0x18

#define USB_ST_HID          0x21
#define USB_ST_REP          0x22
#define USB_ST_PID          0x23

/*
 * commuications class description structures
 *
 * c.f. CDC 5.1
 * c.f. WCMC 6.7.2
 *
 * XXX add the other dozen class descriptor description structures....
 */

struct usb_header_description {
    __u8        bDescriptorSubtype;
    __u16       bcdCDC;
};

struct usb_call_management_description {
    __u8        bmCapabilities;
    __u8        bDataInterface;
};

struct usb_abstract_control_description {
    __u8        bmCapabilities;
};

struct usb_union_function_description {
    __u8        bMasterInterface;
    __u8        bSlaveInterface[1];
};

struct usb_ethernet_networking_description {
    char *      iMACAddress;
    __u8        bmEthernetStatistics;
    __u16       wMaxSegmentSize;
    __u16       wNumberMCFilters;
    __u8        bNumberPowerFilters;
};

struct usb_mobile_direct_line_model_description {
    __u16	bcdVersion;
    __u8	bGUID[16];
};

struct usb_mobile_direct_line_model_detail_description {
    __u8	bGuidDescriptorType;
    __u8	bDetailData[2];
};


struct usb_hid_description {
    __u16   bcdHID;
    __u8    bCountryCode;
    __u8    bDescriptorType1;
    __u16   wDescriptorLength1;
};


struct usb_class_description {
    __u8        bDescriptorSubtype;
    __u8        elements;
    union {
        struct usb_header_description                   header;
        struct usb_call_management_description          call_management;
        struct usb_abstract_control_description         abstract_control;
        struct usb_union_function_description           union_function;
        struct usb_ethernet_networking_description      ethernet_networking;
	struct usb_mobile_direct_line_model_description mobile_direct;
	struct usb_mobile_direct_line_model_detail_description mobile_direct_detail;
    struct usb_hid_description hid;
    } description;
};


/* endpoint modifiers
 * static struct usb_endpoint_description function_default_A_1[] = {
 *
 *     {this_endpoint: 0, attributes: CONTROL,   max_size: 8,  polling_interval: 0 },
 *     {this_endpoint: 1, attributes: BULK,      max_size: 64, polling_interval: 0, direction: IN},
 *     {this_endpoint: 2, attributes: BULK,      max_size: 64, polling_interval: 0, direction: OUT},
 *     {this_endpoint: 3, attributes: INTERRUPT, max_size: 8,  polling_interval: 0},
 *
 *
 */
#define OUT             0x00
#define IN              0x80

#define CONTROL         0x00
#define ISOCHRONOUS     0x01
#define BULK            0x02
#define INTERRUPT       0x03


/* configuration modifiers
 */
#define BMATTRIBUTE_RESERVED            0x80
#define BMATTRIBUTE_SELF_POWERED        0x40
#define BMATTRIBUTE_REMOTE_WAKEUP       0x20

/* OTG bmAttribute modifiers */
#define BMATTRIBUTE_HNP_SUPPORT         0x2
#define BMATTRIBUTE_SRP_SUPPORT         0x1

/*
 * usb device description structures
 */
 
struct usb_otg_description {
    __u8                bmAttributes;
};

struct usb_endpoint_description {
    __u8                bEndpointAddress;
    __u8                bmAttributes;
    __u16               wMaxPacketSize;
    __u8                bInterval;
    __u8                direction;
    __u32               transferSize;           // maximum bulk transfer size
};

struct usb_alternate_description {
    char               *iInterface;
    __u8                bAlternateSetting;
    // list of CDC class descriptions for this alternate interface
    __u8                classes;
    struct usb_class_description *class_list;
    // list of endpoint descriptions for this alternate interface
    __u8                endpoints;
    struct usb_endpoint_description *endpoint_list;
    struct usb_otg_description *otg_description;
};

struct usb_interface_description {

    __u8                bInterfaceClass;
    __u8                bInterfaceSubClass;
    __u8                bInterfaceProtocol;
    char               *iInterface;
    // list of alternate interface descriptions for this interface
    __u8                alternates;
    struct usb_alternate_description *alternate_list;
};

struct usb_configuration_description {
    char               *iConfiguration;
    __u8                bmAttributes;
    __u8                bMaxPower;
    // list of interface descriptons for this configuration
    __u8                interfaces;            
    struct usb_interface_description *interface_list;
    struct usb_otg_description *otg_description;
    int                 configuration_type;
};

#define VENDOR  0xff

struct usb_device_description {
    __u8                bDeviceClass;
    __u8                bDeviceSubClass;
    __u8                bDeviceProtocol;

    __u16               idVendor;
    __u16               idProduct;

    char               *iManufacturer;
    char               *iProduct;
    char               *iSerialNumber;
};



/* 
 * standard usb descriptor structures
 */

struct usb_otg_descriptor {
    __u8                bLength;
    __u8                bDescriptorType;
    __u8                bmAttributes;
} __attribute__((packed));

struct usb_endpoint_descriptor {
    __u8        bLength;
    __u8        bDescriptorType;                // 0x5
    __u8        bEndpointAddress;
    __u8        bmAttributes;
    __u16       wMaxPacketSize;
    __u8        bInterval;
} __attribute__((packed));

struct usb_interface_descriptor {
    __u8        bLength;
    __u8        bDescriptorType;                // 0x04
    __u8        bInterfaceNumber;
    __u8        bAlternateSetting;
    __u8        bNumEndpoints;
    __u8        bInterfaceClass;
    __u8        bInterfaceSubClass;
    __u8        bInterfaceProtocol;
    __u8        iInterface;
} __attribute__((packed));

struct usb_configuration_descriptor {
    __u8        bLength;
    __u8        bDescriptorType;                // 0x2
    __u16       wTotalLength;
    __u8        bNumInterfaces;
    __u8        bConfigurationValue;
    __u8        iConfiguration;
    __u8        bmAttributes;
    __u8        bMaxPower;
} __attribute__((packed));

struct usb_device_descriptor {
    __u8        bLength;
    __u8        bDescriptorType;                // 0x01
    __u16       bcdUSB;
    __u8        bDeviceClass;
    __u8        bDeviceSubClass;
    __u8        bDeviceProtocol;
    __u8        bMaxPacketSize0;
    __u16       idVendor;
    __u16       idProduct;
    __u16       bcdDevice;
    __u8        iManufacturer;
    __u8        iProduct;
    __u8        iSerialNumber;
    __u8        bNumConfigurations;
} __attribute__((packed));

struct usb_string_descriptor {
    __u8        bLength;
    __u8        bDescriptorType;                // 0x03
    __u16       wData[1];
} __attribute__((packed));

struct usb_generic_descriptor {
    __u8        bLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;
} __attribute__((packed));


/* HID class descriptor structure */

struct usb_class_hid_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
	__u16		bcdHID;
	__u8		bCountryCode;
	__u8		bNumDescriptors;
    __u8        bDescriptorType1;
	__u16		wDescriptorLength1;
} __attribute__((packed));


/* 
 * communications class descriptor structures
 *
 * c.f. CDC 5.2 Table 25c
 */

struct usb_class_function_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;
} __attribute__((packed));

struct usb_class_function_descriptor_generic {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;
    __u8        bmCapabilities;
} __attribute__((packed));

struct usb_class_header_function_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x00
    __u16       bcdCDC;
} __attribute__((packed));

struct usb_class_call_management_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x01
    __u8        bmCapabilities;
    __u8        bDataInterface;
} __attribute__((packed));

struct usb_class_abstract_control_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x02
    __u8        bmCapabilities;
} __attribute__((packed));

struct usb_class_direct_line_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x03
} __attribute__((packed));

struct usb_class_telephone_ringer_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x04
    __u8        bRingerVolSeps;
    __u8        bNumRingerPatterns;
} __attribute__((packed));

struct usb_class_telephone_call_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x05
    __u8        bmCapabilities;
} __attribute__((packed));

struct usb_class_union_function_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x06
    __u8        bMasterInterface;
    __u8        bSlaveInterface0[0];
} __attribute__((packed));

struct usb_class_country_selection_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x07
    __u8        iCountryCodeRelDate;
    __u16       wCountryCode0[0];
} __attribute__((packed));


struct usb_class_telephone_operational_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x08
    __u8        bmCapabilities;
} __attribute__((packed));


struct usb_class_usb_terminal_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x09
    __u8        bEntityId;
    __u8        bInterfaceNo;
    __u8        bOutInterfaceNo;
    __u8        bmOptions;
    __u8        bChild0[0];
} __attribute__((packed));

struct usb_class_network_channel_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x0a
    __u8        bEntityId;
    __u8        iName;
    __u8        bChannelIndex;
    __u8        bPhysicalInterface;
} __attribute__((packed));

struct usb_class_protocol_unit_function_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x0b
    __u8        bEntityId;
    __u8        bProtocol;
    __u8        bChild0[0];
} __attribute__((packed));

struct usb_class_extension_unit_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x0c
    __u8        bEntityId;
    __u8        bExtensionCode;
    __u8        iName;
    __u8        bChild0[0];
} __attribute__((packed));

struct usb_class_multi_channel_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x0d
    __u8        bmCapabilities;
} __attribute__((packed));

struct usb_class_capi_control_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x0e
    __u8        bmCapabilities;
} __attribute__((packed));

struct usb_class_ethernet_networking_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x0f
    __u8        iMACAddress;
    __u32       bmEthernetStatistics;
    __u16       wMaxSegmentSize;
    __u16       wNumberMCFilters;
    __u8        bNumberPowerFilters;
} __attribute__((packed));

struct usb_class_atm_networking_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x10
    __u8        iEndSystermIdentifier;
    __u8        bmDataCapabilities;
    __u8        bmATMDeviceStatistics;
    __u16       wType2MaxSegmentSize;
    __u16       wType3MaxSegmentSize;
    __u16       wMaxVC;
} __attribute__((packed));


struct usb_class_mdlm_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x12
    __u16	bcdVersion;
    __u8	bGUID[16];
} __attribute__((packed));

struct usb_class_mdlmd_descriptor {
    __u8        bFunctionLength;
    __u8        bDescriptorType;
    __u8        bDescriptorSubtype;                     // 0x13
    __u8	bGuidDescriptorType;
    __u8	bDetailData[0];

} __attribute__((packed));


/* 
 * descriptor union structures
 */

struct usb_descriptor {
    union {
        struct usb_generic_descriptor generic;
        struct usb_endpoint_descriptor endpoint;
        struct usb_interface_descriptor interface;
        struct usb_configuration_descriptor configuration;
        struct usb_device_descriptor device;
        struct usb_string_descriptor string;
        struct usb_otg_descriptor otg;
    } descriptor;

} __attribute__((packed));

struct usb_class_descriptor {
    union {
        struct usb_class_function_descriptor function;
        struct usb_class_function_descriptor_generic generic;
        struct usb_class_header_function_descriptor header_function;
        struct usb_class_call_management_descriptor call_management;
        struct usb_class_abstract_control_descriptor abstract_control;
        struct usb_class_direct_line_descriptor direct_line;
        struct usb_class_telephone_ringer_descriptor telephone_ringer;
        struct usb_class_telephone_operational_descriptor telephone_operational;
        struct usb_class_telephone_call_descriptor telephone_call;
        struct usb_class_union_function_descriptor union_function;
        struct usb_class_country_selection_descriptor country_selection;
        struct usb_class_usb_terminal_descriptor usb_terminal;
        struct usb_class_network_channel_descriptor network_channel;
        struct usb_class_extension_unit_descriptor extension_unit;
        struct usb_class_multi_channel_descriptor multi_channel;
        struct usb_class_capi_control_descriptor capi_control;
        struct usb_class_ethernet_networking_descriptor ethernet_networking;
        struct usb_class_atm_networking_descriptor atm_networking;
        struct usb_class_mdlm_descriptor mobile_direct;
        struct usb_class_mdlmd_descriptor mobile_direct_detail;
        struct usb_class_hid_descriptor hid;
    } descriptor;

} __attribute__((packed));


struct usb_alternate_instance {
    struct usb_interface_descriptor *interface_descriptor;

    int                         classes;
    struct usb_class_descriptor **classes_descriptor_array;

    int                         endpoints;
    int                         *endpoint_transfersize_array;
    struct usb_endpoint_descriptor **endpoints_descriptor_array;
    struct usb_otg_descriptor   *otg_descriptor;
};

struct usb_interface_instance {


    int                         alternates;
    //struct usb_interface_descriptor **alternates_descriptor_array;
    struct usb_alternate_instance *alternates_instance_array;


};



/* Operations that the bus interface driver can use to interact with a
 * function driver. 
 *
 * Typically these are called to deal with either specific changes of state
 * to an endpoint: * reset, suspend, resume; or to process received data. 
 *
 * The default endpoint 0 driver will use the configure entry point to tell
 * a function driver that it has been configured for use.
 *
 * The urb sent callback is specified in the usbd_urb structure so that specific
 * completion routines can be specified for each type of data being
 * transmitted.
 *
 * The receive urb function is called when there is data to be processed.
 *
 * The receive setup function is called when there is an endpoint zero
 * setup packet to be processed.
 *
 * The alloc urb data function is called to allocate urb data buffers.
 *
 */


struct usb_function_operations {
    void (* event)(struct usb_device_instance *, usb_device_event_t, int);   
    int (* urb_sent) (struct usbd_urb *, int);
    int (* recv_urb) (struct usbd_urb *);
    int (* recv_setup) (struct usbd_urb *);
    int (* alloc_urb_data) (struct usbd_urb *, int );
    void (* dealloc_urb_data) (struct usbd_urb *);
    void (* function_init) (struct usb_bus_instance *, struct usb_device_instance *, struct usb_function_driver *);
    void (* function_exit) (struct usb_device_instance *);
};


struct usb_configuration_instance {
    int                         interfaces;
    struct usb_configuration_descriptor *configuration_descriptor;
    struct usb_interface_instance *interface_instance_array;
    struct usb_function_driver *function_driver;
};




/* Function Driver data structure
 *
 * Function driver and its configuration descriptors. 
 *
 * This is passed to the usb-device layer when registering. It contains all
 * required information about the function driver for the usb-device layer
 * to use the function drivers configuration data and to configure this
 * function driver an active configuration.
 *
 * Note that each function driver registers itself on a speculative basis.
 * Whether a function driver is actually configured will depend on the USB
 * HOST selecting one of the function drivers configurations. 
 *
 * This may be done multiple times WRT to either a single bus interface
 * instance or WRT to multiple bus interface instances. In other words a
 * multiple configurations may be selected for a specific bus interface. Or
 * the same configuration may be selected for multiple bus interfaces. 
 *
 */
struct usb_function_driver {
    const char         *name;
    struct usb_function_operations *ops;// functions 

                                        // device & configuration descriptions 
    struct usb_device_description        *device_description;
    struct usb_configuration_description *configuration_description;
    int                 configurations;

                                        // constructed descriptors
    struct usb_device_descriptor        *device_descriptor;
    struct usb_configuration_instance *configuration_instance_array;

    struct list_head    drivers;        // linked list 
    struct module	*this_module;	// manage inc use counts to prevent unload races
};

void usbd_function_init(struct usb_bus_instance *, struct usb_device_instance *, struct usb_function_driver *);
void usbd_function_close(struct usb_device_instance *);


