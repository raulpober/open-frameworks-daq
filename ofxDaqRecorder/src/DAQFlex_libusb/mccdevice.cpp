#include <iostream>
#include <fstream>
#include <string>
#include <libusb.h>

#include "mccdevice.h"
#include "pollingthread.h"
#include "datatypesandstatics.h"
#include "databuffer.h"
#include "stringutil.h"

using namespace std;

//This method finds the first available device where product ID == idProduct
MCCDevice::MCCDevice(int idProduct)
{
    int i;
    bool found = false;
    ssize_t sizeOfList;
    libusb_device_descriptor desc;
    libusb_device* device;

    //Check if the product ID is a valid MCC product ID
    if(!isMCCProduct(idProduct))
        throw MCC_ERR_INVALID_ID;

    //Initialize USB libraries
    if(libusb_init(NULL) != 0)
    {
        throw MCC_ERR_USB_INIT;
    }

    //Get the list of USB devices connected to the PC
    sizeOfList = libusb_get_device_list(NULL, &list);

    //Traverse the list of USB devices to find the requested device
    for(i=0; (i<sizeOfList) && (!found); i++)
    {
        device = list[i];
        libusb_get_device_descriptor(device, &desc);
        if(desc.idVendor == MCC_VENDOR_ID && desc.idProduct == idProduct)
        {
            found = true;
            cout << "Found " << toNameString(idProduct) << "\n";

            //Open the device
            if(!libusb_open(device, &dev_handle))
            {
                //Claim interface with the device
                if(!libusb_claim_interface(dev_handle,0))
                {
                    //Get input and output endpoints
                    //getEndpoints();
                    getScanParams();
					found = true;
                }
            }
        }
    }

    if(!found)
    {
        throw MCC_ERR_NO_DEVICE;
    }
    else
    {
        this->idProduct = idProduct;
        initDevice();
    }
}

//This method finds a unique device where product ID == idProduct and serial number == mfgSerialNumber
MCCDevice::MCCDevice(int idProduct, string mfgSerialNumber)
{
    int i;
    bool found = false;
    ssize_t sizeOfList;
    libusb_device_descriptor desc;
    libusb_device* device;

    string mfgsermsg = "?DEV:MFGSER";
    string retMessage;

    //Check if the product ID is a valid MCC product ID
    if(!isMCCProduct(idProduct))
        throw MCC_ERR_INVALID_ID;

    //Initialize USB libraries
    if(libusb_init(NULL) != 0)
    {
        throw MCC_ERR_USB_INIT;
    }

    //Get the list of USB devices connected to the PC
    sizeOfList = libusb_get_device_list(NULL, &list);

    //Traverse the list of USB devices to find the requested device
    for(i=0; (i<sizeOfList) && (!found); i++)
    {
        device = list[i];
        libusb_get_device_descriptor(device, &desc);
        if(desc.idVendor == MCC_VENDOR_ID && desc.idProduct == idProduct)
        {
            //Open the device
            if(!libusb_open(device, &dev_handle))
            {
                //Claim interface with the device
                if(!libusb_claim_interface(dev_handle,0))
                {
                    //Get input and output endpoints
                    getScanParams();

                    try{
                        //get the device serial number
                        retMessage = sendMessage(mfgsermsg);
                    }
                    catch(mcc_err err){
                        throw err;
                    }

                    //Erase message while keeping serial number
                    retMessage.erase(0,11);
                    cout << "Found " << toNameString(idProduct) << " with Serial Number " << retMessage << "\n";

                    //check that the serial numbers are the same
                    if(retMessage.compare(mfgSerialNumber))
                        //serial numbers are not the same, release device and continue on
                        libusb_release_interface(dev_handle, 0);
                    else
                        //serial numbers are the same, this is the correct device
                        found = true;
                }
            }
        }
    }

    if(!found)
    {
        throw MCC_ERR_NO_DEVICE;
    }
    else
    {
        this->idProduct = idProduct;
        initDevice();
    }
}



MCCDevice::~MCCDevice () {
    //Free memory and devices
    libusb_release_interface(dev_handle, 0);
    libusb_close(dev_handle);
    libusb_free_device_list(list, true);
    libusb_exit(NULL);
}

