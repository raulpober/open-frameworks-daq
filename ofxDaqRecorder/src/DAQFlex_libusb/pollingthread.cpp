#include <pthread.h>
#include <libusb.h>
#include <errno.h>
#include <iostream>

#include "pollingthread.h"
#include "mccdevice.h"
#include "datatypesandstatics.h"
#include "databuffer.h"

using namespace std;

pollingThread::pollingThread(threadArg* arg)
{
    argPtr = arg;
}

int pollingThread::start()
{
    int code = pthread_create(&thread_, 0, pollingThread::entryPoint, this);
    return code;
}

int pollingThread::stop()
{
    int code = pthread_cancel(thread_);
    return code;
}

/*static */
void * pollingThread::entryPoint(void * pthis)
{
    reinterpret_cast<pollingThread *>(pthis)->run();
}

void pollingThread::run()
{
    try{
        execute();
    }
    catch(mcc_err err)
    {
        cout << errorString(err);
    }
}

void pollingThread::execute()
{
    int err = 0, transferred = 0;
    argPtr->buffer->currIndex = 0;

    unsigned char* dataAsByte = (unsigned char*)argPtr->buffer->data;
    unsigned int timeout = 2000000/(64*argPtr->rate);

    while (1){
        err =  libusb_bulk_transfer(argPtr->dev_handle, argPtr->endpoint_in, &dataAsByte[argPtr->buffer->currIndex*2], 64, &transferred, timeout);
        argPtr->buffer->currIndex += (transferred/2);
        argPtr->buffer->currCount += (transferred/2);
        if(err == LIBUSB_ERROR_TIMEOUT && transferred > 0)//a timeout may indicate that some data was transferred, but not all
            err = 0;
        if (err < 0)
            throw libUSBError(err);
        if(argPtr->buffer->currIndex >= (argPtr->buffer->getNumPoints()))
        {
            argPtr->buffer->currIndex = 0;
        }
    }
}
