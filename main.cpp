#include <stdlib.h>

#include "PlanProcessor.h"
#include "TextureManager.h"

//OpenGL Defines
#include <GL/glut.h>

//OPENCV defines
#include <vector>

//OPIRA defines
#include "OPIRALibrary.h"
#include "OPIRALibraryMT.h"
#include "CaptureLibrary.h"
#include "RegistrationAlgorithms/OCVSurf.h"
using namespace OPIRALibrary;

//Event Handlers
void mouseEvent(int cvEvent, int x, int y, int flags, void* param);
void draw(IplImage *newFrame);

//OpenGL Functions
void render(IplImage* frame_input, std::vector<MarkerTransform> mt);
GLuint GLTextureID; int WINDOW_WIDTH = 640, WINDOW_HEIGHT=480;

//Selection Objects
CvPoint2D32f corners[4]; int cornerCount=0;
IplImage *curFrame=0; bool paused = false;

//OPIRA Objects
Registration *r;
Capture *capture;
bool halfResolutionRegistration = true;

//Plan Objects
vector<SLine> walls;
CvRect roof;

//Rendering Variables
float wallHeight = 100;
float roofAngle = 5;
int roofMode=3;

void main(int argc, char**argv) {
	//Initialise Glut
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(640, 480);
	glutCreateWindow("ARcitect");
	glGenTextures(1, &GLTextureID);

	//Initialise Wall and Roof Textures
	InitTextures();

	//Set up the capture object and the registration object
	capture = new Camera(0, "singleIntrinsics.yml");
	//capture = new Video("20110316191749609.avi", "singleIntrinsics.yml");
	r = new RegistrationOPIRAMT(new OCVSurf());

	//Create the Window
	cvNamedWindow("ARcitect");
	cvSetMouseCallback("ARcitect", mouseEvent);

	//Uncomment this to load a map by default
	/*IplImage *plan = cvLoadImage("easy.png");
	r->addResizedScaledMarker("easy.png",400, max(plan->width, plan->height));
	vector<SLine> origWalls = extractPlan(plan,4);
	walls = reducePlanComplexity(origWalls);
	roof = getPlanBounds(walls);
	cvReleaseImage(&plan);
	*/

	//Start the loop
	bool running = true;
	while (running) {
		//Grab a frame from the camera and draw
		IplImage *newIm = capture->getFrame();
		if (newIm==0) {running=false; break;}

		draw(newIm);

		//Check for key input
		switch (cvWaitKey(1)) {
			case 27:
				running = false; break;
			case 'c':
				r->removeMarker("floorplan.bmp"); cornerCount = 0; walls.clear(); break;
			case 'r':
				roofMode--; if (roofMode<0) roofMode = 3; break;
		}

		cvReleaseImage(&newIm);
	}

	//Clean up
	CleanupTextures();
	cvDestroyAllWindows();
	delete r;
	delete capture;
}

/* Handle the drawing operation */
void draw(IplImage *newFrame) {
	//If the video stream isn't paused
	if (!paused) {
		//Copy the new frame
		if (curFrame) cvReleaseImage(&curFrame); curFrame = cvCloneImage(newFrame);
		vector<MarkerTransform> mt;
		//If we're performing half resolution registration
		if (halfResolutionRegistration) {
			//Shrink the image and camera parameters before registration
			IplImage *halfFrame = cvCreateImage(cvSize(curFrame->width/2, curFrame->height/2), IPL_DEPTH_8U, 3); cvResize(curFrame, halfFrame); 
			CvMat *halfParam = cvCloneMat(capture->getParameters()); halfParam->data.db[0]/=2.0; halfParam->data.db[2]/=2.0; halfParam->data.db[4]/=2.0; halfParam->data.db[5]/=2.0;
			mt = r->performRegistration(halfFrame, halfParam, 0);
			cvReleaseImage(&halfFrame); cvReleaseMat(&halfParam);
		} else {
			//Otherwise just perform registration on the original image
			mt = r->performRegistration(curFrame, capture->getParameters(), 0);
		}
		//Call OpenGL Rendering
		render(curFrame, mt);
		//Clean up
		for (unsigned int i=0; i<mt.size(); i++) { mt.at(i).clear(); } mt.clear();
	} else {
		//If the video is paused, render the click positions
		IplImage *annotateImage = cvCloneImage(curFrame);
		for (int i=0; i<cornerCount; i++) cvCircle(annotateImage, cvPoint(corners[i].x, corners[i].y), 3, cvScalar(255,0,255), -1);
		cvShowImage("ARcitect", annotateImage);
		cvReleaseImage(&annotateImage);
	}
}

