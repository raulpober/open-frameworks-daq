#include "ofxDaqPhidgetSpatialStream.h"

ofxDaqPhidgetSpatialStream::ofxDaqPhidgetSpatialStream(ofxXmlSettings settings){
	haveCalVals = 0;
	this->loadSettings(settings);    
    fifo = new CircularFifo(blockSize,N);
	dataBlock = (char*)malloc(blockSize);
	deviceDataBlock = (char*)malloc(blockSize);
	header = (char*)malloc(FILEHEADERBYTES);
	headerSize = FILEHEADERBYTES;
	deviceDataCount = 0;
	deviceError = false;
	initialized = false;
	running = false;

}


ofxDaqPhidgetSpatialStream::~ofxDaqPhidgetSpatialStream() {
    
	daqLog.logNotice(name,"Deleting data stream...");
	if (running) {
		this->stop();
	}
    delete fifo;
    delete writer;
    free(dataBlock);
}
	
//-------------------------------------------------------------
bool ofxDaqPhidgetSpatialStream::loadSettings(ofxXmlSettings settings){

    
	// Hard code the bytesPerEvent
	// Define the bytes per spatial event
	bytesPerEvent = (3*3*sizeof(double) + 2*sizeof(int) + 2*sizeof(unsigned long) + 2*sizeof(float));
	
	// Try to load settings from file and use
    // defaults if not found
    name = settings.getValue("name","PHIDGET SPATIAL");
	id = settings.getValue("id",1);
    spatialSampleRate = ("settings:spatialsamplerate",8);
	N = settings.getValue("settings:blocks",512);
	int tmp = settings.getValue("settings:blocksize",256);
	blockSize = tmp*bytesPerEvent;
	filePrefix = settings.getValue("settings:fileprefix","TEST");
	filePostfix = settings.getValue("settings:filepostfix","SERIAL");
	fileExt = settings.getValue("settings:fileext","txt");
	
	// Load calibration data (Default values are from 3D calibration)
	settings.pushTag("cal");
	int numVals = settings.getNumTags("val");
    if (numVals != 13){
        ofLogError() << ofGetTimestampString() << "Wrong number of calibration vals skipping." <<endl;
	}
	else {
		for (int i=0;i<numVals;i++){
			calVals[i] = settings.getValue("val",0.0,i);
			daqLog.logNotice(name,"Loaded cal value (" + ofToString(i) + ") = " +ofToString(calVals[i]));
		}
		haveCalVals = 1;
	}
	settings.popTag();
	
	daqLog.logNotice(name,"Loaded settings from XML");
	

}

//-------------------------------------------------------------
bool ofxDaqPhidgetSpatialStream::defineHeader(){

	// Set all the bytes to zeros
	memset(this->header,0,headerSize);
	
	// Repeatedly call write data to fill in the 
	// header
	int index = 0;
	unsigned long systemMicros = ofGetSystemTimeMicros();
	unsigned long unixTime = ofGetUnixTime();
	float timestamp = ofGetElapsedTimef();
	memcpy(header + index,(char*)&unixTime,sizeof(unixTime));
	index += sizeof(unixTime);
	memcpy(header + index,(char*)&systemMicros,sizeof(systemMicros));
	index += sizeof(systemMicros);
	memcpy(header + index,(char*)&timestamp,sizeof(timestamp));
	index += sizeof(timestamp);
	memcpy(header + index,(char*)&blockSize,sizeof(blockSize));
	index += sizeof(blockSize);
	memcpy(header + index,(char*)&spatialSampleRate,sizeof(spatialSampleRate));
	index += sizeof(spatialSampleRate);
	
	
	memcpy(header + index,(char*)&haveCalVals,sizeof(haveCalVals));
	index += sizeof(haveCalVals);
	for (int i=0;i<NUMCALVALS;i++){
		memcpy(header + index,(char*)&calVals[i],sizeof(calVals[i]));
		index += sizeof(calVals[i]);
	}


	return true;
}

//-------------------------------------------------------------
bool ofxDaqPhidgetSpatialStream::start(int elapsedTime){
    //this->startThread(true,false);
    
    // Phidgets Spatial uses and defines its own threads so
    // We don't need to start a thread.

    // Initialize which reads the device info and then
    // opens
	
	// Initialize the writer
	startTime = ofGetElapsedTimef();
    dataRate = 0;
    writer = new ofxDaqWriter(dataDirectoryPath,filePrefix,filePostfix,fileExt);
	
    int result = this->initialize();
    
    if (result){
		daqLog.logError(DAQERR_DEVICE_NOT_INTIALIZED,id,name,1);
        deviceError = true;
        return false;
    }

	running = true;
	
	this->defineHeader();

    bool success = writer->start(elapsedTime);
	
	// Since this is the first file, write the header
	// The rest of the time is is handled by the update method of parent class.
	writer->writeData(header,headerSize);
	
	return success;
}

