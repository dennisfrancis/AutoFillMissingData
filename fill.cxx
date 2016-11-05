#include <fill.hxx>
#include <com/sun/star/beans/NamedValue.hpp>
#include <com/sun/star/frame/DispatchResultEvent.hpp>
#include <com/sun/star/frame/DispatchResultState.hpp>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/frame/XController.hpp>
#include <com/sun/star/frame/XModel.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/sheet/XSheetCellRanges.hpp>
#include <com/sun/star/sheet/XCellRangeAddressable.hpp>
#include <com/sun/star/sheet/XSpreadsheetDocument.hpp>
#include <com/sun/star/sheet/XSpreadsheets.hpp>
#include <com/sun/star/sheet/XSpreadsheet.hpp>
#include <com/sun/star/sheet/XSheetCellCursor.hpp>
#include <com/sun/star/container/XIndexAccess.hpp>
#include <com/sun/star/table/CellRangeAddress.hpp>
#include <com/sun/star/table/CellContentType.hpp>
#include <com/sun/star/container/XIndexContainer.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/table/XCell.hpp>
#include <com/sun/star/util/Color.hpp>
#include <com/sun/star/table/XCellRange.hpp>

#include <com/sun/star/document/XUndoManager.hpp>
#include <com/sun/star/document/XUndoManagerSupplier.hpp>

#include <cppuhelper/supportsservice.hxx>
#include <rtl/ustring.hxx>
#include <rtl/ustrbuf.hxx>

#include <vector>
#include <unordered_set>
#include <chrono>
#include <thread>

#define MAXROW 1048575
#define MAXCOL 1023
#define EMPTYSTRING OUString("__NA__")
#define EMPTYDOUBLE -9999999.0

using rtl::OUString;
using rtl::OUStringBuffer;
using rtl::OUStringHash;
using namespace com::sun::star::uno;
using namespace com::sun::star::frame;
using namespace com::sun::star::sheet;
using namespace com::sun::star::table;
using com::sun::star::beans::NamedValue;
using com::sun::star::beans::XPropertySet;
using com::sun::star::lang::IllegalArgumentException;
using com::sun::star::frame::DispatchResultEvent;
using com::sun::star::frame::DispatchResultEvent;
using com::sun::star::util::Color;
using com::sun::star::container::XIndexAccess;
using com::sun::star::document::XUndoManagerSupplier;
using com::sun::star::document::XUndoManager;


// This is the service name an Add-On has to implement
#define SERVICE_NAME "com.sun.star.task.Job"

enum DataType{ INTEGER, DOUBLE, STRING };

void logError(const char* pStr);
Reference< XModel > getModel( const Reference< XFrame >& rxFrame );
bool getDataRange( const Reference< XModel >& rxModel, CellRangeAddress& rRangeExtended );
Reference< XSpreadsheet > getSheet( const Reference< XModel >& rxModel, const sal_Int32 nSheet );
sal_Bool getColColors( const Reference< XSpreadsheet >& rxSheet, const CellRangeAddress& rRange, std::vector<Color>& rColBGColors );
sal_Bool setColColors( const Reference< XSpreadsheet >& rxSheet, const CellRangeAddress& rRange, const std::vector<Color>& rColBGColors );

void computeMissingValuesInColumnOUString( const Reference< XSpreadsheet >& rxSheet,
					   const CellRangeAddress& rRange,
					   const sal_Int32 nLabelIdx,
					   const std::unordered_set<sal_Int32>& rTestRowIndicesSet,
					   const std::vector<DataType>& rColType,
					   std::vector<OUString>& rComputedMissingValues );

void computeMissingValuesInColumnDouble( const Reference< XSpreadsheet >& rxSheet,
					 const CellRangeAddress& rRange,
					 const sal_Int32 nLabelIdx,
					 const std::unordered_set<sal_Int32>& rTestRowIndicesSet,
					 const std::vector<DataType>& rColType,
					 std::vector<double>& rComputedMissingValues );

void getColumnDataOUString( const Reference< XSpreadsheet >& rxSheet,
			    const sal_Int32 nCol,
			    const sal_Int32 nStartRow,
			    const sal_Int32 nEndRow,
			    const DataType aDataType,
			    std::vector<OUString>& rValsWithMissingData );

void getColumnDataDouble( const Reference< XSpreadsheet >& rxSheet,
			  const sal_Int32 nCol,
			  const sal_Int32 nStartRow,
			  const sal_Int32 nEndRow,
			  const DataType aDataType,
			  std::vector<double>& rValsWithMissingData );

void imputeOUString( std::vector<OUString>& rValsWithMissingData,
		     const DataType aDataType,
		     const std::unordered_set<sal_Int32>& rEmptyValIndexSet );

