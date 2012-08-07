#ifndef GPSTIMESYNC
#define GPSTIMESYNC

    #include "NMEAParser.h"

    class GPSTimeSync {

        public:

            GPSTimeSync();
            GPSTimeSync(int baudRate, string commPort);
            bool syncSystemTime(int timeout);

        protected:

            CNMEAParser GPS;
            char dateTime[24];
            string setTime;
            string timeString;
            string gpsData;
            int baudRate;
            string commPort;
    };

#endif

