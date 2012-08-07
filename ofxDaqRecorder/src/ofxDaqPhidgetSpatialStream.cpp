#include "ofxDaqPhidgetSpatialStream.h"

//--------------------------------------------------------------
ofxDaqPhidgetSpatialStream::ofxDaqPhidgetSpatialStream(){
    fifo = new CircularFifo(256,512);
    blockSize = 256;
    N = 512;
    dataBlock = (char*)malloc(blockSize);
	deviceDataBlock = (char*)malloc(blockSize);
	deviceDataCount = 0;
    deviceError = false;
    spatialSampleRate = 10;
	initialized = false;
	running = false;
}

ofxDaqPhidgetSpatialStream::ofxDaqPhidgetSpatialStream(ofxXmlSettings settings){
    this->loadSettings(settings);    
    fifo = new CircularFifo(blockSize,N);
	dataBlock = (char*)malloc(blockSize);
	deviceDataBlock = (char*)malloc(blockSize);
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

    // Try to load settings from file and use
    // defaults if not found
    name = settings.getValue("name","PHIDGET SPATIAL");
	id = settings.getValue("id",1);
    spatialSampleRate = ("settings:spatialsamplerate",10);
	N = settings.getValue("settings:blocks",512);
	blockSize = settings.getValue("settings:blocksize",256);
	filePrefix = settings.getValue("settings:fileprefix","TEST");
	filePostfix = settings.getValue("settings:filepostfix","SERIAL");
	fileExt = settings.getValue("settings:fileext","txt");
	
	daqLog.logNotice(name,"Loaded settings from XML");

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

    return writer->start(elapsedTime);
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
	int i,j;
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
	
	// The bytes per even are
	int bytesPerEvent = 3*3*sizeof(double) + 4*sizeof(int);
	float timestamp;
	int time;
	
	if (ptr->deviceDataFull(bytesPerEvent)){
		ptr->flushDeviceData();
	}
	
	// Stuff the data and timestamp into the deviceDataBlock
	for(i=0;i < 1;i++){
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
		
		// Timestamp from the device itself
		ptr->insertDeviceData((char*)&(data[i]->timestamp.seconds),sizeof(int));
		ptr->insertDeviceData((char*)&(data[i]->timestamp.microseconds),sizeof(int));
		
		// Timestamp from global framework first is the milisecond timer then hours, min, seconds
		timestamp = ofGetElapsedTimef();
		ptr->insertDeviceData((char*)&(timestamp),sizeof(int));
		time = ofGetHours();
		ptr->insertDeviceData((char*)&(time),sizeof(int));
		time = ofGetMinutes();
		ptr->insertDeviceData((char*)&(time),sizeof(int));
		time = ofGetSeconds();
		ptr->insertDeviceData((char*)&(time),sizeof(int));
		
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

bool ofxDaqPhidgetSpatialStream::flushDeviceData(){
	// write the buffer
	fifo->writeBlock(deviceDataBlock,blockSize);
	deviceDataCount = 0;
	return true;
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

	//Display the properties of the attached spatial device
	display_properties((CPhidgetHandle)spatial);

	//read spatial event data
	daqLog.logNotice(name,"Reading Spatial Data....");
	
	//Set the data rate for the spatial events
	CPhidgetSpatial_setDataRate(spatial, spatialSampleRate);
	
	initialized = true;

	return 0;
}