void imputeDouble( std::vector<double>& rValsWithMissingData,
		   const DataType aDataType,
		   const std::unordered_set<sal_Int32>& rEmptyValIndexSet );

bool imputeOUStringWithMode( std::vector<OUString>& rValsWithMissingData,
			     const DataType aDataType,
			     const std::unordered_set<sal_Int32>& rEmptyValIndexSet );

bool imputeDoubleWithMode( std::vector<double>& rValsWithMissingData,
			   const DataType aDataType,
			   const std::unordered_set<sal_Int32>& rEmptyValIndexSet );

bool imputeOUStringWithMedian( std::vector<OUString>& rValsWithMissingData,
			       const DataType aDataType,
			       const std::unordered_set<sal_Int32>& rEmptyValIndexSet );

bool imputeDoubleWithMedian( std::vector<double>& rValsWithMissingData,
			     const DataType aDataType,
			     const std::unordered_set<sal_Int32>& rEmptyValIndexSet );

OUString getMedianOUString( const std::vector<OUString>& rValsWithMissingData,
			    const DataType aDataType,
			    const size_t nNumEmptyElements );

double getMedianDouble( const std::vector<double>& rValsWithMissingData,
			const DataType aDataType,
			const size_t nNumEmptyElements );


// Helper functions for the implementation of UNO component interfaces.
OUString FillMissingDataImpl_getImplementationName()
throw (RuntimeException)
{
    return OUString ( IMPLEMENTATION_NAME );
}

Sequence< OUString > SAL_CALL FillMissingDataImpl_getSupportedServiceNames()
throw (RuntimeException)
{
    Sequence < OUString > aRet(1);
    OUString* pArray = aRet.getArray();
    pArray[0] =  OUString ( SERVICE_NAME );
    return aRet;
}

Reference< XInterface > SAL_CALL FillMissingDataImpl_createInstance( const Reference< XComponentContext > & rContext)
    throw( Exception )
{
    return (cppu::OWeakObject*) new FillMissingDataImpl( rContext );
}

// Implementation of the recommended/mandatory interfaces of a UNO component.
// XServiceInfo
OUString SAL_CALL FillMissingDataImpl::getImplementationName()
    throw (RuntimeException)
{
    return FillMissingDataImpl_getImplementationName();
}

sal_Bool SAL_CALL FillMissingDataImpl::supportsService( const OUString& rServiceName )
    throw (RuntimeException)
{
    return cppu::supportsService(this, rServiceName);
}

Sequence< OUString > SAL_CALL FillMissingDataImpl::getSupportedServiceNames(  )
    throw (RuntimeException)
{
    return FillMissingDataImpl_getSupportedServiceNames();
}



// XAsyncJob method implementations

Any SAL_CALL FillMissingDataImpl::execute( const Sequence<NamedValue>& rArgs )
    throw(IllegalArgumentException, RuntimeException)
{
    printf("DEBUG>>> Called executeAsync() : this = %p\n", this); fflush(stdout);
    
    FillMissingDataImplInfo aJobInfo;
    OUString aErr = validateGetInfo( rArgs, aJobInfo );
    if ( !aErr.isEmpty() )
    {
	sal_Int16 nArgPos = 0;	
	if ( aErr.startsWith( "Listener" ) )
	    nArgPos = 1;

	throw IllegalArgumentException(
	    aErr,
	    // resolve to XInterface reference:
	    static_cast< ::cppu::OWeakObject * >(this),
	    nArgPos ); // argument pos
    }

    if ( aJobInfo.aEventName.equalsAscii("onFillMissingDataReq"))
	colorData( aJobInfo );
    //logError("About to sleep for 5 seconds");
    //std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    bool bIsDispatch = aJobInfo.aEnvType.equalsAscii("DISPATCH");
    Sequence<NamedValue> aReturn( ( bIsDispatch ? 1 : 0 ) );

    if ( bIsDispatch )
    {
	aReturn[0].Name  = "SendDispatchResult";
	DispatchResultEvent aResultEvent;
	aResultEvent.Source = (cppu::OWeakObject*)this;
	aResultEvent.State = DispatchResultState::SUCCESS;
	aResultEvent.Result <<= true;
	aReturn[0].Value <<= aResultEvent;
    }
    
    return makeAny( aReturn );
}


