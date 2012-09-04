#include "ofxDaqRecorder.h"

ofxDaqRecorder::ofxDaqRecorder(){
    gpsSync = false;
}

ofxDaqRecorder::~ofxDaqRecorder(){
    daqMan->stop();
    delete daqMan;
	delete dcdcUSB;
	ofLogNotice() << ofGetTimestampString() << " : DELETED MANAGER, DCDCUSB, and GPSSYNC" << endl; 
}

//--------------------------------------------------------------
void ofxDaqRecorder::setup(){

    // Set a low frame rate
    ofSetFrameRate(60);
	
    
	if(this->load()){
        daqMan->start(startTime);
    }

}

//--------------------------------------------------------------
void ofxDaqRecorder::update(){

	// Having some issues with this battery voltage call so skip for now
	/*if(ofGetElapsedTimeMillis() - startTime > 60000){

		cout << "Battery Voltage: " << dcdcUSB->getBatteryVoltage() << endl;
		startTime = ofGetElapsedTimeMillis();

	}*/

}

//--------------------------------------------------------------
void ofxDaqRecorder::draw(){
    
    /*string  info  = "FPS:        "+ofToString(ofGetFrameRate(),0)+"\n";
            info += "Percent of free buffers: "+ofToString(floor(daqMan->percentBuffersFree()))+"\n";
            info += "Data Rate: "+ofToString(daqMan->getTotalDataRate())+"\n";
            //info += "Elapsed Time: "+ofToString(elapsedTime)+"\n";
            info += "TimeStamp: "+ofGetTimestampString()+"\n";
    ofSetColor(0);
    ofDrawBitmapString(info, 20, 20);
	*/
}

//--------------------------------------------------------------
void ofxDaqRecorder::keyPressed(int key){

}

//--------------------------------------------------------------
void ofxDaqRecorder::keyReleased(int key){

}

//--------------------------------------------------------------
void ofxDaqRecorder::mouseMoved(int x, int y){

}

//--------------------------------------------------------------
void ofxDaqRecorder::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofxDaqRecorder::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofxDaqRecorder::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofxDaqRecorder::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofxDaqRecorder::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofxDaqRecorder::dragEvent(ofDragInfo dragInfo){

}

//--------------------------------------------------------------
bool ofxDaqRecorder::load(){

    // Read the settings file and create streams
    if (!allSettings.loadFile("settings.xml")){
        ofLogError() << ofGetTimestampString() << " : ERROR loading settings file" <<endl;
    }
	
	// Try to sync the system time with GPS
	//Grab the serial settings for the GPS device
	int gpsBaud = allSettings.getValue("gpsbaud",4800);
	string gpsPort = allSettings.getValue("gpsport","/dev/ttyS0");
	int doGPSSync = allSettings.getValue("gpssync",0);
	
	if (doGPSSync == 1){
	
		// Use the MCCDAQ stream to flash white strobe while waitng for GPS
		ofxDaqMCCDevice * strobeOnly = new ofxDaqMCCDevice();
		// Sync the time
		gpsTimeSync = new GPSTimeSync(gpsBaud,gpsPort);
		ofLogNotice() << ofGetTimestampString() << " : Syncing system time with GPS..." <<endl;
	
		gpsSync = false;
		float timer = ofGetElapsedTimef();
		while (!gpsSync){
			gpsSync = gpsTimeSync->syncSystemTime(1000);
			strobeOnly->flashDIO(1);
			if (ofGetElapsedTimef() - timer > 120){
				break;
			}
		}
		// NB: must delete the MCCDevice so we can use it later in the stream class
		delete strobeOnly;
		delete gpsTimeSync;
	}
    
	// this loads the global settings and turns on
	// logging to file.
	daqMan = new ofxDaqManager(allSettings);
	dcdcUSB = new ofxDaqDCDCUSB(true);
	
	startTime = ofGetElapsedTimeMillis();
	
    int numDevices = allSettings.getNumTags("device");
    if (numDevices == 0){
        ofLogError() << ofGetTimestampString() << ": found no devices" <<endl;
    }

    // Load and allocate the devices
    for(int i=0;i<numDevices;i++){
        allSettings.pushTag("device",i);
        string deviceType = allSettings.getValue("type","");
        
        // Check the different possible types
        if (deviceType == "MCCDEVICE"){
            daqMan->addStream(new ofxDaqMCCDeviceStream(allSettings));
        }
        if (deviceType == "SERIALDEVICE"){
            daqMan->addStream(new ofxDaqSerialStream(allSettings));
        }
		if (deviceType == "PHIDGETSPATIAL"){
			daqMan->addStream(new ofxDaqPhidgetSpatialStream(allSettings));
		}
        allSettings.popTag();
    }
    return true;
}