void MCCDevice::threadedFunction()
{
    int err = 0, transferred = 0;
    buffer->currIndex = 0;

    unsigned char* dataAsByte = (unsigned char*)buffer->data;
    unsigned int timeout = 4;

    while (isThreadRunning()){
        err =  libusb_bulk_transfer(dev_handle, 
            endpoint_in, 
            &dataAsByte[buffer->currIndex*2], 
            bulkTxLength, &transferred, 
            timeout);
        buffer->currIndex += (transferred/2);
        buffer->currCount += (transferred/2);
        if(err == LIBUSB_ERROR_TIMEOUT && transferred > 0)//a timeout may indicate that some data was transferred, but not all
            err = 0;
        if (err < 0)
            throw libUSBError(err);
        if(buffer->currIndex >= (buffer->getNumPoints()))
        {
            buffer->currIndex = 0;
        }
    }
}

//Will return at most a 64 character array.
//Returns response if transfer successful, null if not
string MCCDevice::sendMessage(string message)
{
    try{
        sendControlTransfer(message);
        return getControlTransfer();
    }
    catch(mcc_err err)
    {
        throw err;
    }
}

void MCCDevice::initDevice()
{
    stringstream firmwarefile;
    string response;

    switch(idProduct)
    {
        case USB_1608_GX: //Fall through, same init for USB-1608GX and USB-1608GX-2AO
        case USB_1608_GX_2AO:
            maxCounts = 0xFFFF;
            //Check if the firmware has been loaded already
            response = sendMessage("?DEV:FPGACFG");
            if(!StringUtil::contains(response, "CONFIGURED"))
            {
                cout << "Firmware being flashed...\n";
                //firmware hasn't been loaded yet, do so
                firmwarefile << FIRMWAREPATH << "USB_1608G.rbf";
                transferFPGAfile(firmwarefile.str());

                //Check if the firmware got loaded successfully
                response = sendMessage("?DEV:FPGACFG");
                if(!StringUtil::contains(response, "CONFIGURED"))
                {
                    throw MCC_ERR_FPGA_UPLOAD_FAILED;
                }

                cout << "Firmware successfully flashed...\n";
            } else {
                cout << "Firmware already flashed, skipping this time\n";
            }

            break;

        case USB_7202:
            maxCounts = 0xFFFF;

            break;

        case USB_7204:
            maxCounts = 0xFFF;

            break;

        default:
            break;
    }
}

void MCCDevice::transferFPGAfile(string path)
{
    int size;
    unsigned char * memblock;
    int numBytesTransferred;
    int totalBytesTransferred = 0;
    uint16_t length;

    //Turn on FPGA configure mode
    sendMessage("DEV:FPGACFG=0XAD");

    //Open file for input in binary mode, cursor at end of file
    ifstream file;
    file.open(path.data(), ios::in|ios::binary|ios::ate);

    if (file.is_open())
      {
        size = (int)file.tellg();
        file.seekg(0, ios::beg);
        memblock = new unsigned char[size];
        file.read((char*)memblock, size);
        file.close();

        int packetcount = 0;

        while(totalBytesTransferred < size)
        {
            packetcount++;
            if((size - totalBytesTransferred) > MAX_MESSAGE_LENGTH)
                length = MAX_MESSAGE_LENGTH;
            else
            {
                length = size - totalBytesTransferred;
            }

            numBytesTransferred = libusb_control_transfer(dev_handle, LIBUSB_REQUEST_TYPE_VENDOR + LIBUSB_ENDPOINT_OUT,
                                                  FPGADATAREQUEST, 0, 0, &memblock[totalBytesTransferred], length, 1000);

            if(numBytesTransferred < 0)
                throw libUSBError(numBytesTransferred);

            totalBytesTransferred += numBytesTransferred;
        }

        delete[] memblock;
      }
      else
      {
          throw MCC_ERR_CANT_OPEN_FPGA_FILE;
      }
}

//Send a message to the device
void MCCDevice::sendControlTransfer(string message)
{
    int numBytesTransferred;

    StringUtil::toUpper(message);

    cout << "Sending: " << message << "\n";
    uint16_t length = message.length();
    const char* msgData = message.data();
    unsigned char data[MAX_MESSAGE_LENGTH];
    for (uint16_t i = 0; i < MAX_MESSAGE_LENGTH; i++) {
        data[i] = (i < length) ? msgData[i] : 0;
    }
    numBytesTransferred = libusb_control_transfer(dev_handle, LIBUSB_REQUEST_TYPE_VENDOR + LIBUSB_ENDPOINT_OUT,
                                                  STRINGMESSAGE, 0, 0, data, MAX_MESSAGE_LENGTH, 1000);
    if(numBytesTransferred < 0)
        throw libUSBError(numBytesTransferred);
}

