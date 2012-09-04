#include "ofxDaqMCCDeviceStream.h"

ofxDaqMCCDeviceStream::ofxDaqMCCDeviceStream(ofxXmlSettings settings){
    this->loadSettings(settings);    
    fifo = new CircularFifo(blockSize,N);
    dataRate = 0;
    dataBlock = (char*)malloc(blockSize);
	header = (char*)malloc(FILEHEADERBYTES);
	headerSize = FILEHEADERBYTES;
    deviceError = false;
    initialized = false;
	running = false;
	
	type = 3; // Define the type of data blocks
}


ofxDaqMCCDeviceStream::~ofxDaqMCCDeviceStream() {
	daqLog.logNotice(name,"Deleting data stream...");
	if (running) {
		this->stop();
    }
	delete device;
    delete buffer;    
    delete fifo;
    delete writer;
    free(dataBlock);
	free(header);
}

//-------------------------------------------------------------
void ofxDaqMCCDeviceStream::threadedFunction() {

    bool lastHalfRead = SECONDHALF;

    while (isThreadRunning()){
        if((buffer->currIndex > buffer->getNumPoints()/2) && (lastHalfRead == SECONDHALF))
        {
            //cout << "First Half Ready: " << buffer->getNumPoints()/2 * buffer->sizeOf() << "\n";
            fifo->writeBlock((char*)buffer->data, buffer->getNumPoints()/2 * buffer->sizeOf()); 
            lastHalfRead = FIRSTHALF;
        }
        else if ((buffer->currIndex < buffer->getNumPoints()/2) && (lastHalfRead == FIRSTHALF))
        {
            //cout << "Second Half Ready: " << buffer->getNumPoints()/2 * buffer->sizeOf() << "\n";
            fifo->writeBlock((char*)&buffer->data[buffer->getNumPoints()/2], buffer->getNumPoints()/2 * buffer->sizeOf());
            lastHalfRead = SECONDHALF;
        }
    }
	
}

bool ofxDaqMCCDeviceStream::checkStatus(){

	bool okay = true;
	
	string scanRunning = device->sendMessage("?AISCAN:STATUS");
	daqLog.logNotice(name,scanRunning);
	//string aiStatus = device->sendMessage("?AI:STATUS");
	//daqLog.logNotice(name,aiStatus);
	string tempString = device->sendMessage("?DEV:TEMP");
	daqLog.logNotice(name,tempString);
	// if scan is still running, then flash  happy strobes
	if ((scanRunning.find("RUNNING") != string::npos)){
		device->sendMessage("DIO{0}:VALUE=3");
		ofSleepMillis(10);
		device->sendMessage("DIO{0}:VALUE=0");
		okay = true;
		
	}
	// else flash all (including error) strobe
	else{
		
		device->sendMessage("DIO{0}:VALUE=7");
		ofSleepMillis(10);
		device->sendMessage("DIO{0}:VALUE=0");
		
		// Possibly do a restart here
		okay = false;
	}
	return okay;
}

bool ofxDaqMCCDeviceStream::flashDIO(int val){

	device->sendMessage("DIO{0}:VALUE="+ofToString(val));
	ofSleepMillis(10);
	device->sendMessage("DIO{0}:VALUE=0");
	return true;
}

bool ofxDaqMCCDeviceStream::loadSettings(ofxXmlSettings settings){

    // Try to load settings from file and use
    // defaults if not found
    name = settings.getValue("name","USB-1608-GX-2AO");
    // Only USB-1608-GZ-2AO is supported to just check it
    if (name != "USB-1608-GX-2AO"){
        cout << "error, device type not supported yet." << endl;
    } else {
        deviceType = USB_1608_GX_2AO;
		maxCounts = 0xFFFF;
    }
    id = settings.getValue("id",1);
    sampleRate = settings.getValue("settings:samplerate",100000);
    minVoltage = settings.getValue("settings:minvoltage",-1);
    maxVoltage = settings.getValue("settings:maxvoltage",1);
    lowChan = settings.getValue("settings:lowchan",0);
    highChan = settings.getValue("settings:highchan",3);
	filePrefix = settings.getValue("settings:fileprefix","DAQDEV");
	filePostfix = settings.getValue("settings:filepostfix","MCCDAQ");
	fileExt = settings.getValue("settings:fileext","bin");

    // These values should generally not be changed
    timeout = settings.getValue("settings:timeout",4);
    bulkTxLength = settings.getValue("settings:bulktxlength",2048);

	// Ignore blocksize setting and just match A/D setup
	//blockSize = settings.getValue("settings:blockSize",(int)bulkTxLength);

    N = settings.getValue("settings:blocks",512);

    // Derived settings
	
	// Hard Code these for now since they seem like good values
	numSamples = bulkTxLength*24;
	numChans = highChan-lowChan+1; 
	blockSize = numSamples*numChans; // *2 for 16-bit, but write 1/2 buffer chunks
}

