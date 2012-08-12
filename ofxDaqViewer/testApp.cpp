#include "testApp.h"

float red[] = {0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.02381,
   0.08730,
   0.15079,
   0.21429,
   0.27778,
   0.34127,
   0.40476,
   0.46825,
   0.53175,
   0.59524,
   0.65873,
   0.72222,
   0.78571,
   0.84921,
   0.91270,
   0.97619,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   0.94444,
   0.88095,
   0.81746,
   0.75397,
   0.69048,
   0.62698,
   0.56349,
   0.50000,
};

float green [] = {   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00794,
   0.07143,
   0.13492,
   0.19841,
   0.26190,
   0.32540,
   0.38889,
   0.45238,
   0.51587,
   0.57937,
   0.64286,
   0.70635,
   0.76984,
   0.83333,
   0.89683,
   0.96032,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   0.96032,
   0.89683,
   0.83333,
   0.76984,
   0.70635,
   0.64286,
   0.57937,
   0.51587,
   0.45238,
   0.38889,
   0.32540,
   0.26190,
   0.19841,
   0.13492,
   0.07143,
   0.00794,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000};

float blue [] = {  0.50000,
   0.56349,
   0.62698,
   0.69048,
   0.75397,
   0.81746,
   0.88095,
   0.94444,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   1.00000,
   0.97619,
   0.91270,
   0.84921,
   0.78571,
   0.72222,
   0.65873,
   0.59524,
   0.53175,
   0.46825,
   0.40476,
   0.34127,
   0.27778,
   0.21429,
   0.15079,
   0.08730,
   0.02381,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
   0.00000,
};

testApp::~testApp() {

	this->stopThread(true);
	delete fifo;
	
}

void testApp::setup() {
	plotHeight = 128;
	bufferSize = 512;
	
	fifo = new CircularFifo(bufferSize*4,1024);

    needUDPData = true;
    audioBufferOffset = 0;

	fft = ofxFft::create(bufferSize, OF_FFT_WINDOW_HAMMING);
	// To use FFTW, try:
	// fft = ofxFft::create(bufferSize, OF_FFT_WINDOW_HAMMING, OF_FFT_FFTW);

	spectrogram.allocate(bufferSize, fft->getBinSize(), OF_IMAGE_COLOR);
	memset(spectrogram.getPixels(), 0, (int) (3*spectrogram.getWidth() * spectrogram.getHeight()) );
	spectrogramOffset = spectrogram.getWidth() - 1;

	audioInput = new float[bufferSize];
	memset(audioInput, 0, sizeof(float) * bufferSize);

	mode = SINE;
	appWidth = ofGetWidth();
	appHeight = ofGetHeight();
    minScale = -30;
    maxScale = 10;

	ofBackground(0, 0, 0);
    ofSetFrameRate(60);
    udpConnection.Create();
	udpConnection.Bind(11999);
	udpConnection.SetNonBlocking(true);
	
	audioChannel = 0;
	
	// This tread monitors all the streams
    this->startThread(true,false);
	
}

void testApp::threadedFunction(){

	int msgSize = 100000;
	char udpMessage[msgSize];
	int received;
	int totalSize;
	char *fullMsg;
	int restSize;
	int index;
	
	while (isThreadRunning()){
	
		received = udpConnection.Receive(udpMessage,msgSize);
		
		if (received > 0){
		
			cout << (int)(udpMessage[0]) << endl;
		
			switch (udpMessage[0]){
			
				case GPSMSG:
					// Assume entire message fits in one UDP
					procGPSMsg(udpMessage,msgSize);
					break;
				case IMUMSG:
					// Assume entire message fits in one UDP
					procIMUMsg(udpMessage,msgSize);
					break;
				case MCCDAQMSG:
					
					
					
					memcpy((void*)&totalSize,udpMessage+2,sizeof(int));
					cout << totalSize << endl;
					restSize = totalSize - received;
					if (restSize > 0 && restSize < 10000000) { // just ignore if size is too crazy, ie > 10MB
						fullMsg = new char[totalSize];
						memcpy(fullMsg,udpMessage,received);
						index = received;
						while (restSize > 0){
							received = udpConnection.Receive(udpMessage,msgSize);
							if (index + received < totalSize){
								memcpy(fullMsg+index,udpMessage,received);
								index += received;
								restSize -= received;
							}
							else {
								memcpy(fullMsg+index,udpMessage,totalSize - index);
								restSize = -1;
							}
						}
						procMCCDAQMsg(fullMsg,totalSize);
						delete fullMsg;
					}		
					break;
				default :
					// no action
					break;
			}      
        
		}
		ofSleepMillis(20); 
	}
}

