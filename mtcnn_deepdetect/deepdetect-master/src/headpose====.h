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


class HeadPose{
	public:
		int getHeadPose(const cv::Mat img, cv::Vec3d &pose);
		int init(); //loadModel
	private:
		
		int getLandmark(const cv::Mat img, cv::Mat &landMark);
		int getPose(const cv::Mat landMark, cv::Vec3d &pose);
	
	private:
		void convert_to_grayscale(const cv::Mat& in, cv::Mat& out);
	
	private:
		float fx, fy, cx, cy;
		bool multi_view; 
		std::vector<int> window_sizes_init;
		// Local parameters describing the non-rigid shape
		cv::Mat_<double> params_local;
		// The linear 3D Point Distribution Model
		PDM	pdm;
		CLNF clnf_model;
};


int HeadPose::getHeadPose(const cv::Mat img, cv::Vec3d &pose)
{
	if (img.empty())
	{
		LOG(INFO)<< "Could not get the input image(getHeadPose)";
		return 1;
	}
	// Making sure the image is in uchar grayscale
	cv::Mat_<uchar> grayscale_image;
	convert_to_grayscale(read_image, grayscale_image);
	
	cx = grayscale_image.cols / 2.0f;
	cy = grayscale_image.rows / 2.0f;
	// Use a rough guess-timate of focal length
	fx = 500 * (grayscale_image.cols / 640.0);
	fy = 500 * (grayscale_image.rows / 480.0);
	fx = (fx + fy) / 2.0;
	fy = fx;
	
	cv::Mat landMark;
	getLandmark(grayscale_image, landMark);
	
			
	return 0;
}

int HeadPose::getLandmark(const cv::Mat img, cv::Mat &landMark)
{
	// Can have multiple hypotheses
	vector<cv::Vec3d> rotation_hypotheses;
	if(multi_view)
	{
		// Try out different orientation initialisations
		// It is possible to add other orientation hypotheses easilly by just pushing to this vector
		rotation_hypotheses.push_back(cv::Vec3d(0,0,0));
		rotation_hypotheses.push_back(cv::Vec3d(0,0.5236,0));
		rotation_hypotheses.push_back(cv::Vec3d(0,-0.5236,0));
		rotation_hypotheses.push_back(cv::Vec3d(0.5236,0,0));
		rotation_hypotheses.push_back(cv::Vec3d(-0.5236,0,0));
		
	}
	else
	{
		// Assume the face is close to frontal
		rotation_hypotheses.push_back(cv::Vec3d(0,0,0));
	}
	
	// Store the current best estimate
	double best_likelihood;
	cv::Vec6d best_global_parameters;
	cv::Mat_<double> best_local_parameters;
	cv::Mat_<double> best_detected_landmarks;
	cv::Mat_<double> best_landmark_likelihoods;
	bool best_success;
	
	int hierarchical_models_size = 3; //clnf_model.hierarchical_models.size()
	vector<double> best_likelihood_h( hierarchical_models_size );
	vector<cv::Vec6d> best_global_parameters_h( hierarchical_models_size );
	vector<cv::Mat_<double>> best_local_parameters_h( hierarchical_models_size );
	vector<cv::Mat_<double>> best_detected_landmarks_h( hierarchical_models_size );
	vector<cv::Mat_<double>> best_landmark_likelihoods_h( hierarchical_models_size );
	
	for(size_t hypothesis = 0; hypothesis < rotation_hypotheses.size(); ++hypothesis)
	{
		
	}
	
	return 0;
}

int HeadPose::getPose(const cv::Mat landMark, cv::Vec3d &pose)
{
	
	return 0;
}

