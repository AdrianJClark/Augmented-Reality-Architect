#ifndef PLANPROCESSOR_H
#define PLANPROCESSOR_H

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc_c.h>
#include <vector>

using namespace std;


/* Line Structure with a few useful functions */
struct SLine {
	CvPoint p1;
	CvPoint p2;
	/* Find Angle of Line */
	double getAngle() { return atan2(double(p2.y-p1.y), double(p2.x - p1.x))*(180.0/CV_PI); }
	/* Find Length of Line */
	double getLength() { double xdiff = p2.x-p1.x, ydiff = p2.y-p1.y; return pow(xdiff*xdiff + ydiff*ydiff, 0.5);}
	/* Find the distance a point is from a line */
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

/* Join two lines together, returning the largest possible combination*/
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

/* Extract a floor plan from an image */
vector<SLine> extractPlan(IplImage *image, int dilateLevel=1, double cannyThresh1 = 50, double cannyThresh2 = 150, int houghTresh = 5, double houghParam1 = 5, double houghParam2 = 10) {
	//Convert to Grayscale if required
	IplImage *newIm;
	if (image->nChannels>1) {
		newIm = cvCreateImage(cvGetSize(image), IPL_DEPTH_8U, 1);
		cvCvtColor(image, newIm, CV_RGB2GRAY);
	} else 
		newIm = cvCloneImage(image);

	//Dilate the image to remove fine lines
	if (dilateLevel>0) cvDilate(newIm, newIm, 0, dilateLevel);

	//Perform canny edge detection
	IplImage *cannyIm = cvCreateImage(cvGetSize(newIm), IPL_DEPTH_8U, 1); cvZero(cannyIm);
	cvCanny(newIm, cannyIm, cannyThresh1, cannyThresh2);

	//Perform probabilistic hough lines
	CvMemStorage* storage = cvCreateMemStorage(0); 
	CvSeq* lines = cvHoughLines2( cannyIm, storage, CV_HOUGH_PROBABILISTIC, 1, CV_PI/180, houghTresh, houghParam1, houghParam2); 

	//Copy the hough lines into our own data structure
	vector<SLine> Lines;
	for(int i = 0; i < lines->total; i++ ) {
		CvPoint* line = (CvPoint*)cvGetSeqElem(lines,i); 
		SLine l; l.p1 = line[0]; l.p2 = line[1]; 
		//Ensure all lines are going in the same direction
		if (l.getAngle()<0) { CvPoint pTmp = l.p1; l.p1 = l.p2; l.p2 = pTmp; }
		Lines.push_back(l);
	}

	cvReleaseMemStorage(&storage);
	cvReleaseImage(&cannyIm);
	cvReleaseImage(&newIm);

	return Lines;
}

/* Attempt to join line segments together to reduce the plans complexity */
vector<SLine> reducePlanComplexity(vector<SLine> walls, int angleThresh=5, int distThresh=5, bool multipass=false) {
	vector<SLine> newWall; newWall.insert(newWall.begin(), walls.begin(), walls.end());

	//Perform clean up of the image
	bool haschanged = true;
	while (haschanged) {
		haschanged = false;
		//Loop through every pair of lines
		for (int i=0; i< newWall.size(); i++) {
			for (int j=i+1; j<newWall.size(); j++) {
				
				//If (1) the lines are basically parallel and close OR (2) one line basically includes the other
				if ((abs(newWall.at(i).getAngle() - newWall.at(j).getAngle())<angleThresh || abs(newWall.at(i).getAngle() - newWall.at(j).getAngle())>180-angleThresh) && (newWall.at(i).getDist(newWall.at(j).p1)<distThresh || newWall.at(i).getDist(newWall.at(j).p2)<distThresh) ||
					(newWall.at(i).getDist(newWall.at(j).p1)<distThresh && newWall.at(i).getDist(newWall.at(j).p2)<distThresh) || (newWall.at(j).getDist(newWall.at(i).p1)<distThresh && newWall.at(j).getDist(newWall.at(i).p2)<distThresh)) {
						
						//Set the haschanged flag
						if (multipass) haschanged = true;

						//Get the union of the two lines
						SLine unionLine = joinLines(newWall.at(i), newWall.at(j));

						//Align the union to the longest of the two original lines
						if (newWall.at(i).getLength()> newWall.at(j).getLength()) {
							double length = unionLine.getLength(); double angle = newWall.at(i).getAngle();
							if (abs(angle-unionLine.getAngle())>90) angle = 180-angle;
							unionLine.p2.x = unionLine.p1.x + cos(angle*CV_PI/180.0)*length; unionLine.p2.y = unionLine.p1.y + sin(angle*CV_PI/180.0)*length;
						} else {
							double length = unionLine.getLength(); double angle = newWall.at(j).getAngle();
							if (abs(angle-unionLine.getAngle())>90) angle = 180-angle;
							unionLine.p2.x = unionLine.p1.x + cos(angle*CV_PI/180.0)*length; unionLine.p2.y = unionLine.p1.y + sin(angle*CV_PI/180.0)*length;
						}

						if (unionLine.getAngle()<0) { CvPoint pTmp = unionLine.p1; unionLine.p1 = unionLine.p2; unionLine.p2 = pTmp; }

						//Update the line list
						newWall.erase(newWall.begin()+i); newWall.erase(newWall.begin()+j-1);
						newWall.push_back(unionLine);
						if (i>0) i--; j=i+1;

				}
										

			}
		}
	}

	return newWall;
}

/* Find the bounds of the plan */
CvRect getPlanBounds(vector<SLine> walls) {
	int minX = INT_MAX, minY = INT_MAX, maxX=0, maxY=0;
	for (int i=0; i<walls.size(); i++) {
		if (walls.at(i).p1.x < minX ) minX = walls.at(i).p1.x;
		if (walls.at(i).p1.x > maxX ) maxX = walls.at(i).p1.x;
		if (walls.at(i).p2.x < minX ) minX = walls.at(i).p2.x;
		if (walls.at(i).p2.x > maxX ) maxX = walls.at(i).p2.x;

		if (walls.at(i).p1.y < minY ) minY = walls.at(i).p1.y;
		if (walls.at(i).p1.y > maxY ) maxY = walls.at(i).p1.y;
		if (walls.at(i).p2.y < minY ) minY = walls.at(i).p2.y;
		if (walls.at(i).p2.y > maxY ) maxY = walls.at(i).p2.y;
	}
	return cvRect(minX, minY, maxX-minX, maxY-minY);
}

#endif
