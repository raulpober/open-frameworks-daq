#include "ofxDaqDCDCUSB.h"

//------------------------------------------------------------
ofxDaqDCDCUSB::ofxDaqDCDCUSB(bool systemCall){
	
	if (!systemCall){
	
		deviceError = false;
		device = dcdc_connect();
		name = "DCDC-USB";
		id = 1;
		if (device == NULL) 
		{
			daqLog.logError(DAQERR_DEVICE_ERROR,id,name,"Cannot connect to " + name);
			deviceError = true;
		}
    
		if (dcdc_setup(device) < 0)
		{
		fprintf(stderr, "Cannot setup device\n");
			daqLog.logError(DAQERR_DEVICE_ERROR,id,name,"Cannot setup device");
			deviceError = true;
		}
	}
}

//------------------------------------------------------------
ofxDaqDCDCUSB::~ofxDaqDCDCUSB(){
}

//------------------------------------------------------------
float ofxDaqDCDCUSB::getBatteryVoltage(){

	float batteryVoltage = 0.0;
	if (systemCall){
		FILE *lsofFile_p = popen("dcdc-usb -a", "r");

		if (!lsofFile_p)
		{
			return -1.0;
		}

		char buffer[1024];
		char *line_p;
		float tmp;
		line_p = fgets(buffer, sizeof(buffer), lsofFile_p);
		while(line_p != NULL){
			
			string data(buffer);
			int found = data.find("input voltage:");
			if (found != string::npos){
				//cout << data << endl;
				int count = sscanf(buffer,"input voltage: %f",&batteryVoltage);
			}
			line_p = fgets(buffer, sizeof(buffer), lsofFile_p);
		}
		pclose(lsofFile_p);
	}
	else{
		int ret;
	
		float scale = 0.1558f; // This is taken from dcdc_parse_data.c

		// Check battery voltage
		if ((ret = dcdc_get_status(device, data, MAX_TRANSFER_SIZE)) <= 0)
		{
			daqLog.logError(DAQERR_DEVICE_ERROR,id,name,"Failed to get status from device");
			return -1.0;
		}
		batteryVoltage = ((float) data[3]) * scale;
	}
	// Return the battery voltage
	return batteryVoltage;
}
