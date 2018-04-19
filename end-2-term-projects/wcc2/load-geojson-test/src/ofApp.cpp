#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){

    ofSetVerticalSync(true);
	ofSetFrameRate(60);

    // TYPE
    font.load("fonts/AndaleMono.ttf", 16, true, true, true, 1.0f);

    // ARDUINO
    joystick = ofVec2f(0, 0);
    joystick_pressed = false;
    arduino.connect("/dev/tty.usbmodem1421", 57600);
    can_setup_arduino = false;

    // listen for EInitialized notification. this indicates that
	// the arduino is ready to receive commands and it is safe to
	// call setup_arduino()
	ofAddListener(arduino.EInitialized, this, &ofApp::setupArduino);
	can_setup_arduino = false;

    // OSC
    current_tweeted_city = "";
    osc_receiver.setup(9000);

    // 3D
    overall_rotation = ofVec3f(130, -6, -78);
    text_scale = 0.2f;
    // fireworks
    // firework.setup(ofVec3f(0,0,0), ofFloatColor(1.0, 0, 0));
    // don't use the normal gl texture
	ofDisableArbTex();
    ofLoadImage(firework_texture, "dot.png");
    glPointSize(12);

    // LIGHTS
    // key_light_1.setAttenuation(1.0f, 0.f, 0.001f);
    key_light_1.setDirectional();

    // CAMERA
    cam.setDistance(80);
    cam.setNearClip(0.5);
    // camera placement settings
    // movement
    cam_move_speed = 0.065f;
    cam_position = ofPoint(2.38566, -19.6323, 29.135);
    cam_move_velocity = ofVec3f(0, 0, 0);
    cam_move_acceleration = ofVec3f(0, 0, 0);
    // orientation
    cam_orient_speed = 0.05f;
    cam_orientation = ofVec3f(31, 0, 0);
    cam_orient_velocity = ofVec3f(0, 0, 0);
    cam_orient_acceleration = ofVec3f(0, 0, 0);

    // GEOJSON
    geojson_scale = 400;

    std::string file_path = "world_cities_countries.geojson";

    // create the actual geojson meshes and return the centroid
    ofPoint geoshape_centroid = vv_geojson::create_geojson_map(file_path, font, poly_meshes, cities, geojson_scale);
    
    // let the cam start at the centroid of the shape
    cam.lookAt(geoshape_centroid);

    cout << "overall centroid: " << geoshape_centroid << endl;

    cout << "ended parsing of file" << endl;
    cout << "poly_meshes.size(): " << poly_meshes.size() << endl;
    cout << "cities.size(): " << cities.size() << endl;
}

//--------------------------------------------------------------
void ofApp::update(){

    for (int f = 0; f < fireworks.size(); f++){
        fireworks.at(f).update();
    }

    updateArduino();
    compute_cam_movement();
    compute_cam_orientation();

    // check for osc messages
	while(osc_receiver.hasWaitingMessages()){
        ofxOscMessage m;
        osc_receiver.getNextMessage(m);
        if(m.getAddress() == "/twitter-app"){
			current_tweeted_city = "#" + m.getArgAsString(0);
            cout << "heard a tweet related to: " << current_tweeted_city << endl;

            ofVec3f city_pos;
            bool found = false;

            for (int i = 0; i < cities.size(); i++){
                
                if (cities.at(i).name == current_tweeted_city){
                    city_pos = cities.at(i).position;
                    found = true;
                }

                if (found) break;
            }

            // find the location of the tweeted city
            // while (i < cities.size()){
            //     cout << "i: " << i << ", cities.size(): " << cities.size() << endl;
            //     // TODO: binary search instead of stupid loop
            //     if (cities.at(i).name == current_tweeted_city){
            //         city_pos = cities.at(i).position;
            //         found = true;
            //     }
            //     if (found){
            //         break;
            //     }
            //     i++;
            // }

            if (found){
                if (fireworks.size() > 15){
                    fireworks.pop_front();
                }
                // add a firework
                Firework firework;
                firework.setup(city_pos, ofFloatColor(1.0, 0, 0));
                fireworks.push_back(firework);
            }
            else {
                cout << "!!!!!!ATTENTION!!!!!!" << endl;
                cout << "city " << current_tweeted_city << " not found!" << endl;
            }
		}
    }
}

