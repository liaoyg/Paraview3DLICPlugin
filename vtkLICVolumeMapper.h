#ifndef vtkLIC3DMapper_h
#define vtkLIC3DMapper_h

#include "vtkMapper.h"

class vtkActor;
class vtkDataSet;
class vtkImageData;
class vtkRenderer;

class VTK_EXPORT vtkLIC3DMapper : public vtkMapper
{
public:
	static vtkLIC3DMapper* New();
	vtkTypeMacro(vtkLIC3DMapper, vtkMapper);
	void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;
	
	// Set parameters for 3D LIC rendering
	//@{
	/**
	* Get/Set animation status. Default is true.
	*/
	virtual void SetAnimate(bool);
	vtkGetMacro(Animate, bool);
	vtkBooleanMacro(Animate, bool);
	//@}

	//@{
	/**
	* Get/Set the Alpha blending between new trajectory and previous.
	* Default is 0.95
	*/
	vtkSetMacro(Alpha, double);
	vtkGetMacro(Alpha, double);
	//@}

	//@{
	/**
	* Get/Set the integration step factor.
	* Default is 0.01
	*/
	vtkSetMacro(StepLength, double);
	vtkGetMacro(StepLength, double);
	//@}

	//@{
	/**
	* Get/Set the number of particles.
	* Default is 1000.
	*/
	void SetNumberOfParticles(int);
	vtkGetMacro(NumberOfParticles, int);
	//@}

	//@{
	/**
	* Get/Set the maximum number of iteration before particles die.
	* Default is 600.
	*/
	vtkSetMacro(MaxTimeToLive, int);
	vtkGetMacro(MaxTimeToLive, int);
	//@}

	//@{
	/**
	* Get/Set the maximum number of animation steps before the animation stops.
	* Default is 1.
	*/
	vtkSetMacro(NumberOfAnimationSteps, int);
	vtkGetMacro(NumberOfAnimationSteps, int);
	//@}

	/**
	* Returns if the mapper does not expect to have translucent geometry. This
	* may happen when using ColorMode is set to not map scalars i.e. render the
	* scalar array directly as colors and the scalar array has opacity i.e. alpha
	* component.  Default implementation simply returns true. Note that even if
	* this method returns true, an actor may treat the geometry as translucent
	* since a constant translucency is set on the property, for example.
	*/
	bool GetIsOpaque() VTK_OVERRIDE { return true; }

	/**
	* WARNING: INTERNAL METHOD - NOT INTENDED FOR GENERAL USE
	* DO NOT USE THIS METHOD OUTSIDE OF THE RENDERING PROCESS
	*/
	void Render(vtkRenderer* ren, vtkActor* vol) VTK_OVERRIDE;

	/**
	* WARNING: INTERNAL METHOD - NOT INTENDED FOR GENERAL USE
	* Release any graphics resources that are being consumed by this mapper.
	* The parameter window could be used to determine which graphic
	* resources to release.
	*/
	void ReleaseGraphicsResources(vtkWindow*) VTK_OVERRIDE;

protected:
	vtkLIC3DMapper();
	~vtkLIC3DMapper() override;

	// Rendering parameter variable
	double Alpha;
	double StepLength;
	int MaxTimeToLive;
	int NumberOfParticles;
	int NumberOfAnimationSteps;
	int AnimationSteps;
	bool Animate;

	class Private;
	Private* Internal;

	friend class Private;

	// see algorithm for more info
	int FillInputPortInformation(int port, vtkInformation* info) VTK_OVERRIDE;

	vtkLIC3DMapper(const vtkLIC3DMapper&) = delete;
	void operator=(const vtkLIC3DMapper&) = delete;
};

#endif

