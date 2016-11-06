#include <cppu/unotype.hxx>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <math.h>

#include "datatypes.hxx"

#define K 5

using com::sun::star::uno::Sequence;
using com::sun::star::uno::Any;


double pround(double fX, sal_Int32 nPrecision)
{
    if (fX == 0.)
        return fX;
    sal_Int32 nEx = floor(log10(abs(fX))) - nPrecision + 1;
    double fDiv = pow(10, nEx);
    return floor(fX / fDiv + 0.5) * fDiv;
}

sal_Int32 getPrecision(double fVal)
{
    char aStr[100];
    sprintf( aStr, "%.15f", fVal);
    bool bDecimal = false;
    sal_Int32 nPrecision = 0;
    for ( sal_Int32 nIdx = 0; aStr[nIdx]; ++nIdx )
    {
        if ( aStr[nIdx] == '.' )
        {
            bDecimal = true;
            continue;
        }

        if( bDecimal )
        {
            ++nPrecision;
            if ( aStr[nIdx] == '0' )
            {
                bool bFound = true;
                for ( sal_Int32 nIdx2 = nIdx+1; aStr[nIdx] && nIdx2 < 4; ++nIdx2 )
                {
                    if ( aStr[nIdx2] != '0' )
                    {
                        bFound = false;
                        break;
                    }
                }
                if ( bFound )
                    return nPrecision - 1;
            }
        }
    }

    return nPrecision - 1;
}

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
    std::vector< sal_Int32 > nTrainIndices( nNumTrain );

    for ( sal_Int32 nIdx = 0, nIdxTrain = 0; nIdx < nNumRows; ++nIdx )
	if ( rTestRowIndicesSet.count( nIdx ) == 0 )
	    nTrainIndices[nIdxTrain++] = nIdx;

    std::vector<std::pair<double, sal_Int32>> aDistances( nNumTrain );
    for ( sal_Int32 nTestIdx : rTestRowIndicesSet )
    {
	sal_Int32 nDistanceIdx = 0;
	for ( sal_Int32 nTrainIdx : nTrainIndices )
	{
	    double fDistance2 = 0.0;
	    for ( sal_Int32 nColIdx = 0; nColIdx < nNumCols; ++nColIdx )
	    {
		// Do not use labels/targets we are trying to predict as features !!
		if( nColIdx == nLabelIdx )
		    continue;
		
		if ( rColType[nColIdx] == DOUBLE )
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

	    aDistances[nDistanceIdx].first  = fDistance2;
	    aDistances[nDistanceIdx].second = nTrainIdx;
	    ++nDistanceIdx;
	    // Finished calculating distance from test point to one train point.
	}

	// Finished calculating distance from test point to all train points.

	std::sort( aDistances.begin(), aDistances.end() );

	if ( rColType[nLabelIdx] == DOUBLE )
	{
	    double fTarget = 0;
	    double fScale = 0;
	    //sal_Int32 nPrecision = 0;
	    for ( sal_Int32 nCandIdx = 0; nCandIdx < K; ++nCandIdx )
	    {
		double fSimilarity = exp( -aDistances[nCandIdx].first );
		if ( fSimilarity < 1.0E-10 )
		    fSimilarity = 1.0E-10;
		double fCandTarget;
		// Read the label/target of the candidate train point
		rDataArray[aDistances[nCandIdx].second][nLabelIdx] >>= fCandTarget;
		/*sal_Int32 nCandPrec = getPrecision( fCandTarget );
		if ( nCandPrec > nPrecision )
		    nPrecision = nCandPrec;
		*/
		    
		//printf("DEBUG>>> Testidx = %d : Cand %d has fSimilarity = %.6f, fCandTarget = %.4f\n", nTestIdx, nCandIdx, fSimilarity, fCandTarget); fflush(stdout);
		fTarget += ( fSimilarity * fCandTarget );
		fScale  += fSimilarity;
	    }
	    // Find the weighted target for the test point.
	    fTarget = fTarget / fScale;
	    //printf("DEBUG>>> Testidx = %d : found target = %.6f, scale = %.6f precision = %d\n", nTestIdx, fTarget, fScale, nPrecision); fflush(stdout);
	    //fTarget = pround( fTarget, nPrecision );
	    rDataArray[nTestIdx][nLabelIdx] <<= fTarget;
	}
	else if ( rColType[nLabelIdx] == INTEGER )
	{
	    std::unordered_multiset<double> aVotes;
	    sal_Int32 nMaxCount = 0;
	    double fTarget = 0.0;
	    for ( sal_Int32 nCandIdx = 0; nCandIdx < K; ++nCandIdx )
	    {
		double fCandTarget;
		// Read the label/target of the candidate train point
		rDataArray[aDistances[nCandIdx].second][nLabelIdx] >>= fCandTarget;
		aVotes.insert( fCandTarget );
		sal_Int32 nCount = aVotes.count( fCandTarget );
		if ( nCount > nMaxCount )
		{
		    nMaxCount = nCount;
		    fTarget   = fCandTarget;
		}
	    }

	    rDataArray[nTestIdx][nLabelIdx] <<= fTarget;
	}
	else
	{
	    std::unordered_multiset<OUString, OUStringHash> aVotes;
	    sal_Int32 nMaxCount = 0;
	    OUString aTarget;
	    for ( sal_Int32 nCandIdx = 0; nCandIdx < K; ++nCandIdx )
	    {
		OUString aCandTarget;
		// Read the label/target of the candidate train point
		rDataArray[aDistances[nCandIdx].second][nLabelIdx] >>= aCandTarget;
		aVotes.insert( aCandTarget );
		sal_Int32 nCount = aVotes.count( aCandTarget );
		if ( nCount > nMaxCount )
		{
		    nMaxCount = nCount;
		    aTarget   = aCandTarget;
		}
	    }

	    rDataArray[nTestIdx][nLabelIdx] <<= aTarget;
	}
    }
}