//-------------------------------------------------------------
bool ofxDaqPhidgetSpatialStream::stop(){
    //this->stopThread(true);
    
    // Close the device
	if (initialized) {
		daqLog.logNotice(name,"Closing Device");
		CPhidget_close((CPhidgetHandle)spatial);
		CPhidget_delete((CPhidgetHandle)spatial);
		initialized = false;
	}

	running = false;

    return writer->stop();
}

//-------------------------------------------------------------
float ofxDaqPhidgetSpatialStream::getDataRate() {
    return dataRate;
}

//callback that will run if the Spatial is attached to the computer
int AttachHandler(CPhidgetHandle spatial, void *userptr)
{
	int serialNo;
	CPhidget_getSerialNumber(spatial, &serialNo);
	ofLogNotice() << ofGetTimestampString() << ": Spatial " << ofToString(serialNo) << " attached!" << endl;

	return 0;
}

//callback that will run if the Spatial is detached from the computer
int DetachHandler(CPhidgetHandle spatial, void *userptr)
{
	int serialNo;
	CPhidget_getSerialNumber(spatial, &serialNo);
	ofLogNotice() << ofGetTimestampString() << ": Spatial " << ofToString(serialNo) << " detached!" << endl;

	return 0;
}

//callback that will run if the Spatial generates an error
int ErrorHandler(CPhidgetHandle spatial, void *userptr, int ErrorCode, const char *unknown)
{
	ofLogError() << ofGetTimestampString() << ": Phidget Spatial Error: " + ofToString(ErrorCode) + "," + ofToString(unknown) << endl;
	return 0;
}

//callback that will run at datarate
//data - array of spatial event data structures that holds the spatial data packets that were sent in this event
//count - the number of spatial data event packets included in this event
int SpatialDataHandler(CPhidgetSpatialHandle spatial, void *userptr, CPhidgetSpatial_SpatialEventDataHandle *data, int count)
{
	//printf("Number of Data Packets in this event: %d\n", count);
	ofxDaqPhidgetSpatialStream * ptr = (ofxDaqPhidgetSpatialStream *)userptr;
	/*for(i = 0; i < count; i++)
	{
		printf("=== Data Set: %d ===\n", i);
		printf("Acceleration> x: %6f  y: %6f  x: %6f\n", data[i]->acceleration[0], data[i]->acceleration[1], data[i]->acceleration[2]);
		printf("Angular Rate> x: %6f  y: %6f  x: %6f\n", data[i]->angularRate[0], data[i]->angularRate[1], data[i]->angularRate[2]);
		printf("Magnetic Field> x: %6f  y: %6f  x: %6f\n", data[i]->magneticField[0], data[i]->magneticField[1], data[i]->magneticField[2]);
		printf("Timestamp> seconds: %d -- microseconds: %d\n", data[i]->timestamp.seconds, data[i]->timestamp.microseconds);
	}

	printf("---------------------------------------------\n");
	*/
	
	// Check that we can fit the data in the buffer
	int i,j;

	for(i=0;i < count;i++){
	
		if (ptr->deviceDataFull(ptr->getBytesPerEvent())){
			ptr->flushDeviceData();
		}

		//Acceleration
		for(j=0;j<3;j++){
			ptr->insertDeviceData((char*)&(data[i]->acceleration[j]),sizeof(double));
		}
		//angularRate
		for(j=0;j<3;j++){
			ptr->insertDeviceData((char*)&(data[i]->angularRate[j]),sizeof(double));
		}
		//Magnetometer
		for(j=0;j<3;j++){
			ptr->insertDeviceData((char*)&(data[i]->magneticField[j]),sizeof(double));
		}
	
		// Timestamps from HW and then system 
		unsigned long systemMicros = ofGetSystemTimeMicros();
		unsigned long unixTime = ofGetUnixTime();
		float timestamp = ofGetElapsedTimef();
	
		// Timestamp from the device itself and other timing variables
		ptr->insertDeviceData((char*)&(data[i]->timestamp.seconds),sizeof(int));
		ptr->insertDeviceData((char*)&(data[i]->timestamp.microseconds),sizeof(int));
		ptr->insertDeviceData((char*)&(unixTime),sizeof(unsigned long));
		ptr->insertDeviceData((char*)&(systemMicros),sizeof(unsigned long));
		ptr->insertDeviceData((char*)&(timestamp),sizeof(float));
		ptr->insertDeviceData((char*)&(timestamp),sizeof(float));
	}	

	return 0;
}

bool ofxDaqPhidgetSpatialStream::insertDeviceData(char * data, int bytes){
	
	memcpy(deviceDataBlock+deviceDataCount,data,bytes);
	deviceDataCount += bytes;
	return true;
}

