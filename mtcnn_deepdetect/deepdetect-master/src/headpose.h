//deepdetect
#include <glog/logging.h>
// c++
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
// opencv
#include <opencv2/opencv.hpp>
//headpose
#include <headpose/LandmarkCoreIncludes.h>


void create_display_image(const cv::Mat& orig, cv::Mat& display_image, LandmarkDetector::CLNF& clnf_model)
{
	
	// Draw head pose if present and draw eye gaze as well

	// preparing the visualisation image
	/*display_image = orig.clone();		

	// Creating a display image			
	cv::Mat xs = clnf_model.detected_landmarks(cv::Rect(0, 0, 1, clnf_model.detected_landmarks.rows/2));
	cv::Mat ys = clnf_model.detected_landmarks(cv::Rect(0, clnf_model.detected_landmarks.rows/2, 1, clnf_model.detected_landmarks.rows/2));
	double min_x, max_x, min_y, max_y;

	cv::minMaxLoc(xs, &min_x, &max_x);
	cv::minMaxLoc(ys, &min_y, &max_y);

	double width = max_x - min_x;
	double height = max_y - min_y;

	int minCropX = max((int)(min_x-width/3.0),0);
	int minCropY = max((int)(min_y-height/3.0),0);

	int widthCrop = min((int)(width*5.0/3.0), display_image.cols - minCropX - 1);
	int heightCrop = min((int)(height*5.0/3.0), display_image.rows - minCropY - 1);

	double scaling = 350.0/widthCrop;
	
	// first crop the image
	display_image = display_image(cv::Rect((int)(minCropX), (int)(minCropY), (int)(widthCrop), (int)(heightCrop)));
		
	// now scale it
	cv::resize(display_image.clone(), display_image, cv::Size(), scaling, scaling);

	// Make the adjustments to points
	xs = (xs - minCropX)*scaling;
	ys = (ys - minCropY)*scaling;

	cv::Mat shape = clnf_model.detected_landmarks.clone();

	xs.copyTo(shape(cv::Rect(0, 0, 1, clnf_model.detected_landmarks.rows/2)));
	ys.copyTo(shape(cv::Rect(0, clnf_model.detected_landmarks.rows/2, 1, clnf_model.detected_landmarks.rows/2)));

	// Do the shifting for the hierarchical models as well
	for (size_t part = 0; part < clnf_model.hierarchical_models.size(); ++part)
	{
		cv::Mat xs = clnf_model.hierarchical_models[part].detected_landmarks(cv::Rect(0, 0, 1, clnf_model.hierarchical_models[part].detected_landmarks.rows / 2));
		cv::Mat ys = clnf_model.hierarchical_models[part].detected_landmarks(cv::Rect(0, clnf_model.hierarchical_models[part].detected_landmarks.rows / 2, 1, clnf_model.hierarchical_models[part].detected_landmarks.rows / 2));

		xs = (xs - minCropX)*scaling;
		ys = (ys - minCropY)*scaling;

		cv::Mat shape = clnf_model.hierarchical_models[part].detected_landmarks.clone();

		xs.copyTo(shape(cv::Rect(0, 0, 1, clnf_model.hierarchical_models[part].detected_landmarks.rows / 2)));
		ys.copyTo(shape(cv::Rect(0, clnf_model.hierarchical_models[part].detected_landmarks.rows / 2, 1, clnf_model.hierarchical_models[part].detected_landmarks.rows / 2)));

	}

	LandmarkDetector::Draw(display_image, clnf_model);*/
						
}


class HeadPose{
	public:
		int getHeadPose(const cv::Mat img, cv::Vec3f &pose);
		int init(std::string model_path); //loadModel
	
	private:
		void convert_to_grayscale(const cv::Mat& in, cv::Mat& out);
		
	public:
		//LandmarkDetector::CLNF clnf_model("");

};


