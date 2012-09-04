#ifndef GPSTIMESYNC
#define GPSTIMESYNC

    #include "NMEAParser.h"
	#include "ofMain.h"

    class GPSTimeSync {

        public:

            GPSTimeSync();
            GPSTimeSync(int baudRate, string commPort);
			~GPSTimeSync();
            bool syncSystemTime(int timeout);

        protected:

            CNMEAParser GPS;
			// The serial comm
            ofSerial serialPort;
            char dateTime[24];
            string setTime;
            string timeString;
            string gpsData;
            int baudRate;
            string commPort;
    };

#endif

