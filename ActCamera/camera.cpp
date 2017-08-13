#include <opencv2/opencv.hpp>
#include "camera.h"

act::Camera::Camera(char _id) : VCConfig(_id)
{
	if (fd != -1)
	{
		usbNumber = usb_info.back() - 0x30;
		videoCapture.open(_id);
		videoCapture.set(CV_CAP_PROP_FRAME_WIDTH, 320);
		videoCapture.set(CV_CAP_PROP_FRAME_HEIGHT, 240);
		videoCapture >> originalImage;

		cols = originalImage.cols;
		rows = originalImage.rows;

		ROIRect = {0, 0, cols, rows};
	}
}

//void act::Camera::showTrackbar()
//{
//	trackbarSet = new trackbar[6];
//
//	trackbarSet[0] = trackbar("minB", 255);
//	trackbarSet[1] = trackbar("maxB", 255);
//	trackbarSet[2] = trackbar("minG", 255);
//	trackbarSet[3] = trackbar("maxG", 255);
//	trackbarSet[4] = trackbar("minR", 255);
//	trackbarSet[5] = trackbar("maxR", 255);
//	
//	cv::namedWindow("TKB");
//	
//	for (auto i = 0; i < 6; i++)
//	{
//		cv::createTrackbar(trackbarSet[i].name, "TKB", &trackbarSet[i].slider, trackbarSet[i].maxValue, trackbarSet[i].callback);
//		trackbarSet[i].callback(trackbarSet[i].slider, 0);
//	}
//
//	delete[] trackbarSet;
//}

void act::Camera::findConnectedComponents(cv::Mat &binary)
{
	auto bin = binary.clone();

	std::vector<cv::Point> stk_white;
	bool farFlag = 0;
	unsigned int counter = 0;
	unsigned long long coreX = 0;
	unsigned long long coreY = 0;

	for (auto i = 0; i < bin.rows; ++i)
	{
		for (auto j = 0; j < bin.cols; ++j)
		{
			//when find a point of ball, push it into stack stk_white
			if (bin.ptr(i)[j] == 255)
			{
				bin.ptr(i)[j] = 150;
				stk_white.push_back({j, i});
			}

			//if find a white point connected, which means there are element in stk_white
			while (!stk_white.empty())
			{
				auto pix = stk_white.back();
				
				//if this connected component is too away from comera, get rid of it
				if (pix.y <= 5)
					farFlag = 1;

				stk_white.pop_back();
				counter++;
				coreX += pix.x;
				coreY += pix.y;

				auto row_0 = pix.y - 1, row_1 = pix.y, row_2 = pix.y + 1;
				auto col_0 = pix.x - 1, col_1 = pix.x, col_2 = pix.x + 1;

				//pay attention to the usage of "do {} while(0)"
#define __pass__(x, y)  do { bin.ptr(y)[x] = 150; stk_white.push_back({x, y}); } while (0)
				//row_0
				if (row_0 >= 0 && col_0 >= 0 && bin.ptr(row_0)[col_0] == 255)
					__pass__(col_0, row_0);
				if (row_0 >= 0 && bin.ptr(row_0)[col_1] == 255)
					__pass__(col_1, row_0);
				if (row_0 >= 0 && col_2 < bin.cols && bin.ptr(row_0)[col_2] == 255)
					__pass__(col_2, row_0);
				//row_1
				if (col_0 >= 0 && bin.ptr(row_1)[col_0] == 255)
					__pass__(col_0, row_1);
				if (col_2 < bin.cols && bin.ptr(row_1)[col_1] == 255)
					__pass__(col_1, row_1);
				//row_2
				if (row_2 < bin.rows && col_0 >= 0 && bin.ptr(row_2)[col_0] == 255)
					__pass__(col_0, row_2);
				if (row_2 < bin.rows && bin.ptr(row_2)[col_1] == 255)
					__pass__(col_1, row_2);
				if (row_2 < bin.rows && col_2 < bin.cols && bin.ptr(row_2)[col_2] == 255)
					__pass__(col_2, row_2);
#undef __pass__
			}
			//push_back the core of connected components and the number of white point
			if (counter > 20)
			{
				coreX /= counter;
				coreY /= counter;
				CCCore.push_back(cv::Point((int)coreX, (int)coreY));

				//if pix number size up or CC is too far, push pix num into CCSize, else pop x and y out from CCCore
				if ((counter < 0.6f * STD_PIXS || counter > 3.0f * STD_PIXS) || farFlag == 1)
				{
					CCCore.pop_back();				
				}
				else
					CCSize.push_back(counter);	
			}
			coreX = 0;
			coreY = 0;

			farFlag = 0;

			counter = 0;
		}
	}
	////test reference pix num of specific y
	//while (!CCSize.empty() && !CCCore.empty())
	//{
	//	std::cout << CCSize.back() << "  ";
	//	std::cout << CCCore.back() << "  ";

	//	CCSize.pop_back();
	//	CCCore.pop_back();
	//}
	//std::cout << std::endl;
}

