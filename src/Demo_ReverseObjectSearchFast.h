#pragma once

#include "ofMain.h"

#include "ofxGui.h"
#include "ofxLearn.h"
#include "ofxKDTree.h"
#include "ofxScreenGrabCropped.h"
#include "ofxDarknet.h"

class ofxPPSlide;


class ReverseObjectSearchFastDemo : public ofxPPElement {
public:

    struct Image {
        vector<float> encoding;
        vector<double> projectedEncoding;
        string filename;
        float probability;
        ofRectangle crop;
    };

    ReverseObjectSearchFastDemo(ofxPPSlide *parent, string name, float x, float y, float w, float h) : ofxPPElement(parent, name, x, y, w, h) {
        
    }
    
    void start() {
        if (!loaded) {
            setupData();
            loaded = true;
        }
    }
    
    void stop() {
    }
    
    void setDarknetReference(ofxDarknet *darknet){
        this->darknet = darknet;
    }

    void setupData() {
        maxPCASamples.set("max PCA samples", 50000, 10, 100000);
        bExtractDir.addListener(this, &ReverseObjectSearchFastDemo::extractDirectory);
        bSave.addListener(this, &ReverseObjectSearchFastDemo::saveDialog);
        bLoad.addListener(this, &ReverseObjectSearchFastDemo::loadDialog);
        tScreenDebug.addListener(this, &ReverseObjectSearchFastDemo::toggleScreenGrabDebug);
        tWebcam.addListener(this, &ReverseObjectSearchFastDemo::enableWebcam);
        tVideo.addListener(this, &ReverseObjectSearchFastDemo::enableVideo);
        tScreen.addListener(this, &ReverseObjectSearchFastDemo::enableScreenGrab);
        bSampleImage.addListener(this, &ReverseObjectSearchFastDemo::analyzeImage);
        
        guiOptions.setup();
        guiOptions.setName("Options");
        guiOptions.setPosition(ofGetWidth()-200, box.getY());
        guiOptions.add(numPCAcomponents.set("num PCA components", 40, 2, 1500));
        guiOptions.add(bExtractDir.setup("analyze directory"));
        guiOptions.add(bSave.setup("save"));
        guiOptions.add(bLoad.setup("load"));
        
        guiView.setup();
        guiView.setName("View");
        guiView.setPosition(0, box.getY());
        guiView.add(yoloThreshold.set("yolo threshold", 0.5, 0.0, 1.0));
        guiView.add(yoloMaxOverlap.set("max overlap", 0.5, 0.0, 1.0));
        guiView.add(headerHeight.set("header height", 200, 100, 480));
        guiView.add(thumbHeight.set("thumb height", 76, 60, 420));
        guiView.add(numResults.set("num results", 16, 5, 100));
        guiView.add(bSampleImage.setup("query random image"));
        guiView.add(tWebcam.set("query webcam", false));
        guiView.add(tVideo.set("query video", false));
        guiView.add(tScreen.set("query screengrab (OSX)", false));
        guiView.add(tScreenDebug.set(" > set screengrab window", false));
                
        screen.setup(box.getWidth()-17, box.getHeight()-10, true && !IS_PROJECTING);
    }
    
    void update() {
        if (tWebcam) {
            analyzeWebcam();
        } else if (tScreen) {
            analyzeScreen();
        } else if (tVideo) {
            analyzeVideo();
        }
    }
    
    void draw() {
        if (activeImage.isAllocated()) {
            if (screen.isDebug()) {
                screen.drawDebug();
            } else {
                drawResults();
            }
        }
        guiOptions.draw();
        guiView.draw();
    }
    
    void drawResults(){
        float margin = 3;
        int numRows = floor((box.getHeight()-headerHeight)/(thumbHeight+margin));
        float y = box.getY() + box.getHeight() - numRows * (thumbHeight + margin);
        float x = box.getX() + margin;
        
        ofPushMatrix();
        ofPushStyle();
        ofTranslate(box.getX()+210, box.getY());
        float z = headerHeight / activeImage.getHeight();
        activeImage.draw(0, 0, z * activeImage.getWidth(), headerHeight);
        for(detected_object d : detections ){
            ofSetColor( d.color );
            ofNoFill();
            ofRectangle rect = ofRectangle(z * d.rect.x, z * d.rect.y, z * d.rect.width, z * d.rect.height);
            ofSetLineWidth(3);
            ofDrawRectangle( rect );
            ofDrawBitmapStringHighlight( d.label + ": " + ofToString(d.probability), rect.x, rect.y+20 );
        }
        
        ofPopStyle();
        ofPopMatrix();
        
        for (int i=0; i<resultImages.size(); i++) {
            for (int j=0; j<resultImages[i].size(); j++) {
                float thumbWidth = thumbHeight * resultImages[i][j].getWidth() / resultImages[i][j].getHeight();
                resultImages[i][j].draw(x, y, thumbWidth, thumbHeight);
                x += (margin + thumbWidth);
            }
            y += thumbHeight + margin;
            x = box.getX() + margin;
        }
        
        ofDrawBitmapStringHighlight("Query image", 210, box.getY()+13);
        ofDrawBitmapStringHighlight("Nearest neighbor images:", margin + 5, ofGetHeight() - numRows * (thumbHeight + margin) + 13);
    }

