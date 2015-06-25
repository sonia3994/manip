//*-- Author :  Phil Harvey - 11/03/2005
//_______________________________________________________________________
//
// PInterfaceUSB
//
// USB hardware interfaces
//
#include <stdio.h>
#include <string.h>
#include <usb.h>
#include <curses.h>
#include "PInterfaceUSB.h"
#include "display.h"

//#define DEBUG_USB     // uncomment this to print USB read/write strings

// gymnastics necessary due to problem including <usb.h> in the header file
// because it thoroughly confuses rootcint when generating the dictionary

#define fDev ((struct usb_device *)mDev)
#define fHandle ((usb_dev_handle *)mHandle)

//--------------------------------------------------------------------------------------

PInterfaceUSB::PInterfaceUSB()
{
    fDebug = 0;
    mDev = NULL;
    mHandle = NULL;
    fSerialNumber = NULL;
    fVendor = fProduct = 0;
    fReadEndpoint = fWriteEndpoint = 0;
    fTimeout = 100;
}

PInterfaceUSB::~PInterfaceUSB()
{
    Close();
    delete fSerialNumber;
    fSerialNumber = NULL;
}

int PInterfaceUSB::Open(int vendor, int product, int instance)
{
    // open USB connection to device (or close/open if already open)
    // Returns: 0 = device not found, 1 = success, <0 = error code
    fVendor = vendor;
    fProduct = product;
    int n = OpenDevice(vendor, product, instance);;
    return n;
}

int PInterfaceUSB::Close()
{
    // Close USB device
    // Returns: 0 on success, <0 on error
    int result = 0;
    if (fHandle) {
        result = usb_close(fHandle);
        if (result == 0) result = 1;
        mHandle = NULL;
    }
    return result;
}

int PInterfaceUSB::ClearHalt(int writeEndpoint)
{
    int ep = writeEndpoint ? fWriteEndpoint : fReadEndpoint;
    if (!fHandle) return -1;
    return(usb_clear_halt(fHandle, ep));
}

int PInterfaceUSB::Read(char *buff, int len)
{
    // Read from USB device (uses current Endpoint and Timeout)
    // Returns: number of bytes read or <0 on error

    if (!fHandle) {
        //fprintf(stderr, "PInterfaceUSB::ReadBuff : USB device not open\n");
        return -1;
    }
    int rtn = usb_bulk_read(fHandle, fReadEndpoint, buff, len, fTimeout);
    if (fDebug > 1) {
        if (rtn < 0) {
            if (rtn != -110) dprintf("ERROR: %s\n",usb_strerror());
            if (fDebug > 2 || rtn != -110) dprintf("USB read error %d\n(%s)\n",
                                                  rtn, usb_strerror());
        } else {
            dprintf("USB read %d bytes:\n", rtn);
            for (int i=0; i<rtn; i+=2) {
                if (i>=16 and i<rtn-16) {
                    dprintf("   ...\n");
                    i = rtn-16;
                }
                dprintf("%5d) 0x%.2x", i/2, (unsigned char)buff[i]);
                if (i+1 < rtn) {
                    dprintf(" 0x%.2x    0x%.4x",
                        (unsigned char)buff[i+1],
                        (unsigned short)((unsigned char)buff[i]+((unsigned char)buff[i+1]<<8)));
                }
                dprintf("\n");
            }
        }
    }
#ifdef DEBUG_USB
    dprintf("read %d %.*s",rtn,rtn>0?rtn:0,buff);
#endif
    return rtn;
}

int PInterfaceUSB::Write(char *buff, int len)
{
    // Write to USB device (uses current Endpoint and Timeout)
    // Returns: number of bytes written or <0 on error
    if (!fHandle) {
        //fprintf(stderr, "PInterfaceUSB::WriteBuff : USB device not open\n");
        return -1;
    }
    if (len < 0) len = strlen(buff);

    int rtn = usb_bulk_write(fHandle, fWriteEndpoint, buff, len, fTimeout);
        if( rtn < 0 ) {
            //dprintf("ERROR: %s\n",usb_strerror());
            //fprintf(stderr, "PInterfaceUSB::WriteBuff : Error writing to USB device [%d]\n(%s)\n",
            //        rtn, usb_strerror());
        } else {
            if (fDebug > 1) {
                dprintf("PInterfaceUSB::WriteBuff : USB write %d bytes:\n", rtn);
                for (int i=0; i<rtn; i+=2) {
                    dprintf("%5d) 0x%.2x", i/2, (unsigned char)buff[i]);
                    if (i+1 < rtn) {
                        dprintf(" 0x%.2x    0x%.4x",
                                     (unsigned char)buff[i+1],
                                     (unsigned short)((unsigned char)buff[i]+((unsigned char)buff[i+1]<<8)));
                    }
                    dprintf("\n");
                }
            }
    }
#ifdef DEBUG_USB
    dprintf("write %d %s",rtn,buff);//TEST
#endif
    return rtn;
}

void PInterfaceUSB::SetDebug(int lvl)
{
    fDebug = lvl;
    usb_set_debug(lvl);
}

int PInterfaceUSB::OpenDevice(int vendor, int product, int instance)
{
    // Open USB device
    // - 'vendor' and 'product' may be set to zero to accept any device
    // - 'instance' is the device index, where 0 is the first matching device
    // Returns: 0 = device not found, 1 = success, <0 = error code
    int result;

    Close();

    usb_init();
    result = usb_find_busses();
    if (result >= 0) result = usb_find_devices();
    if (result < 0) {
        //fprintf(stderr, "PInterfaceUSB::OpenDevice : Unable to find USB device [0x%x]\n", result);
        return result;
    }
    for (struct usb_bus *bus=usb_busses; bus; bus=bus->next) {
        for (struct usb_device *dev=bus->devices; dev; dev=dev->next) {
            if (fDebug > 1) {
                dprintf("USB device found: Vendor 0x%.4x product 0x%.4x\n",
                       dev->descriptor.idVendor,dev->descriptor.idProduct);
            }
            // look for the specified vendor and product
            if (vendor && dev->descriptor.idVendor != vendor) continue;
            if (product && dev->descriptor.idProduct != product) continue;
            if (instance--) continue;   // look for the next one if specified
            usb_dev_handle *udev = usb_open(dev);
            if (!udev) {
                //fprintf(stderr, "PInterfaceUSB::OpenDevice : Failed to open USB device ( NULL pointer ) \n");
                return -1;
            }
            // save the serial number string
            int isn = dev->descriptor.iSerialNumber;
            char serial[256];
            result = usb_get_string_simple(udev, isn, serial, sizeof(serial));
            if (result > 0) {
                // attempt to claim the interface
                if (usb_set_configuration(udev, 1)) {
                    //fprintf(stderr, "PInterfaceUSB::OpenDevice : Failed to set configuration\n");
                    usb_close(udev);
                    return -2;
                }
                if (usb_claim_interface(udev, 0)) {
                    //fprintf(stderr, "PInterfaceUSB::OpenDevice : Failed to claim interface\n");
                    usb_close(udev);
                    return -3;
                }
                // save device information
                mDev = dev;
                mHandle = udev;
                fVendor = dev->descriptor.idVendor;
                fProduct = dev->descriptor.idProduct;
                fSerialNumber = new char[strlen(serial)+1];
                if (fSerialNumber) strcpy(fSerialNumber, serial);
                return 1;   // success! (and leave device open)
            }
            usb_close(udev);
        }
    }
    return 0;   
}