OUString FillMissingDataImpl::validateGetInfo( const Sequence<NamedValue>& rArgs,
					       FillMissingDataImpl::FillMissingDataImplInfo& rJobInfo )
{
    // Extract all sublists from rArgs.
    Sequence<NamedValue> aGenericConfig;
    Sequence<NamedValue> aEnvironment;

    sal_Int32 nNumNVs = rArgs.getLength();
    for ( sal_Int32 nIdx = 0; nIdx < nNumNVs; ++nIdx )
    {
	if ( rArgs[nIdx].Name.equalsAscii("Config") )
	    rArgs[nIdx].Value >>= aGenericConfig;
	else if ( rArgs[nIdx].Name.equalsAscii("Environment") )
	    rArgs[nIdx].Value >>= aEnvironment;
    }

    // Analyze the environment info. This sub list is the only guaranteed one!
    if ( !aEnvironment.hasElements() )
	return OUString("Args : no environment");

    sal_Int32 nNumEnvEntries = aEnvironment.getLength();
    for ( sal_Int32 nIdx = 0; nIdx < nNumEnvEntries; ++nIdx )
    {
	if ( aEnvironment[nIdx].Name.equalsAscii("EnvType") )
	    aEnvironment[nIdx].Value >>= rJobInfo.aEnvType;
	
	else if ( aEnvironment[nIdx].Name.equalsAscii("EventName") )
	    aEnvironment[nIdx].Value >>= rJobInfo.aEventName;

	else if ( aEnvironment[nIdx].Name.equalsAscii("Frame") )
	    aEnvironment[nIdx].Value >>= rJobInfo.xFrame;
    }

    // Further the environment property "EnvType" is required as minimum.

    if ( rJobInfo.aEnvType.isEmpty() ||
	 ( ( !rJobInfo.aEnvType.equalsAscii("EXECUTOR") ) &&
	   ( !rJobInfo.aEnvType.equalsAscii("DISPATCH") )
	     )	)
	return OUString("Args : \"" + rJobInfo.aEnvType + "\" isn't a valid value for EnvType");

    // Analyze the set of shared config data.
    if ( aGenericConfig.hasElements() )
    {
	sal_Int32 nNumGenCfgEntries = aGenericConfig.getLength();
	for ( sal_Int32 nIdx = 0; nIdx < nNumGenCfgEntries; ++nIdx )
	    if ( aGenericConfig[nIdx].Name.equalsAscii("Alias") )
		aGenericConfig[nIdx].Value >>= rJobInfo.aAlias;
    }

    return OUString("");
}

void FillMissingDataImpl::colorData( const FillMissingDataImpl::FillMissingDataImplInfo& rJobInfo )
{
    if ( !rJobInfo.xFrame.is() )
    {
	logError("colorData : Frame passed is null, cannot color data !");
	return;
    }
    Reference< XModel > xModel = getModel( rJobInfo.xFrame );
    if ( !xModel.is() )
    {
	logError("colorData : xModel is invalid");
	return;
    }

    Reference< XUndoManagerSupplier > xUndoSupplier( xModel, UNO_QUERY );
    Reference< XUndoManager > xUndoMgr;
    if ( xUndoSupplier.is() )
	xUndoMgr = xUndoSupplier->getUndoManager();

    if ( xUndoMgr.is() )
	xUndoMgr->enterUndoContext( OUString("FillMissingDataImpl_UNDO") );

    CellRangeAddress aRange;
    bool bGotRange = getDataRange( xModel, aRange );
    if ( !bGotRange )
    {
	logError("colorData : Could not get data range !");
	return;
    }
    sal_Int32 nNumCols = aRange.EndColumn - aRange.StartColumn + 1;
    sal_Int32 nNumRows = aRange.EndRow - aRange.StartRow + 1;
    printf("DEBUG>>> nNumCols = %d, nNumRows = %d\n", nNumCols, nNumRows);fflush(stdout);

    std::vector<Color> aColBGColors(nNumCols);

    Reference< XSpreadsheet > xSheet = getSheet( xModel, aRange.Sheet );
    sal_Bool bOK = getColColors( xSheet, aRange, aColBGColors );
    if ( !bOK )
    {
	logError("colorData : getColColors() failed");
	return;
    }

    /*
    bOK = setColColors( xSheet, aRange, aColBGColors );
    if ( !bOK )
    {
	logError("colorData : setColColors() failed");
	return;
    }
    */
    if ( xUndoMgr.is() )
	xUndoMgr->leaveUndoContext();
}

void logError(const char* pStr)
{
    printf("DEBUG>>> %s\n", pStr);
    fflush(stdout);
}

Reference< XModel > getModel( const Reference< XFrame >& rxFrame )
{
    Reference< XModel > xModel;
    if ( !rxFrame.is() )
	return xModel;

    Reference< XController > xCtrl = rxFrame->getController();
    if ( !xCtrl.is() )
    {
	logError("getModel : xCtrl is invalid");
	return xModel;
    }

    xModel = xCtrl->getModel();
    return xModel;
}

