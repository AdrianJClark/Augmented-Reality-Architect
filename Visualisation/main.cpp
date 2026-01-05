#include "stdlib.h"
#include "cv.h"
#include "highgui.h"
#include <vector>

using namespace std;

struct SLine {
	CvPoint p1;
	CvPoint p2;
	double getAngle() { return atan2(double(p2.y-p1.y), double(p2.x - p1.x))*(180.0/CV_PI); }
	double getLength() { double xdiff = p2.x-p1.x, ydiff = p2.y-p1.y; return pow(xdiff*xdiff + ydiff*ydiff, 0.5);}
	double getDist(CvPoint p) {
		float A = p.x - p1.x, B = p.y - p1.y;
		float C = p2.x - p1.x, D = p2.y - p1.y;

		float dot = A * C + B * D;
		float len_sq = C * C + D * D;
		float param = dot / len_sq;

		CvPoint closestPoint;
		if(param < 0) closestPoint = p1;
		else if(param > 1) closestPoint = p2;
		else {
			closestPoint.x = p1.x + param * C;
			closestPoint.y = p1.y + param * D;
		}

		return abs(pow(double((p.x-closestPoint.x)*(p.x-closestPoint.x)+(p.y-closestPoint.y)*(p.y-closestPoint.y)),0.5));
	}
};

SLine joinLines(SLine l1, SLine l2);

