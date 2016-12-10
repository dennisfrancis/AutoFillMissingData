#include <cppu/unotype.hxx>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <math.h>

#include "datatypes.hxx"

#define K 5
#define KSETSIZE 8
#define NUMFOLD 3
#define KMAX 15

using com::sun::star::uno::Sequence;
using com::sun::star::uno::Any;

void computeMissingValuesInColumnKNN( Sequence< Sequence< Any > >& rDataArray,
				      const sal_Int32 nLabelIdx,
				      const std::unordered_set<sal_Int32>& rTestRowIndicesSet,
				      const std::vector<DataType>& rColType,
				      const std::vector<std::pair<double, double>>& rFeatureScales );

sal_Int32 findBestK( const Sequence< Sequence< Any > >& rDataArray,
		     const sal_Int32 nLabelIdx,
		     const std::vector< sal_Int32 >& rDataIndices,
		     const std::vector<DataType>& rColType,
		     const std::vector<std::pair<double, double>>& rFeatureScales );


void getDistancesMatrix( const Sequence< Sequence< Any > >& rDataArray,
			 const sal_Int32 nLabelIdx,
			 const std::vector< sal_Int32 >& rTrainIndices,
			 const std::vector< sal_Int32 >& rTestIndices,
			 const std::vector<DataType>& rColType,
			 const std::vector<std::pair<double, double>>& rFeatureScales,
			 std::vector<std::vector<std::pair<double, sal_Int32>>>& rDistancesMatrix );

void fitPredict( const sal_Int32 nKparam,
		 const Sequence< Sequence< Any > >& rDataArray,
		 const sal_Int32 nLabelIdx,
		 const std::vector<DataType>& rColType,
		 const std::vector<std::vector<std::pair<double, sal_Int32>>>& rDistancesMatrix,
		 std::vector< Any >& rTargets );

double getScore( const Sequence< Sequence< Any > >& rDataArray,
		 const sal_Int32 nLabelIdx,
		 const std::vector< sal_Int32 >& rValidIndices,
		 const std::vector<DataType>& rColType,
		 const std::vector< Any >& rPreds );

void computeMissingValuesInColumnKNN( Sequence< Sequence< Any > >& rDataArray,
				      const sal_Int32 nLabelIdx,
				      const std::unordered_set<sal_Int32>& rTestRowIndicesSet,
				      const std::vector<DataType>& rColType,
				      const std::vector<std::pair<double, double>>& rFeatureScales )
{
    sal_Int32 nNumCols  = rColType.size();
    sal_Int32 nNumRows  = rDataArray.getLength();
    sal_Int32 nNumTest  = rTestRowIndicesSet.size();
    sal_Int32 nNumTrain = nNumRows - nNumTest;
    std::vector< sal_Int32 > aTrainIndices( nNumTrain );
    std::vector< sal_Int32 > aTestIndices( nNumTest );

    for ( sal_Int32 nIdx = 0, nIdxTrain = 0, nIdxTest = 0; nIdx < nNumRows; ++nIdx )
    {
	if ( rTestRowIndicesSet.count( nIdx ) == 0 )
	    aTrainIndices[nIdxTrain++] = nIdx;
	else
	    aTestIndices[nIdxTest++] = nIdx;
    }

    sal_Int32 nKparam = K;
    sal_Int32 nNumValid  = ( nNumTrain / NUMFOLD );
    sal_Int32 nNumTrain2 = nNumTrain - nNumValid;
    if ( nNumTrain >= NUMFOLD && nNumTrain2 > KMAX )
    {   // Find k param using NUMFOLD-crossvalidation.
	nKparam = findBestK( rDataArray, nLabelIdx, aTrainIndices, rColType, rFeatureScales );
    }
    else
    {
	printf("DEBUG>>> computeMissingValuesInColumnKNN : nNumTrain2(%d) is lower than min reqd %d, skipping tuning step.\n",
	       nNumTrain2, KMAX + 1);
	fflush(stdout);
    }

    std::vector<std::vector<std::pair<double, sal_Int32>>> aDistancesMatrix( nNumTest );
    getDistancesMatrix( rDataArray, nLabelIdx, aTrainIndices,
			aTestIndices, rColType, rFeatureScales,
			aDistancesMatrix );
    std::vector< Any > aTargets( nNumTest );
    fitPredict( nKparam, rDataArray, nLabelIdx, rColType, aDistancesMatrix, aTargets );
    for ( sal_Int32 nIdx = 0; nIdx < nNumTest; ++nIdx )
    {
	sal_Int32 nTestIdx = aTestIndices[nIdx];
	rDataArray[nTestIdx][nLabelIdx] = aTargets[nIdx];
    }
    
}