void act::Camera::getImage()
{
	auto col_val = new min_max[originalImage(ROIRect).rows];
	auto row_val = new min_max[originalImage(ROIRect).cols];

	//get basic image for later handle
	basicImage = originalImage(ROIRect).clone();
	cv::cvtColor(basicImage, basicImage, CV_BGR2HSV_FULL);

	//get the contours value of the green field
	noBackgroundImage = originalImage(ROIRect).clone();
	cv::cvtColor(noBackgroundImage, noBackgroundImage, CV_BGR2HSV_FULL);

	//get all objects that are black and white
	allBallImage = cv::Mat::zeros(basicImage.rows, basicImage.cols, CV_8UC1);
	for (auto i = 0; i < basicImage.rows; i++)
	{
		for (auto j = 0; j < basicImage.cols; j++)
		{
			auto pix = basicImage.ptr<cv::Vec3b>(i)[j];

			//white golf ball & black golf ball
			if ((pix[1] < 70 && pix[2] > 185) || pix[2] < 90)
				*allBallImage.ptr<uchar>(i, j) = 255;
			else
				*allBallImage.ptr<uchar>(i, j) = 0;
		}
	}

	//get all objects that are green
	allGreenImage = cv::Mat::zeros(basicImage.rows, basicImage.cols, CV_8UC1);
	for (auto i = 0; i < basicImage.rows; i++)
	{
		for (auto j = 0; j < basicImage.cols; j++)
		{
			auto pix = basicImage.ptr<cv::Vec3b>(i)[j];

			//green field and orange border, red/blue(storeroom), fix me
			if ((pix[0] > 60 && pix[0] < 130) || (pix[0] < 30 && pix[1] > 100 && pix[1] < 200 && pix[2] > 100))
				*allGreenImage.ptr<uchar>(i, j) = 255;
			else
				*allGreenImage.ptr<uchar>(i, j) = 0;
		}
	}

	//findContours
	size_t maxCtsSize = 0;
	int maxCtsNumber = 0;

	fieldCtsImage = allGreenImage.clone();
	cv::findContours(fieldCtsImage, fieldContours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
	for (size_t i = 0; i < fieldContours.size(); i++)
	{
		if (fieldContours[i].size() > maxCtsSize)
		{
			maxCtsSize = fieldContours[i].size();
			maxCtsNumber = i;
		}
	}

	int CtsCount = fieldContours[maxCtsNumber].size();
	cv::Point point0Cts = fieldContours[maxCtsNumber][CtsCount - 1];
	for (int i = 0; i < CtsCount; i++)
	{
		cv::Point pointCts = fieldContours[maxCtsNumber][i];
		cv::line(fieldCtsImage, point0Cts, pointCts, cv::Scalar(255));
		point0Cts = pointCts;
	}

	//get convex hull of the field
	fieldCHImage = cv::Mat::zeros(basicImage.rows, basicImage.cols, CV_8UC1);
	if (CtsCount > 0)
	{
		std::vector<cv::Point> hull;
		convexHull(fieldContours[maxCtsNumber], hull);

		int hullCount = hull.size();
		cv::Point point0CC = hull[hullCount - 1];
		for (int i = 0; i < hullCount; i++)
		{
			cv::Point pointCC = hull[i];
			cv::line(fieldCHImage, point0CC, pointCC, cv::Scalar(255));
			point0CC = pointCC;
		}
	}

	//delete the part outside the convex hull of allBallImage
	noBGBallImage = allBallImage.clone();
	for (auto i = 0; i < fieldCHImage.rows; i++)
		for (auto j = 0; j < fieldCHImage.cols; j++)
		{
			*noBGBallImage.ptr<uchar>(i, j) = 0;
			if (*fieldCHImage.ptr<uchar>(i, j) == 255)
				break;
		}
	for (auto i = 0; i < fieldCHImage.rows; i++)
		for (auto j = fieldCHImage.cols - 1; j >= 0; j--)
		{
			*noBGBallImage.ptr<uchar>(i, j) = 0;
			if (*fieldCHImage.ptr<uchar>(i, j) == 255)
				break;
		}
	for (auto j = 0; j < fieldCHImage.cols; j++)
		for (auto i = 0; i < fieldCHImage.rows; i++)
		{
			*noBGBallImage.ptr<uchar>(i, j) = 0;
			if (*fieldCHImage.ptr<uchar>(i, j) == 255)
				break;
		}
	for (auto j = 0; j < fieldCHImage.cols; j++)
		for (auto i = fieldCHImage.rows - 1; i >= 0; i--)
		{
			*noBGBallImage.ptr<uchar>(i, j) = 0;
			if (*fieldCHImage.ptr<uchar>(i, j) == 255)
				break;
		}
	
	//find connected components and get rid of oversize and undersize parts in noBGBallImage
	findConnectedComponents(noBGBallImage);

	//image processing of allBallImage
	cv::Mat element = getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(1, 1));
	cv::morphologyEx(noBGBallImage, noBGBallImage, CV_MOP_CLOSE, element);

	delete[] col_val;
	delete[] row_val;
}