    void toggleScreenGrabDebug(bool & debug) {
        screen.setDebug(debug);
        if (debug && !tScreen) {
            tScreen = true;
            enableScreenGrab((bool&)tScreen);
        }
        if (!screen.isDebug()) {
            screenPixels.allocate(screen.getGrabber().getTextureReference().getWidth(),
                                  screen.getGrabber().getTextureReference().getHeight(), 4);
        }
    }
    
    void enableWebcam(bool & enable) {
        if (!enable) return;
        tScreen = false;
        tVideo = false;
        cam.initGrabber(640, 480);
        movie.close();
    }
    
    void enableScreenGrab(bool & enable) {
        if (!enable) return;
        tWebcam = false;
        tVideo = false;
        cam.close();
        movie.close();
    }
    
    void enableVideo(bool & enable) {
        if (!enable) return;
        tScreen = false;
        tWebcam = false;
        cam.close();
        ofFileDialogResult result = ofSystemLoadDialog("Load a movie");
        movie.load(result.getPath());
        movie.play();
    }
    
    void analyzeWebcam() {
        cam.update();
        if (cam.isFrameNew()) {
            activeImage.setFromPixels(cam.getPixels());
            detections = darknet->yolo(activeImage, yoloThreshold, yoloMaxOverlap);
            queryResults();
        }
    }
    
    void analyzeVideo() {
        movie.update();
        if (movie.isFrameNew()) {
            activeImage.setFromPixels(movie.getPixels());
            detections = darknet->yolo(activeImage, yoloThreshold, yoloMaxOverlap);
            queryResults();
        }
    }
    
    void analyzeScreen() {
        screen.update();
        if (!screen.isDebug()) {
            screen.getGrabber().getTextureReference().readToPixels(screenPixels);
            screenPixels.setImageType(OF_IMAGE_COLOR);
            activeImage.setFromPixels(screenPixels);
            detections = darknet->yolo(activeImage, yoloThreshold, yoloMaxOverlap);
            queryResults();
        }
    }
    
    void analyzeImage() {
        tWebcam = false;
        tScreen = false;
        tVideo = false;
        int idx = floor(ofRandom(images.size()));
        activeImage.load(baseDir+"/"+images[idx].filename);
        detections = darknet->yolo(activeImage, yoloThreshold, yoloMaxOverlap);
        queryResults();
    }
    
    void queryResults() {
        resultImages.resize(detections.size());
        for (int i=0; i<detections.size(); i++) {
            vector<float> encoding = detections[i].features;
            vector<double> encodingD(encoding.begin(), encoding.end());
            vector<double> projectedEncoding = rp.project(encodingD);
            kdTree.getKNN(projectedEncoding, numResults, indexes, distances);
            resultImages[i].resize(numResults);
            for (int j=0; j<numResults; j++) {
                resultImages[i][j].load(baseDir+"/"+images[indexes[j]].filename);
                float x = images[indexes[j]].crop.getX();
                float y = images[indexes[j]].crop.getY();
                float w = images[indexes[j]].crop.getWidth();
                float h = images[indexes[j]].crop.getHeight();
                resultImages[i][j].crop(x, y, w, h);
            }
        }
    }
    
    void getImagePathsRecursive(ofDirectory dir){
        const string allowed_ext[] = {"jpg", "png", "gif", "jpeg"};
        ofDirectory new_dir;
        int size = dir.listDir();
        for (int i = 0; i < size; i++){
            if (dir.getFile(i).isDirectory()){
                new_dir = ofDirectory(dir.getFile(i).getAbsolutePath());
                new_dir.listDir();
                new_dir.sort();
                getImagePathsRecursive(new_dir);
            }
            else if (find(begin(allowed_ext), end(allowed_ext), ofToLower(dir.getFile(i).getExtension())) != end(allowed_ext)) {
                candidateFiles.push_back(dir.getFile(i));
            }
        }
    }
    
