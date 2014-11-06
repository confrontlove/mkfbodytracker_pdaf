#ifndef __PF2DRAO
#define __PF2DRAO

#include <iostream>
#include <opencv/cv.h>
#include <ros/ros.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "my_gmm.h"
#include "KF_model.h"


class ParticleFilter
{
	public:
		ParticleFilter(int states, int nParticles);
		~ParticleFilter();		
		void update(cv::Mat measurement, std::vector<int> bins);
		cv::Mat getEstimator();
		cv::Mat getProbMap(cv::Mat H, cv::Mat M);
		std::vector<int> getPreviousHandBins();
		my_gmm gmm;
		std::vector<int> resample(std::vector<double> weights, int N);
	protected:
		cv::Mat chol(cv::Mat in);
		double mvnpdf(cv::Mat x, cv::Mat u, cv::Mat sigma);
		cv::Mat logmvnpdf(cv::Mat x, cv::Mat u, cv::Mat sigma);
		double wsum;
		double maxWeight(std::vector<double> weights);
		cv::Mat idx_v;
};



#endif
