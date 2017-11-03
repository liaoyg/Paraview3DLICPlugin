#include "vtkLIC3DRepresentation.h"

#include "vtkAlgorithmOutput.h"
#include "vtkCellData.h"
#include "vtkColorTransferFunction.h"
#include "vtkCommand.h"
#include "vtkCompositeDataToUnstructuredGridFilter.h"
#include "vtkExtentTranslator.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPExtentTranslator.h"
#include "vtkPVCacheKeeper.h"
#include "vtkPVLODActor.h"
#include "vtkPVRenderView.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkSmartPointer.h"
#include "vtkLIC3DMapper.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkStructuredData.h"
#include "vtkTransform.h"
#include "vtkUnsignedCharArray.h"
#include "vtkProjectedTetrahedraMapper.h"
#include "vtkVolume.h"
#include "vtkPVLODVolume.h"
#include "vtkVolumeProperty.h"
#include "vtkResampleToImage.h"
#include "vtkPiecewiseFunction.h"

#include <algorithm>
#include <map>
#include <string>

namespace
{
	//----------------------------------------------------------------------------
	void vtkGetNonGhostExtent(int* resultExtent, vtkImageData* dataSet)
	{
		// this is really only meant for topologically structured grids
		dataSet->GetExtent(resultExtent);

		if (vtkUnsignedCharArray* ghostArray = vtkUnsignedCharArray::SafeDownCast(
			dataSet->GetCellData()->GetArray(vtkDataSetAttributes::GhostArrayName())))
		{
			// We have a ghost array. We need to iterate over the array to prune ghost
			// extents.

			int pntExtent[6];
			std::copy(resultExtent, resultExtent + 6, pntExtent);

			int validCellExtent[6];
			vtkStructuredData::GetCellExtentFromPointExtent(pntExtent, validCellExtent);

			// The start extent is the location of the first cell with ghost value 0.
			vtkIdType numTuples = ghostArray->GetNumberOfTuples();
			for (vtkIdType cc = 0; cc < numTuples; ++cc)
			{
				if (ghostArray->GetValue(cc) == 0)
				{
					int ijk[3];
					vtkStructuredData::ComputeCellStructuredCoordsForExtent(cc, pntExtent, ijk);
					validCellExtent[0] = ijk[0];
					validCellExtent[2] = ijk[1];
					validCellExtent[4] = ijk[2];
					break;
				}
			}

			// The end extent is the  location of the last cell with ghost value 0.
			for (vtkIdType cc = (numTuples - 1); cc >= 0; --cc)
			{
				if (ghostArray->GetValue(cc) == 0)
				{
					int ijk[3];
					vtkStructuredData::ComputeCellStructuredCoordsForExtent(cc, pntExtent, ijk);
					validCellExtent[1] = ijk[0];
					validCellExtent[3] = ijk[1];
					validCellExtent[5] = ijk[2];
					break;
				}
			}

			// convert cell-extents to pt extents.
			resultExtent[0] = validCellExtent[0];
			resultExtent[2] = validCellExtent[2];
			resultExtent[4] = validCellExtent[4];

			resultExtent[1] = std::min(validCellExtent[1] + 1, resultExtent[1]);
			resultExtent[3] = std::min(validCellExtent[3] + 1, resultExtent[3]);
			resultExtent[5] = std::min(validCellExtent[5] + 1, resultExtent[5]);
		}
	}
}

vtkStandardNewMacro(vtkLIC3DRepresentation);

vtkLIC3DRepresentation::vtkLIC3DRepresentation()
{
	//this->LICMapper = vtkLIC3DMapper::New();
	//this->Property = vtkProperty::New();
	//
	//this->Actor = vtkPVLODActor::New();
	//this->Actor->SetProperty(this->Property);
	//this->Actor->SetEnableLOD(0);

	this->ResampleToImageFilter = vtkResampleToImage::New();
	this->ResampleToImageFilter->SetSamplingDimensions(128, 128, 128);

	this->RayCastMapper = vtkProjectedTetrahedraMapper::New();
	this->Volume = vtkPVLODVolume::New();
	this->VolProperty = vtkVolumeProperty::New();
	this->Volume->SetProperty(this->VolProperty);
	this->Volume->SetEnableLOD(0);

	this->CacheKeeper = vtkPVCacheKeeper::New();

	this->Cache = vtkImageData::New();

	this->MBMerger = vtkCompositeDataToUnstructuredGridFilter::New();

	this->CacheKeeper->SetInputData(this->Cache);

	vtkMath::UninitializeBounds(this->DataBounds);
	this->DataSize = 0;

	this->Origin[0] = this->Origin[1] = this->Origin[2] = 0;
	this->Spacing[0] = this->Spacing[1] = this->Spacing[2] = 0;
	this->WholeExtent[0] = this->WholeExtent[2] = this->WholeExtent[4] = 0;
	this->WholeExtent[1] = this->WholeExtent[3] = this->WholeExtent[5] = -1;
}


