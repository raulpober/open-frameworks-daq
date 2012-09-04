#ifndef OFXDAQMCCDEVICE
#define OFXDAQMCCDEVICE

    #include "DAQFlex_libusb/mccdevice.h"

    class ofxDaqMCCDevice  {

        public:

            ofxDaqMCCDevice();
            ~ofxDaqMCCDevice();
			bool flashDIO(int val);
            bool initialize();             

        protected:

            // Specific for DAQFlex
            int deviceType;
            MCCDevice* device;
            bool initialized;
			bool deviceError;

    };

#endif