int HeadPose::init()
{
	fx = 0, fy = 0, cx = 0, cy = 0;
	multi_view = false; //input
	
	// Just for initialisation
	window_sizes_init.at(0) = 11;
	window_sizes_init.at(1) = 9;
	window_sizes_init.at(2) = 7;
	window_sizes_init.at(3) = 5;
	bool wild = true; //input
	if (wild)
	{
		window_sizes_init[0] = 15; 
		window_sizes_init[1] = 13; 
		window_sizes_init[2] = 11; 
		window_sizes_init[3] = 9;
	}
	
	std::string  s = "/root/zhangjing/deepdetect/deepdetect-master/model/clnf_general.txt ";
	clnf_model.Read( s );
	// local parameters (shape)
	params_local.create(pdm.NumberOfModes(), 1);
	LOG(INFO)<<pdm.NumberOfModes()<<", pdm.NumberOfModes() 333333333333333333";
	params_local.setTo(0.0);
	
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

class CLNF{
	public:
		void Read();
		void Read_CLNF(string clnf_location);
	public:
		PDM pdm;
};

void CLNF::Read()
{
	std::string  s = "/opt/zhangjing/OpenFace-master/build/bin/model/clnf_general.txt ";
	Read_CLNF( s );
	

}

void CLNF::Read_CLNF(string clnf_location)
{
	ifstream locations(clnf_location.c_str(), ios_base::in);

	if(!locations.is_open())
	{
		LOG(INFO) << "Couldn't open the CLNF model file aborting" << endl;
		//cout.flush();
		return;
	}

	string line;	
	vector<string> intensity_expert_locations;
	vector<string> ccnf_expert_locations;

	// The other module locations should be defined as relative paths from the main model
	boost::filesystem::path root = boost::filesystem::path(clnf_location).parent_path();
	// The main file contains the references to other files
	while (!locations.eof())
	{ 
		getline(locations, line);
		stringstream lineStream(line);

		string module;
		string location;

		// figure out which module is to be read from which file
		lineStream >> module;		
		getline(lineStream, location);
		
		if(location.size() > 0)
			location.erase(location.begin()); // remove the first space
				
		// remove carriage return at the end for compatibility with unix systems
		if(location.size() > 0 && location.at(location.size()-1) == '\r')
		{
			location = location.substr(0, location.size()-1);
		}

		// append the lovstion to root location (boost syntax)
		location = (root / location).string();
				
		if (module.compare("PDM") == 0) 
		{
			LOG(INFO) << "Reading the PDM module from: " << location << "....";
			
			//ofstream outfile("/opt/zhangjing/OpenFace-master/build/out.txt", ofstream::app);  
			//outfile<<location;  
			//outfile<<"\n";
			//outfile.close();
			//location = "/opt/zhangjing/OpenFace-master/build/bin/model/pdms/In-the-wild_aligned_PDM_68.txt";
			pdm.Read(location);

			LOG(INFO) << "Done" << endl;
		}
	}


}


class PDM{
	public:
		int init();
		
	public:
		// The 3D mean shape vector of the PDM [x1,..,xn,y1,...yn,z1,...,zn]
		cv::Mat_<double> mean_shape;	
		// Principal components or variation bases of the model, 
		cv::Mat_<double> princ_comp;	
		// Eigenvalues (variances) corresponding to the bases
		cv::Mat_<double> eigen_values;	
		// Number of vertices
		inline int NumberOfPoints() const {return mean_shape.rows/3;}
		
		// Listing the number of modes of variation
		inline int NumberOfModes() const {return princ_comp.cols;}
};


void PDM::Read(string location)
{
	ifstream pdmLoc(location, ios_base::in);

	SkipComments(pdmLoc);
	// Reading mean values
	ReadMat(pdmLoc,mean_shape);

	SkipComments(pdmLoc);
	// Reading principal components
	ReadMat(pdmLoc,princ_comp);
	
	SkipComments(pdmLoc);	
	// Reading eigenvalues	
	ReadMat(pdmLoc,eigen_values);

}


//============================================================================
// Matrix reading functionality
//============================================================================

// Reading in a matrix from a stream
void ReadMat(std::ifstream& stream, cv::Mat &output_mat)
{
	// Read in the number of rows, columns and the data type
	int row,col,type;
	
	stream >> row >> col >> type;

	output_mat = cv::Mat(row, col, type);
	
	switch(output_mat.type())
	{
		case CV_64FC1: 
		{
			cv::MatIterator_<double> begin_it = output_mat.begin<double>();
			cv::MatIterator_<double> end_it = output_mat.end<double>();
			
			while(begin_it != end_it)
			{
				stream >> *begin_it++;
			}
		}
		break;
		case CV_32FC1:
		{
			cv::MatIterator_<float> begin_it = output_mat.begin<float>();
			cv::MatIterator_<float> end_it = output_mat.end<float>();

			while(begin_it != end_it)
			{
				stream >> *begin_it++;
			}
		}
		break;
		case CV_32SC1:
		{
			cv::MatIterator_<int> begin_it = output_mat.begin<int>();
			cv::MatIterator_<int> end_it = output_mat.end<int>();
			while(begin_it != end_it)
			{
				stream >> *begin_it++;
			}
		}
		break;
		case CV_8UC1:
		{
			cv::MatIterator_<uchar> begin_it = output_mat.begin<uchar>();
			cv::MatIterator_<uchar> end_it = output_mat.end<uchar>();
			while(begin_it != end_it)
			{
				stream >> *begin_it++;
			}
		}
		break;
		default:
			printf("ERROR(%s,%d) : Unsupported Matrix type %d!\n", __FILE__,__LINE__,output_mat.type()); abort();

	}
}

// Skipping lines that start with # (together with empty lines)
void SkipComments(std::ifstream& stream)
{	
	while(stream.peek() == '#' || stream.peek() == '\n'|| stream.peek() == ' ' || stream.peek() == '\r')
	{
		std::string skipped;
		std::getline(stream, skipped);
	}
}