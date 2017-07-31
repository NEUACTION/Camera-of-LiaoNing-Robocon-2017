#include <iostream>
#include <vector>
#include <unistd.h>
#include "debug.h"
#include "videoconfig.h"
#include "camera.h"

//calculate time of every circulation
#define __TIMER_START__ timer.start()
#define __TIMER_PRINT__ do { timer.end(); std::cout << (int)(timer.getTime() * 1000) << std::endl; } while (0)

static std::vector<int> getCameraList(int size);

const int32_t EXPOSURE_TIME = 300;  //time of exposure
const int32_t WHITE_THRESOLD = 125; //threshold value of write ball
const int32_t BLACK_THRESOLD = 35;  //threshold value of black ball

const int32_t CAMERA_NUMBER = 1; //camera number

int main(int argc, char *argv[])
{
    //get camera list
	auto list = getCameraList(CAMERA_NUMBER);

    if (list.size() < CAMERA_NUMBER)
    {
        std::cout << "There are not " << CAMERA_NUMBER << " cameras." << std::endl;
        exit(-1);
    }
	
	//instantiated Camera object
    act::Camera cam0((char)list[0]);

    if (!cam0.is_open())
    {
        std::cout << "Open Camera failed!" << std::endl;
        exit(-2);
    }

	//initialization cam0
    cam0.setExposureValue(false, EXPOSURE_TIME);
    cam0.setROIRect(cv::Rect(0, 50, cam0.cols, cam0.rows - 50));
	cam0.setAutoWhiteBalance(1);

    act::Timestamp timer;
    while (1)
    {
        //__TIMER_PRINT__;
        //__TIMER_START__;

		//update frame
        cam0.update();

		std::vector<int> CCSize;
		std::vector<cv::Point> CCCore;
		cam0.findConnectedComponents(cam0.getNoBGBallImage(), CCSize, CCCore);
		cam0.areaSort(cam0.getNoBGBallImage(), CCSize, CCCore);

		//while (!CCSize.empty() && !CCCore.empty())
		//{
		//	std::cout << CCSize.back() << "  ";
		//	std::cout << CCCore.back() << "  ";

		//	CCSize.pop_back();
		//	CCCore.pop_back();
		//}
		//std::cout << std::endl;

		//show all images that have been used
		cam0.showImage();

		//if push down Esc, kill the progress
        if (cv::waitKey(10) == 27)
        {
            break;
        }
    }

    return 0;
}

//find 'size' num of camera and return sequence number of them
std::vector<int> getCameraList(int size)
{
	std::vector<int> list;

	//traversal 50 devices
	char name[15] = "/dev/video";
	int fd = 0;

	for (int i = 0; i < 50 && list.size() < 1; ++i)
	{
		sprintf(&name[10], "%d", i);

		if ((fd = open(name, O_RDWR | O_NONBLOCK, 0)) != -1)
		{
			list.push_back(i);
			close(fd);
		}
	}

	return list;
}