#ifndef TEXTUREMANAGER_H
#define TEXTUREMANAGER_H

using namespace std;



//Variables and event handlers for wall textures
int selectedWallTexture=0;
vector<IplImage*> WallTextures;
void renderWallWindow(int x=-1, int y=-1);
void wallWindowEvent(int cvEvent, int x, int y, int flags, void* param);

//Variables and event handlers for roof textures
int selectedRoofTexture=0;
vector<IplImage*> RoofTextures;
void renderRoofWindow(int x=-1, int y=-1);
void roofWindowEvent(int cvEvent, int x, int y, int flags, void* param);


/* Initialize all the textures for walls and roofs */
void InitTextures() {
	WallTextures.push_back(cvLoadImage("Textures/Wall/brick_wall_bump_texture_map.jpg"));
	WallTextures.push_back(cvLoadImage("Textures/Wall/dark_brown_brick_texture_tileable.jpg"));
	WallTextures.push_back(cvLoadImage("Textures/Wall/seamless_stone_wall_texture.jpg"));
	WallTextures.push_back(cvLoadImage("Textures/Wall/tileable_wood_texture_by_ftIsis_Stock.jpg"));

	cvNamedWindow("Wall Textures");
	cvSetMouseCallback("Wall Textures", wallWindowEvent);
	renderWallWindow();

	RoofTextures.push_back(cvLoadImage("Textures/Roof/22114.jpg"));
	RoofTextures.push_back(cvLoadImage("Textures/Roof/22115.jpg"));
	RoofTextures.push_back(cvLoadImage("Textures/Roof/1223678399.jpg"));
	RoofTextures.push_back(cvLoadImage("Textures/Roof/4463498148_0691022f56.jpg"));

	cvNamedWindow("Roof Textures");
	cvSetMouseCallback("Roof Textures", roofWindowEvent);
	renderRoofWindow();

}

/* Clean up the Textures */
void CleanupTextures() {
	for (int i=0; i<WallTextures.size(); i++) cvReleaseImage(&WallTextures.at(i));
	for (int i=0; i<RoofTextures.size(); i++) cvReleaseImage(&RoofTextures.at(i));
}

/* Handle Mouse Move and Mouse Click events on the Wall Texture Windows */
void wallWindowEvent(int cvEvent, int x, int y, int flags, void* param) {
	if (cvEvent == CV_EVENT_MOUSEMOVE) {
		renderWallWindow(x,y);
	} else if (cvEvent == CV_EVENT_LBUTTONDOWN) {
		selectedWallTexture = y/150; renderWallWindow();
	}
}

/* Render the Wall Texture Window */
void renderWallWindow(int x, int y) {
	IplImage *wallTex = cvCreateImage(cvSize(150, 150*WallTextures.size()), IPL_DEPTH_8U, 3); cvSet(wallTex, cvScalarAll(215));

	if (x>-1 && y >-1) {
		int hoverTex = y/150;
		cvRectangle(wallTex, cvPoint(0, hoverTex*150), cvPoint(150, (hoverTex+1)*150), cvScalar(255, 128, 128), -1);
	}

	cvRectangle(wallTex, cvPoint(0, selectedWallTexture*150), cvPoint(150, (selectedWallTexture+1)*150), cvScalar(255), -1);


	for (int i=0; i<WallTextures.size(); i++) {
		cvSetImageROI(wallTex, cvRect(25, i*150+25, 100, 100));
		cvResize(WallTextures.at(i), wallTex);
	}
	cvResetImageROI(wallTex);
	cvShowImage("Wall Textures", wallTex); cvWaitKey(1);
	cvReleaseImage(&wallTex);
}


/* Handle Mouse Move and Mouse Click events on the Roof Texture Windows */
void roofWindowEvent(int cvEvent, int x, int y, int flags, void* param) {
	if (cvEvent == CV_EVENT_MOUSEMOVE) {
		renderRoofWindow(x,y);
	} else if (cvEvent == CV_EVENT_LBUTTONDOWN) {
		selectedRoofTexture = y/150; renderRoofWindow();
	}
}

/* Render the Roof Texture Window */
void renderRoofWindow(int x, int y) {
	IplImage *roofTex = cvCreateImage(cvSize(150, 150*RoofTextures.size()), IPL_DEPTH_8U, 3); cvSet(roofTex, cvScalarAll(215));

	if (x>-1 && y >-1) {
		int hoverTex = y/150;
		cvRectangle(roofTex, cvPoint(0, hoverTex*150), cvPoint(150, (hoverTex+1)*150), cvScalar(255, 128, 128), -1);
	}

	cvRectangle(roofTex, cvPoint(0, selectedRoofTexture*150), cvPoint(150, (selectedRoofTexture+1)*150), cvScalar(255), -1);


	for (int i=0; i<RoofTextures.size(); i++) {
		cvSetImageROI(roofTex, cvRect(25, i*150+25, 100, 100));
		cvResize(RoofTextures.at(i), roofTex);
	}
	cvResetImageROI(roofTex);
	cvShowImage("Roof Textures", roofTex); cvWaitKey(1);
	cvReleaseImage(&roofTex);
}
#endif