void main(int argc, char**argv) {
	//Load in the source images
	IplImage *newIm = cvLoadImage("hard.png",0);
	cvShowImage("map", newIm);

	//Dilate the image to remove fine lines
	cvDilate(newIm, newIm, 0, 2);

	//Perform canny edge detection
	IplImage *cannyIm = cvCreateImage(cvGetSize(newIm), IPL_DEPTH_8U, 1); cvZero(cannyIm);
	cvCanny(newIm, cannyIm, 50, 150);
	cvShowImage("canny", cannyIm);

	//Perform probabilistic hough lines
	CvMemStorage* storage = cvCreateMemStorage(0); 
	CvSeq* lines = cvHoughLines2( cannyIm, storage, CV_HOUGH_PROBABILISTIC, 1, CV_PI/180, 5, 5, 10 ); 
	
	//Copy the hough lines into our own data structure
	vector<SLine> Lines;
	for(int i = 0; i < lines->total; i++ ) {
		CvPoint* line = (CvPoint*)cvGetSeqElem(lines,i); 
		SLine l; l.p1 = line[0]; l.p2 = line[1]; 
		//Ensure all lines are going in the same direction
		if (l.getAngle()<0) { CvPoint pTmp = l.p1; l.p1 = l.p2; l.p2 = pTmp; }
		Lines.push_back(l);
	}

	//Render the original image
	IplImage *origIm = cvCreateImage(cvGetSize(newIm), IPL_DEPTH_8U, 3); cvSet(origIm, cvScalarAll(255));
	for(int i = 0; i < Lines.size(); i++ ) {
		cvLine( origIm, Lines.at(i).p1, Lines.at(i).p2, CV_RGB(0,0,0), 2, 8 ); 
		cvCircle(origIm, Lines.at(i).p1, 2, cvScalar(0,0,255));
		cvCircle(origIm, Lines.at(i).p2, 2, cvScalar(0,128,0));
	}
	cvShowImage("orig", origIm);

	printf("Before %d\n", Lines.size());

	//Perform clean up of the image
	IplImage *cleanupImg = cvCreateImage(cvGetSize(newIm), IPL_DEPTH_8U, 3); cvSet(cleanupImg, cvScalarAll(255));
	bool haschanged = true;
	while (haschanged) {
		haschanged = false;
		//Loop through every pair of lines
		for (int i=0; i< Lines.size(); i++) {
			for (int j=i+1; j<Lines.size(); j++) {
				
				float angleThresh = 5; float distThresh = 5;
				//If (1) the lines are basically parallel and close OR (2) one line basically includes the other
				if ((abs(Lines.at(i).getAngle() - Lines.at(j).getAngle())<angleThresh || abs(Lines.at(i).getAngle() - Lines.at(j).getAngle())>180-angleThresh) && (Lines.at(i).getDist(Lines.at(j).p1)<distThresh || Lines.at(i).getDist(Lines.at(j).p2)<distThresh) ||
					(Lines.at(i).getDist(Lines.at(j).p1)<distThresh && Lines.at(i).getDist(Lines.at(j).p2)<distThresh) || (Lines.at(j).getDist(Lines.at(i).p1)<distThresh && Lines.at(j).getDist(Lines.at(i).p2)<distThresh)) {
						
						//Set the haschanged flag
						haschanged = true;

						//Get the union of the two lines
						SLine unionLine = joinLines(Lines.at(i), Lines.at(j));

						//Align the union to the longest of the two original lines
						if (Lines.at(i).getLength()> Lines.at(j).getLength()) {
							double length = unionLine.getLength(); double angle = Lines.at(i).getAngle();
							if (abs(angle-unionLine.getAngle())>90) angle = 180-angle;
							unionLine.p2.x = unionLine.p1.x + cos(angle*CV_PI/180.0)*length; unionLine.p2.y = unionLine.p1.y + sin(angle*CV_PI/180.0)*length;
						} else {
							double length = unionLine.getLength(); double angle = Lines.at(j).getAngle();
							if (abs(angle-unionLine.getAngle())>90) angle = 180-angle;
							unionLine.p2.x = unionLine.p1.x + cos(angle*CV_PI/180.0)*length; unionLine.p2.y = unionLine.p1.y + sin(angle*CV_PI/180.0)*length;
						}
						if (unionLine.getAngle()<0) { CvPoint pTmp = unionLine.p1; unionLine.p1 = unionLine.p2; unionLine.p2 = pTmp; }

						//Draw the changed lines
						cvLine( cleanupImg, unionLine.p1, unionLine.p2, CV_RGB(255,0,255), 3, 8 );
						cvLine( cleanupImg, Lines.at(i).p1, Lines.at(i).p2, CV_RGB(0,128,0), 2, 8 ); 
						cvLine( cleanupImg, Lines.at(j).p1, Lines.at(j).p2, CV_RGB(0,0,255), 2, 8 ); 
						cvShowImage("cleanup", cleanupImg); cvWaitKey(1); 

						//Update the line list
						Lines.erase(Lines.begin()+i); Lines.erase(Lines.begin()+j-1);
						Lines.push_back(unionLine);
						if (i>0) i--; j=i+1;

						//Redraw the map
						cvSet(cleanupImg, cvScalarAll(255));
						for(int x = 0; x < Lines.size(); x++ ) cvLine( cleanupImg, Lines.at(x).p1, Lines.at(x).p2, CV_RGB(0,0,0), 2, 8 ); 
					
				}
										

			}
		}
	}

	printf("After %d\n", Lines.size());

	cvShowImage("cleanup", cleanupImg);
	cvWaitKey(0);

	cvReleaseImage(&cleanupImg);
	cvReleaseImage(&newIm);
	cvReleaseImage(&cannyIm);
	exit(0);
}

SLine joinLines(SLine l1, SLine l2) {
	SLine l[6]; l[0] = l1; l[1] = l2;
	l[2].p1 = l1.p1; l[2].p2 = l2.p1; 
	l[3].p1 = l1.p1; l[3].p2 = l2.p2; 
	l[4].p1 = l1.p2; l[4].p2 = l2.p1; 
	l[5].p1 = l1.p2; l[5].p2 = l2.p2; 

	double maxLength = l[0].getLength(); int maxIndex = 0;
	for (int i=1; i<6; i++) 
		if (l[i].getLength() > maxLength) {maxLength = l[i].getLength(); maxIndex = i;}

	if (l[maxIndex].getAngle()<0) { CvPoint pTmp = l[maxIndex].p1; l[maxIndex].p1 = l[maxIndex].p2; l[maxIndex].p2 = pTmp; }
	return l[maxIndex];
}