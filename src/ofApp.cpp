#include "ofApp.h"

EmoEngineEventHandle eEvent			= EE_EmoEngineEventCreate();
EmoStateHandle eState				= EE_EmoStateCreate();
unsigned int userID					= 0;
const unsigned short composerPort	= 1726;
float secs							= 1;
unsigned int datarate				= 0;
bool readytocollect					= false;
int option							= 0;
int state							= 0;
int stimIndex                       = 0;

EE_DataChannel_t targetChannelList[] = {
		ED_O1
	};

const char header[] = "O1, STIM,";

ofImage upArrow, leftArrow, rightArrow;
ofImage gUpArrow, gLeftArrow, gRightArrow;

ofxXmlSettings settings;

string com, stream = "";

bool startAgain, resultReceived, start, moveable = false;
bool stimChanged = true;
int result = -1;

clock_t t1, t2;
clock_t stimTimer1, stimTimer2;

int stimNumber = 0;
int lastStim = -1;
float feedbackX, feedbackY, moveX, moveY;
std::ofstream ofs("eegLog.csv",std::ios::trunc);
//--------------------------------------------------------------
void ofApp::setup(){

    ofs << header << std::endl;

    moveX = 0;
    moveY = 0;

    settings.loadFile("settings.xml");
    stream = settings.getValue("settings:stream","stream");
    com = settings.getValue("settings:com","comport");

    font.loadFont("font.ttf", 32);

    upArrow.loadImage("images/up.png");
    leftArrow.loadImage("images/left.png");
    rightArrow.loadImage("images/right.png");

    gUpArrow.loadImage("images/gup.png");
    gLeftArrow.loadImage("images/gleft.png");
    gRightArrow.loadImage("images/gright.png");


	ofBackground(255);
    if (EE_EngineConnect() != EDK_OK) {
					throw "Emotiv Engine start up failed.";
    }
	// open an outgoing connection to HOST:PORT
	sender.setup(HOST, PORT);
	stimSender.setup(HOST, 7401);
    receiver.setup(7402);

    ofSetLogLevel(OF_LOG_VERBOSE);
    ofSetFrameRate(30);
    loadCameras();

    ofSetVerticalSync(true);

	bSendSerialMessage = false;
	serial.listDevices();
	vector <ofSerialDeviceInfo> deviceList = serial.getDeviceList();

	// this should be set to whatever com port your serial device is connected to.
	// (ie, COM4 on a pc, /dev/tty.... on linux, /dev/tty... on a mac)
	// arduino users check in arduino app....
	int baud = 9600;
	serial.setup(com, baud); // connect to COM port at 9600

	nTimesRead = 0;
	nBytesRead = 0;
	readTime = 0;
	memset(bytesReadString, 0, 4);

    hData = EE_DataCreate();
    EE_DataSetBufferSizeInSec(secs);
    ofxOscMessage m;
    m.setAddress("/start");
    sender.sendMessage(m);
    t1 = clock();
}
//------------------------------------------------------------------------------
IPCameraDef& ofApp::getNextCamera()
{
    nextCamera = (nextCamera + 1) % ipcams.size();
    return ipcams[nextCamera];
}
//------------------------------------------------------------------------------
void ofApp::loadCameras()
{

    // all of these cameras were found using this google query
    // http://www.google.com/search?q=inurl%3A%22axis-cgi%2Fmjpg%22
    // some of the cameras below may no longer be valid.

    // to define a camera with a username / password
    //ipcams.push_back(IPCameraDef("http://148.61.142.228/axis-cgi/mjpg/video.cgi", "username", "password"));

	ofLog(OF_LOG_NOTICE, "---------------Loading Streams---------------");
    std::cout << "Connecting to: " << stream << std::endl;
    ipcams.push_back(IPCameraDef(stream)); // home
    ofLog(OF_LOG_NOTICE, "-----------Loading Streams Complete----------");
    IPCameraDef& cam = getNextCamera();
    SharedIPVideoGrabber c = IPVideoGrabber::makeShared();
    c->setCameraName(cam.getName());
    c->setURI(cam.getURL());
    c->connect(); // connect immediately
    // if desired, set up a video resize listener
    ofAddListener(c->videoResized, this, &ofApp::videoResized);
    grabbers.push_back(c);
}
//------------------------------------------------------------------------------
void ofApp::videoResized(const void* sender, ofResizeEventArgs& arg)
{
    // find the camera that sent the resize event changed
    for(std::size_t i = 0; i < NUM_CAMERAS; i++)
    {
        if(sender == &grabbers[i])
        {
            std::stringstream ss;
            ss << "videoResized: ";
            ss << "Camera connected to: " << grabbers[i]->getURI() + " ";
            ss << "New DIM = " << arg.width << "/" << arg.height;
            ofLogVerbose("ofApp") << ss.str();
        }
    }
}
//--------------------------------------------------------------
void ofApp::update(){

    if(receiver.hasWaitingMessages()){
            ofxOscMessage msg;
            while(receiver.getNextMessage(&msg)) {
                 if (msg.getAddress() == "/r"){
                    resultReceived = true;
                    result = msg.getArgAsFloat(0);
                    std::cout << result << std::endl;
                 }
            }
        }

    if(startAgain){
        ofxOscMessage m;
        m.setAddress("/start");
        sender.sendMessage(m);
        t1 = clock();
        startAgain = false;
    }
    if(start){
        //camAndSerialUpdate();
        t2=clock();

        if((t2-t1)/CLOCKS_PER_SEC < 9.0){



        state = EE_EngineGetNextEvent(eEvent);
        if (state == EDK_OK) {

				EE_Event_t eventType = EE_EmoEngineEventGetType(eEvent);
				EE_EmoEngineEventGetUserId(eEvent, &userID);

				// Log the EmoState if it has been updated
				if (eventType == EE_UserAdded) {
					std::cout << "User added";
					EE_DataAcquisitionEnable(userID,true);
					readytocollect = true;
				}
			}

			if (readytocollect) {
                stimTimer2 = clock();
                if((stimTimer2-stimTimer1) / CLOCKS_PER_SEC > 1){

                    stimNumber++;
                    if(stimNumber >= 3){
                        stimNumber = 0;
                    }
                    stimChanged = true;
                    stimTimer1 = clock();
                }

                if(stimChanged){
                    ofxOscMessage m;
                    m.setAddress("/st");
                    m.addIntArg(stimNumber);
                    stimSender.sendMessage(m);
                    stimChanged = false;
                }

                    EE_DataUpdateHandle(0, hData);

					unsigned int nSamplesTaken=0;
                    EE_DataGetNumberOfSample(hData,&nSamplesTaken);

                    //std::cout << "Updated " << nSamplesTaken << std::endl;


						if (nSamplesTaken != 0) {
						unsigned int channelCount = sizeof(targetChannelList)/sizeof(EE_DataChannel_t);
						double ** buffer = new double*[channelCount];
                        for (int i=0; i<channelCount; i++)
                            buffer[i] = new double[nSamplesTaken];

                        EE_DataGetMultiChannels(hData, targetChannelList, channelCount, buffer, nSamplesTaken);
                        for (int sampleIdx=0 ; sampleIdx<(int)nSamplesTaken ; ++sampleIdx) {
                            for (int i = 0 ; i<sizeof(targetChannelList)/sizeof(EE_DataChannel_t) ; i++) {
                                //std::cout << buffer[i][sampleIdx] << std::endl;
                                ofxOscMessage m;
                                m.setAddress("/d");
                                //std::cout <<"1: "<< buffer[i][sampleIdx] << " 2: " << buffer[i][sampleIdx+1] << " SUM: " << buffer[i][sampleIdx]+buffer[i][sampleIdx+1] << " AVG: " << (buffer[i][sampleIdx]+buffer[i][sampleIdx+1])/2 << std::endl;
                                ofs << buffer[i][sampleIdx] << ", " << ofToString(stimNumber) << ", " << std::endl;
                                m.addFloatArg(buffer[i][sampleIdx]);
                                sender.sendMessage(m);
                            }
                        }
                        for (int i=0; i<channelCount; i++)
								delete buffer[i];
							delete buffer;

						}
			}
			//Sleep(100);

        } else {
            ofxOscMessage m;
            m.setAddress("/end");
            sender.sendMessage(m);
            startAgain = true;
        }
    }

}
//--------------------------------------------------------------
void ofApp::draw(){
    feedbackX = ofGetWidth()/2 + moveX;
    feedbackY = ofGetHeight() - 100 + moveY;

    if(!start){
        string welcome = "Emotiv EPOC Controlled RC Car";
        ofBackground(0);
        font.drawString(welcome, (ofGetWidth()/2)-font.stringWidth(welcome)/2,(ofGetHeight()/2)-font.stringHeight(welcome)/2);
        font.drawString("PRESS SPACE TO BEGIN", (ofGetWidth()/2)-font.stringWidth("PRESS SPACE TO BEGIN")/2,20+(ofGetHeight()/2)+font.stringHeight(welcome)/2);
    } else {
        ofBackground(0);
        // display instructions
        ofPushStyle();
        ofSetColor(255);
        string buf;
        buf = "sending osc:" + string(HOST) + " " + ofToString(PORT);
        ofDrawBitmapString(buf, 10, 20);
        ofCircle(feedbackX,feedbackY,100);
        ofPopStyle();

        gUpArrow.draw((ofGetWidth()/2)-128,0, 256, 256);
        gLeftArrow.draw(0,(ofGetHeight()/2)-128, 256, 256);
        gRightArrow.draw(ofGetWidth()-256,(ofGetHeight()/2)-128, 256, 256);

        //camAndSerialDraw();
        if(resultReceived == false){
            if(stimNumber == 0){ //up arrow
                leftArrow.draw(0,(ofGetHeight()/2)-128, 256, 256);
            } else if(stimNumber == 1){ //left arrow
                upArrow.draw((ofGetWidth()/2)-128,0, 256, 256);
            } else if(stimNumber == 2){ //right arrow
                rightArrow.draw(ofGetWidth()-256,(ofGetHeight()/2)-128, 256, 256);
            }
        } else {
            string decision;
            decision = "Moving: ";
            if(result == 1){
                decision += "FORWARD.";
                moveY -= 50;
                if(moveable)
                    moveForward();
                //upArrow.draw((ofGetWidth()/2)-32,0, 64, 64);
            } else if(result == 0){
                decision += "LEFT.";
                moveX -= 50;
                if(moveable)
                    moveForwardLeft();
                //leftArrow.draw(0,ofGetHeight()/2, 64, 64);
            } else if(result == 2){
                decision += "RIGHT.";
                 moveX += 50;
                if(moveable)
                    moveForwardRight();
                //rightArrow.draw(ofGetWidth()-64,ofGetHeight()/2, 64, 64);
            }
            ofPushStyle();
            ofSetColor(255);
            ofDrawBitmapString(decision, 10, ofGetHeight()-20);
            std::cout << "Result: " << decision << std::endl;
            ofPopStyle();
            resultReceived = false;
            startAgain = true;

        }
    }
}
//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if(key == 'N'){
        ofxOscMessage m;
        m.setAddress("/end");
        sender.sendMessage(m);
    }
    //camera refresh
    if(key == 'R' || key == 'r')
    {
        // initialize connection
        for(std::size_t i = 0; i < NUM_CAMERAS; i++)
        {
            ofRemoveListener(grabbers[i]->videoResized, this, &ofApp::videoResized);
            SharedIPVideoGrabber c = IPVideoGrabber::makeShared();
            IPCameraDef& cam = getNextCamera();
            c->setUsername(cam.getUsername());
            c->setPassword(cam.getPassword());
            Poco::URI uri(cam.getURL());
            c->setURI(uri);
            c->connect();
            grabbers[i] = c;
        }
    }

    if(key == ' ' && !start){
        start = true;
        stimTimer1 = clock();
    }
    if(key == ' ' && start){
        moveable = true;
    }

}
void ofApp::exit(){
    EE_DataFree(hData);
    EE_EngineDisconnect();
    EE_EmoStateFree(eState);
    EE_EmoEngineEventFree(eEvent);
}
//--------------------------------------------------------------
void ofApp::camAndSerialUpdate(){
// update the cameras
    for(std::size_t i = 0; i < grabbers.size(); i++)
    {
        grabbers[i]->update();
    }

    //serial
    if (bSendSerialMessage){

		// (1) write the letter "a" to serial:
		serial.writeByte('a');

		// (2) read
		// now we try to read 3 bytes
		// since we might not get them all the time 3 - but sometimes 0, 6, or something else,
		// we will try to read three bytes, as much as we can
		// otherwise, we may have a "lag" if we don't read fast enough
		// or just read three every time. now, we will be sure to
		// read as much as we can in groups of three...

		nTimesRead = 0;
		nBytesRead = 0;
		int nRead  = 0;  // a temp variable to keep count per read

		unsigned char bytesReturned[3];

		memset(bytesReadString, 0, 4);
		memset(bytesReturned, 0, 3);

		while( (nRead = serial.readBytes( bytesReturned, 3)) > 0){
			nTimesRead++;
			nBytesRead = nRead;
		};

		memcpy(bytesReadString, bytesReturned, 3);

		bSendSerialMessage = false;
		readTime = ofGetElapsedTimef();
	}
}
//--------------------------------------------------------------
void ofApp::camAndSerialDraw(){
    ofBackground(0,0,0);

	ofSetHexColor(0xffffff);

    int row = 0;
    int col = 1;

    int x = 64;
    int y = 64;

    int w = ofGetWidth() - 128 / NUM_COLS;
    int h = ofGetHeight() - 64 / NUM_ROWS;

    float totalKbps = 0;
    float totalFPS = 0;

    for(std::size_t i = 0; i < grabbers.size(); i++)
    {
        ofPushMatrix();
        ofTranslate(x,y);
        ofSetColor(255,255,255,255);
        grabbers[i]->draw(0,0,w,h); // draw the camera

        ofEnableAlphaBlending();

        float kbps = grabbers[i]->getBitRate() / 1000.0f; // kilobits / second, not kibibits / second
        totalKbps+=kbps;

        float fps = grabbers[i]->getFrameRate();
        totalFPS+=fps;


        ofPopMatrix();
    }

    // keep track of some totals
    float avgFPS = totalFPS / NUM_CAMERAS;
    float avgKbps = totalKbps / NUM_CAMERAS;

    ofEnableAlphaBlending();
    ofSetColor(255);
    // ofToString formatting available in 0072+
    ofDrawBitmapString(" AVG FPS: " + ofToString(avgFPS,2/*,7,' '*/), 10,17);
    ofDrawBitmapString("AVG Kb/S: " + ofToString(avgKbps,2/*,7,' '*/), 10,29);
    ofDrawBitmapString("TOT Kb/S: " + ofToString(totalKbps,2/*,7,' '*/), 10,41);

    string buf;
	buf = "listening for osc messages on port" + ofToString(PORT);
	ofDrawBitmapString(buf, 10, 53);

    ofDisableAlphaBlending();

}
//--------------------------------------------------------------
void ofApp::keyReleased(int key){
    if(key == ' ' && start){
        moveable = false;
    }
}
//--------------------------------------------------------------
void ofApp::moveForwardLeft(){
    serial.writeByte('q');
}
//--------------------------------------------------------------
void ofApp::moveForwardRight(){
    serial.writeByte('e');
}
//--------------------------------------------------------------
void ofApp::moveForward(){
    serial.writeByte('w');
}
//--------------------------------------------------------------
void ofApp::moveBackwards(){
    serial.writeByte('x');
}
//--------------------------------------------------------------
void ofApp::moveBackwardsRight(){
    serial.writeByte('c');
}
//--------------------------------------------------------------
void ofApp::moveBackwardsLeft(){
    serial.writeByte('z');
}
//--------------------------------------------------------------
void ofApp::moveRight(){
    serial.writeByte('d');
}
//--------------------------------------------------------------
void ofApp::moveLeft(){
    serial.writeByte('a');
}


