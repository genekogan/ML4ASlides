#pragma once

#include "ofMain.h"

#include "ofxGui.h"
#include "ofxScreenGrab.h"
#include "ofxScreenGrabCropped.h"
#include "ofxCcv.h"
#include "ofxLearn.h"
#include "ofxKDTree.h"


class ofxPPSlide;


class ReverseImageSearchFastDemo : public ofxPPElement {
public:
    
    struct Image {
        vector<float> encoding;
        vector<double> projectedEncoding;
        string filename;
    };

    ReverseImageSearchFastDemo(ofxPPSlide *parent, string name, float x, float y, float w, float h) : ofxPPElement(parent, name, x, y, w, h) {
        
    }
    
    void start() {
        if (!loaded) {
            setupData();
            loaded = true;
        }
    }
    
    void stop() {
    }
    
    void setCcvReference(ofxCcv *ccv) {
        this->ccv = ccv;
    }

    void setupData() {
        maxPCASamples.set("max PCA samples", 50000, 10, 100000);
        bExtractDir.addListener(this, &ReverseImageSearchFastDemo::extractDirectory);
        bSave.addListener(this, &ReverseImageSearchFastDemo::saveDialog);
        bLoad.addListener(this, &ReverseImageSearchFastDemo::loadDialog);
        tScreenDebug.addListener(this, &ReverseImageSearchFastDemo::toggleScreenGrabDebug);
        tWebcam.addListener(this, &ReverseImageSearchFastDemo::enableWebcam);
        tVideo.addListener(this, &ReverseImageSearchFastDemo::enableVideo);
        tScreen.addListener(this, &ReverseImageSearchFastDemo::enableScreenGrab);
        bSampleImage.addListener(this, &ReverseImageSearchFastDemo::analyzeImage);
        
        tScreenDebug.addListener(this, &ReverseImageSearchFastDemo::toggleScreenGrabDebug);
        tWebcam.addListener(this, &ReverseImageSearchFastDemo::enableWebcam);
        tVideo.addListener(this, &ReverseImageSearchFastDemo::enableVideo);
        tScreen.addListener(this, &ReverseImageSearchFastDemo::enableScreenGrab);
        bSampleImage.addListener(this, &ReverseImageSearchFastDemo::analyzeImage);
        
        guiView.setup();
        guiView.setName("View");
        guiView.setPosition(box.getX(), box.getY());
        guiView.add(headerHeight.set("header height", 320, 100, 480));
        guiView.add(thumbHeight.set("thumb height", 240, 90, 420));
        guiView.add(numResults.set("num results", 20, 5, 100));
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
        guiView.draw();
    }
    