sal_Int32 findBestK( const Sequence< Sequence< Any > >& rDataArray,
		     const sal_Int32 nLabelIdx,
		     const std::vector< sal_Int32 >& rDataIndices,
		     const std::vector<DataType>& rColType,
		     const std::vector<std::pair<double, double>>& rFeatureScales )
{
    // Split dataset to train and validation sets.
    std::vector< sal_Int32 > aDataSetIndices( rDataIndices );
    
    sal_Int32 nNumValid  = ( aDataSetIndices.size() / NUMFOLD );
    sal_Int32 nNumTrain  = aDataSetIndices.size() - nNumValid;
    std::vector< sal_Int32 > aTrainIndices( nNumTrain );
    std::vector< sal_Int32 > aValidIndices( nNumValid );
    std::vector< Any > aTargets( nNumValid );

    std::vector< sal_Int32 > aKparamSet = { 3, 5, 7, 9, 11, 13, KMAX };

    sal_Int32 nBestK = aKparamSet[0];
    double fBestScore = -1.0;

    printf("DEBUG>>> Finding best K for nLabelIdx = %d using validation set\n", nLabelIdx); fflush(stdout);

    std::random_shuffle( aDataSetIndices.begin(), aDataSetIndices.end() );
    for ( sal_Int32 nIdx = 0; nIdx < nNumValid; ++nIdx )
	aValidIndices[nIdx] = aDataSetIndices[nIdx];
    for ( sal_Int32 nIdx = nNumValid; nIdx < aDataSetIndices.size(); ++nIdx )
	aTrainIndices[nIdx - nNumValid] = aDataSetIndices[nIdx];

    std::vector<std::vector<std::pair<double, sal_Int32>>> aDistancesMatrix( nNumValid );
    getDistancesMatrix( rDataArray, nLabelIdx, aTrainIndices,
			aValidIndices, rColType, rFeatureScales,
			aDistancesMatrix );
    for ( sal_Int32 nCandKparam : aKparamSet )
    {

	fitPredict( nCandKparam, rDataArray, nLabelIdx, rColType, aDistancesMatrix, aTargets );
	
	double fCandAvgScore = getScore( rDataArray, nLabelIdx, aValidIndices, rColType, aTargets );

	printf("DEBUG>>>    For K = %d, avg score = %.4f\n", nCandKparam, fCandAvgScore ); fflush(stdout);
	if ( fCandAvgScore >= fBestScore ) // Equality because of Occam's razor
	{
	    fBestScore = fCandAvgScore;
	    nBestK     = nCandKparam;
	}
    }
    printf("DEBUG>>>     Best K = %d =======\n", nBestK ); fflush(stdout);

    return nBestK;
}