bool ofxDaqPhidgetSpatialStream::deviceDataFull(int nextWriteBytes){
	return deviceDataCount > blockSize - nextWriteBytes;
}

bool ofxDaqPhidgetSpatialStream::writeDeviceData(CPhidgetSpatial_SpatialEventDataHandle *data, int count){


}

bool ofxDaqPhidgetSpatialStream::flushDeviceData(){
	// write the buffer
	fifo->writeBlock(deviceDataBlock,blockSize);
	deviceDataCount = 0;
	return true;
}	

int ofxDaqPhidgetSpatialStream::getBytesPerEvent(){
	return bytesPerEvent;
}


	

//Display the properties of the attached phidget to the screen.  
//We will be displaying the name, serial number, version of the attached device, the number of accelerometer, gyro, and compass Axes, and the current data rate
// of the attached Spatial.
int ofxDaqPhidgetSpatialStream::display_properties(CPhidgetHandle phid)
{
	int serialNo, version;
	const char* ptr;
	int numAccelAxes, numGyroAxes, numCompassAxes, dataRateMax, dataRateMin;

	CPhidget_getDeviceType(phid, &ptr);
	CPhidget_getSerialNumber(phid, &serialNo);
	CPhidget_getDeviceVersion(phid, &version);
	CPhidgetSpatial_getAccelerationAxisCount((CPhidgetSpatialHandle)phid, &numAccelAxes);
	CPhidgetSpatial_getGyroAxisCount((CPhidgetSpatialHandle)phid, &numGyroAxes);
	CPhidgetSpatial_getCompassAxisCount((CPhidgetSpatialHandle)phid, &numCompassAxes);
	CPhidgetSpatial_getDataRateMax((CPhidgetSpatialHandle)phid, &dataRateMax);
	CPhidgetSpatial_getDataRateMin((CPhidgetSpatialHandle)phid, &dataRateMin);

	
	/*ofLog() << ptr << endl;
	ofLog() << "Serial Number: " << serialNo << endl << "Version: " << version << endl;
	ofLog() << "Number of Accel Axes: " << numAccelAxes << endl;
	ofLog() << "Number of Accel Axes: " << numAccelAxes << endl;
	ofLog() << "Number of Gyro Axes: " << numGyroAxes << endl;
	ofLog() << "Number of Compass Axes: " << numCompassAxes << endl;
	ofLog() << "Datarate> Max: " << dataRateMax << endl << "Min: " << dataRateMin << endl;
	*/

	return 0;
}

int ofxDaqPhidgetSpatialStream::initialize()
{
	int result;
	const char *err;

    spatial = 0;
	
	if (initialized) {
		return 0;
	}

	//create the spatial object
	CPhidgetSpatial_create(&spatial);

	//Set the handlers to be run when the device is plugged in or opened from software, unplugged or closed from software, or generates an error.
	CPhidget_set_OnAttach_Handler((CPhidgetHandle)spatial, AttachHandler, NULL);
	CPhidget_set_OnDetach_Handler((CPhidgetHandle)spatial, DetachHandler, NULL);
	CPhidget_set_OnError_Handler((CPhidgetHandle)spatial, ErrorHandler, NULL);

	//Registers a callback that will run according to the set data rate that will return the spatial data changes
	//Requires the handle for the Spatial, the callback handler function that will be called, 
	//and an arbitrary pointer that will be supplied to the callback function (may be NULL)
	CPhidgetSpatial_set_OnSpatialData_Handler(spatial, SpatialDataHandler, this);

	//open the spatial object for device connections
	CPhidget_open((CPhidgetHandle)spatial, -1);

	//get the program to wait for a spatial device to be attached
	daqLog.logNotice(name,"Waiting for spatial to be attached....");
	if((result = CPhidget_waitForAttachment((CPhidgetHandle)spatial, 10000)))
	{
		CPhidget_getErrorDescription(result, &err);
		daqLog.logError(DAQERR_DEVICE_ERROR,id,name,"Error Waiting for attachment: " + ofToString(err));
		return 1;
	}

	// If we have cal data, insert it now
	if (haveCalVals){
		daqLog.logNotice(name,"Setting Calibration Values...");
		CPhidgetSpatial_setCompassCorrectionParameters(spatial,calVals[0],
			calVals[1],
			calVals[2],
			calVals[3],
			calVals[4],
			calVals[5],
			calVals[6],
			calVals[7],
			calVals[8],
			calVals[9],
			calVals[10],
			calVals[11],
			calVals[12]
		);
	}

	//Display the properties of the attached spatial device
	display_properties((CPhidgetHandle)spatial);

	//read spatial event data
	daqLog.logNotice(name,"Reading Spatial Data....");
	
	//Set the data rate for the spatial events
	CPhidgetSpatial_setDataRate(spatial, spatialSampleRate);
	
	initialized = true;

	return 0;
}