    void drawResults(){
        float margin = 3;
        int numRows = floor((box.getHeight()-headerHeight)/(thumbHeight+margin));
        float y = box.getY() + headerHeight + margin;
        float x = box.getX() + margin;
        activeImage.draw(box.getX() + 205, box.getY(), headerHeight * activeImage.getWidth() / activeImage.getHeight(), headerHeight);
        for (int i=1; i<resultImages.size(); i++) {
            float thumbWidth = thumbHeight * resultImages[i-1].getWidth() / resultImages[i-1].getHeight();
            resultImages[i-1].draw(x, y, thumbWidth, thumbHeight);
            x += (margin + thumbWidth);
            if (x > (box.getWidth() - thumbWidth * 0.33)) {
                y += thumbHeight + margin;
                x = box.getX() + margin;
            }
        }
        ofDrawBitmapStringHighlight("Query image", box.getX() + 210, box.getY() + 13);
        ofDrawBitmapStringHighlight("Nearest neighbor images:", box.getX() + margin + 5, box.getY() + box.getHeight() - numRows * (thumbHeight + margin) + 13);
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
            activeEncoding = ccv->encode(activeImage, ccv->numLayers()-1);
            queryResults();
        }
    }
    
    void analyzeVideo() {
        movie.update();
        if (movie.isFrameNew()) {
            activeImage.setFromPixels(movie.getPixels());
            activeEncoding = ccv->encode(activeImage, ccv->numLayers()-1);
            queryResults();
        }
    }
    
    void analyzeScreen() {
        screen.update();
        if (!screen.isDebug()) {
            screen.getGrabber().getTextureReference().readToPixels(screenPixels);
            screenPixels.setImageType(OF_IMAGE_COLOR);
            activeImage.setFromPixels(screenPixels);
            activeEncoding = ccv->encode(activeImage, ccv->numLayers()-1);
            queryResults();
        }
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
    
    void analyzeImage() {
        tWebcam = false;
        tScreen = false;
        tVideo = false;
        int idx = floor(ofRandom(images.size()));
        activeImage.load(images[idx].filename);
        if (images[idx].encoding.size() == 0) {
            images[idx].encoding = ccv->encode(activeImage, ccv->numLayers()-1);
        }
        activeEncoding = images[idx].encoding;
        queryResults();
    }
    
    void queryResults() {
        vector<double> encoding;
        for (int i=0; i<activeEncoding.size(); i++) {encoding.push_back(activeEncoding[i]);}
        vector<double> projectedEncoding = pca.project(encoding);
        kdTree.getKNN(projectedEncoding, numResults, indexes, distances);
        resultImages.resize(numResults-1);
        for (int i=1; i<numResults; i++) {
            resultImages[i-1].load(images[indexes[i]].filename);
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
        ofLog() << "Gathering images recursively from "+directory;
        candidateFiles.clear();
        ofDirectory dir = ofDirectory(directory);
        getImagePathsRecursive(dir);
        int numImages = 450;//candidateFiles.size();
        for(int i=0; i<numImages; i++) {
            if (i % 200 == 0) ofLog() << "extracting features for image "<<i<<"/"<<numImages;
            bool success = activeImage.load(candidateFiles[i]);
            if (success) {
                Image image;
                image.filename = candidateFiles[i].getAbsolutePath();
                image.encoding = ccv->encode(activeImage, ccv->numLayers()-1);
                images.push_back(image);
            }
            else {
                ofLog(OF_LOG_ERROR, "Failed to load image: "+candidateFiles[i].getAbsolutePath());
            }
        }
        ofLog() << "finished extracting features for "<<images.size()<<" images.";
    }
    
    void runPCAonImageSet(){
        vector<int> pcaIndexes;
        for (int i=0; i<images.size(); i++) {pcaIndexes.push_back(i);}
        if (maxPCASamples < images.size()) {
            random_shuffle(pcaIndexes.begin(), pcaIndexes.end());
            pcaIndexes.resize(maxPCASamples);
        }
        
        for (int i=0; i<pcaIndexes.size(); i++) {
            if (i%200==0) ofLog() << "copying encoding for image "<<i<<"/"<<pcaIndexes.size();
            int idx = pcaIndexes[i];
            vector<double> sample(images[idx].encoding.begin(), images[idx].encoding.end());
            pca.addSample(sample);
        }
        
        ofLog() << "Run PCA";
        int startTime = ofGetElapsedTimef();
        pca.pca(numPCAcomponents);
        ofLog() << "Finished PCA in "<<(ofGetElapsedTimef() - startTime)<<" sec";
        
        ofLog() << "Project original samples into reduced space";
        for (int i=0; i<images.size(); i++) {
            if (i%200==0) ofLog() << "getting PCA-projected encoding for image "<<i<<"/"<<images.size();
            vector<double> sample(images[i].encoding.begin(), images[i].encoding.end());
            images[i].projectedEncoding = pca.project(sample);
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
    
    void save(string path) {
        ofLog()<<"Saving to "<<path;
        const char *filepath = path.c_str();
        ofstream fout(filepath, ios::binary);
        vector<vector<double> > projectedEncodings;
        vector<string> filenames;
        for (auto image : images) {
            projectedEncodings.push_back(image.projectedEncoding);
            filenames.push_back(image.filename);
        }
        dlib::serialize(projectedEncodings, fout);
        dlib::serialize(filenames, fout);
        dlib::serialize(pca.getE(), fout);
        dlib::serialize(pca.getV(), fout);
        dlib::serialize(pca.getColumnMeans(), fout);
        ofLog()<<"Saved "<<images.size()<<" image vectors to "<<path<<endl;
    }
    
    void load(string path) {
        ofLog()<<"Loading from "<<path;
        const char *filepath = path.c_str();
        ifstream fin(filepath, ios::binary);
        vector<vector<double> > projectedEncodings;
        vector<string> filenames;
        vector<double> column_means;
        dlib::matrix<double, 0, 0> E, V;
        dlib::deserialize(projectedEncodings, fin);
        dlib::deserialize(filenames, fin);
        dlib::deserialize(E, fin);
        dlib::deserialize(V, fin);
        dlib::deserialize(column_means, fin);
        pca.setE(E);
        pca.setV(V);
        pca.setColumnMeans(column_means);
        images.clear();
        for (int i=0; i<filenames.size(); i++) {
            Image image;
            image.filename = filenames[i];
            image.projectedEncoding = projectedEncodings[i];
            images.push_back(image);
        }
        ofLog()<<"Loaded "<<images.size()<<" image vectors "<<path<<endl;
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
        if (result.bSuccess) {
            string path = result.getPath();
            load(path);
            runKDTree();
        }
    }
    
    void extractDirectory() {
        ofFileDialogResult result = ofSystemLoadDialog("Which directory to scan?", true);
        if (result.bSuccess) {
            string folder = result.getPath();
            extractFeaturesForDirectory(folder);
            runPCAonImageSet();
            runKDTree();
        }
    }
    
    void keyPressed(int key) {
        if (key=='1') {
            analyzeImage();
        } else if (key=='2') {
            tWebcam = true;
            enableWebcam((bool&) tWebcam);
        } else if (key=='3') {
            tVideo = true;
            enableVideo((bool&) tVideo);
        } else if (key=='4') {
            tScreen = true;
            enableScreenGrab((bool&) tScreen);
        } else if (key=='d') {
            tScreenDebug = true;
            toggleScreenGrabDebug((bool&) tScreenDebug);
        }
    }
    
    bool mouseMoved(int x, int y ){
        screen.mouseMoved(x, y);
        return true;
    }
    
    bool mouseDragged(int x, int y){
        screen.mouseDragged(x, y);
        return true;
    }
    
    bool mousePressed(int x, int y){
        screen.mousePressed(x, y);
        return true;
    }
    
    bool mouseReleased(int x, int y){
        screen.mouseReleased(x, y);
        return true;
    }
    
    vector<Image> images;
    vector<ofFile> candidateFiles;
    
    ofxCcv *ccv;
    ofxLearnPCA pca;
    ofxKDTree kdTree;
    
    ofxScreenGrabCropped screen;
    ofPixels screenPixels;
    ofVideoGrabber cam;
    ofVideoPlayer movie;
    ofImage activeImage;
    
    vector<size_t> indexes;
    vector<double> distances;
    vector<float> activeEncoding;
    
    ofImage queryImage;
    vector<ofImage> resultImages;
    
    ofxPanel guiView;
    ofxButton bExtractDir, bSave, bLoad, bSampleImage;
    ofParameter<bool> tWebcam, tVideo, tScreen, tScreenDebug;
    ofParameter<int> numResults, numPCAcomponents, maxPCASamples;
    ofParameter<float> thumbHeight, headerHeight;

};