//Receive a message from the device. This should follow a call to sendControlTransfer.
//It will return a pointer to at most a 64 character array.
string MCCDevice::getControlTransfer()
{
    int messageLength;
    unsigned char message[64];
    string strmessage;

    messageLength = libusb_control_transfer(dev_handle,  LIBUSB_REQUEST_TYPE_VENDOR + LIBUSB_ENDPOINT_IN,
				                               STRINGMESSAGE, 0, 0, message, 64, 1000);

    if(messageLength < 0)
        throw libUSBError(messageLength);

    strmessage = (char*)message;
    cout << "Got: " << strmessage << "\n\n";
    return strmessage;
}

/*Reads analog in scan data.
  length is the length of the data array (max bytes to transfer)
*/
void MCCDevice::readScanData(unsigned short* data, int length, int rate)
{
    int err = 0, totalTransferred = 0, transferred;
    unsigned char* dataAsByte = (unsigned char*)data;
    unsigned int timeout = 2000000/(64*rate);

    do{
        err =  libusb_bulk_transfer(dev_handle, endpoint_in, &dataAsByte[totalTransferred], 64, &transferred, timeout);
        totalTransferred += transferred;
        if(err == LIBUSB_ERROR_TIMEOUT && transferred > 0)//a timeout may indicate that some data was transferred, but not all
            err = 0;
    }while (totalTransferred < length*2 && err >= 0);

    //cout << "Transferred " << totalTransferred << " bytes\n";
    if (err < 0)
        throw libUSBError(err);
}

void MCCDevice::startContinuousTransfer(unsigned int rate, dataBuffer* buffer)
{
    continuousInfo = new threadArg;

    continuousInfo->dev_handle = dev_handle;
    continuousInfo->endpoint_in = endpoint_in;
    continuousInfo->rate = rate;
    continuousInfo->buffer = buffer;

    pollThread = new pollingThread(continuousInfo);
    pollThread->start();
}

void MCCDevice::stopContinuousTransfer()
{
    pollThread->stop();
    delete pollThread;
}

void MCCDevice::startBackgroundTransfer(unsigned int sampleRate, unsigned int timeout, unsigned int bulkTxLength, dataBuffer* buffer)
{
    this->sampleRate = sampleRate;
    this->timeout = timeout;
    this->bulkTxLength = bulkTxLength;
    this->buffer = buffer;
    this->startThread(true,false);
}

void MCCDevice::stopBackgroundTransfer()
{
    this->stopThread(true);
}


void MCCDevice::transferCallbackFunction(struct libusb_transfer *transfer_cb)
{
    int err;
    intTransferInfo *info;
    cout << "Callback Reached\n";

    switch(transfer_cb->status)
    {
        case LIBUSB_TRANSFER_COMPLETED:
            cout << "Transfer Completed\n";

            info = (intTransferInfo*)transfer_cb->user_data;

            //info->ti_callback((unsigned short*)(transfer_cb->buffer), transfer_cb->actual_length);
            //resubmit the transfer
            cout << "Transferred " << transfer_cb->actual_length << " bytes\n";
            err = libusb_submit_transfer(transfer_cb);
            if (err < 0)
                throw libUSBError(err);
            break;
        case LIBUSB_TRANSFER_CANCELLED:
            cout << "Transfer Was Stopped\n";
            break;
        case LIBUSB_TRANSFER_ERROR:
            cout << "libusb Transfer Error\n";
            throw MCC_ERR_TRANSFER_FAILED;
        case LIBUSB_TRANSFER_TIMED_OUT:
            cout << "libusb Timed Out\n";
            throw MCC_ERR_LIBUSB_TIMEOUT;
        case LIBUSB_TRANSFER_STALL: // For bulk endpoints: halt condition detected (endpoint stalled).
            cout << "libusb Transfer Stall\n";
            throw MCC_ERR_LIBUSB_TRANSFER_STALL;
        case LIBUSB_TRANSFER_NO_DEVICE:
            cout << "Device Not Connected\n";
            throw MCC_ERR_NO_DEVICE;
        case LIBUSB_TRANSFER_OVERFLOW: //Device sent more data than requested
            cout << "libusb Transfer Overflow\n";
            throw MCC_ERR_LIBUSB_TRANSFER_OVERFLOW;
        default:
            throw MCC_ERR_UNKNOWN_LIB_USB_ERR;
    }
}

