#include "GPSTimeSync.h"

//---------------------------------------------
GPSTimeSync::GPSTimeSync(){
    baudRate = 4800;
    commPort = "/dev/ttyS0";
	// Open the port
	// Setup the serial port
    serialPort.setup(commPort,baudRate);
	serialPort.setVerbose(true);
}

//---------------------------------------------
GPSTimeSync::GPSTimeSync(int baudRate, string commPort){
    this->baudRate = baudRate;
    this->commPort = commPort;
	// Open the port
	// Setup the serial port
    serialPort.setup(commPort,baudRate);
	serialPort.setVerbose(true);
}

GPSTimeSync::~GPSTimeSync(){
	// Close the port
	serialPort.close();
}

//---------------------------------------------
bool GPSTimeSync::syncSystemTime(int timeout){   

    ofLogNotice() << "Waiting for GPS data..." << endl;
    bool validGPS = false;
    int startTime = ofGetElapsedTimeMillis(); 
	unsigned char serialData[1024];  

    while (1){    


        // This is where we'd read from the port to get GPS NMEA strings        
        //gpsData.assign("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A");
        int bytesAvailable = serialPort.available();
		int bytesToRead = bytesAvailable;
		if (bytesAvailable > 1024) {
			bytesToRead = 1024;
		}
		int bytesRead = serialPort.readBytes(serialData,bytesToRead);
		
		
        // Process the data
        for (unsigned int i = 0;i<bytesRead;i++){
            GPS.ProcessNMEA(serialData[i]);
        }

        // Check that fix is valid
        if (GPS.m_btRMCDataValid == 'A'){
            validGPS = true;
            break;
        }

        // Check for timeout
        if (ofGetElapsedTimeMillis() - startTime > timeout){
            validGPS = false;
            break;
        }
		
		// A short delay
		ofSleepMillis(20);

    }

    if (!validGPS){
        ofLogNotice() << "GPS time sync timed out." << endl;
        return false;
    }

    // Got valid GPS so set clock:

    // Set the system date and time
    // This is the format string for the linux system command "date"
    sprintf(dateTime,"%02d/%02d/%02d %02d:%02d:%02d",
    (int)GPS.m_btRMCMonth,
    (int)GPS.m_btRMCDay,
    (int)GPS.m_wRMCYear,
    (int)GPS.m_btRMCHour, 
    (int)GPS.m_btRMCMinute,
    (int)GPS.m_btRMCSecond);

    ofLogNotice() << "Setting system time..." << endl;

    timeString.assign(dateTime);
    setTime.assign("date -s \"" + timeString + "\"");
    cout << setTime << endl;

    // Set the system time
    bool result = system(setTime.c_str()); 

    ofLogNotice() << "System time set command: " << setTime.c_str() << endl;
	ofLogNotice() << "System time: " << ofGetTimestampString() << endl;
	
	

    return validGPS;

}
