#ifndef OFXDAQMCCDEVICESTREAM
#define OFXDAQMCCDEVICESTREAM

    #include "ofxDaqStream.h"
    #include "DAQFlex_libusb/mccdevice.h"

    #define FIRSTHALF true
    #define SECONDHALF false
	#define FILEHEADERBYTES 256

    class ofxDaqMCCDeviceStream : public ofxDaqStream, public ofThread {

        public:

            ofxDaqMCCDeviceStream(ofxXmlSettings settings);
            ~ofxDaqMCCDeviceStream();
            bool start(int elapsedTime);
            bool stop();
            bool loadSettings(ofxXmlSettings settings);
            bool dataValid(char  * dataIn,int bufferSize);
            float getDataRate();
            void threadedFunction();
			bool defineHeader();

            // MCC Device specific functions
            void fillCalConstants(unsigned int lowChan, unsigned int highChan);
            bool initialize();
            
                         

        protected:

            // Specific for DAQFlex
            int deviceType;
            MCCDevice* device;
            dataBuffer* buffer;
            int numChans;
            int numSamples;
            int lowChan;
            int highChan;
            unsigned int sampleRate;
            unsigned int timeout;
            unsigned int bulkTxLength;
			unsigned int maxCounts;
            float *calSlope;
            float *calOffset;
            int minVoltage;
            int maxVoltage;
			
            bool initialized;

    };

#endif