void testApp::procMCCDAQMsg(char * msg, int length){

	// UPdate the spectrogram
	lock();
	
	// Loop through and get the raw data
	unsigned short tmpVal;
	int index = 0;
	int dataOffset = 2 + 1024;
	float * audioFromUDP = new float[(length - dataOffset)];
	for (int j = audioChannel+dataOffset;j<length;j+=2*4){
		memcpy((char*)&tmpVal,(void*)(msg+j),2);
		audioFromUDP[index] = (float)(tmpVal-32768.0)/65536.0;
		index++;
	}
	
	for(int i=0;i+bufferSize<=index;i+=bufferSize){

		int cpy = bufferSize;
		if (i+bufferSize > index){
			cpy = index-i;
		}
		memcpy((char*)audioInput,(char*)(&audioFromUDP[i]),sizeof(float)*cpy);
		fft->setSignal(audioInput);

		float* curFft = fft->getAmplitude();
		int spectrogramWidth = (int) spectrogram.getWidth();
		unsigned char* pixels = spectrogram.getPixels();
		for(int i=0;i<spectrogram.getHeight();i++){
			memcpy(pixels+i*spectrogramWidth*3,pixels+i*spectrogramWidth*3+3,3*(spectrogramWidth-1));
		}


		float tmp;
		spectrogramOffset = spectrogramWidth - 1;
		for(int i = 0; i < fft->getBinSize(); i++){
			// Transform the amplitudes
			tmp = 10*log10(curFft[i]);

			spectrogram.setColor(spectrogramOffset,i,GetColor(tmp,minScale,maxScale));
			//pixels[i * spectrogramWidth + spectrogramOffset] = (unsigned char) (tmpChar);
		}
		
	
	}
	delete audioFromUDP;
	unlock();
}

void testApp::procGPSMsg(char * msg, int length){
}
void testApp::procIMUMsg(char * msg, int length){
}


void testApp::update() {
        
}

void testApp::draw() {
	ofSetHexColor(0xffffff);
	ofPushMatrix();
	ofTranslate(16, 16);
	ofDrawBitmapString("Time Domain", 0, 0);
	lock();
	plot(audioInput, bufferSize, plotHeight / 2, 0);
	ofTranslate(0, plotHeight + 16);
	ofDrawBitmapString("Frequency Domain", 0, 0);
	//fft->draw(0, 0, fft->getSignalSize(), plotHeight);
	ofTranslate(0, plotHeight + 16);
	spectrogram.update();
	spectrogram.draw(0, 0);
	unlock();
	ofPopMatrix();
	string msg = ofToString((int) ofGetFrameRate()) + " fps";
	ofDrawBitmapString(msg, appWidth - 80, appHeight - 20);
    msg = "Cmin = " + ofToString((int)minScale) + "\nCMax = " + ofToString((int)maxScale);
    ofDrawBitmapString(msg, appWidth - 200, appHeight - 20);
}

void testApp::plot(float* array, int length, float scale, float offset) {
	ofNoFill();
	ofRect(0, 0, length, plotHeight);
	glPushMatrix();
	glTranslatef(0, plotHeight / 2 + offset, 0);
	ofBeginShape();
	for (int i = 0; i < length; i++)
		ofVertex(i, array[i] * scale);
	ofEndShape();
	glPopMatrix();
}

void testApp::keyPressed(int key) {
	switch (key) {
	case 'm':
		mode = MIC;
		break;
	case 'n':
		mode = NOISE;
		break;
	case 's':
		mode = SINE;
		break;
    case 'r':
        mode = REMOTE;
        break;
    case 'z':
        maxScale += 1;
        break;
    case 'x':
        maxScale -= 1;
        if (maxScale <= minScale)
            maxScale += 1;
        break;
    case 'q':
        minScale += 1;
        if (minScale >= maxScale)
            minScale -= 1;
        break;
    case 'w':
        minScale -= 1;
        break;
	}
}

ofColor testApp::GetColor(float value,float min,float max)
{

   unsigned char g = ofMap(value,min,max,0,63,true);
   return(ofColor(255*red[g],255*green[g],255*blue[g]));
}