//-------------------------------------------------------------
bool ofxDaqMCCDeviceStream::start(int elapsedTime){

    // If not initialized we need to do that now
    if (!initialized){
        this->initialize();
    }    

    // If there was a device error, then just error out
    if (deviceError) {
        return false;
    }

    // Start the AISCAN
    device->sendMessage("AISCAN:START");

    // Start the USB transfer polling thread
    this->device->startBackgroundTransfer(sampleRate,
        this->timeout,
        this->bulkTxLength,
        this->buffer);

	// Setup the file I/O
	startTime = ofGetElapsedTimef();
	writer = new ofxDaqWriter(dataDirectoryPath,filePrefix,filePostfix,fileExt);
	
	// Define the file header
	this->defineHeader();

    // Start the background reader thread.
    this->startThread(true,false);
	
	running = true;

    bool success = writer->start(elapsedTime);
	
	// Since this is the first file, write the header
	// The rest of the time is is handled by the update method of parent class.
	writer->writeData(header,headerSize);
	
	return success;
}

//-------------------------------------------------------------
bool ofxDaqMCCDeviceStream::restart(){
    
    if (deviceError) {
        return false;
    }

    device->sendMessage("AISCAN:STOP");
	//ofSleepMillis(5);
	daqLog.logNotice(name,"RESTARTED AISCAN");
	device->sendMessage("AISCAN:START");
}


//-------------------------------------------------------------
bool ofxDaqMCCDeviceStream::stop(){
    
    if (deviceError) {
        return false;
    }

    device->sendMessage("AISCAN:STOP");
    this->stopThread(true);
    this->device->stopBackgroundTransfer();
	running = false;
    return writer->stop();
}

//-------------------------------------------------------------
bool ofxDaqMCCDeviceStream::dataValid(char  * dataIn,int bufferSize) {
    for(int i = 0;i < bufferSize; i++) {
        if (*(dataIn + i) != (char)i) { 
            return false;
        }    
    }
    return true;
}    

//-------------------------------------------------------------
bool ofxDaqMCCDeviceStream::defineHeader(){

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
	memcpy(header + index,(char*)&sampleRate,sizeof(sampleRate));
	index += sizeof(sampleRate);
	memcpy(header + index,(char*)&maxCounts,sizeof(maxCounts));
	index += sizeof(maxCounts);
	memcpy(header + index,(char*)&minVoltage,sizeof(minVoltage));
	index += sizeof(minVoltage);
	memcpy(header + index,(char*)&maxVoltage,sizeof(maxVoltage));
	index += sizeof(maxVoltage);
	memcpy(header + index,(char*)&numChans,sizeof(numChans));
	index += sizeof(numChans);
	for (int i=0;i<numChans;i++){
		memcpy(header + index, (char*)&calSlope[i],sizeof(calSlope[i]));
		index += sizeof(calSlope[i]);
		memcpy(header + index, (char*)&calOffset[i],sizeof(calOffset[i]));
		index += sizeof(calOffset[i]);
	}

	return true;
}

