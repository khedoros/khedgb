#include<opencv2/core/core.hpp>
#include<opencv2/highgui/highgui.hpp>
#include<opencv2/imgproc/imgproc.hpp>
#include<iostream>
#include<string>

int main(int argc, char ** argv) {
    cv::Mat image;
    cv::Mat gray_image;
    cv::VideoCapture cap;//(0);
    //cap.open(true);
    int num=0;
    while(num<10) {
        cap>>image;
        //cv::cvtColor( image, gray_image, CV_BGR2GRAY );
        std::string filename = std::string("image-") + std::to_string(num) + ".jpg";
        //cv::imwrite( filename.c_str(), gray_image );
        cv::imwrite( filename.c_str(), image );
        num++;
        //cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );
        //cv::imshow("Webcam capture test", image);
        //cv::waitKey(0);
    }
    return 0;
}