bool getDataRange( const Reference< XModel >& rxModel, CellRangeAddress& rRangeExtended )
{
    Reference< XCellRangeAddressable > xRange( rxModel->getCurrentSelection(), UNO_QUERY );
    if ( !xRange.is() )
    {
	logError("getDataRange : Could not get simple data range !");
	return false;
    }

    CellRangeAddress aRange = xRange->getRangeAddress();
    Reference< XSpreadsheet > xSheet = getSheet( rxModel, aRange.Sheet );
    if ( !xSheet.is() )
    {
	logError("getDataRange : Could not get sheet !");
	return false;
    }
    sal_Int32 nStartCol  = (rRangeExtended.StartColumn = aRange.StartColumn);
    sal_Int32 nEndCol    = (rRangeExtended.EndColumn   = aRange.EndColumn);
    sal_Int32 nStartRow  = (rRangeExtended.StartRow    = aRange.StartRow);
    sal_Int32 nEndRow    = (rRangeExtended.EndRow      = aRange.EndRow);


    bool bStop = false;
    // Extend nStartCol
    for ( sal_Int32 nCol = nStartCol-1; (nCol >= 0 && !bStop); --nCol )
    {
	bool bColEmpty = true;
	for ( sal_Int32 nRow = nStartRow; nRow <= nEndRow; ++nRow )
	{
	    Reference< XCell > xCell = xSheet->getCellByPosition(nCol, nRow);
	    if ( !xCell.is() )
	    {
		printf("DEBUG>>> getDataRange : xCell(%d, %d) is invalid.\n", nCol, nRow );
		fflush(stdout);
	    }
	    else if ( xCell->getType() != CellContentType_EMPTY )
	    {
		bColEmpty = false;
		break;
	    }
	}
	if ( bColEmpty )
	{
	    bStop = true;
	    nStartCol = nCol + 1;
	}

	if( nCol == 0 )
	    nStartCol = 0;
    }

    bStop = false;
    // Extend nEndCol
    for ( sal_Int32 nCol = nEndCol+1; (nCol <= MAXCOL && !bStop); ++nCol )
    {
	bool bColEmpty = true;
	for ( sal_Int32 nRow = nStartRow; nRow <= nEndRow; ++nRow )
	{
	    Reference< XCell > xCell = xSheet->getCellByPosition(nCol, nRow);
	    if ( !xCell.is() )
	    {
		printf("DEBUG>>> getDataRange : xCell(%d, %d) is invalid.\n", nCol, nRow );
		fflush(stdout);
	    }
	    else if ( xCell->getType() != CellContentType_EMPTY )
	    {
		bColEmpty = false;
		break;
	    }
	}
	if ( bColEmpty )
	{
	    bStop = true;
	    nEndCol = nCol - 1;
	}

	if( nCol == MAXCOL )
	    nEndCol = MAXCOL;
    }

    //printf("DEBUG>>> nStartCol = %d, nEndCol = %d\n", nStartCol, nEndCol); fflush(stdout);
    
    bStop = false;
    // Extend nStartRow
    for ( sal_Int32 nRow = nStartRow - 1; (nRow >= 0 && !bStop); --nRow )
    {
	bool bRowEmpty = true;
	for ( sal_Int32 nCol = nStartCol; nCol <= nEndCol; ++nCol )
	{
	    Reference< XCell > xCell = xSheet->getCellByPosition(nCol, nRow);
	    if ( !xCell.is() )
	    {
		printf("DEBUG>>> getDataRange : xCell(%d, %d) is invalid.\n", nCol, nRow );
		fflush(stdout);
	    }
	    else if ( xCell->getType() != CellContentType_EMPTY )
	    {
		//printf("DEBUG>>> found cell at col = %d, row = %d non-empty\n", nCol, nRow ); fflush(stdout);
		bRowEmpty = false;
		break;
	    }
	}
	if ( bRowEmpty )
	{
	    bStop = true;
	    nStartRow = nRow + 1;
	}

	if( nRow == 0 )
	    nStartRow = 0;
    }

    //printf("DEBUG>>> nStartCol = %d, nEndCol = %d\n", nStartCol, nEndCol); fflush(stdout);
    //printf("DEBUG>>> nStartRow = %d, nEndRow = %d\n", nStartRow, nEndRow); fflush(stdout);
    
    bStop = false;
    // Extend nEndRow
    for ( sal_Int32 nRow = nEndRow + 1; (nRow <= MAXROW && !bStop); ++nRow )
    {
	bool bRowEmpty = true;
	for ( sal_Int32 nCol = nStartCol; nCol <= nEndCol; ++nCol )
	{
	    Reference< XCell > xCell = xSheet->getCellByPosition(nCol, nRow);
	    if ( !xCell.is() )
	    {
		printf("DEBUG>>> getDataRange : xCell(%d, %d) is invalid.\n", nCol, nRow );
		fflush(stdout);
	    }
	    else if ( xCell->getType() != CellContentType_EMPTY )
	    {
		bRowEmpty = false;
		break;
	    }
	}
	if ( bRowEmpty )
	{
	    bStop = true;
	    nEndRow = nRow - 1;
	}

	if ( nRow == MAXROW )
	    nEndRow = MAXROW;
    }

    //printf("DEBUG>>> nStartCol = %d, nEndCol = %d\n", nStartCol, nEndCol); fflush(stdout);
    //printf("DEBUG>>> nStartRow = %d, nEndRow = %d\n", nStartRow, nEndRow); fflush(stdout);

    rRangeExtended.Sheet        = aRange.Sheet;
    rRangeExtended.StartRow     = nStartRow;
    rRangeExtended.EndRow       = nEndRow;
    rRangeExtended.StartColumn  = nStartCol;
    rRangeExtended.EndColumn    = nEndCol;

    return true;
}


