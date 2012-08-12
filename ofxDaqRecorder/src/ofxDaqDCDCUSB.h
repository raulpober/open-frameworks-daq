#pragma once

#include "ofxDaq.h"
#include "ofxDaqLog.h"

extern "C" {
#include "dcdc-usb/dcdc-usb.h"
}
class ofxDaqDCDCUSB {
		
	public:
		
		ofxDaqDCDCUSB(bool systemCall);
		~ofxDaqDCDCUSB();
		float getBatteryVoltage();
		
		
	protected:	
		
		struct usb_dev_handle *device;
		unsigned char data[MAX_TRANSFER_SIZE];
		int ret;
		bool deviceError;
		bool systemCall;
		ofxDaqLog daqLog;
		string name;
		int id;
	
};