void act::Camera::areaSort(cv::Mat ballImage)
{
	//modify with macro ROWS_CUTS
	cv::line(ballImage, cv::Point(128, 0), cv::Point(0  , 90), cv::Scalar(255));
	cv::line(ballImage, cv::Point(193, 0), cv::Point(320, 98), cv::Scalar(255));

	int areaLNum = 0, areaMNum = 0, areaRNum = 0, incNum = 0;
	int targetArea = 0;

	ballPositionImage = cv::Mat::zeros(basicImage.rows, basicImage.cols, CV_8UC1);

	while (!CCSize.empty() && !CCCore.empty())
	{
		//modify with macro ROWS_CUTS
		double borderXLeft = 128.44f - 1.43f * (float)CCCore.back().y;
		double borderXRight = 192.76f + 1.3f * (float)CCCore.back().y;

		//judge the number of golf ball in this connected component
		if ((float)CCSize.back() >= 0.6f * STD_PIXS && (float)CCSize.back() <= 1.5f * STD_PIXS)
			incNum = 1;
		else if ((float)CCSize.back() > 1.5f * STD_PIXS && (float)CCSize.back() <= 2.25f * STD_PIXS)
			incNum = 2;
		else if ((float)CCSize.back() > 2.25f * STD_PIXS)
			incNum = 3;
		else
			incNum = 0;
		
		if ((float)CCCore.back().x < borderXLeft)
			areaLNum += incNum;
		else if ((float)CCCore.back().x > borderXRight)
			areaRNum += incNum;
		else
			areaMNum += incNum;

		incNum = 0;

		cv::circle(ballPositionImage, CCCore.back(), 5, 255, 1);

		CCSize.pop_back();
		CCCore.pop_back();
	}

	//need better judging condition, fix me
	if (areaLNum >= areaRNum && areaLNum > areaMNum)
		targetArea = 1;
	else if (areaMNum >= areaRNum && areaMNum >= areaLNum)
		targetArea = 2;
	else if (areaRNum >= areaLNum && areaRNum > areaMNum)
		targetArea = 3;
	else
		targetArea = 2;

	serialPrintf(fd, "%d", targetArea);
	std::cout << areaLNum << "   " << areaMNum << "   " << areaRNum << "   Target Area:" << targetArea << std::endl;

	areaLNum = 0;
	areaMNum = 0;
	areaRNum = 0;
}