Reference< XSpreadsheet > getSheet( const Reference< XModel >& rxModel, const sal_Int32 nSheet )
{
    Reference< XSpreadsheet > xSpreadsheet;
    
    Reference< XSpreadsheetDocument > xSpreadsheetDocument( rxModel, UNO_QUERY );
    if ( !xSpreadsheetDocument.is() )
    {
	logError("getSheet : Cannot get xSpreadsheetDocument");
	return xSpreadsheet;
    }

    Reference< XIndexAccess > xSpreadsheets( xSpreadsheetDocument->getSheets(), UNO_QUERY );
    if ( !xSpreadsheets.is() )
    {
	logError("getSheet : Cannot get xSpreadsheets");
	return xSpreadsheet;
    }
    Any aSheet = xSpreadsheets->getByIndex( nSheet );
    xSpreadsheet = Reference< XSpreadsheet >( aSheet, UNO_QUERY );
    return xSpreadsheet;
}

sal_Bool getColColors( const Reference< XSpreadsheet >& rxSheet, const CellRangeAddress& rRange, std::vector<Color>& rColBGColors )
{
    sal_Int32 nNumCols = rRange.EndColumn - rRange.StartColumn + 1;
    sal_Int32 nNumRows = rRange.EndRow - rRange.StartRow; // Don't count the header
    std::vector<bool> aIsColComplete( nNumCols );
    std::vector<DataType> aColType( nNumCols );
    std::vector<std::vector<sal_Int32>> aCol2BlankRowIdx( nNumCols );

    try
    {
	 DataType aType;

	for ( sal_Int32 nCol = rRange.StartColumn, nColIdx = 0; nCol <= rRange.EndColumn; ++nCol, ++nColIdx )
	{
	    aType = INTEGER;
	    bool bIsComplete = true;
	    std::vector<sal_Int32> aBlankRowIdx;
	    for ( sal_Int32 nRow = rRange.StartRow+1, nRowIdx = 0; nRow <= rRange.EndRow; ++nRow, ++nRowIdx )
	    {
		Reference< XCell > xCell = rxSheet->getCellByPosition(nCol, nRow);
		if ( !xCell.is() )
		    logError("getColColors : xCell is invalid.");
		else
		{
		    if ( xCell->getType() == CellContentType_TEXT )
		    {
			aType = STRING;
		    }
		    else if ( xCell->getType() == CellContentType_VALUE && aType == INTEGER )
		    {
			double fVal = xCell->getValue();
			if ( fVal != static_cast<double>(static_cast<sal_Int64>(fVal)) )
			    aType = DOUBLE;
		    }
		    else if ( xCell->getType() == CellContentType_EMPTY )
		    {
			bIsComplete = false;
			aBlankRowIdx.push_back( nRowIdx );
		    }
		}
	    }
	    
	    aIsColComplete[nColIdx]    = bIsComplete;
	    aColType[nColIdx]          = aType;
	    aCol2BlankRowIdx[nColIdx]  = std::move( aBlankRowIdx );

	    printf("DEBUG>>> col = %d, Type = %d, isComplete = %d\n", nColIdx, aType, int(bIsComplete));fflush(stdout);

	    switch( aType )
	    {
	    case INTEGER:
		rColBGColors[nColIdx] = static_cast<Color>(0xb0fccc); //b7bb96ff
		break;
	    case DOUBLE:
		rColBGColors[nColIdx] = static_cast<Color>(0xb0f2fc); //a3f796ff
		break;
	    case STRING:
		rColBGColors[nColIdx] = static_cast<Color>(0xfcd9b0); //96dff7ff
		break;
	    }
	}
    }
    catch( Exception& e )
    {
	fprintf(stderr, "DEBUG>>> getColColors : caught UNO exception: %s\n",
		OUStringToOString( e.Message, RTL_TEXTENCODING_ASCII_US ).getStr());
	fflush(stderr);
	return false;
    }

    for ( sal_Int32 nLabelIdx = 0; nLabelIdx < nNumCols; ++nLabelIdx )
    {
	if ( aIsColComplete[nLabelIdx] )
	    continue;

	// Got a column nLabelIdx that should be the next label column

	std::vector<sal_Int32> aTestRowIndices = aCol2BlankRowIdx[nLabelIdx];

	if ( aTestRowIndices.size() > ( nNumRows - 5 ) )
	{
	    printf("DEBUG>>> getColColors() : Not enough training samples for Label column = %d, test size = %d, skipping this column\n",
		   nLabelIdx, aTestRowIndices.size());
	    fflush(stdout);
	    continue;
	}

	std::unordered_set<sal_Int32> aTestRowIndicesSet;
	for ( sal_Int32 nTestRowIdx : aTestRowIndices )
	    aTestRowIndicesSet.insert( nTestRowIdx );

	if ( aColType[nLabelIdx] == INTEGER || aColType[nLabelIdx] == DOUBLE )
	{
	    std::vector<double> aComputedMissingValues(aTestRowIndices.size());
	    computeMissingValuesInColumnDouble( rxSheet, rRange, nLabelIdx, aTestRowIndicesSet, aColType, aComputedMissingValues );
	    sal_Int32 nMissingIdx = 0;
	    for ( sal_Int32 nRowIdx : aTestRowIndices )
	    {
		Reference< XCell > xCell = rxSheet->getCellByPosition(rRange.StartColumn + nLabelIdx, rRange.StartRow + 1 + nRowIdx );
		xCell->setValue( aComputedMissingValues[nMissingIdx++] );
	    }
	}
	else if ( aColType[nLabelIdx] == STRING )
	{
	    std::vector<OUString> aComputedMissingValues(aTestRowIndices.size());
	    computeMissingValuesInColumnOUString( rxSheet, rRange, nLabelIdx, aTestRowIndicesSet, aColType, aComputedMissingValues );
	    sal_Int32 nMissingIdx = 0;
	    for ( sal_Int32 nRowIdx : aTestRowIndices )
	    {
		Reference< XCell > xCell = rxSheet->getCellByPosition(rRange.StartColumn + nLabelIdx, rRange.StartRow + 1 + nRowIdx );
		xCell->setFormula( aComputedMissingValues[nMissingIdx++] );
	    }
	}

	for ( sal_Int32 nRowIdx : aTestRowIndices )
	{
	    Reference< XCell > xCell = rxSheet->getCellByPosition( nLabelIdx + rRange.StartColumn, nRowIdx + rRange.StartRow + 1 );
	    Reference< XPropertySet > xPropSet( xCell, UNO_QUERY );
	    if ( !xPropSet.is() )
	    {
		printf("DEBUG>>> getColColors : Cannot get xPropSet for Cell with Column = %d, Row = %d !\n", nLabelIdx, nRowIdx);
		fflush(stdout);
		continue;
	    }
	    xPropSet->setPropertyValue( "CellBackColor", makeAny( 0xee1100 ) );
	}
	
    }


    return true;
}