//--------------------------------------------------------------
void ofApp::draw(){

    ofBackground(255);
    ofSetColor(0);
    ofFill();

    ofEnableDepthTest();

    cam.setPosition(cam_position); // see compute_cam_movement()
    cam.setOrientation(cam_orientation);

    cam.begin();
    
    key_light_1.enable();
    key_light_1.setOrientation(cam_orientation);

    ofDrawAxis(100);

    // move shape to the center of the world
    ofTranslate(-geoshape_centroid);

    // draw the vbo meshes for the polygons
    for (int i = 0; i < poly_meshes.size(); i++){
        poly_meshes.at(i).draw();
    }

    // draw the text of each city
    for (int i = 0; i < cities.size(); i++){
        ofPushMatrix();
            ofTranslate(cities.at(i).position);
            ofTranslate(0, 0, -0.1f);
            ofRotateX(-90);
            // ofScale(0.02, 0.02, 0.02);
            for (int m = 0; m < cities.at(i).meshes.size(); m++){
                cities.at(i).meshes.at(m).draw();
            }
        ofPopMatrix();
    }

    // fireworks
    ofEnableAlphaBlending();
    ofEnableBlendMode(OF_BLENDMODE_MULTIPLY);
    ofEnablePointSprites();
    ofSetColor(255);

    for (int f = 0; f < fireworks.size(); f++){
        
        Firework * firework = &fireworks.at(f);

        if (!firework->exploded()){
            ofDrawSphere(
                firework->initial_particle.position.x,
                firework->initial_particle.position.y,
                firework->initial_particle.position.z,
                0.3);
        }
        else {
            firework_texture.bind();
            firework->mesh.draw();
            firework_texture.unbind();
        }
    }

    ofDisableAlphaBlending();
    ofDisableBlendMode();
    ofDisablePointSprites();

    cam.end();
    key_light_1.disable();

    ofDisableDepthTest();

    // 2D STUFF

    ofSetColor(0);
    ofFill();
    // ofDrawBitmapString(current_tweeted_city, 20, 30);
    font.drawString(current_tweeted_city, 20, 30);
    ofDrawBitmapString("fps: " + ofToString(ofGetFrameRate()), 20, 50);
    if (!can_setup_arduino){
        ofDrawBitmapString("arduino not ready...\n", 20, 70);
    }
    else {
        if (joystick_pressed) ofDrawBitmapString("joystick pressed!", 20, 70);
        // ofDrawBitmapString(analog_status, 20, 90);
    }
}

//--------------------------------------------------------------
void ofApp::compute_cam_movement(){

    // add joystick acceleration
    cam_move_acceleration += joystick;

    float max_speed = 0.21f;
    
    ofVec3f friction = cam_move_velocity;
    friction.normalize();
    friction *= -1;
    friction *= 0.003f;

    cam_move_velocity += cam_move_acceleration;
    
    cam_move_velocity += friction;
    cam_move_velocity *= 0.975f;
    cam_move_velocity.limit(max_speed);
    if (cam_move_velocity.length() < 0.0009f){
        cam_move_velocity = ofVec3f(0, 0, 0);
    }
    // update position
    cam_position += cam_move_velocity;
    // reset acceleration
    cam_move_acceleration = ofVec3f(0, 0, 0);
}

