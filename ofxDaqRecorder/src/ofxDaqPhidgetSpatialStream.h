#ifndef OFXDAQPHIDGETSPATIALSTREAM
#define OFXDAQPHIDGETSPATIALSTREAM

	#include <phidget21.h>
    #include "ofxDaqStream.h"
	
	#define FILEHEADERBYTES 256
	#define NUMCALVALS 13

    class ofxDaqPhidgetSpatialStream : public ofxDaqStream, public ofThread {

        public:

			ofxDaqPhidgetSpatialStream(ofxXmlSettings settings);
            ~ofxDaqPhidgetSpatialStream();
            bool start(int elapsedTime);
            bool stop();
            bool loadSettings(ofxXmlSettings settings);
            float getDataRate();
			bool defineHeader();
            //void threadedFunction();
			bool insertDeviceData(char * data, int bytes); 
			bool writeDeviceData(CPhidgetSpatial_SpatialEventDataHandle *data,int count);
			bool deviceDataFull(int nextWriteBytes);
			int getBytesPerEvent();
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
			unsigned int haveCalVals;
			double calVals[13];
			
			int bytesPerEvent;


    };

#endif
