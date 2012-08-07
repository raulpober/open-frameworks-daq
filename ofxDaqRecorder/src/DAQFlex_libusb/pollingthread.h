#ifndef POLLINGTHREAD_H
#define POLLINGTHREAD_H

#include <libusb.h>
#include <pthread.h>
#include "datatypesandstatics.h"
#include "databuffer.h"

typedef struct _threadArg
{
    libusb_device_handle* dev_handle;
    unsigned char endpoint_in;
    unsigned int rate;
    dataBuffer* buffer;
} threadArg;

class pollingThread
{
   public:
        pollingThread(threadArg * arg);
        int start();
        int stop();
   private:
        void run();
        static void * entryPoint(void*);
        void execute();

        threadArg * argPtr;

        pthread_t thread_;

        static void mSleep(unsigned int msec);
};
#endif
