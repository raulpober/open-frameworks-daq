#ifndef OFXDAQPHIDGETSPATIALSTREAM
#define OFXDAQPHIDGETSPATIALSTREAM

	#include <phidget21.h>
    #include "ofxDaqStream.h"

    class ofxDaqPhidgetSpatialStream : public ofxDaqStream, public ofThread {

        public:

            ofxDaqPhidgetSpatialStream();
			ofxDaqPhidgetSpatialStream(ofxXmlSettings settings);
            ~ofxDaqPhidgetSpatialStream();
            bool start(int elapsedTime);
            bool stop();
            bool loadSettings(ofxXmlSettings settings);
            float getDataRate();
            //void threadedFunction();
			bool insertDeviceData(char * data, int bytes); 
			bool deviceDataFull(int nextWriteBytes);
			bool flushDeviceData();
						
			 

        protected:

            // Specific for Phidgets Spatial Sensors
            int initialize();
            int display_properties(CPhidgetHandle phid);
            //Declare a spatial handle
	        CPhidgetSpatialHandle spatial;

            // Parameters for the Spatial 3/3/3 sensor
            unsigned int spatialSampleRate;
			char * deviceDataBlock;
			unsigned int deviceDataCount;
			
			bool initialized;


    };

#endif