//scale and calibrate data
float MCCDevice::scaleAndCalibrateData(unsigned short data, float minVoltage, float maxVoltage, float scale, float offset)
{
    float calibratedData;
    float scaledAndCalibratedData;
    float fullScale = maxVoltage - minVoltage;

    //Calibrate the data
    calibratedData = (float)data*scale + offset;

    //Scale the data
    scaledAndCalibratedData = (calibratedData/(float)maxCounts)*fullScale + minVoltage;

    return scaledAndCalibratedData;
}

//Convert an MCC product ID to a product name
//Get the device input and output endpoints
/*void MCCDevice::getEndpoints()
{
    int numBytesTransferred;

    unsigned char* epDescriptor = new unsigned char[64];

    numBytesTransferred = libusb_control_transfer(dev_handle, STRINGMESSAGE, LIBUSB_REQUEST_GET_DESCRIPTOR,
                                (0x02 << 8) | 0, 0, epDescriptor, 64, 1000);

    if(numBytesTransferred < 0)
        throw libUSBError(numBytesTransferred);

    endpoint_in = getEndpointInAddress(epDescriptor, numBytesTransferred);
    endpoint_out = getEndpointOutAddress(epDescriptor, numBytesTransferred);
}*/

//Convert an MCC product ID to a product name
//Get the device input and output endpoints
void MCCDevice::getScanParams()
{
    int numBytesTransferred;

    unsigned char* epDescriptor = new unsigned char[64];

    numBytesTransferred = libusb_control_transfer(dev_handle, STRINGMESSAGE, LIBUSB_REQUEST_GET_DESCRIPTOR,
                                (0x02 << 8) | 0, 0, epDescriptor, 64, 1000);

    if(numBytesTransferred < 0)
        throw libUSBError(numBytesTransferred);

    endpoint_in = getEndpointInAddress(epDescriptor, numBytesTransferred);
    endpoint_out = getEndpointOutAddress(epDescriptor, numBytesTransferred);
    bulkPacketSize = getBulkPacketSize(epDescriptor, numBytesTransferred);
	cout << "Bulk Packet Size: " << bulkPacketSize << endl;
}

unsigned short MCCDevice::getBulkPacketSize(unsigned char* data, int data_length){
    int descriptorType;
    int length;
    int index = 0;

    while(true){
        length = data[index];
        descriptorType = data[index+1];

        if(length == 0)
            break;

        if(descriptorType != 0x05){
            index += length;
        } else {
            if((data[index+2] & 0x80) != 0){
                //found the packet size
                return (unsigned short)(data[index+5] << 8) | (unsigned short)(data[index + 4]);
            } else {
                index += length;
            }
        }
    }
}


//Find the input endpoint from an endpoint descriptor
unsigned char MCCDevice::getEndpointInAddress(unsigned char* data, int data_length)
{
    int descriptorType;
    int length;
    int index = 0;

    while (true)
    {
        length = data[index];
        descriptorType = data[index + 1];

        if (length == 0)
            break;

        if (descriptorType != 0x05)
        {
            index += length;
        }
        else
        {
            if ((data[index + 2] & 0x80) != 0)
                return data[index + 2];
            else
                index += length;
        }

        if (index >= data_length)
            break;
    }

    return 0;
}

//Find the output endpoint from an endpoint descriptor
unsigned char MCCDevice::getEndpointOutAddress(unsigned char* data, int data_length)
{
    int descriptorType;
    int length;
    int index = 0;

    while (true)
    {
        length = data[index];
        descriptorType = data[index + 1];

        if (length == 0)
            break;

        if (descriptorType != 0x05)
        {
            index += length;
        }
        else
        {
            if ((data[index + 2] & 0x80) == 0)
                return data[index + 2];
            else
                index += length;
        }

        if (index >= data_length)
            break;
    }

    return 0;
}

void MCCDevice::flushInputData()
{
    int bytesTransfered = 0;
	int status;

	do
	{
        unsigned char buf[64];
		status = libusb_bulk_transfer(dev_handle, endpoint_in, buf, 64, &bytesTransfered, 200);
    } while (bytesTransfered > 0 && status == 0);
}
