/*
 * pf2DRao.cpp
 * 
 * Copyright 2013 Michael Burke <mgb45@chillip>
 * 
 * 
 */
#include "pf2DRao.h"

using namespace cv;
using namespace std;

// GMM storage class
my_gmm::my_gmm()
{
}
		
my_gmm::~my_gmm()
{
	mean.clear();
	weight.clear();
}

//Re-initialise tracker with large uncertainty and random state
void my_gmm::resetTracker(int d)
{
	tracks.clear();
	for (int i = 0; i < nParticles; i++)
	{
		state_params temp;
		temp.state = cv::Mat::zeros(d,1,CV_64F);
		randu(temp.state,1,480);
		for (int j = 2; j < d-6; j+=2)
		{
			randn(temp.state.row(j),3,5);
		}
		for (int j = d-6; j < d; j++)
		{
			randn(temp.state.row(j),0,5);
		}
		temp.cov = cv::Mat::zeros(d,d,CV_64F);
		temp.weight = 1.0/(double)nParticles;
		setIdentity(temp.cov, Scalar::all(4500));
		tracks.push_back(temp);
	}
}

// Load a gaussian for gmm with mean, sigma and weight		
void my_gmm::loadGaussian(cv::Mat u, cv::Mat s, double w)
{
	mean.push_back(u);
	cv::Mat temp;
	invert(s,temp,DECOMP_CHOLESKY);
	weight.push_back(w);
	
	KF_model tracker;
		
	cv::invert(Sigma_a.inv() + temp, tracker.Q, DECOMP_LU);
	tracker.R = 0.05*cv::Mat::eye(6,6, CV_64F);
	
	tracker.F = tracker.Q*Sigma_a.inv();
	
	tracker.B = tracker.Q*temp*u.t();

	tracker.H = cv::Mat::zeros(6,s.cols, CV_64F);
	tracker.H.at<double>(0,9) = 1;
	tracker.H.at<double>(1,10) = 1;
	tracker.H.at<double>(2,0) = 1;
	tracker.H.at<double>(3,1) = 1;
	tracker.H.at<double>(4,12) = 1;
	tracker.H.at<double>(5,13) = 1;
	
	KFtracker.push_back(tracker);
}

KF_model::KF_model()
{
}

KF_model::~KF_model()
{
}

void KF_model::predict(cv::Mat state_in, cv::Mat cov_in, cv::Mat &state_out, cv::Mat &cov_out)
{
	state_out = F*state_in + B;
	cov_out = F*cov_in*F.t() + Q;
}

void KF_model::update(cv::Mat measurement, cv::Mat state_in, cv::Mat cov_in, cv::Mat &state_out, cv::Mat &cov_out)
{
	cv::Mat y = measurement - H*state_in;
	cv::Mat S = H*cov_in*H.t() + R;
	cv::Mat K = cov_in*H.t()*S.inv();
	
	state_out = state_in + K*y;
	cov_out = (cv::Mat::eye(cov_in.rows,cov_in.cols,cov_in.type()) - K*H)*cov_in;
}

ParticleFilter::ParticleFilter(int states, int nParticles)
{
	cv::Mat Sigma_a = Mat::zeros(states, states, CV_64F);
	setIdentity(Sigma_a, Scalar::all(5));
	Sigma_a.at<double>(0,0) = 5600;
	Sigma_a.at<double>(1,1) = 5600;
	Sigma_a.at<double>(2,2) = 1;
	Sigma_a.at<double>(3,3) = 560;
	Sigma_a.at<double>(4,4) = 560;
	Sigma_a.at<double>(5,5) = 0.1;
	Sigma_a.at<double>(8,8) = 0.1;
	Sigma_a.at<double>(11,11) = 0.1;
	Sigma_a.at<double>(14,14) = 0.1;
	Sigma_a.at<double>(15,15) = 0.09;
	Sigma_a.at<double>(16,16) = 0.09;
	Sigma_a.at<double>(17,17) = 0.09;
	Sigma_a.at<double>(18,18) = 0.01;
	Sigma_a.at<double>(19,19) = 0.01;
	Sigma_a.at<double>(20,20) = 0.01;
	gmm.Sigma_a = Sigma_a;
	gmm.nParticles = nParticles;
	gmm.resetTracker(states);
}

ParticleFilter::~ParticleFilter()
{
}
		
// Weighted	average pose estimate
cv::Mat ParticleFilter::getEstimator()
{
	cv::Mat estimate;
	for (int i = 0;  i < gmm.nParticles; i++)
	{
		estimate = estimate + 1.0/(double)gmm.nParticles*gmm.tracks[i].state;
	}
	return estimate;
}

// Evaulate multivariate gaussian - measurement model
double ParticleFilter::mvnpdf(cv::Mat x, cv::Mat u, cv::Mat sigma)
{
	cv::Mat sigma_i;
	invert(sigma,sigma_i,DECOMP_CHOLESKY);
	cv::Mat x_u(x.size(),x.type());
	x_u = x - u;
	cv::Mat temp = -0.5*x_u.t()*sigma_i*x_u;
	return 1.0/(pow(2.0*M_PI,sigma.rows/2.0)*sqrt(cv::determinant(sigma)))*exp(temp.at<double>(0,0));
}