int HeadPose::getHeadPose(const cv::Mat img, cv::Vec3f &pose)
{
	if (img.empty())
	{
		LOG(INFO)<< "Could not get the input image(getHeadPose)";
		return 1;
	}
	
	std::string model_path = "/root/zhangjing/deepdetect/models/headpose/model/main_clnf_general.txt";
	vector<std::string> arguments;// = get_arguments(argc, argv);
	arguments.push_back("-wild");
	
	LOG(INFO)<<"Loading the model 11111111111111111";
	LandmarkDetector::FaceModelParameters det_parameters(arguments);
	//cout<<det_parameters.sigma<<", 3333333333";

	// No need to validate detections, as we're not doing tracking
	//det_parameters.validate_detections = false;	
	LOG(INFO)<<"Loading the model";
	//LandmarkDetector::CLNF clnf_model();//model_path);//"/opt/zhangjing/OpenFace-master/build/bin/model/main_clnf_general.txt");
	LOG(INFO)<<"Model loaded";
			
	/*float fx = 0, fy = 0, cx = 0, cy = 0;
	int device = -1;
	cx = img.cols / 2.0f;
	cy = img.rows / 2.0f;
	// Use a rough guess-timate of focal length
	fx = 500 * (img.cols / 640.0);
	fy = 500 * (img.rows / 480.0);
	fx = (fx + fy) / 2.0;
	fy = fx;
	
	// Detect faces in an image
	vector<cv::Rect_<double> > face_detections;
	cv::Rect_<double> roi;
	roi = cv::Rect_<double>(0,0,img.cols-1,img.rows-1);//(17,42,76,75);
	face_detections.push_back( roi );
	
	// perform landmark detection for every face detected
	for(size_t face=0; face < face_detections.size(); ++face)
	{
		cv::Mat crop_img(img, face_detections[face]);//(cv::Range(0,grayscale_image.cols-1),cv::Range(0,grayscale_image.rows-1));
		
		// Making sure the image is in uchar grayscale
		cv::Mat_<uchar> grayscale_image;
		convert_to_grayscale(crop_img, grayscale_image);
		
		//cout<<"col = "<<crop_img.cols<<", row="<<crop_img.rows<<endl;
		cv::imwrite("/root/zhangjing/deepdetect/deepdetect-master/build/main/crop_pose.bmp", crop_img);
			
		// if there are multiple detections go through them
		bool success = LandmarkDetector::DetectLandmarksInImage(grayscale_image, face_detections[face], clnf_model, det_parameters);

		// Estimate head pose and eye gaze				
		//cout <<"fx="<<fx<<",fy="<<fy<<",cx="<<cx<<",cy="<<cy<< endl;
		cv::Vec6d headPose = LandmarkDetector::GetPose(clnf_model, fx, fy, cx, cy); //no 
		//cout <<"fx="<<fx<<",fy="<<fy<<",cx="<<cx<<",cy="<<cy<< endl;
		cout <<"headPose"<< headPose[3] << " " << headPose[4] << " " << headPose[5] << endl;
		pose = cv::Vec3f(headPose[3],headPose[4],headPose[5]);
		
		if (success && 1)
		{
			// displaying detected landmarks
			cv::Mat display_image;
			//create_display_image(img, display_image, clnf_model);
			bool write_success = cv::imwrite("/root/zhangjing/deepdetect/deepdetect-master/build/main/face_landmark.jpg",display_image);
		}
	}
	*/
	return 0;
}



int HeadPose::init(std::string model_path)
{
	/*vector<string> arguments;// = get_arguments(argc, argv);
	arguments.push_back("-wild");
	LandmarkDetector::FaceModelParameters det_parameters1(arguments);
	// No need to validate detections, as we're not doing tracking
	//det_parameters1.validate_detections = false;
	//det_parameters(det_parameters1);
	
	LOG(INFO)<<"Loading the model";
	LandmarkDetector::CLNF model(model_path);//"/opt/zhangjing/OpenFace-master/build/bin/model/main_clnf_general.txt");
	clnf_model = model;
	LOG(INFO)<<"Model loaded";
	*/
	return 0;
}

void HeadPose::convert_to_grayscale(const cv::Mat& in, cv::Mat& out)
{
	if(in.channels() == 3)
	{
		// Make sure it's in a correct format
		if(in.depth() != CV_8U)
		{
			if(in.depth() == CV_16U)
			{
				cv::Mat tmp = in / 256;
				tmp.convertTo(tmp, CV_8U);
				cv::cvtColor(tmp, out, CV_BGR2GRAY);
			}
		}
		else
		{
			cv::cvtColor(in, out, CV_BGR2GRAY);
		}
	}
	else if(in.channels() == 4)
	{
		cv::cvtColor(in, out, CV_BGRA2GRAY);
	}
	else
	{
		if(in.depth() == CV_16U)
		{
			cv::Mat tmp = in / 256;
			out = tmp.clone();
		}
		else if(in.depth() != CV_8U)
		{
			in.convertTo(out, CV_8U);
		}
		else
		{
			out = in.clone();
		}
	}
}