    void extractFeaturesForDirectory(string directory) {
        baseDir = directory;
        ofLog() << "Gathering images recursively from "+baseDir;
        candidateFiles.clear();
        ofDirectory dir = ofDirectory(baseDir);
        getImagePathsRecursive(dir);
        int numImages = candidateFiles.size();
        for(int i=0; i<numImages; i++) {
            if (i % 200 == 0) ofLog() << "extracting features for image "<<i<<"/"<<numImages;
            string imagePath = candidateFiles[i].getAbsolutePath();
            ofStringReplace(imagePath, dir.getAbsolutePath(), "");
            if (imageMap.count(imagePath)) {
                continue;
            }
            bool success = activeImage.load(candidateFiles[i]);
            if (success) {
                detections = darknet->yolo(activeImage, yoloThreshold, yoloMaxOverlap);
                for (int j=0; j<detections.size(); j++) {
                    Image image;
                    image.filename = imagePath;
                    image.encoding = detections[j].features;
                    image.crop = detections[j].rect;
                    image.probability = detections[j].probability;
                    images.push_back(image);
                    imageMap[image.filename] = true;
                }
            }
            else {
                ofLog(OF_LOG_ERROR, "Failed to load image: "+candidateFiles[i].getAbsolutePath());
            }
        }
        ofLog() << "finished extracting features for "<<images.size()<<" objects from "<<imageMap.size()<<" images.";
    }
    
    void runPCAonImageSet(){
        vector<int> pcaIndexes;
        for (int i=0; i<images.size(); i++) {pcaIndexes.push_back(i);}
        //    if (maxPCASamples < images.size()) {
        //        random_shuffle(pcaIndexes.begin(), pcaIndexes.end());
        //        pcaIndexes.resize(maxPCASamples);
        //    }
        
        rp.clearTrainingInstances();
        for (int i=0; i<pcaIndexes.size(); i++) {
            if (i%200==0) ofLog() << "copying encoding for image "<<i<<"/"<<pcaIndexes.size();
            int idx = pcaIndexes[i];
            vector<double> sample(images[idx].encoding.begin(), images[idx].encoding.end());
            rp.addSample(sample);
        }
        
        ofLog() << "Run PCA";
        int startTime = ofGetElapsedTimef();
        rp.randomProjection(numPCAcomponents);
        //    rp.pca(numPCAcomponents);
        ofLog() << "Finished PCA in "<<(ofGetElapsedTimef() - startTime)<<" sec";
        
        ofLog() << "Project original samples into reduced space";
        for (int i=0; i<images.size(); i++) {
            if (i%200==0) ofLog() << "getting PCA-projected encoding for image "<<i<<"/"<<images.size();
            vector<double> sample(images[i].encoding.begin(), images[i].encoding.end());
            images[i].projectedEncoding = rp.project(sample);
        }
        ofLog() << "finished extracting projected encodings for "<<images.size()<<" images.";
    }
    
    void runKDTree() {
        kdTree.clear();
        for (int i=0; i<images.size(); i++) {
            if (i%2000==0) ofLog() << "kd-tree: adding image "<<i<<"/"<<images.size();
            kdTree.addPoint(images[i].projectedEncoding);
        }
        ofLog() << "build kd-tree" << endl;
        int startTime = ofGetElapsedTimef();
        kdTree.constructKDTree();
        ofLog() << "finished constructiong kd-tree for "<<images.size()<<" images in "<<(ofGetElapsedTimef() - startTime)<<" sec";
    }
    
    void save(string path, bool featuresOnly=false) {
        ofLog()<<"Saving to "<<path;
        const char *filepath = path.c_str();
        ofstream fout(filepath, ios::binary);
        vector<vector<double> > projectedEncodings;
        vector<vector<float> > encodings;
        vector<vector<float> > crops;
        vector<float> probabilities;
        vector<string> filenames;
        for (auto image : images) {
            if (featuresOnly) {
                encodings.push_back(image.encoding);
            }
            projectedEncodings.push_back(image.projectedEncoding);
            crops.push_back(vector<float>{image.crop.x, image.crop.y, image.crop.width, image.crop.height});
            probabilities.push_back(image.probability);
            filenames.push_back(image.filename);
        }
        if (featuresOnly) {
            dlib::serialize(encodings, fout);
        }
        dlib::serialize(projectedEncodings, fout);
        dlib::serialize(crops, fout);
        dlib::serialize(probabilities, fout);
        dlib::serialize(filenames, fout);
        if (!featuresOnly) {
            //dlib::serialize(rp.getE(), fout);
            dlib::serialize(rp.getV(), fout);
            dlib::serialize(rp.getColumnMeans(), fout);
        }
        ofLog()<<"Saved "<<images.size()<<" image vectors from "<<imageMap.size()<<" images to "<<path<<endl;
    }
    