/* The mouse event handler for the main window */
void mouseEvent(int cvEvent, int x, int y, int flags, void* param) {

	//If the right button pushed, pause the video stream
	if(cvEvent==CV_EVENT_RBUTTONDOWN) paused = true;

	//If the left button pushed and the video is paused
	if(cvEvent==CV_EVENT_LBUTTONDOWN && paused == true) {
		//Store the click position
		corners[cornerCount] = cvPoint2D32f(x, y); cornerCount++;
		
		//If 4 positions have been stored
		if (cornerCount>3) {
			//The width and height is the average of the width and height of opposite corners
			int width = (abs(corners[1].x - corners[0].x) + abs(corners[2].x - corners[3].x)) /2;
			int height = (abs(corners[3].y - corners[0].y) + abs(corners[2].y - corners[1].y)) /2;

			//Set up the co-ordinates of the window
			CvPoint2D32f imageSize[4]; 
			imageSize[0] = cvPoint2D32f(0,0); imageSize[1] = cvPoint2D32f(width,0);
			imageSize[2] = cvPoint2D32f(width,height); imageSize[3] = cvPoint2D32f(0,height);

			//Calculate the perspective transform
			CvMat *perspective = cvCreateMat(3, 3, CV_32FC1);
			cvGetPerspectiveTransform(corners, imageSize, perspective);
	
			//Undistort the image
			IplImage *unDistort = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);
			cvWarpPerspective(curFrame, unDistort, perspective);
			cvReleaseMat(&perspective);

			//Load the image into OPIRA, with a max resolution of 400 to keep things running fast
			cvSaveImage("floorplan.bmp", unDistort);
			r->addResizedScaledMarker("floorplan.bmp", 400, max(unDistort->width, unDistort->height));

			//Calculate the plan
			vector<SLine> origWalls = extractPlan(unDistort,1);
			walls = reducePlanComplexity(origWalls);
			roof = getPlanBounds(walls);

			//Clean up and unpause
			cvReleaseImage(&unDistort);
			paused = false;
		}
	}

}

