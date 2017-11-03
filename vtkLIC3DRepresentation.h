#ifndef vtkLIC3DRepresentation_h
#define vtkLIC3DRepresentation_h

#include "vtkNew.h"
#include "vtkPVDataRepresentation.h"

class vtkColorTransferFunction;
class vtkExtentTranslator;
class vtkImageData;
class vtkInformation;
class vtkInformationRequestKey;
class vtkPExtentTranslator;
class vtkPiecewiseFunction;
class vtkPolyDataMapper;
class vtkProperty;
class vtkPVCacheKeeper;
class vtkPVLODActor;
class vtkVolume;
class vtkScalarsToColors;
class vtkLIC3DMapper;
class vtkProjectedTetrahedraMapper;
class vtkPVLODVolume;
class vtkVolumeProperty;
class vtkResampleToImage;

class VTK_EXPORT vtkLIC3DRepresentation : public vtkPVDataRepresentation
{
public:
	static vtkLIC3DRepresentation* New();
	vtkTypeMacro(vtkLIC3DRepresentation, vtkPVDataRepresentation);
	void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

	/**
	* vtkAlgorithm::ProcessRequest() equivalent for rendering passes. This is
	* typically called by the vtkView to request meta-data from the
	* representations or ask them to perform certain tasks e.g.
	* PrepareForRendering.
	*/
	int ProcessViewRequest(vtkInformationRequestKey*, vtkInformation*, vtkInformation*) VTK_OVERRIDE;

	/**
	* This needs to be called on all instances of vtkGeometryRepresentation when
	* the input is modified. This is essential since the geometry filter does not
	* have any real-input on the client side which messes with the Update
	* requests.
	*/
	void MarkModified() VTK_OVERRIDE;

	/**
	* Get/Set the visibility for this representation. When the visibility of
	* representation of false, all view passes are ignored.
	*/
	void SetVisibility(bool val) VTK_OVERRIDE;

	//***************************************************************************
	// Forwarded to vtkProperty.
	/*virtual void SetAmbientColor(double r, double g, double b);
	virtual void SetColor(double r, double g, double b);
	virtual void SetDiffuseColor(double r, double g, double b);
	virtual void SetEdgeColor(double r, double g, double b);
	virtual void SetInterpolation(int val);
	virtual void SetLineWidth(double val);
	virtual void SetOpacity(double val);
	virtual void SetPointSize(double val);
	virtual void SetSpecularColor(double r, double g, double b);
	virtual void SetSpecularPower(double val);
*/
	//***************************************************************************
	// Forwarded to Actor.
	virtual void SetOrientation(double, double, double);
	virtual void SetOrigin(double, double, double);
	virtual void SetPickable(int val);
	virtual void SetPosition(double, double, double);
	virtual void SetScale(double, double, double);
	virtual void SetUserTransform(const double[16]);

	//***************************************************************************
	// Forwarded to vtkVolumeProperty and vtkProperty (when applicable).
	void SetInterpolationType(int val);
	void SetColor(vtkColorTransferFunction* lut);
	void SetScalarOpacity(vtkPiecewiseFunction* pwf);
	void SetScalarOpacityUnitDistance(double val);

	//***************************************************************************
	// Forwarded to vtkStreamLinesMapper
	//virtual void SetAnimate(bool val);
	//virtual void SetAlpha(double val);
	//virtual void SetStepLength(double val);
	//virtual void SetNumberOfParticles(int val);
	//virtual void SetMaxTimeToLive(int val);
	//virtual void SetNumberOfAnimationSteps(int val);

	//virtual void SetInputVectors(int, int, int, int attributeMode, const char* name);

	//***************************************************************************
	// Forwarded to Mapper and LODMapper.
	//virtual void SetInterpolateScalarsBeforeMapping(int val);
	//virtual void SetLookupTable(vtkScalarsToColors* val);

	//@{
	/**
	* Sets if scalars are mapped through a color-map or are used
	* directly as colors.
	* 0 maps to VTK_COLOR_MODE_DIRECT_SCALARS
	* 1 maps to VTK_COLOR_MODE_MAP_SCALARS
	* @see vtkScalarsToColors::MapScalars
	*/
	//void SetMapScalars(int val);

	/**
	* Fill input port information.
	*/
	int FillInputPortInformation(int port, vtkInformation* info) VTK_OVERRIDE;

	/**
	* Provides access to the actor used by this representation.
	*/
	vtkPVLODVolume* GetActor() { return this->Volume; }

	/**
	* Convenience method to get the array name used to scalar color with.
	*/
	const char* GetColorArrayName();

	// Description:
	// Set the input data arrays that this algorithm will process. Overridden to
	// pass the array selection to the mapper.
	void SetInputArrayToProcess(
		int idx, int port, int connection, int fieldAssociation, const char* name) VTK_OVERRIDE;
	void SetInputArrayToProcess(
		int idx, int port, int connection, int fieldAssociation, int fieldAttributeType) VTK_OVERRIDE
	{
		this->Superclass::SetInputArrayToProcess(
			idx, port, connection, fieldAssociation, fieldAttributeType);
	}
	void SetInputArrayToProcess(int idx, vtkInformation* info) VTK_OVERRIDE
	{
		this->Superclass::SetInputArrayToProcess(idx, info);
	}
	void SetInputArrayToProcess(int idx, int port, int connection, const char* fieldAssociation,
		const char* attributeTypeorName) VTK_OVERRIDE
	{
		this->Superclass::SetInputArrayToProcess(
			idx, port, connection, fieldAssociation, attributeTypeorName);
	}
protected:
	vtkLIC3DRepresentation();
	~vtkLIC3DRepresentation() override;

	int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) VTK_OVERRIDE;

	/**
	* Adds the representation to the view.  This is called from
	* vtkView::AddRepresentation().  Subclasses should override this method.
	* Returns true if the addition succeeds.
	*/
	bool AddToView(vtkView* view) VTK_OVERRIDE;

	/**
	* Removes the representation to the view.  This is called from
	* vtkView::RemoveRepresentation().  Subclasses should override this method.
	* Returns true if the removal succeeds.
	*/
	bool RemoveFromView(vtkView* view) VTK_OVERRIDE;

	/**
	* Overridden to check with the vtkPVCacheKeeper to see if the key is cached.
	*/
	bool IsCached(double cache_key) VTK_OVERRIDE;

	/**
	* Passes on parameters to the active mapper
	*/
	void UpdateMapperParameters();

	/**
	* Used in ConvertSelection to locate the rendered prop.
	*/
	//virtual vtkPVLODActor* GetRenderedProp() { return this->Actor; };

	vtkImageData* Cache;
	vtkAlgorithm* MBMerger;
	vtkPVCacheKeeper* CacheKeeper;
	//vtkLIC3DMapper* LICMapper;
	//vtkProperty* Property;
	//vtkPVLODActor* Actor;

	vtkProjectedTetrahedraMapper* RayCastMapper;
	vtkVolumeProperty* VolProperty;
	vtkPVLODVolume* Volume;

	vtkResampleToImage* ResampleToImageFilter;

	unsigned long DataSize;
	double DataBounds[6];

	// meta-data about the input image to pass on to render view for hints
	// when redistributing data.
	vtkNew<vtkPExtentTranslator> PExtentTranslator;
	double Origin[3];
	double Spacing[3];
	int WholeExtent[6];

private:
	vtkLIC3DRepresentation (const vtkLIC3DRepresentation&) = delete;
	void operator=(const vtkLIC3DRepresentation&) = delete;
};

#endif // ! vtkLIC3DRepresentation_h