void computeMissingValuesInColumnOUString( const Reference< XSpreadsheet >& rxSheet,
					   const CellRangeAddress& rRange,
					   const sal_Int32 nLabelIdx,
					   const std::unordered_set<sal_Int32>& rTestRowIndicesSet,
					   const std::vector<DataType>& rColType,
					   std::vector<OUString>& rComputedMissingValues )
{
    sal_Int32 nNumCols = rRange.EndColumn - rRange.StartColumn + 1;
    sal_Int32 nNumRows = rRange.EndRow - rRange.StartRow; // Don't count the header
    
    std::vector<OUString> aValsWithMissingData(nNumRows);
    getColumnDataOUString( rxSheet, rRange.StartColumn + nLabelIdx, rRange.StartRow + 1, rRange.EndRow, rColType[nLabelIdx], aValsWithMissingData );
    imputeOUString( aValsWithMissingData, rColType[nLabelIdx], rTestRowIndicesSet );
    sal_Int32 nMissingIdx = 0;
    for( const sal_Int32 nTestIdx : rTestRowIndicesSet )
	rComputedMissingValues[nMissingIdx++] = aValsWithMissingData[nTestIdx];

}

void computeMissingValuesInColumnDouble( const Reference< XSpreadsheet >& rxSheet,
					 const CellRangeAddress& rRange,
					 const sal_Int32 nLabelIdx,
					 const std::unordered_set<sal_Int32>& rTestRowIndicesSet,
					 const std::vector<DataType>& rColType,
					 std::vector<double>& rComputedMissingValues )
{
    sal_Int32 nNumCols = rRange.EndColumn - rRange.StartColumn + 1;
    sal_Int32 nNumRows = rRange.EndRow - rRange.StartRow; // Don't count the header
    
    std::vector<double> aValsWithMissingData(nNumRows);
    getColumnDataDouble( rxSheet, rRange.StartColumn + nLabelIdx, rRange.StartRow + 1, rRange.EndRow, rColType[nLabelIdx], aValsWithMissingData );
    imputeDouble( aValsWithMissingData, rColType[nLabelIdx], rTestRowIndicesSet );
    sal_Int32 nMissingIdx = 0;
    for( const sal_Int32 nTestIdx : rTestRowIndicesSet )
	rComputedMissingValues[nMissingIdx++] = aValsWithMissingData[nTestIdx];

}

