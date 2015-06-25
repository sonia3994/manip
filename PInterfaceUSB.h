//
// File:    PInterfaceUSB.h
//
// Author:	Phil Harvey - 11/03/2005
//
#ifndef __PInterfaceUSB_h__
#define __PInterfaceUSB_h__

class PInterfaceUSB
{
public:
    PInterfaceUSB();
    virtual ~PInterfaceUSB();

    virtual int Open(int vendor, int product, int instance=0);
    virtual int Close();
    virtual int Read(char *buff, int bsize);
    virtual int Write(char *buff, int len=-1);

    void    SetReadEndpoint(int e)    { fReadEndpoint = e; }
    void    SetWriteEndpoint(int e)   { fWriteEndpoint = e; }
    void    SetTimeout(int t)         { fTimeout = t; }
    void    SetProductID(int p)       { fProduct = p; }
    void    SetVendorID(int v)        { fVendor = v; }
    void    SetDebug(int lvl=1);

    int     GetReadEndpoint()   { return fReadEndpoint; }
    int     GetWriteEndpoint()  { return fWriteEndpoint; }
    int     GetTimeout()        { return fTimeout; }

    int     GetVendorID()       { return fVendor; }
    int     GetProductID()      { return fProduct; }
    char  * GetSerialNumber()   { return fSerialNumber; }
    int     ClearHalt(int writeEndpoint=0);

private:
    int OpenDevice(int vendor=0, int product=0, int instance=0);

    void *  mDev;               //! pointer to usb device structure
    void *  mHandle;            //! usb device handle
    char *  fSerialNumber;      //!
    int     fVendor;
    int     fProduct;
    int     fReadEndpoint;
    int     fWriteEndpoint;
    int     fTimeout;
    int     fDebug;
};

#endif // __PInterfaceUSB_h__
