#include "math.h"
#include "string.h"
#include <time.h>
#include "OS_specific.h"
#if OS_type==2
#include <conio.h>
#include <io.h>
#endif

#include <sys/types.h>
#include <sys/stat.h> 

#include "Patches.h"
#include "ImageRepresentation.h"

#include "StrongClassifierDirectSelection.h"
#include "StrongClassifierStandard.h"
#include "Detector.h"
#include "SemiBoostingTracker.h"

#include "ImageSource.h"
#include "ImageSourceDir.h"
#include "ImageHandler.h"
#include "ImageSourceUSBCam.h"
#include "ImageSourceAVIFile.h"
#include "fstream"

#include "cv.h"
//#include "cvcam.h"
#include "highgui.h"

int mouse_pointX;
int mouse_pointY;
int mouse_value;
bool mouse_exit;
Rect trackingRect;
bool keyboard_pressed = false;

int readConfigFile(char* configFile, ImageSource::InputDevice& input, int& numBaseClassifiers, float& overlap, float& searchFactor, char* resultDir, char* source, Rect& initBB) {
	printf ("parsing config file %s\n", configFile);

	fstream f;
	char cstring[1000];
	int readS=0;
	f.open(configFile, fstream::in);	
	if (!f.eof()) {
		//version
		//skip first line
		f.getline(cstring, sizeof(cstring)); 
		printf("%s\n", cstring);
		f.getline(cstring, sizeof(cstring));
		printf("%s\n", cstring);
		char param1[200]; strcpy(param1,"");
		char param2[200]; strcpy(param2,"");
		char param3[200]; strcpy(param3,"");

		printf("%s\n", cstring);
		readS=sscanf(cstring, "%s %s", param1,param2);
		char match[100];
		strcpy(match,"0.3");
		if (param2[2] != match[2]) {
			printf("%s %s\n", param2, match);
			printf("ERROR: unsupported version of config file!\n");
			return -1;
		}
		printf("  %s %s\n", param1, param2);

		//source
		f.getline(cstring, sizeof(cstring)); 
		f.getline(cstring, sizeof(cstring)); 
		f.getline(cstring, sizeof(cstring)); 
		readS=sscanf (cstring, "  %s %s %s", param1,param2, param3);
		strcpy(match,"USB");
		if (param3[0]==match[0])
			input=ImageSource::USB;
		else {
			strcpy(match,"AVI");
			if (param3[0]==match[0])
				input=ImageSource::AVI;
			else {
				strcpy(match,"IMAGES");
				if (param3[0]==match[0])
					input=ImageSource::DIRECTORY;
				else
					return -1;
			}
		}
		printf("  %s %s %s\n", param1, param2, param3);
		f.getline(cstring, sizeof(cstring));
		f.getline(cstring, sizeof(cstring));
		//directory
		if (input!=ImageSource::USB) {
			readS=sscanf (cstring, "%s %s %s", param1,param2, param3);
			strcpy( source, param3 );

			printf("  %s %s %s\n", param1, param2, param3);

		}

		f.getline(cstring, sizeof(cstring)); 
		f.getline(cstring, sizeof(cstring)); 
		f.getline(cstring, sizeof(cstring)); 
		//debug information
		readS=sscanf (cstring, "  %s %s %s", param1,param2, param3);
		f.getline(cstring, sizeof(cstring)); 
		strcpy(match,"true");
		if (param3[0]==match[0]) {

			readS=sscanf (cstring, "  %s %s %s", param1,param2, param3);
			printf("  %s %s %s\n", param1, param2, param3);

			//check if result dir exists
			if ( access( param3, 0 ) == 0 )
			{
				struct stat status;
				stat( param3, &status );

				if ( status.st_mode & S_IFDIR )
					strcpy(resultDir, param3);
				else
					resultDir = NULL;
			}
			else resultDir = NULL;
			if (resultDir == NULL)
				printf ("    ERROR: resulDir does not exist - switch off debug\n");
		} else {
			resultDir=NULL;
		}

		f.getline(cstring, sizeof(cstring)); 
		f.getline(cstring, sizeof(cstring)); 
		f.getline(cstring, sizeof(cstring)); 
		//boosting parameters
		readS=sscanf (cstring, "%s %s %d", param1,param2, &numBaseClassifiers);
		printf("  %s %s %i\n", param1, param2, numBaseClassifiers);

		f.getline(cstring, sizeof(cstring));
		f.getline(cstring, sizeof(cstring)); 
		f.getline(cstring, sizeof(cstring)); 
		// search region
		readS=sscanf (cstring, "%s %s %f", param1,param2, &overlap);
		printf("  %s %s %5.3f\n", param1, param2, overlap);
		f.getline(cstring, sizeof(cstring)); 
		readS=sscanf (cstring, "%s %s %f", param1,param2, &searchFactor);
		printf("  %s %s %5.3f\n", param1, param2, searchFactor);

		f.getline(cstring, sizeof(cstring)); 
		f.getline(cstring, sizeof(cstring)); 
		f.getline(cstring, sizeof(cstring)); 
		// initialization bounding box: MOUSE or COORDINATES 
		readS=sscanf (cstring, "  %s %s %s", param1,param2, param3);
		strcpy(match,"MOUSE");
		if (param3[0]==match[0])
			initBB=Rect(0,0,0,0);
		else {
			strcpy(match,"COORDINATES");
			if (param3[0]==match[0]) {
				f.getline(cstring, sizeof(cstring)); 
				f.getline(cstring, sizeof(cstring));
				f.getline(cstring, sizeof(cstring));
				readS=sscanf (cstring, "%s %s %i %i %i %i", param1,param2, &initBB.left, &initBB.upper, &initBB.width, &initBB.height);
				printf("  %s %s %i %i %i %i\n", param1, param2, initBB.left, initBB.upper, initBB.width, initBB.height);
			}
		}

	} else 
		return -1;

	f.close();
	printf ("parsing done\n\n");


	return 0;
}