void getColumnDataOUString( const Reference< XSpreadsheet >& rxSheet,
			    const sal_Int32 nCol,
			    const sal_Int32 nStartRow,
			    const sal_Int32 nEndRow,
			    const DataType aDataType,
			    std::vector<OUString>& rValsWithMissingData )
{
    for ( sal_Int32 nRow = nStartRow, nRowIdx = 0; nRow <= nEndRow; ++nRow, ++nRowIdx )
    {
	Reference< XCell > xCell = rxSheet->getCellByPosition( nCol, nRow );
	OUString aVal;
	if ( xCell->getType() == CellContentType_EMPTY )
	    aVal = EMPTYSTRING;
	else if ( aDataType == STRING )
	    aVal = xCell->getFormula();
	
	rValsWithMissingData[nRowIdx] = aVal;
    }
}

void getColumnDataDouble( const Reference< XSpreadsheet >& rxSheet,
			  const sal_Int32 nCol,
			  const sal_Int32 nStartRow,
			  const sal_Int32 nEndRow,
			  const DataType aDataType,
			  std::vector<double>& rValsWithMissingData )
{
    for ( sal_Int32 nRow = nStartRow, nRowIdx = 0; nRow <= nEndRow; ++nRow, ++nRowIdx )
    {
	Reference< XCell > xCell = rxSheet->getCellByPosition( nCol, nRow );
	double aVal;
	if ( xCell->getType() == CellContentType_EMPTY )
	    aVal = EMPTYDOUBLE;
	else if ( aDataType != STRING )
	    aVal = xCell->getValue();
	
	rValsWithMissingData[nRowIdx] = aVal;
    }
}

void imputeOUString( std::vector<OUString>& rValsWithMissingData,
		     const DataType aDataType,
		     const std::unordered_set<sal_Int32>& rEmptyValIndexSet )
{
    if ( aDataType == STRING )
	imputeOUStringWithMode( rValsWithMissingData, aDataType, rEmptyValIndexSet );
}

void imputeDouble( std::vector<double>& rValsWithMissingData,
		   const DataType aDataType,
		   const std::unordered_set<sal_Int32>& rEmptyValIndexSet )
{
    if ( aDataType == DOUBLE )
	imputeDoubleWithMedian( rValsWithMissingData, aDataType, rEmptyValIndexSet );
    else if ( aDataType == INTEGER )
    {
	if ( !imputeDoubleWithMode( rValsWithMissingData, aDataType, rEmptyValIndexSet ) )
	    imputeDoubleWithMedian( rValsWithMissingData, aDataType, rEmptyValIndexSet );
    }
}


bool imputeOUStringWithMode( std::vector<OUString>& rValsWithMissingData,
			     const DataType aDataType,
			     const std::unordered_set<sal_Int32>& rEmptyValIndexSet )
{
    std::unordered_multiset<OUString, OUStringHash> aMultiSet;
    OUString aImputeVal;
    sal_Int32 nMaxCount = 0;
    for ( const auto aElement : rValsWithMissingData )
    {
	if ( aElement == EMPTYSTRING )
	    continue;

	aMultiSet.insert( aElement );
	sal_Int32 nCount = aMultiSet.count( aElement );
	if ( nCount > nMaxCount )
	{
	    aImputeVal = aElement;
	    nMaxCount = nCount;
	}
    }

    for ( sal_Int32 nMissingIdx : rEmptyValIndexSet )
	rValsWithMissingData[nMissingIdx] = aImputeVal;

    return true;
}

