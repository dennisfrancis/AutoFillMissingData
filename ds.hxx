
#include <vector>
#include <iostream>

#include <dlib/svm_threaded.h>
#include <dlib/rand.h>

#define NUMFOLDS 5

using namespace dlib;

typedef std::vector<double> sample_type;
typedef one_vs_one_trainer<any_trainer<sample_type> > ovo_trainer;
typedef radial_basis_kernel<sample_type> rbf_kernel;



class DataSet;

class Classifier
{
    Classifier( DataSet* pDset );
    ~Classifier();

    void fit( const std::vector<double>& rGammaSet );
    double predict( const sample_type& rSample );

private:

    DataSet* mpDset;
    one_vs_one_decision_function<ovo_trainer> aDecisionFunction;
}

class DataSet
{
    friend Classifier;
    DataSet()
	: mnNumFeatures( 0 ), mnNumSamples( 0 ) {}
    ~DataSet();

    bool addLabeledSample( std::vector<double>&& x, double y );
	
private:

    long mnNumFeatures;
    long mnNumSamples;
    
    std::vector<std::vector<double>> maSamples;
    std::vector<double> maLabels;
};


void Classifier::fit( const std::vector<double>& rGammaSet )
{
    // Do 5 fold crossvalidation for each gamma in rGammaSet and find the best gamma.

    ovo_trainer aTrainer;
    krr_trainer<rbf_kernel> aRBFTrainer;
    double fBestGamma = rGammaSet[0];
    double fBestAcc = 0;
    for ( auto fGamma : rGammaSet )
    {
	std::cout << "DEBUG>>> About to train with gamma = "
	    << fGamma << "\n" << std::flush;
	aRBFTrainer.set_kernel( rbf_kernel( fGamma ) );
	aTrainer.set_trainer( aRBFTrainer );
	randomize_samples( mpDset->maSamples, mpDset->maLabels );
	matrix<double> aCM = cross_validate_multiclass_trainer( aTrainer, mpDset->maSamples, mpDset->maLabels, NUMFOLDS );
	double fAcc = sum( diag( aCM ) ) / sum( aCM );
	std::cout << "DEBUG>>> Accuracy = " << fAcc << "\n" << std::flush;
	if ( fAcc > fBestAcc )
	{
	    fBestAcc = fAcc;
	    fBestGamma = fGamma;
	}
    }
    std::cout << "DEBUG>>> Best gamma = " << fBestGamma << "\n" << std::flush;
    aRBFTrainer.set_kernel( rbf_kernel( fBestGamma ) );
    aTrainer.set_trainer( aRBFTrainer );
    aDecisionFunction = aTrainer.train( mpDset->maSamples, mpDset->maLabels );
    
}

double Classifier::predict( const sample_type& rSample )
{
    return aDecisionFunction( rSample );
}

bool DataSet::addLabeledSample( std::vector<double>&& x, double y )
{
    if ( mnNumFeatures != 0 && x.size() != mnNumFeatures )
    {
	std::cout << "DEBUG>>> DataSet::addLabeledSample : mnNumFeatures = "
		  << mnNumFeatures << " but current sample has "
		  << x.size() << " features, skipping...\n" << std::flush;
	return false;
    }

    maSamples.push_back( std::move(x) );
    maLabels.push_back( y );
    return true;
}
