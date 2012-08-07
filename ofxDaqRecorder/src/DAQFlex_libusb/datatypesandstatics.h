#ifndef DATATYPESANDSTATICS_H
#define DATATYPESANDSTATICS_H

#include <string>
#include <sstream>

#define MCC_VENDOR_ID 0x09db

//Device Product IDs
#define USB_2001_TC 0x00F9
#define USB_7202 0x00F2
#define USB_7204 0x00F0
#define USB_1608_GX 0x0111
#define USB_1608_GX_2AO 0x0112

#define FIRMWAREPATH "/usr/lib/daqflex/"

#define SLOPE 0
#define OFFSET 1

#define FIRSTHALF true
#define SECONDHALF false

using namespace std;

//typedef void (*mcc_cb_fn)(unsigned short* data, int transferred);

//Error Codes
enum mcc_err{
    MCC_ERR_NO_DEVICE,
    MCC_ERR_INVALID_ID,
    MCC_ERR_USB_INIT,
    MCC_ERR_PIPE,
    MCC_ERR_LIBUSB_TIMEOUT,
    MCC_ERR_TRANSFER_FAILED,
    MCC_ERR_LIBUSB_TRANSFER_STALL,
    MCC_ERR_LIBUSB_TRANSFER_OVERFLOW,
    MCC_ERR_UNKNOWN_LIB_USB_ERR,
    MCC_ERR_INVALID_BUFFER_SIZE,
    MCC_ERR_CANT_OPEN_FPGA_FILE,
    MCC_ERR_FPGA_UPLOAD_FAILED,
};

//Convert a libusb error code into an mcc_err
static mcc_err libUSBError(int err)
{
    switch(err)
    {
        case LIBUSB_ERROR_TIMEOUT:
            return MCC_ERR_LIBUSB_TIMEOUT;
        case LIBUSB_ERROR_PIPE:
            return MCC_ERR_PIPE;
        case LIBUSB_ERROR_NO_DEVICE:
            return MCC_ERR_NO_DEVICE;
        default:
            return MCC_ERR_UNKNOWN_LIB_USB_ERR;
    }
};

//Convert an mcc_err int to a human readable string
static string errorString(int err)
{
    stringstream unknownerror;

    switch(err)
    {
        case MCC_ERR_NO_DEVICE:
            return "No Matching Device Found\n";
        case MCC_ERR_INVALID_ID:
            return "Invalid Device ID\n";
        case MCC_ERR_USB_INIT:
            return "Failed to Init USB\n";
        case MCC_ERR_PIPE:
            return "Libusb Pipe Error, possibly invalid command\n";
        case MCC_ERR_LIBUSB_TIMEOUT:
            return "Transfer Timed Out\n";
        case MCC_ERR_UNKNOWN_LIB_USB_ERR:
            return "Unknown LibUSB Error\n";
        case MCC_ERR_INVALID_BUFFER_SIZE:
            return "Buffer must be and integer multiple of 32\n";
        case MCC_ERR_CANT_OPEN_FPGA_FILE:
            return "Cannot open FPGA file\n";
        case MCC_ERR_FPGA_UPLOAD_FAILED:
            return "FPGA firmware could not be uploaded\n";
        default:
            unknownerror << "Error number " << err << " has no text\n";
            return unknownerror.str();
    }
};

static string toNameString(int idProduct)
{
    switch(idProduct)
    {
        case USB_2001_TC:
            return "USB-2001-TC";
        case USB_7202:
            return "USB-7202";
        case USB_7204:
            return "USB-7204";
        case USB_1608_GX:
            return "USB-1608GX";
        case USB_1608_GX_2AO:
            return "USB-1608GX-2AO";
        default:
            return "Invalid Product ID";
    }
};

//Is the specified product ID is an MCC product ID?
static bool isMCCProduct(int idProduct)
{
    switch(idProduct)
    {
        case USB_2001_TC: case USB_7202: case USB_7204:
        case USB_1608_GX: case USB_1608_GX_2AO://same for all products
            return true;
        default:
            return false;
        break;
    }
};

#endif