// Update stage
void ParticleFilter::update(cv::Mat measurement)
{
	// Propose indicators
	
	//std::vector<int> indicators = resampleStratified(gmm.weight, gmm.nParticles);
	std::vector<double> weights;
	wsum = 0;
	std::vector<state_params> new_tracks;
	state_params temp;
	cv::Mat state, cov;
	for (int j = 0; j < gmm.nParticles; j++) //update KF for each track
	{
		for (int i = 0; i < (int)gmm.KFtracker.size(); i++) //update each KF for each track 
		{
			gmm.KFtracker[i].predict(gmm.tracks[j].state,gmm.tracks[j].cov,temp.state,temp.cov);
			temp.weight = mvnpdf(measurement,gmm.KFtracker[i].H*temp.state,gmm.KFtracker[i].H*temp.cov*gmm.KFtracker[i].H.t()+gmm.KFtracker[i].R)*gmm.weight[i]*gmm.tracks[j].weight;
			weights.push_back(temp.weight);
			new_tracks.push_back(temp);
			wsum = wsum + temp.weight;
		}
	}
		
	//double Neff = 0;;
	for (int i = 0; i < (int)weights.size(); i++)
	{
		weights[i] = weights[i]/wsum;
		//Neff = Neff + weights[i]*weights[i];
	}
	//Neff = 1.0/Neff;
	//ROS_INFO("Effective particle num: %f",Neff);
		
	// Re-sample tracks
	//indicators.clear();
	std::vector<int> indicators;
	std::vector<double> new_weights;
	//indicators = resample(weights, gmm.nParticles);
	indicators = resampleStratifiedFernhead(weights, gmm.nParticles,new_weights);
	//ROS_INFO("Weights %d, KFs %d, particles %d, indicators %d.",(int)weights.size(),(int)gmm.KFtracker.size(),gmm.nParticles,(int)indicators.size());
	div_t k;
	for (int j = 0; j < gmm.nParticles; j++) //update KF for each track using indicator samples
	{
		k = div(indicators[j], (int)gmm.KFtracker.size());
		gmm.KFtracker[k.rem].update(measurement,new_tracks[indicators[j]].state,new_tracks[indicators[j]].cov,gmm.tracks[j].state,gmm.tracks[j].cov);
		gmm.tracks[j].weight = new_weights[j];
	}
	wsum = 1.0;
}

// Return best weight
double ParticleFilter::maxWeight(std::vector<double> weights)
{
	double mw = 0;
	for (int i = 0; i < (int)weights.size(); i++)
	{
		if (weights[i] > mw)
		{
			mw = weights[i];
		}
	}
	return mw;
}

// Resample according to weights
std::vector<int> ParticleFilter::resample(std::vector<double> weights, int N)
{
	std::vector<int> indicators;
	int idx = rand() % N;
	double beta = 0.0;
	double mw = maxWeight(weights);
	if (mw == 0)
	{
		weights.clear();
		for (int i = 0; i < N; i++)
		{
			indicators.push_back(i);
			weights.push_back(1.0/(double)N);
		}
	}
	else
	{
		idx = 0;
		double step = 1.0 / (double)N;
		beta = ((double)rand()/RAND_MAX)*step;
		for (int i = 0; i < N; i++)
		{
			while (beta > weights[idx])
			{
				beta -= weights[idx];
				idx = (idx + 1) % (int)weights.size();
			}
			beta += step;
			indicators.push_back(idx);
		}
	}
	return indicators;
}

// Stratified resampling
std::vector<int> ParticleFilter::resampleStratified(std::vector<double> weights, int N)
{
	std::vector<int> indicators;
	double wc[(int)weights.size()];
	wc[0] = weights[0];
	for (int j = 1; j < (int)weights.size(); j++)
	{
		wc[j] = wc[j-1] + weights[j];
	}
	
	int k = 0;
	for (int i = 0; i < N; i++)
	{
		while (wc[k]/wc[(int)weights.size()-1] < ((i-1)+((double)rand()/RAND_MAX))/(double)N)
		{
			k++;
		}
		indicators.push_back(k);
	}
	return indicators;
}

// Ferhead optimal + stratified resampling
std::vector<int> ParticleFilter::resampleStratifiedFernhead(std::vector<double> weights, int N,std::vector<double> &new_weights)
{
	// Solve for c roots of [sum(min(c*weight,1)) = N]
	std::vector<double> sw = weights;
	std::sort(sw.begin(),sw.end());
	double c = -DBL_MAX,temp_c = 0, wcs = 0;
	for (int j = 0; j < (int)sw.size(); j++)
	{
		wcs = wcs + sw[j];
		temp_c = ((double)N - ((double)sw.size()-(double)(j+1)))/wcs;
		if (temp_c > c)
		{
			c = temp_c;
		}
	}
		
	// Retain all particles with weight > 1/c
	std::vector<int> indicators;
	std::vector<double> remnants;
	std::vector<int> remnant_idx;
	for (int i = 0; i < (int)weights.size(); i++)
	{
		if (weights[i] >= 1.0/c)
		{
			indicators.push_back(i);
			new_weights.push_back(weights[i]);
		}
		else
		{
			remnant_idx.push_back(i);
			remnants.push_back(weights[i]);
		}
	}
	
	// Perform stratified sampling on remaining weights
	int L = N - (int)indicators.size();
	//~ ROS_WARN("%f %f, %d %d",c,temp_c,L,(int)remnants.size());
	std::vector<int> ind2 = resampleStratified(remnants, L);
	for (int j = 0; j < L; j++)
	{
		indicators.push_back(remnant_idx[ind2[j]]);
		new_weights.push_back(1.0/c);
	}
	
	return indicators;
}
