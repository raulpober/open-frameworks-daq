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
	int timeCounter = ofGetElapsedTimeMillis();

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
		if ((ofGetElapsedTimeMillis() - timeCounter) > 10000){
			daqLog.logNotice(name,device->sendMessage("?AISCAN:STATUS"));
			timeCounter = ofGetElapsedTimeMillis();
		}
    }
	
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
	
	// copy the file header for the writer
	writer->createHeader(this->header,headerSize);

    // Start the background reader thread.
    this->startThread(true,false);
	
	running = true;

    bool success = writer->start(elapsedTime);
	return success;
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
	float timestamp = ofGetElapsedTimef();
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
float ofxDaqMCCDeviceStream::getDataRate() {
    return dataRate;
}

// MCCDevice helper functions

//-------------------------------------------------------------
bool ofxDaqMCCDeviceStream::initialize(){

    // Setup default values
    string response;
    stringstream strLowChan, strHighChan, strRate, strNumSamples,
                        strCalSlope, strCalOffset, strRange;

    calSlope = new float[numChans];
    calOffset = new float[numChans];

    try {    

        device = new MCCDevice(deviceType);

        //create a buffer for the scan data
        buffer = new dataBuffer(numSamples*numChans);

        // Sets up the channel and AISCAN parameters

        //Flush out any old data from the buffer
        device->flushInputData();

        //Configure an input scan
        device->sendMessage("AISCAN:XFRMODE=BLOCKIO"); //Good for fast acquisitions

        device->sendMessage("AISCAN:RANGE=BIP1V");//Set the voltage range on the device
        minVoltage = -1;//Set range for scaling purposes
        maxVoltage = 1;

        strLowChan << "AISCAN:LOWCHAN=" << lowChan;//Form the message
        device->sendMessage(strLowChan.str());//Send the message

    
        strHighChan << "AISCAN:HIGHCHAN=" << highChan;
        device->sendMessage(strHighChan.str());

        strRate << "AISCAN:RATE=" << sampleRate;
        device->sendMessage(strRate.str());

        device->sendMessage("AISCAN:SAMPLES=0");//Continuous scan, set samples to 0

        //Fill cal constants for later use
        fillCalConstants(lowChan, highChan);

    }

    catch (mcc_err err){
    
        delete device;
        deviceError = true;
        cout << errorString(err);
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