bool imputeDoubleWithMode( std::vector<double>& rValsWithMissingData,
			   const DataType aDataType,
			   const std::unordered_set<sal_Int32>& rEmptyValIndexSet )
{
    std::unordered_multiset<double> aMultiSet;
    double aImputeVal;
    sal_Int32 nMaxCount = 0;
    for ( const auto aElement : rValsWithMissingData )
    {
	if ( aElement == EMPTYDOUBLE )
	    continue;

	aMultiSet.insert( aElement );
	sal_Int32 nCount = aMultiSet.count( aElement );
	if ( nCount > nMaxCount )
	{
	    aImputeVal = aElement;
	    nMaxCount = nCount;
	}
    }

    bool bGood = true;
    if ( aDataType == INTEGER )
    {
	if ( nMaxCount < 2 ) // Ensure at least two samples of top class
	    bGood = false;
    }
    if ( bGood )
	for ( sal_Int32 nMissingIdx : rEmptyValIndexSet )
	    rValsWithMissingData[nMissingIdx] = aImputeVal;

    return bGood;
}

bool imputeOUStringWithMedian( std::vector<OUString>& rValsWithMissingData,
			       const DataType aDataType,
			       const std::unordered_set<sal_Int32>& rEmptyValIndexSet )
{
    OUString aMedian = getMedianOUString( rValsWithMissingData, aDataType, rEmptyValIndexSet.size() );
    sal_Int32 nNumEntries = rValsWithMissingData.size();
    for ( sal_Int32 nIdx : rEmptyValIndexSet )
	rValsWithMissingData[nIdx] = aMedian;
    return true;
}


bool imputeDoubleWithMedian( std::vector<double>& rValsWithMissingData,
			     const DataType aDataType,
			     const std::unordered_set<sal_Int32>& rEmptyValIndexSet )
{
    double aMedian = getMedianDouble( rValsWithMissingData, aDataType, rEmptyValIndexSet.size() );
    sal_Int32 nNumEntries = rValsWithMissingData.size();
    for ( sal_Int32 nIdx : rEmptyValIndexSet )
	rValsWithMissingData[nIdx] = aMedian;
    return true;
}

OUString getMedianOUString( const std::vector<OUString>& rValsWithMissingData,
			    const DataType aDataType,
			    const size_t nNumEmptyElements )
{
    std::vector<OUString> aCopy( rValsWithMissingData );
    std::sort( aCopy.begin(), aCopy.end() );
    size_t nElements = aCopy.size() - nNumEmptyElements;
    return aCopy[nNumEmptyElements + (nElements/2)];
}

double getMedianDouble( const std::vector<double>& rValsWithMissingData,
			const DataType aDataType,
			const size_t nNumEmptyElements )
{
    std::vector<double> aCopy( rValsWithMissingData );
    std::sort( aCopy.begin(), aCopy.end() );
    size_t nElements = aCopy.size() - nNumEmptyElements;
    double aMedian;

    if ( (nElements % 2 ) == 0 )
    {
	double fMed1 = aCopy[nNumEmptyElements + (nElements/2)];
	double fMed2 = aCopy[nNumEmptyElements + (nElements/2) - 1];
	aMedian = 0.5*( fMed1 + fMed2 );
    }
    else
	aMedian = aCopy[nNumEmptyElements + (nElements/2)];

    return aMedian;
}


sal_Bool setColColors( const Reference< XSpreadsheet >& rxSheet, const CellRangeAddress& rRange, const std::vector<Color>& rColBGColors )
{
    Reference< XCellRange > xCellRange( rxSheet, UNO_QUERY );
    if ( !xCellRange.is() )
    {
	logError("setColColors : cannot get xCellRange");
	return false;
    }

    try {
	for ( sal_Int32 nCol = rRange.StartColumn, nColIdx = 0; nCol <= rRange.EndColumn; ++nCol, ++nColIdx )
	{
	    Reference< XCellRange > xThisCol = xCellRange->getCellRangeByPosition( nCol, rRange.StartRow, nCol, rRange.EndRow );
	    Reference< XPropertySet > xPropSet( xThisCol, UNO_QUERY );
	    if ( !xPropSet.is() )
	    {
		printf("DEBUG>>> setColColors : Cannot get xPropSet for Column = %d !\n", nCol);
		fflush(stdout);
		continue;
	    }
	    xPropSet->setPropertyValue( "CellBackColor", makeAny( rColBGColors[nColIdx] ) );
	}
    }
    catch( Exception& e )
    {
	fprintf(stderr, "DEBUG>>> setColColors : caught UNO exception: %s\n",
		OUStringToOString( e.Message, RTL_TEXTENCODING_ASCII_US ).getStr());
	fflush(stderr);
	return false;
    }
    return true;
}