void fitPredict( const sal_Int32 nKparam,
		 const Sequence< Sequence< Any > >& rDataArray,
		 const sal_Int32 nLabelIdx,
		 const std::vector<DataType>& rColType,
		 const std::vector<std::vector<std::pair<double, sal_Int32>>>& rDistancesMatrix,
		 std::vector< Any >& rTargets )
{
    sal_Int32 nNumTest  = rDistancesMatrix.size();

    for ( sal_Int32 nTargetIdx = 0; nTargetIdx < nNumTest; ++nTargetIdx )
    {

	if ( rColType[nLabelIdx] == DOUBLE )
	{
	    double fTarget = 0;
	    double fScale = 0;
	    //sal_Int32 nPrecision = 0;
	    for ( sal_Int32 nCandIdx = 0; nCandIdx < nKparam; ++nCandIdx )
	    {
		double fSimilarity = exp( -rDistancesMatrix[nTargetIdx][nCandIdx].first );
		if ( fSimilarity < 1.0E-10 )
		    fSimilarity = 1.0E-10;
		double fCandTarget;
		// Read the label/target of the candidate train point
		rDataArray[rDistancesMatrix[nTargetIdx][nCandIdx].second][nLabelIdx] >>= fCandTarget;

		//printf("DEBUG>>> Testidx = %d : Cand %d has fSimilarity = %.6f, fCandTarget = %.4f\n", nTestIdx, nCandIdx, fSimilarity, fCandTarget); fflush(stdout);
		fTarget += ( fSimilarity * fCandTarget );
		fScale  += fSimilarity;
	    }
	    // Find the weighted target for the test point.
	    fTarget = fTarget / fScale;
	    //printf("DEBUG>>> Testidx = %d : found target = %.6f, scale = %.6f precision = %d\n", nTestIdx, fTarget, fScale, nPrecision); fflush(stdout);
	    rTargets[nTargetIdx] <<= fTarget;
	}
	
	else if ( rColType[nLabelIdx] == INTEGER )
	{
	    std::unordered_multiset<double> aVotes;
	    sal_Int32 nMaxCount = 0;
	    double fTarget = 0.0;
	    for ( sal_Int32 nCandIdx = 0; nCandIdx < nKparam; ++nCandIdx )
	    {
		double fCandTarget;
		// Read the label/target of the candidate train point
		rDataArray[rDistancesMatrix[nTargetIdx][nCandIdx].second][nLabelIdx] >>= fCandTarget;
		aVotes.insert( fCandTarget );
		sal_Int32 nCount = aVotes.count( fCandTarget );
		if ( nCount > nMaxCount )
		{
		    nMaxCount = nCount;
		    fTarget   = fCandTarget;
		}
	    }

	    rTargets[nTargetIdx] <<= fTarget;
	}
	else
	{
	    std::unordered_multiset<OUString, OUStringHash> aVotes;
	    sal_Int32 nMaxCount = 0;
	    OUString aTarget;
	    for ( sal_Int32 nCandIdx = 0; nCandIdx < nKparam; ++nCandIdx )
	    {
		OUString aCandTarget;
		// Read the label/target of the candidate train point
		rDataArray[rDistancesMatrix[nTargetIdx][nCandIdx].second][nLabelIdx] >>= aCandTarget;
		aVotes.insert( aCandTarget );
		sal_Int32 nCount = aVotes.count( aCandTarget );
		if ( nCount > nMaxCount )
		{
		    nMaxCount = nCount;
		    aTarget   = aCandTarget;
		}
	    }

	    rTargets[nTargetIdx] <<= aTarget;
	}

    }
}