//-------------------------------------------------------------
bool ofxDaqMCCDeviceStream::sendDataBlock(ofxUDPManager * udpConnection){

	
	// Prepare the data to send:
	//
	// One byte: TypeID
	// One byte: multiMessage? 0-> all in one message, 1 -> multiple meesages
	// Total Bytes (includes 2 above)
	// Header Bytes
	// Data Bytes
	//
	// Total length (bytes) = 2 + headerLength + dataLength
	
	if (!dataBlockFresh){
		return true;
	}
	
	int sent = 0;
	int index = 0;
	
	
	// Write out packets
	int bytesLeft = blockSize;
	int dataPointer = 0;
	int txSize = 512;
	char * msg = (char*)malloc(txSize);
	
	if (msg == NULL){
		return false;
	}
	
	// Want to send a multiple of number of channels in bytes up to 512 limit
	int cpySize = txSize - 2*numChans - 2;	
	
	while(bytesLeft){
		msg[0] = type;
		index += 1;
		msg[1] = 2*numChans;
		index += 1;
		if (dataPointer + cpySize >= blockSize){
			break;
		} 
		memcpy(msg+2 + 2*numChans,dataBlock+dataPointer,cpySize); 
		udpConnection->Send(msg,txSize);
		dataPointer += cpySize;
		bytesLeft -= cpySize;
	}
	
	// Mark the data block as being stale
	dataBlockFresh = false;
	
	free(msg);
	
	return true;
}
	

//-------------------------------------------------------------
float ofxDaqMCCDeviceStream::getDataRate() {
    return dataRate;
}

// MCCDevice helper functions

//-------------------------------------------------------------
bool ofxDaqMCCDeviceStream::initialize(){

    // Setup default values
    string response;
    stringstream strLowChan, strHighChan, strRate, strNumSamples,
                        strCalSlope, strCalOffset, strRange, strMode;

    calSlope = new float[numChans];
    calOffset = new float[numChans];

    try {    

        device = new MCCDevice(deviceType);

        //create a buffer for the scan data
        buffer = new dataBuffer(blockSize);

        // Sets up the channel and AISCAN parameters

        //Flush out any old data from the buffer
        device->flushInputData();

        //Configure an input scan
		device->sendMessage("AI:CHMODE=DIFF"); //Good for fast acquisitions
		
        device->sendMessage("AISCAN:XFRMODE=BLOCKIO"); //Good for fast acquisitions

        strRange << "AISCAN:RANGE=BIP" << (maxVoltage) << "V";
		device->sendMessage(strRange.str());//Set the voltage range on the device

        strLowChan << "AISCAN:LOWCHAN=" << lowChan;//Form the message
        device->sendMessage(strLowChan.str());//Send the message

    
        strHighChan << "AISCAN:HIGHCHAN=" << highChan;
        device->sendMessage(strHighChan.str());

        strRate << "AISCAN:RATE=" << sampleRate;
        device->sendMessage(strRate.str());

        device->sendMessage("AISCAN:SAMPLES=0");//Continuous scan, set samples to 0

        //Fill cal constants for later use
        fillCalConstants(lowChan, highChan);
		
		// Set DIO port to output
		device->sendMessage("DIO{0}:DIR=OUT");

    }

    catch (mcc_err err){
    
        delete device;
        deviceError = true;
		daqLog.logError(DAQERR_DEVICE_ERROR,id,name,errorString(err));
        return false;
    }    

    initialized = true;
    return true;

}

//-------------------------------------------------------------
void ofxDaqMCCDeviceStream::fillCalConstants(unsigned int lowChan, unsigned int highChan){

    unsigned int currentChan;
    stringstream strCalSlope, strCalOffset;
    string response;

    for(currentChan=lowChan; currentChan <= highChan; currentChan++){
        //Clear out last channel message
        strCalSlope.str("");
        strCalOffset.str("");

        //Set up the string message
        strCalSlope<< "?AI{" << currentChan << "}:SLOPE";
        strCalOffset<< "?AI{" << currentChan << "}:OFFSET";

        //Send the message and put the values into an array containing all channel cal data
        response = device->sendMessage(strCalSlope.str());
        calSlope[currentChan] = ofToFloat(response.erase(0,12)); // Removes the AI{x}:SLOPE= part of the reply
        response = device->sendMessage(strCalOffset.str());
        calOffset[currentChan] = ofToFloat(response.erase(0,13)); // Removes the AI{x}:OFFSET= part of the reply

        cout << "Channel " << currentChan << " Calibration Slope: " << calSlope[currentChan] << " Offset: " << calOffset[currentChan] << "\n";
    }
}

