#ifndef __MYGMM
#define __MYGMM

#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "KF_model.h"

class state_params
{
	public:
		state_params();
		~state_params();
		state_params(const state_params& other);
		cv::Mat state;
		cv::Mat cov;
		double weight;
};

class my_gmm
{
	public:
		my_gmm();
		~my_gmm();
		void loadGaussian(cv::Mat u, cv::Mat s, cv::Mat &H, cv::Mat &m, double w, double g);
		void resetTracker(std::vector<int> bins);
		std::vector<cv::Mat> mean;
		std::vector<cv::Mat> cov;
		std::vector<double> weight;
		std::vector<KF_model> KFtracker;
		std::vector<state_params> tracks;
		int nParticles;
};

#endif
