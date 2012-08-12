#include "ofxDaqRecorder.h"
#include "ofAppNoWindow.h"

//--------------------------------------------------------------
int main(){
	ofAppBaseWindow* window = new ofAppNoWindow();
	//ofAppGlutWindow window; // create a window
	// set width, height, mode (OF_WINDOW or OF_FULLSCREEN)
	ofSetupOpenGL(window, 512, 512, OF_WINDOW);
	ofRunApp(new ofxDaqRecorder()); // start the app
}
