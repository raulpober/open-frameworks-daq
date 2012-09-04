#include "ofxDaqMCCDevice.h"

ofxDaqMCCDevice::ofxDaqMCCDevice(){
    //this->loadSettings(settings);  
	deviceType = USB_1608_GX_2AO;  
    deviceError = false;
    initialized = false;
	this->initialize();

}


ofxDaqMCCDevice::~ofxDaqMCCDevice() {
	delete device;

}

bool ofxDaqMCCDevice::flashDIO(int val){

	if (!initialized){
		return false;
	}

	device->sendMessage("DIO{0}:VALUE="+ofToString(val));
	ofSleepMillis(10);
	device->sendMessage("DIO{0}:VALUE=0");
	return true;
}

// MCCDevice helper functions

//-------------------------------------------------------------
bool ofxDaqMCCDevice::initialize(){

    // Setup default values
    try {    

        device = new MCCDevice(deviceType);

        // Sets up the channel and AISCAN parameters

        //Flush out any old data from the buffer
        device->flushInputData();
		
		// Set DIO port to output
		device->sendMessage("DIO{0}:DIR=OUT");

    }

    catch (mcc_err err){
    
        delete device;
        deviceError = true;
		cout << "error initializing MCC Device" << endl;
        return false;
    }    

    initialized = true;
    return true;

}