/* The OpenGL 3D rendering function */
void render(IplImage* frame_input, std::vector<MarkerTransform> mt)
{
	//Clear the depth buffer 
	glClearDepth( 1.0 ); glClear(GL_DEPTH_BUFFER_BIT); glDepthFunc(GL_LEQUAL);

	//Set the viewport to the window size
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    //Set the Projection Matrix to an ortho slightly larger than the window
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 1.0, -1.0);
    //Set the modelview to the identity
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();

	//Turn off Light and enable a texture
	glDisable(GL_LIGHTING);	glEnable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST);

	//Bind the background texture
	glBindTexture(GL_TEXTURE_2D, GLTextureID);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);	
	glTexImage2D(GL_TEXTURE_2D, 0, 3, frame_input->width, frame_input->height, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, frame_input->imageData);
    glColor3f(255, 255, 255);
	
	//Draw the background
    glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0.0, 0.0);	glVertex2f(0.0, 0.0);
        glTexCoord2f(1.0, 0.0);	glVertex2f(WINDOW_WIDTH, 0.0);
        glTexCoord2f(0.0, 1.0);	glVertex2f(0.0, WINDOW_HEIGHT);
        glTexCoord2f(1.0, 1.0);	glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
    glEnd();

	glEnable(GL_DEPTH_TEST);

	//Loop through all the markers found
	for (unsigned int i =0; i<mt.size(); i++) {
		double* projectionMat = mt.at(i).projMat;
		double* translationMat = mt.at(i).transMat;
		CvSize markerSize = mt.at(i).marker.size;

		//Set the Viewport Matrix
		glViewport(0,0,WINDOW_WIDTH,WINDOW_HEIGHT);

		//Load the Projection Matrix
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixd( projectionMat );

		//Load the camera modelview matrix 
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixd( translationMat );

		//Set up the wall texture
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);	
		glTexImage2D(GL_TEXTURE_2D, 0, 3, WallTextures[selectedWallTexture]->width, WallTextures[selectedWallTexture]->height, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, WallTextures[selectedWallTexture]->imageData);

		//Loop through and draw all the walls
		glBegin(GL_QUADS);
		for (int i=0; i<walls.size(); i++) {
			float width = walls.at(i).getLength()/wallHeight;
			glTexCoord2f(0,0); glVertex3f(walls.at(i).p1.x, walls.at(i).p1.y, -wallHeight);
			glTexCoord2f(width,0); glVertex3f(walls.at(i).p2.x, walls.at(i).p2.y, -wallHeight);
			glTexCoord2f(width,1); glVertex3f(walls.at(i).p2.x, walls.at(i).p2.y, 0);
			glTexCoord2f(0,1); glVertex3f(walls.at(i).p1.x, walls.at(i).p1.y, 0);
		}
		glEnd();

		//If the roof is being rendered
		if (roofMode>0) {
			//If in blend mode
			if (roofMode<3) {
				//Blend as appropriate
				glEnable(GL_BLEND);	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glColor4f(1,1,1,roofMode==2?0.7:0.5);
			}
			//Set up the roof texture
			glTexImage2D(GL_TEXTURE_2D, 0, 3, RoofTextures[selectedRoofTexture]->width, RoofTextures[selectedRoofTexture]->height, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, RoofTextures[selectedRoofTexture]->imageData);
			
			//Draw the roof
			glBegin(GL_TRIANGLES);
				float width = float(roof.width)/50.0; float height = float(roof.height)/50.0;
				float roofHeight = wallHeight + tan(roofAngle*CV_PI/180.0)*max(roof.width, roof.height);
				glTexCoord2f(0,0); glVertex3f(roof.x, roof.y, -wallHeight);
				glTexCoord2f(width,0); glVertex3f(roof.x+roof.width, roof.y, -wallHeight);
				glTexCoord2f(width/2,1); glVertex3f(roof.x+(roof.width/2.0), roof.y+(roof.height/2.0), -roofHeight);

				glTexCoord2f(0,0); glVertex3f(roof.x, roof.y+roof.height, -wallHeight);
				glTexCoord2f(width,0); glVertex3f(roof.x+roof.width, roof.y+roof.height, -wallHeight);
				glTexCoord2f(width/2,1); glVertex3f(roof.x+(roof.width/2.0), roof.y+(roof.height/2.0), -roofHeight);

				glTexCoord2f(0,0); glVertex3f(roof.x, roof.y, -wallHeight);
				glTexCoord2f(width,0); glVertex3f(roof.x, roof.y+roof.height, -wallHeight);
				glTexCoord2f(width/2,1); glVertex3f(roof.x+(roof.width/2.0), roof.y+(roof.height/2.0), -roofHeight);

				glTexCoord2f(0,0); glVertex3f(roof.x+roof.width, roof.y, -wallHeight);
				glTexCoord2f(width,0); glVertex3f(roof.x+roof.width, roof.y+roof.height, -wallHeight);
				glTexCoord2f(width/2,1); glVertex3f(roof.x+(roof.width/2.0), roof.y+(roof.height/2.0), -roofHeight);
			glEnd();
			
			//Clean up
			glDisable(GL_BLEND); 
			glColor4f(1,1,1,1);
		}


	}
	
	//Copy the OpenGL Graphics context into an IPLImage
	IplImage* outImage = cvCreateImage(cvSize(WINDOW_WIDTH,WINDOW_HEIGHT), IPL_DEPTH_8U, 3);
	glReadPixels(0,0,WINDOW_WIDTH,WINDOW_HEIGHT,GL_RGB, GL_UNSIGNED_BYTE, outImage->imageData);
	cvCvtColor( outImage, outImage, CV_BGR2RGB );
	cvFlip(outImage, outImage);

	cvShowImage("ARcitect", outImage);
	cvReleaseImage(&outImage);
}