vtkLIC3DRepresentation::~vtkLIC3DRepresentation()
{
	//this->LICMapper->Delete();
	//this->Property->Delete();
	//this->Actor->Delete();
	this->CacheKeeper->Delete();
	this->Cache->Delete();
	this->MBMerger->Delete();

	this->ResampleToImageFilter->Delete();
	this->RayCastMapper->Delete();
	this->VolProperty->Delete();
	this->Volume->Delete();
}

int vtkLIC3DRepresentation::FillInputPortInformation(int, vtkInformation* info)
{
	info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkUnstructuredGridBase");
	info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
	info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");
	info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
	return 1;
}

int vtkLIC3DRepresentation::ProcessViewRequest(
	vtkInformationRequestKey* request_type, vtkInformation* inInfo, vtkInformation* outInfo)
{
	if (!this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo))
	{
		this->MarkModified();
		return 0;
	}
	if (request_type == vtkPVView::REQUEST_UPDATE())
	{
		vtkPVRenderView::SetPiece(inInfo, this, this->CacheKeeper->GetOutputDataObject(0));
		// BUG #14792.
		// We report this->DataSize explicitly since the data being "delivered" is
		// not the data that should be used to make rendering decisions based on
		// data size.
		outInfo->Set(vtkPVRenderView::NEED_ORDERED_COMPOSITING(), 1);

		vtkPVRenderView::SetGeometryBounds(inInfo, this->DataBounds);

		// Pass partitioning information to the render view.
		vtkPVRenderView::SetOrderedCompositingInformation(inInfo, this,
			this->PExtentTranslator.GetPointer(), this->WholeExtent, this->Origin, this->Spacing);

		vtkPVRenderView::SetRequiresDistributedRendering(inInfo, this, true);
	}
	else if (request_type == vtkPVView::REQUEST_UPDATE_LOD())
	{
		vtkPVRenderView::SetRequiresDistributedRenderingLOD(inInfo, this, true);
	}
	else if (request_type == vtkPVView::REQUEST_RENDER())
	{
		this->UpdateMapperParameters();
	}

	this->MarkModified();

	return 1;
}

