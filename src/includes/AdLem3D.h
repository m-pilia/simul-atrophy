#ifndef ADLEM3D_H
#define ADLEM3D_H

class PetscAdLemTaras3D; //CAN'T seem to get away with this forward declaration!


#include<string>

#include <iostream>
#include <itkImage.hxx>
#include <itkDiffusionTensor3D.hxx>

#include <itkImageFileReader.hxx>
#include <itkImageFileWriter.hxx>

//#include<petscsys.h>

/* Linear Elastic Model/Viscous fluid model? 3D for AD deformation. This model contains parameters and
   inputs of the following system:
   div(mu grad(v)) - grad(p) = (mu + lambda)grad(a)
   div(v)          + c*p          = -a

   where,
   mu and lambda are the Lame coefficients,
   v is the velcoity/displacement for small time step,
   a is the atrophy,
   c is a function of position. Options:
   c=0 everywhere, to enforce incompressibility everywhere.
   c=non-zero, 1/lambda ? pressureMasscoeff; at places where one wants to release the incompressibility.
   We will use lambda as a symmetric +ve definte tensor image with
   tensor at each voxel that describes anisotropy of the computational
   domain.
*/


template <unsigned int DIM = 3>
    class AdLem3D{
public:
enum bcType {
    DIRICHLET_AT_WALLS, DIRICHLET_AT_SKULL
};

typedef typename itk::Image<double, DIM>                  ScalarImageType;
typedef typename itk::Image<int, DIM>                     IntegerImageType;
typedef typename itk::Image<itk::Vector<double,DIM>, DIM>   VectorImageType;
typedef typename itk::Image<itk::DiffusionTensor3D< double >, DIM>  TensorImageType;

typedef typename itk::ImageFileReader<IntegerImageType> IntegerImageReaderType;
typedef typename itk::ImageFileReader<ScalarImageType>  ScalarImageReaderType;
typedef typename itk::ImageFileReader<TensorImageType>  TensorImageReaderType;
typedef typename itk::ImageFileWriter<ScalarImageType>  ScalarImageWriterType;
typedef typename itk::ImageFileWriter<IntegerImageType>  IntegerImageWriterType;
typedef typename itk::ImageFileWriter<VectorImageType>  VectorImageWriterType;
typedef typename itk::ImageFileWriter<TensorImageType>  TensorImageWriterType;

AdLem3D();
~AdLem3D();

int getXnum() const;
int getYnum() const;
int getZnum() const;


double getXspacing() const;
double getYspacing() const;
double getZspacing() const;

// ---------- Boundary condition related functions
void setBoundaryConditions(const std::string& boundaryCondition, bool relaxIcInCsf, float relaxIcPressureCoeff=0.,
			   bool zeroVelAtFalx=false, bool slidingAtFalx=false, int falxZeroVelDir=0);
bcType getBcType() const;
void setWallVelocities(std::vector<double>& wallVelocities);
void getWallVelocities(std::vector<double>& wallVelocities); //copies mWallVelocities content.

//--***********Model parameters related functions***************//
void setLameParameters(bool isMuConstant, bool useTensorLambda,
		       double muBrain = 1, double muCsf = 1,
		       double lambdaBrain = 1, double lambdaCsf = 1,
		       std::string lambdaImageFile = "", std::string muImageFile = "");
bool isMuConstant() const;
bool isLambdaTensor() const;

double getMuBrain() const;
double getMuCsf() const;
double getLambdaBrain() const;
double getLambdaCsf() const;


// ---------- Default label values denotes that these labels are irrelevant for the selected boundary conditions.
int setBrainMask(std::string maskImageFile, int skullLabel = -1, int relaxIcLabel = -1, int falxCerebriLabel = -1);
int setBrainMask(typename IntegerImageType::Pointer brainMask, int skullLabel = -1, int relaxIcLabel = -1, int falxCerebriLabel = -1);
typename IntegerImageType::Pointer getBrainMaskImage();
void writeBrainMaskToFile(std::string fileName);

bool noLameInRhs() const; //Return whether to put (mu+lambda) in RHS of the momentum equation or not.
bool relaxIcInCsf() const; //Return whether relax IC in CSF or not.
float getRelaxIcPressureCoeff() const; //The value of k in the eqn div(u) + kp = 0.

bool zeroVelAtFalx() const; //Return whether set Falx Cerebri to zero or not.
bool slidingAtFalx() const; //Return whether set sliding boundary condition at Falx or not.
int getFalxSlidingZeroVelDir() const; //Return the component (dir) in which velocity is to be set zero when using sliding condition.


//string should be either of "mu", "lambda" or "atrophy".
//(Mi,Mj) is the element position of a tensor. Currently lambda can be a tensor.
// Top left position is: (Mi,Mj)=(0,0).
double dataAt(std::string dType, int x, int y, int z, unsigned int Mi, unsigned int Mj) const;
double brainMaskAt(int x, int y, int z) const; //returns int unlike dataAt()

int getRelaxIcLabel() const;  //Return the label present in BrainMask where the IC is to be relaxed.
//Relaxes IC on those region where maskImageFile has values relaxIcLabel.
//Normally, relaxIcPressureCoeff should be non-zero.
//If you do want to relax IC anywhere, set relaxIcPressureCoeff to zero.

int getSkullLabel() const;

int getFalxCerebriLabel() const;
//All images intended to be used by the model must already be set before calling
//setDomainRegionXX fxs.
void setDomainRegionFullImage(); //sets largestPossibleRegion.
//Selected region. This will itkExtractImageFilter and all the image pointers
//will then point to the output of this filter.
void setDomainRegion(unsigned int origin[3], unsigned int size[3]);

//solver related functions
void solveModel(bool noLameInRhs=false, bool tarasUse12pointStencilForDiv=false, bool operatorChanged = false);
typename VectorImageType::Pointer getVelocityImage();
typename ScalarImageType::Pointer getPressureImage();
typename ScalarImageType::Pointer getDivergenceImage();
typename VectorImageType::Pointer getForceImage();

void writeSolutionForMatlab(std::string resultsPath,
			    bool writeSystemSolution = false);
void writeVelocityImage(std::string fileName);
void writePressureImage(std::string fileName);
void writeDivergenceImage(std::string fileName);
void writeForceImage(std::string fileName);
void writeResidual(std::string fileName);

//Atrophy related functions
void setAtrophy(typename ScalarImageType::Pointer inputAtrophy);
void setAtrophy(std::string atrophyImageFile);
bool isAtrophySumZero(double sumMaxValue);
typename ScalarImageType::Pointer getAtrophyImage();

//If redistributeatrophy:
//     Distribute uniformly the non-zero atrophy values in CSF regions to the nearest tissue volumes (if in 3X3 neigborhood)
//     Useful when interpolation during warping of the atrophy map leaks some of the atrophy values from the brain tissue
//     to the neighboring CSF voxels.
//If not relaxICIncsf:
//          call prescribeUniformexpansionincsf() (maskValue has no use when this is true)
//else:
//          set maskValue in the atrophy map in the regions where brainMask has label maskLabel.
void modifyAtrophy(int maskLabel, double maskValue, bool redistributeAtrophy = false, bool relaxIcInCsf = true);
void scaleAtrophy(double factor);
void prescribeUniformExpansionInCsf();//Set atrophy_in_csf  = - total_atrophy_elsewhere/num_of_csf_voxels.
void writeAtrophyToFile(std::string fileName);

protected:
typename IntegerImageType::Pointer    mBrainMask;         //Input segmentation.
bool                        mIsBrainMaskSet;

//Choice to use Lame parameters in RHS or not.
bool mNoLameInRhs;

//strictly follow incompressibilty constraint(IC) only at non-CSF parts.
bool	mRelaxIcInCsf;
float	mRelaxIcPressureCoeff;	//Non-zero = > IC is relaxed at the regions where brainMask has the value mRelaxIcLabel.
int		mRelaxIcLabel;  //Label value in the brainMask where the IC is to be relaxed. (usually CSF regions)
int		mSkullLabel;	//Label value in the brainMask where the velocity will be imposed to be zero(usually non-brain regions!)

//Boundary condtions on Falx cerebri
bool	 mZeroVelAtFalx, mSlidingAtFalx; //If mZeroVelAtFalx then set v=0, elseif mSlidingAtFalx then set one component v_c = 0
int	 mFalxZeroVelDir; //Direction to choose the component v_c to set it to zero when mSlidingAtFalx is true.
int	 mFalxCerebriLabel;

typename ScalarImageType::RegionType	mDomainRegion;	//Set this only when all the images
// that are going to be used in the model are set! This will be used to
//extract the desired region will be extracted the corresponding images.

typename ScalarImageType::Pointer    mAtrophy;	//Input atrophy-map.
bool			mIsAtrophySet; //true when mAtrophy is set.

//parameters for Gray matter, White matter and Csf:
bool			mUseTensorLambda, mUseMuImage;
typename TensorImageType::Pointer    mLambda;	//Input diffusion tensor image lambda.
bool			mIsLambdaImageSet;	//true when mLambda is set.
typename ScalarImageType::Pointer    mMu;	//Input Mu image; used when isMuConstant is false.
bool			mIsMuImageSet;	// true when mMu is set.

double			mMuBrain, mMuCsf;
double			mLambdaBrain, mLambdaCsf;
bool			mIsMuConstant;	//piecewise constant can have different mu values in tissue and CSF. Currently mUseMuImage has the same value as mIsMuConstant. This could change later!

//Boundary condition:
AdLem3D::bcType     mBc;
bool		mIsBcSet;
std::vector<double> mWallVelocities;	//velocity components vx,vy,vz on S,W,N,E,F,B walls.

// integer number to keep track of how many times the solver is solved.
int mNumOfSolveCalls;


//    VectorImageType::Pointer    mAtrophyGradient;   //gradient of the atrophy computed within the solver
typename VectorImageType::Pointer    mForce;             //Force term computed i.e. lambda*grad(a)
typename ScalarImageType::Pointer    mPressure;          //Output pressure-map.
typename VectorImageType::Pointer    mVelocity;          //Output velocity field.
typename ScalarImageType::Pointer    mDivergence;        //Divergence map of the solution (computed by the solver)

// Memory for different result images needed to be allocated only once.
bool                        mVelocityAllocated;
bool                        mPressureAllocated;
bool                        mForceAllocated;
bool                        mDivergenceAllocated;

// Variables to track whether the result images have been updated after the most recent
// solve.
bool                        mVelocityLatest;
bool                        mPressureLatest;
bool                        mForceLatest;
bool                        mDivergenceLatest;

//Solver option
PetscAdLemTaras3D   *mPetscSolverTaras;
bool                mPetscSolverTarasUsed;

// Hide this from the user, user must use setLameParameters interface to set lambda.
// This method will be called by setLameParameters depending on user's argument to that
// method.
void setLambda(typename TensorImageType::Pointer inputLambda);
void setMu(typename ScalarImageType::Pointer inputMu);

// Internal data access utility methods.
// The class that inherits will have to provide public interface to these by
// properly adapting to the grid type they use.
double muAt(int x, int y, int z) const;
// Li and Lj are by default zero.
// If mUseTensorLambda is false and Li and Lj have default argumetns,
// then the method is implemented such a way that lambda is effectively a scalar.
double lambdaAt(int x, int y, int z,
		unsigned int Li = 0, unsigned int Lj = 0) const;
double aAt(int x, int y, int z) const;


void updateStateAfterSolveCall(); // update all the state and track variables that depend on
// or should change once the solveModel() function is called.
void createVelocityImage();
void createPressureImage();
void createForceImage();
void createDivergenceImage();

void updateImages(const std::string& whichImage);
};

#include "AdLem3D.hxx"

#endif // ADLEM3D_H