void getDistancesMatrix( const Sequence< Sequence< Any > >& rDataArray,
			 const sal_Int32 nLabelIdx,
			 const std::vector< sal_Int32 >& rTrainIndices,
			 const std::vector< sal_Int32 >& rTestIndices,
			 const std::vector<DataType>& rColType,
			 const std::vector<std::pair<double, double>>& rFeatureScales,
			 std::vector<std::vector<std::pair<double, sal_Int32>>>& rDistancesMatrix )
{
    sal_Int32 nNumTrain = rTrainIndices.size();
    sal_Int32 nNumTest  = rTestIndices.size();
    sal_Int32 nNumCols  = rColType.size();
    for ( sal_Int32 nIdx = 0; nIdx < nNumTest; ++nIdx )
	rDistancesMatrix[nIdx].resize( nNumTrain );

    sal_Int32 nDistanceIdxTest = 0;
    for ( sal_Int32 nTestIdx : rTestIndices )
    {
	sal_Int32 nDistanceIdxTrain = 0;
	for ( sal_Int32 nTrainIdx : rTrainIndices )
	{
	    double fDistance2 = 0.0;
	    for ( sal_Int32 nColIdx = 0; nColIdx < nNumCols; ++nColIdx )
	    {
		// Do not use labels/targets we are trying to predict as features !!
		if( nColIdx == nLabelIdx )
		    continue;
		
		if ( rColType[nColIdx] == DOUBLE || rColType[nColIdx] == INTEGER )
		{
		    double fTrainVal, fTestVal, fDiff;
		    rDataArray[nTrainIdx][nColIdx] >>= fTrainVal;
		    rDataArray[nTestIdx][nColIdx]  >>= fTestVal;
		    fTestVal  = (( fTestVal  - rFeatureScales[nColIdx].first ) / rFeatureScales[nColIdx].second );
		    fTrainVal = (( fTrainVal - rFeatureScales[nColIdx].first ) / rFeatureScales[nColIdx].second );
		    // Now origin is 0 and scale is ~ in [-1, 1]
		    fDiff = fTrainVal - fTestVal;
		    fDistance2 += ( fDiff * fDiff );
		    // Max distance^2 can be 2*2 = 4.0
		}
		else  // Discrete case
		{
		    // Exact match of class will contribute 0 to distance^2
		    // and all other combinations are treated dissimilar hence contribute
		    // a score of 4.0 to distance^2
		    fDistance2 += ( ( rDataArray[nTrainIdx][nColIdx] ==
				      rDataArray[nTestIdx][nColIdx] ) ? 0.0 : 4.0 );
		}
	    }

	    rDistancesMatrix[nDistanceIdxTest][nDistanceIdxTrain].first  = fDistance2;
	    rDistancesMatrix[nDistanceIdxTest][nDistanceIdxTrain].second = nTrainIdx;
	    ++nDistanceIdxTrain;
	    // Finished calculating distance from test point to one train point.
	}

	// Finished calculating distance from test point to all train points.

	std::sort( rDistancesMatrix[nDistanceIdxTest].begin(), rDistancesMatrix[nDistanceIdxTest].end() );
	++nDistanceIdxTest;
    }
}

// Outputs score in range [-Inf,1], 1 being the best score.
double getScore( const Sequence< Sequence< Any > >& rDataArray,
		 const sal_Int32 nLabelIdx,
		 const std::vector< sal_Int32 >& rValidIndices,
		 const std::vector<DataType>& rColType,
		 const std::vector< Any >& rPreds )
{
    sal_Int32 nNumValid = rValidIndices.size();
    double fScore = 0.0;
    // Calculate R^2 score.
    if ( rColType[nLabelIdx] == DOUBLE )
    {
	double fAvgTarget = 0.0;
	for ( sal_Int32 nValidIdx : rValidIndices )
	{
	    double fTarget;
	    rDataArray[nValidIdx][nLabelIdx] >>= fTarget;
	    fAvgTarget += fTarget;
	}
	
	fAvgTarget /= nNumValid;
	sal_Int32 nPredIdx = 0;
	double fNum = 0.0, fDen = 0.0;
	for ( sal_Int32 nValidIdx : rValidIndices )
	{
	    double fPred, fTarget;
	    rDataArray[nValidIdx][nLabelIdx] >>= fTarget;
	    rPreds[nPredIdx++] >>= fPred;

	    double fDiff1 = ( fTarget - fPred );
	    double fDiff2 = ( fTarget - fAvgTarget );
	    fNum += ( fDiff1 * fDiff1 );
	    fDen += ( fDiff2 * fDiff2 );
	}

	if ( fDen == 0.0 )
	    fScore = 1.0;
	else
	    fScore = 1 - ( fNum / fDen );
    }
    else
    {
	double fNumTruePositives = 0.0;
	sal_Int32 nPredIdx = 0;
	for ( sal_Int32 nValidIdx : rValidIndices )
	{
	    if ( rDataArray[nValidIdx][nLabelIdx] ==
		 rPreds[nPredIdx++] )
		fNumTruePositives += 1.0;
	}

	fScore = ( fNumTruePositives / nNumValid );
    }

    return fScore;
}