int vtkLIC3DRepresentation::RequestData(
	vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
	vtkMath::UninitializeBounds(this->DataBounds);
	this->DataSize = 0;
	this->Origin[0] = this->Origin[1] = this->Origin[2] = 0;
	this->Spacing[0] = this->Spacing[1] = this->Spacing[2] = 0;
	this->WholeExtent[0] = this->WholeExtent[2] = this->WholeExtent[4] = 0;
	this->WholeExtent[1] = this->WholeExtent[3] = this->WholeExtent[5] = -1;

	// Pass caching information to the cache keeper.
	this->CacheKeeper->SetCachingEnabled(this->GetUseCache());
	this->CacheKeeper->SetCacheTime(this->GetCacheKey());

	if (inputVector[0]->GetNumberOfInformationObjects() == 1)
	{
		vtkDataObject* inputDO = vtkDataObject::GetData(inputVector[0], 0);
		vtkDataSet* inputDS = vtkDataSet::SafeDownCast(inputDO);
		vtkImageData* inputImage = vtkImageData::SafeDownCast(inputDS);
		vtkMultiBlockDataSet* inputMB = vtkMultiBlockDataSet::SafeDownCast(inputDO);
		if (inputImage)
		{
			if (!this->GetUsingCacheForUpdate())
			{
				this->Cache->ShallowCopy(inputImage);
				if (inputImage->HasAnyGhostCells())
				{
					int ext[6];
					vtkGetNonGhostExtent(ext, this->Cache);
					// Yup, this will modify the "input", but that okay for now. Ultimately,
					// we will teach the volume mapper to handle ghost cells and this won't
					// be needed. Once that's done, we'll need to teach the KdTree
					// generation code to handle overlap in extents, however.
					this->Cache->Crop(ext);
				}
			}
			// Collect information about volume that is needed for data redistribution
			// later.
			this->PExtentTranslator->GatherExtents(inputImage);
			inputImage->GetOrigin(this->Origin);
			inputImage->GetSpacing(this->Spacing);
			vtkStreamingDemandDrivenPipeline::GetWholeExtent(
				inputVector[0]->GetInformationObject(0), this->WholeExtent);
		}
		else if (inputDS)
		{
			if (!this->GetUsingCacheForUpdate())
			{
				this->CacheKeeper->SetInputData(inputDS);
			}
		}
		else if (inputMB)
		{
			vtkCompositeDataToUnstructuredGridFilter::SafeDownCast(this->MBMerger)->SetInputData(inputMB);
			if (!this->GetUsingCacheForUpdate())
			{
				this->CacheKeeper->SetInputConnection(this->MBMerger->GetOutputPort());
			}
		}
		else
		{
			//this->ResampleToImageFilter->SetInputDataObject(inputDO);
			//this->CacheKeeper->SetInputConnection(this->ResampleToImageFilter->GetOutputPort(0));
			this->CacheKeeper->SetInputConnection(this->GetInternalOutputPort());
		}

		this->CacheKeeper->Update();
		//this->LICMapper->SetInputConnection(this->CacheKeeper->GetOutputPort());
		this->RayCastMapper->SetInputConnection(this->CacheKeeper->GetOutputPort());

		vtkDataSet* output = vtkDataSet::SafeDownCast(this->CacheKeeper->GetOutputDataObject(0));
		this->DataSize = output->GetActualMemorySize();
	}
	else
	{
		// when no input is present, it implies that this processes is on a node
		// without the data input i.e. either client or render-server.
		//this->LICMapper->RemoveAllInputs();
		this->RayCastMapper->RemoveAllInputs();
		this->Volume->SetEnableLOD(1);
	}

	return this->Superclass::RequestData(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
bool vtkLIC3DRepresentation::IsCached(double cache_key)
{
	return this->CacheKeeper->IsCached(cache_key);
}

//----------------------------------------------------------------------------
void vtkLIC3DRepresentation::MarkModified()
{
	if (!this->GetUseCache())
	{
		// Cleanup caches when not using cache.
		this->CacheKeeper->RemoveAllCaches();
	}
	this->Superclass::MarkModified();
}

//----------------------------------------------------------------------------
bool vtkLIC3DRepresentation::AddToView(vtkView* view)
{
	// FIXME: Need generic view API to add props.
	vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
	if (rview)
	{
		//rview->GetRenderer()->AddActor(this->Actor);
		rview->GetRenderer()->AddVolume(this->Volume);
		// Indicate that this is a prop to be rendered during hardware selection.
		return this->Superclass::AddToView(view);
	}
	return false;
}

//----------------------------------------------------------------------------
bool vtkLIC3DRepresentation::RemoveFromView(vtkView* view)
{
	vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
	if (rview)
	{
		//rview->GetRenderer()->RemoveActor(this->Actor);
		rview->GetRenderer()->RemoveActor(this->Volume);
		return this->Superclass::RemoveFromView(view);
	}
	return false;
}

//----------------------------------------------------------------------------
void vtkLIC3DRepresentation::UpdateMapperParameters()
{
	//this->Actor->SetMapper(this->LICMapper);
	//this->Actor->SetVisibility(1);
	const char* colorArrayName = NULL;
	int fieldAssociation = vtkDataObject::FIELD_ASSOCIATION_POINTS;

	vtkInformation* info = this->GetInputArrayInformation(0);
	if (info && info->Has(vtkDataObject::FIELD_ASSOCIATION()) &&
		info->Has(vtkDataObject::FIELD_NAME()))
	{
		colorArrayName = info->Get(vtkDataObject::FIELD_NAME());
		fieldAssociation = info->Get(vtkDataObject::FIELD_ASSOCIATION());
	}
	this->RayCastMapper->SelectScalarArray(colorArrayName);

	switch (fieldAssociation)
	{
	case vtkDataObject::FIELD_ASSOCIATION_CELLS:
		this->RayCastMapper->SetScalarMode(VTK_SCALAR_MODE_USE_CELL_FIELD_DATA);
		//this->LODMapper->SetScalarMode(VTK_SCALAR_MODE_USE_CELL_FIELD_DATA);
		break;

	case vtkDataObject::FIELD_ASSOCIATION_NONE:
		this->RayCastMapper->SetScalarMode(VTK_SCALAR_MODE_USE_FIELD_DATA);
		//this->LODMapper->SetScalarMode(VTK_SCALAR_MODE_USE_FIELD_DATA);
		break;

	case vtkDataObject::FIELD_ASSOCIATION_POINTS:
	default:
		this->RayCastMapper->SetScalarMode(VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
		//this->LODMapper->SetScalarMode(VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
		break;
	}

	this->Volume->SetMapper(this->RayCastMapper);
	this->Volume->SetVisibility(1);
}

//----------------------------------------------------------------------------
void vtkLIC3DRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
	this->Superclass::PrintSelf(os, indent);
}

//***************************************************************************
// Forwarded to vtkVolumeProperty.
//----------------------------------------------------------------------------
void vtkLIC3DRepresentation::SetInterpolationType(int val)
{
	this->VolProperty->SetInterpolationType(val);
}

//----------------------------------------------------------------------------
void vtkLIC3DRepresentation::SetColor(vtkColorTransferFunction* lut)
{
	this->VolProperty->SetColor(lut);
	//this->LODMapper->SetLookupTable(lut);
}

//----------------------------------------------------------------------------
void vtkLIC3DRepresentation::SetScalarOpacity(vtkPiecewiseFunction* pwf)
{
	this->VolProperty->SetScalarOpacity(pwf);
}

//----------------------------------------------------------------------------
void vtkLIC3DRepresentation::SetScalarOpacityUnitDistance(double val)
{
	this->VolProperty->SetScalarOpacityUnitDistance(val);
}

//***************************************************************************
// Forwarded to Property.
//----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetColor(double r, double g, double b)
//{
//	//this->Property->SetColor(r, g, b);
//}
//
////----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetLineWidth(double val)
//{
//	//this->Property->SetLineWidth(val);
//}
//
////----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetOpacity(double val)
//{
//	//this->Property->SetOpacity(val);
//}
//
////----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetPointSize(double val)
//{
//	//this->Property->SetPointSize(val);
//}
//
////----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetAmbientColor(double r, double g, double b)
//{
//	//this->Property->SetAmbientColor(r, g, b);
//}
//
////----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetDiffuseColor(double r, double g, double b)
//{
//	//this->Property->SetDiffuseColor(r, g, b);
//}
//
////----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetEdgeColor(double r, double g, double b)
//{
//	//this->Property->SetEdgeColor(r, g, b);
//}
//
////----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetInterpolation(int val)
//{
//	//this->Property->SetInterpolation(val);
//}
//
////----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetSpecularColor(double r, double g, double b)
//{
//	//this->Property->SetSpecularColor(r, g, b);
//}
//
////----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetSpecularPower(double val)
//{
//	//this->Property->SetSpecularPower(val);
//}

//***************************************************************************
// Forwarded to Actor.
//----------------------------------------------------------------------------
void vtkLIC3DRepresentation::SetVisibility(bool val)
{
	this->Superclass::SetVisibility(val);
	//this->Actor->SetVisibility(val ? 1 : 0);
	this->Volume->SetVisibility(val ? 1 : 0);
}

//----------------------------------------------------------------------------
void vtkLIC3DRepresentation::SetOrientation(double x, double y, double z)
{
	//this->Actor->SetOrientation(x, y, z);
	this->Volume->SetOrientation(x, y, z);
}

//----------------------------------------------------------------------------
void vtkLIC3DRepresentation::SetOrigin(double x, double y, double z)
{
	//this->Actor->SetOrigin(x, y, z);
	this->Volume->SetOrigin(x, y, z);
}

//----------------------------------------------------------------------------
void vtkLIC3DRepresentation::SetPickable(int val)
{
	//this->Actor->SetPickable(val);
	this->Volume->SetPickable(val);
}

//----------------------------------------------------------------------------
void vtkLIC3DRepresentation::SetPosition(double x, double y, double z)
{
	//this->Actor->SetPosition(x, y, z);
	this->Volume->SetPosition(x, y, z);
}

//----------------------------------------------------------------------------
void vtkLIC3DRepresentation::SetScale(double x, double y, double z)
{
	//this->Actor->SetScale(x, y, z);
	this->Volume->SetScale(x, y, z);
}

//----------------------------------------------------------------------------
void vtkLIC3DRepresentation::SetUserTransform(const double matrix[16])
{
	vtkNew<vtkTransform> transform;
	transform->SetMatrix(matrix);
	//this->Actor->SetUserTransform(transform.GetPointer());
	this->Volume->SetUserTransform(transform.GetPointer());
}

//***************************************************************************
// Forwarded to StreamLinesMapper.
//----------------------------------------------------------------------------
//
//void vtkLIC3DRepresentation::SetAnimate(bool val)
//{
//	this->LICMapper->SetAnimate(val);
//}
//
//----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetAlpha(double val)
//{
//	this->LICMapper->SetAlpha(val);
//}
//
//----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetStepLength(double val)
//{
//	this->LICMapper->SetStepLength(val);
//}
//
//----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetNumberOfParticles(int val)
//{
//	this->LICMapper->SetNumberOfParticles(val);
//}
//
//----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetMaxTimeToLive(int val)
//{
//	this->LICMapper->SetMaxTimeToLive(val);
//}
//
//----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetNumberOfAnimationSteps(int val)
//{
//	this->LICMapper->SetNumberOfAnimationSteps(val);
//}
//
//----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetInputVectors(
//	int vtkNotUsed(idx), int port, int connection, int fieldAssociation, const char* name)
//{
//	this->LICMapper->SetInputArrayToProcess(1, port, connection, fieldAssociation, name);
//}

//----------------------------------------------------------------------------
const char* vtkLIC3DRepresentation::GetColorArrayName()
{
	vtkInformation* info = this->GetInputArrayInformation(0);
	if (info && info->Has(vtkDataObject::FIELD_ASSOCIATION()) &&
		info->Has(vtkDataObject::FIELD_NAME()))
	{
		return info->Get(vtkDataObject::FIELD_NAME());
	}
	return NULL;
}

//****************************************************************************
// Methods merely forwarding parameters to internal objects.
//****************************************************************************

////----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetLookupTable(vtkScalarsToColors* val)
//{
//	this->LICMapper->SetLookupTable(val);
//}
//
////----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetMapScalars(int val)
//{
//	if (val < 0 || val > 1)
//	{
//		vtkWarningMacro(<< "Invalid parameter for vtkStreamLinesRepresentation::SetMapScalars: "
//			<< val);
//		val = 0;
//	}
//	int mapToColorMode[] = { VTK_COLOR_MODE_DIRECT_SCALARS, VTK_COLOR_MODE_MAP_SCALARS };
//	this->LICMapper->SetColorMode(mapToColorMode[val]);
//}
//
////----------------------------------------------------------------------------
//void vtkLIC3DRepresentation::SetInterpolateScalarsBeforeMapping(int val)
//{
//	this->LICMapper->SetInterpolateScalarsBeforeMapping(val);
//}

void vtkLIC3DRepresentation::SetInputArrayToProcess(
	int idx, int port, int connection, int fieldAssociation, const char* name)
{
	this->Superclass::SetInputArrayToProcess(idx, port, connection, fieldAssociation, name);

	if (idx == 1)
	{
		return;
	}

	//this->LICMapper->SetInputArrayToProcess(idx, port, connection, fieldAssociation, name);
	this->RayCastMapper->SetInputArrayToProcess(idx, port, connection, fieldAssociation, name);

	//if (name && name[0])
	//{
	//	this->LICMapper->SetScalarVisibility(1);
	//	this->LICMapper->SelectColorArray(name);
	//	this->LICMapper->SetUseLookupTableScalarRange(1);
	//	
	//}
	//else
	//{
	//	this->LICMapper->SetScalarVisibility(0);
	//	this->LICMapper->SelectColorArray(static_cast<const char*>(NULL));
	//}

	switch (fieldAssociation)
	{
	case vtkDataObject::FIELD_ASSOCIATION_CELLS:
		//this->LICMapper->SetScalarMode(VTK_SCALAR_MODE_USE_CELL_FIELD_DATA);
		this->RayCastMapper->SetScalarMode(VTK_SCALAR_MODE_USE_CELL_FIELD_DATA);
		break;

	case vtkDataObject::FIELD_ASSOCIATION_POINTS:
	default:
		//this->LICMapper->SetScalarMode(VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
		this->RayCastMapper->SetScalarMode(VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
		break;
	}
}