    void load(string path, string baseDir_, bool featuresOnly=false) {
        baseDir = baseDir_;
        ofLog()<<"Loading from "<<path;
        const char *filepath = path.c_str();
        ifstream fin(filepath, ios::binary);
        vector<vector<double> > projectedEncodings;
        vector<vector<float> > encodings;
        vector<vector<float> > crops;
        vector<float> probabilities;
        vector<string> filenames;
        vector<double> column_means;
        dlib::matrix<double, 0, 0> E, V;
        if (featuresOnly) {
            dlib::deserialize(encodings, fin);
        }
        dlib::deserialize(projectedEncodings, fin);
        dlib::deserialize(crops, fin);
        dlib::deserialize(probabilities, fin);
        dlib::deserialize(filenames, fin);
        if (!featuresOnly) {
            //dlib::deserialize(E, fin);
            dlib::deserialize(V, fin);
            dlib::deserialize(column_means, fin);
            //rp.setE(E);
            rp.setV(V);
            rp.setColumnMeans(column_means);
        }
        imageMap.clear();
        images.clear();
        for (int i=0; i<filenames.size(); i++) {
            Image image;
            image.filename = filenames[i];
            if (featuresOnly) {
                image.encoding = encodings[i];
            }
            image.projectedEncoding = projectedEncodings[i];
            image.crop = ofRectangle(crops[i][0], crops[i][1], crops[i][2], crops[i][3]);
            image.probability = probabilities[i];
            images.push_back(image);
            imageMap[image.filename] = true;
        }
        ofLog()<<"Loaded "<<images.size()<<" image vectors from "<<imageMap.size()<<" images in "<<path<<endl;
    }
    
    void saveKDTree(string path) {
        kdTree.save(path);
    }
    
    void loadKDTree(string path) {
        kdTree.clear();
        for (auto image : images) {
            kdTree.addPoint(image.projectedEncoding);
        }
        kdTree.load(path);
    }
    
    void saveDialog() {
        //save(ofToDataPath("data.dat"));
        ofFileDialogResult result = ofSystemSaveDialog("data.dat", "Where to save saved features");
        if (result.bSuccess) {
            string path = result.getPath();
            save(path);
        }
    }
    
    void loadDialog() {
        //load(ofToDataPath("data.dat"));
        ofFileDialogResult result = ofSystemLoadDialog("Load saved feature vectors");
        ofFileDialogResult result2 = ofSystemLoadDialog("Where is the dataset located?", true);
        if (result.bSuccess) {
            string path = result.getPath();
            string baseDir_ = result2.getPath();
            load(path, baseDir_);
            runKDTree();
        }
    }
    
    void extractDirectory() {
        ofFileDialogResult result = ofSystemLoadDialog("Which directory to scan?", true);
        if (result.bSuccess) {
            extractFeaturesForDirectory(result.filePath);
            runPCAonImageSet();
            runKDTree();
        }
    }
    
    bool mouseMoved(int x, int y ){
        if (tScreen && screen.isDebug()) screen.mouseMoved(x, y);
        return true;
    }
    
    bool mouseDragged(int x, int y){
        if (tScreen && screen.isDebug()) screen.mouseDragged(x, y);
        return true;
    }
    
    bool mousePressed(int x, int y){
        if (tScreen && screen.isDebug()) screen.mousePressed(x, y);
        return true;
    }
    
    bool mouseReleased(int x, int y){
        if (tScreen && screen.isDebug()) screen.mouseReleased(x, y);
        return true;
    }
    
    void keyPressed(int key) {
//        if (key=='1') {
//            analyzeImage();
//        } else if (key=='2') {
//            tWebcam = true;
//            enableWebcam((bool&) tWebcam);
//        } else if (key=='3') {
//            tVideo = true;
//            enableVideo((bool&) tVideo);
//        } else if (key=='4') {
//            tScreen = true;
//            enableScreenGrab((bool&) tScreen);
//        } else if (key=='d') {
//            tScreenDebug = true;
//            toggleScreenGrabDebug((bool&) tScreenDebug);
//        }
    }

    vector<Image> images;
    map<string, bool> imageMap;
    vector<ofFile> candidateFiles;
    string baseDir;
    
    ofxDarknet *darknet;
    ofxLearnRandomProjection rp;
    ofxKDTree kdTree;
    
    ofxScreenGrabCropped screen;
    ofPixels screenPixels;
    ofVideoGrabber cam;
    ofVideoPlayer movie;
    ofImage activeImage;
    
    vector<detected_object> detections;
    vector<size_t> indexes;
    vector<double> distances;
    
    ofImage queryImage;
    vector<vector<ofImage> > resultImages;
    
    ofxPanel guiOptions, guiView;
    ofxButton bExtractDir, bSave, bLoad, bSampleImage;
    ofParameter<bool> tWebcam, tVideo, tScreen, tScreenDebug;
    ofParameter<int> numResults, numPCAcomponents, maxPCASamples;
    ofParameter<float> thumbHeight, headerHeight;
    ofParameter<float> yoloThreshold, yoloMaxOverlap;
};