//--------------------------------------------------------------
void ofApp::compute_cam_orientation(){

    float max_speed = 0.21f;
    
    ofVec3f friction = cam_orient_velocity;
    friction.normalize();
    friction *= -1;
    friction *= 0.003f;

    cam_orient_velocity += cam_orient_acceleration;
    
    cam_orient_velocity += friction;
    cam_orient_velocity *= 0.975f;
    cam_orient_velocity.limit(max_speed);
    // after a certain speed threshold, save computing time by remaining still
    if (cam_orient_velocity.length() < 0.0009f){
        cam_orient_velocity = ofVec3f(0, 0, 0);
    }
    // update position
    cam_orientation += cam_orient_velocity;
    // reset acceleration
    cam_orient_acceleration = ofVec3f(0, 0, 0);
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
    cout << "ofMap(mouseX, 0, ofGetWidth(), -90, 90): " << ofMap(mouseX, 0, ofGetWidth(), -90, 90) << endl;
    cout << "ofMap(mouseY, 0, ofGetHeight(), -90, 90): " << ofMap(mouseY, 0, ofGetHeight(), -90, 90) << endl;
    cout << "overall_rotation: " << overall_rotation << endl;
    cout << "cam properties" << endl;
    cout << "cam.getGlobalPosition():    " << cam.getGlobalPosition() << endl;
    cout << "cam.getGlobalOrientation(): " << cam.getGlobalOrientation() << endl;
    cout << "cam.getDistance():          " << cam.getDistance() << endl;
    cout << "cam_orientation: " << cam_orientation << endl;
    cout << "cam_position:    " << cam_position << endl;
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

    switch (key){
        // CAMERA MOVEMENTS
        case '[': {
            cam_move_acceleration.z+=cam_move_speed;
            break;
        }
        case ']': {
            cam_move_acceleration.z-=cam_move_speed;
            break;
        }
        // case OF_KEY_UP: {
        //     cam_move_acceleration.y+=cam_move_speed;
        //     break;
        // }
        // case OF_KEY_DOWN: {
        //     cam_move_acceleration.y-=cam_move_speed;
        //     break;
        // }
        // case OF_KEY_RIGHT: {
        //     cam_move_acceleration.x+=cam_move_speed;
        //     break;
        // }
        // case OF_KEY_LEFT: {
        //     cam_move_acceleration.x-=cam_move_speed;
        //     break;
        // }
        //
        case 'q':{
            cam_orient_acceleration.x -= cam_orient_speed;
            break;
        }
        case 'w':{
            cam_orient_acceleration.x += cam_orient_speed;
            break;
        }
        case 'a':{
            cam_orient_acceleration.y -= cam_orient_speed;
            break;
        }
        case 's':{
            cam_orient_acceleration.y += cam_orient_speed;
            break;
        }
        case 'z':{
            cam_orient_acceleration.z -= cam_orient_speed;
            break;
        }
        case 'x':{
            cam_orient_acceleration.z += cam_orient_speed;
            break;
        }
    }

}

//--------------------------------------------------------------
// ARDUINO METHODS
//--------------------------------------------------------------
//--------------------------------------------------------------
void ofApp::setupArduino(const int & version) {
	
	// remove listener because we don't need it anymore
	ofRemoveListener(arduino.EInitialized, this, &ofApp::setupArduino);

    /* // a little blink to start
    for (int i = 0; i < 5; i++){
	    arduino.sendDigital(13, ARD_LOW);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
	    arduino.sendDigital(13, ARD_HIGH);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    } */
    
    // it is now safe to send commands to the Arduino
    can_setup_arduino = true;
    
    // print firmware name and version to the console
    ofLogNotice() << arduino.getFirmwareName(); 
    ofLogNotice() << "firmata v" << arduino.getMajorFirmwareVersion() << "." << arduino.getMinorFirmwareVersion();

    // set pin 2 as INPUT_PULLUP
    arduino.sendDigitalPinMode(2, ARD_INPUT);
    arduino.sendDigital(2, ARD_HIGH);

    // set pin A0, A1 to analog input
    arduino.sendAnalogPinReporting(0, ARD_ANALOG);
    arduino.sendAnalogPinReporting(1, ARD_ANALOG);
	
    // Listen for changes on the digital and analog pins
    ofAddListener(arduino.EDigitalPinChanged, this, &ofApp::digitalPinChanged);
    ofAddListener(arduino.EAnalogPinChanged, this, &ofApp::analogPinChanged);    
}

//--------------------------------------------------------------
void ofApp::updateArduino(){
	arduino.update();
}

//--------------------------------------------------------------
// digital pin event handler, called whenever a digital pin value has changed
//--------------------------------------------------------------
void ofApp::digitalPinChanged(const int & pinNum) {
    if (pinNum == 2) joystick_pressed = !arduino.getDigital(pinNum);
    // DEBUG
    // if (joystick_pressed) firework.setup(ofVec3f(0,0,0), ofFloatColor(1.0, 0, 0));
}

//--------------------------------------------------------------
// analog pin event handler, called whenever an analog pin value has changed
//--------------------------------------------------------------
void ofApp::analogPinChanged(const int & pinNum) {
    float joystick_speed_divider = 6.5f;
    switch(pinNum){
        case 0:{
            joystick.y = ofMap(arduino.getAnalog(pinNum), 1023, 0, -cam_move_speed/joystick_speed_divider, cam_move_speed/joystick_speed_divider);
            break;
        }
        case 1:{
            joystick.x = ofMap(arduino.getAnalog(pinNum), 1023, 0, -cam_move_speed/joystick_speed_divider, cam_move_speed/joystick_speed_divider);
            break;
        }
    }
    analog_status = "joystick x: " + ofToString(joystick.x) + ", y: " + ofToString(joystick.y);
}