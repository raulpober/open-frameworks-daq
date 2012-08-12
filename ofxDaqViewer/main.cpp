#include "ofMain.h"
#include "testApp.h"
#include "ofAppGlutWindow.h"

int main() {
	ofAppGlutWindow window;
	ofSetupOpenGL(&window, 1024 + 32, (128 + 16) * 4 + 16, OF_WINDOW);
	ofRunApp(new testApp());
}
