#pragma once

#include "ofMain.h"
#include "ofxFft.h"
#include "ofxNetwork.h"
#include "CircularFifo.h"

#define MIC 0
#define NOISE 1
#define SINE 2
#define REMOTE 3

#define NOTYPE 0
#define GPSMSG 1
#define IMUMSG 2
#define MCCDAQMSG 3

class testApp : public ofBaseApp, public ofThread{
public:
	~testApp();
	void setup();
	void update();
	void plot(float* array, int length, float scale, float offset);
	void draw();
	void keyPressed(int key);
	void threadedFunction();
    ofColor GetColor(float value,float vmin,float vmax);
	
	void procGPSMsg(char * msg, int length);
	void procIMUMsg(char * msg, int length);
	void procMCCDAQMsg(char * msg, int length);

	int plotHeight, bufferSize;

	ofxFft* fft;

	float* audioInput;

	float appWidth;
	float appHeight;

	ofImage spectrogram;
	int spectrogramOffset;
    float minScale;
    float maxScale;

    ofxUDPManager udpConnection;
    bool needUDPData;
    int audioAvailable;
    int audioBufferOffset;
	
	CircularFifo * fifo;

	int mode;
	
	// UI controls
	int audioChannel;
};