void on_mouse( int event, int x, int y, int flags, void* param )
{
	switch( event )
	{
	case CV_EVENT_LBUTTONDOWN:
		trackingRect.upper = y;
		trackingRect.left = x;
		break;
	case CV_EVENT_LBUTTONUP:
		trackingRect.height =  y-trackingRect.upper;
		trackingRect.width = x-trackingRect.left;
		break;
	case CV_EVENT_MOUSEMOVE:

		if (flags == CV_EVENT_FLAG_LBUTTON)
		{
			trackingRect.height = y-trackingRect.upper;
			trackingRect.width = x-trackingRect.left;
		}
		break;
	}
}


void track(ImageSource::InputDevice input, int numBaseClassifier, float overlap, float searchFactor, char* resultDir, Rect initBB, char* source = NULL)
{
	unsigned char *curFrame=NULL;
        int key;
	//choose the image source
	ImageSource *imageSequenceSource;
	switch (input)
	{
	case ImageSource::AVI:
		imageSequenceSource = new ImageSourceAVIFile(source);
		break;
	case ImageSource::DIRECTORY:
		imageSequenceSource = new ImageSourceDir(source);
		break;
	case ImageSource::USB:
		imageSequenceSource = new ImageSourceUSBCam();
		break;
	default:
		return;
	}

	ImageHandler* imageSequence = new ImageHandler (imageSequenceSource);
	imageSequence->getImage();

	imageSequence->viewImage ("Tracking...", false);
	cvSetMouseCallback( "Tracking...", on_mouse, 0 );

	printf("1.) Select bounding box to initialize tracker.\n");
	printf("2.) Switch to DOS-window and press enter when done.\n");

	trackingRect.upper = -1;
	trackingRect.left = -1;
	trackingRect.width = -1;
	trackingRect.height = -1;

if (initBB.width==0 && initBB.height==0) {
	//mark the object
	#if OS_type==2
		while (!kbhit())		
		{			
			if (input == ImageSource::AVI || input == ImageSource::DIRECTORY)
				imageSequence->reloadImage();
			else
				imageSequence->getImage();

			if (trackingRect.height != -1)
			{
				imageSequence->paintRectangle (trackingRect, Color (255,255,0), 2);
			}
			imageSequence->viewImage ("Tracking...", false);
			cvWaitKey(25);
		}
		getchar();	
	#endif

	#if OS_type==1
			while (!keyboard_pressed)
			{
					if (input == ImageSource::AVI || input == ImageSource::DIRECTORY)
							imageSequence->reloadImage();
					else
							imageSequence->getImage();

					if (trackingRect.height != -1)
					{
							imageSequence->paintRectangle (trackingRect, Color (255,255,0), 2);
					}
					imageSequence->viewImage ("Tracking...", false);
				   key = cvWaitKey(25);
					if((char)key !=-1) keyboard_pressed = true;
			}
	#endif
	if (trackingRect.height == -1) return;
	} else {
		trackingRect=initBB;
	}
	curFrame = imageSequence->getGrayImage ();
	ImageRepresentation* curFrameRep = new ImageRepresentation(curFrame, imageSequence->getImageSize()); 
	Rect wholeImage;
	wholeImage = imageSequence->getImageSize();

	printf ("init tracker...");
	SemiBoostingTracker* tracker;
	tracker = new SemiBoostingTracker (curFrameRep, trackingRect, wholeImage, numBaseClassifier);
	printf (" done.\n");
	
	Size trackingRectSize;
	trackingRectSize = trackingRect;
	printf ("start tracking (stop by pressing any key)...\n\n");

	FILE* resultStream;
	if (resultDir[0]!=0)
	{
		char *myBuff = new char[255];
		sprintf (myBuff, "%s/SemiBoostingTracker.txt", resultDir);
		resultStream = fopen(myBuff, "w");
		delete[] myBuff;
	}

	int counter = 0;
	bool trackerLost = false;

	key=(char)-1;
	//tracking loop
        while ((char)key == -1)
	{
		clock_t timeWatch;
		timeWatch = clock();
		
		//do tracking
		counter++;

		imageSequence->getImage();
		if (curFrame!=NULL)
			delete[] curFrame;

		curFrame = imageSequence->getGrayImage ();
		if (curFrame == NULL)
			break;

		//calculate the patches within the search region
		Patches *trackingPatches;	
		Rect searchRegion;
		if (!trackerLost)
		{
			searchRegion = tracker->getTrackingROI(searchFactor);
			trackingPatches = new PatchesRegularScan(searchRegion, wholeImage, trackingRectSize, overlap);
		}
		else
		{
			//extend the search region (double size or full window)
			searchRegion = tracker->getTrackingROI(searchFactor*2.0f);
			//searchRegion = wholeImage;
			trackingPatches = new PatchesRegularScan(searchRegion, wholeImage, trackingRectSize, overlap);
		}
		curFrameRep->setNewImageAndROI(curFrame, searchRegion);

		if (!tracker->track(curFrameRep, trackingPatches))
		{
			trackerLost = true;
		}
		else {
			trackerLost = false;
		}
	
		delete trackingPatches;


		//display results
		if (!trackerLost)
			imageSequence->paintRectangle (tracker->getTrackedPatch(), Color(255,255,0), 2);
		imageSequence->viewImage("Tracking...", false);	
	
		//write images and results (debug)
		if (resultDir[0]!=0)
		{
			if (trackerLost)
				fprintf (resultStream, "%8d 0 0 0 0 -1\n", counter);
			else
				fprintf (resultStream, "%8d %3d %3d %3d %3d %5.3f\n", counter, tracker->getTrackedPatch().left, tracker->getTrackedPatch().upper, tracker->getTrackedPatch().width, tracker->getTrackedPatch().height, tracker->getConfidence());
			
			char *myBuff = new char[255];
			sprintf (myBuff, "%s/frame%08d.jpg", resultDir, counter+2);
			imageSequence->saveImage(myBuff);
		}

		// wait for opencv display
				// wait for opencv display
		key=cvWaitKey(25);

		#if OS_type==2
				if (kbhit())
					key=(char) 0;
		#endif

		timeWatch=clock()-timeWatch;
		double framesPerSecond=1000.0/timeWatch;
		
		if (!trackerLost)
			//printf("TRACKING: confidence: %5.3f  fps: %5.2f   \r", tracker->getConfidence(), framesPerSecond);
			printf("TRACKING: confidence: %5.3f (prior: %6.3f)  fps: %5.2f   \r", tracker->getConfidence(), tracker->getPriorConfidence(), framesPerSecond);
		else
			printf("TRACKER LOST, waiting...                                \r");

	}

	printf ("\n\ntracking stopped\n");

	
	if (resultDir[0]!=0)
		fclose(resultStream);

	//clean up
	delete tracker;
	delete imageSequenceSource;
	delete imageSequence;
	if (curFrame == NULL)
		delete[] curFrame;
	delete curFrameRep;
}


int main(int argc, char* argv[])
{
	srand( (unsigned)time( NULL ) );

	printf ("-------------------------------------------------------\n");
	printf ("            Semi-Supervised Boosting Tracker           \n");
	printf ("-------------------------------------------------------\n\n");

	ImageSource::InputDevice input;
	input = ImageSource::DIRECTORY;

	int numBaseClassifier;
	char* source;
	char* resultDir;
	float overlap, searchFactor;
	Rect initBB;

	resultDir=new char[100];
	memset( resultDir, '\0', 100 );
	source=new char[100];
	memset( source, '\0', 100 );
	initBB=Rect(0,0,0,0);

	//read parameters from config file
	int ret;
	if (argc >= 2)
		ret = readConfigFile(argv[1], input, numBaseClassifier, overlap, searchFactor, resultDir, source, initBB);
	else
		ret = readConfigFile("D:\\SemiBoostingData\\config.txt", input, numBaseClassifier, overlap, searchFactor, resultDir, source, initBB);

	if (ret < 0)
	{
		printf ("ERROR: config file damaged\n");
		return -1;
	}

	//start tracking
	track(input, numBaseClassifier, overlap, searchFactor, resultDir, initBB, source);

	return 0;
}
