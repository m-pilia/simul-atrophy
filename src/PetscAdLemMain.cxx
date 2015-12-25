#include "AdLem3D.h"
#include "GlobalConstants.h"

#include <iostream>
#include <sstream>
#include <petscsys.h>

#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>

#include <itkWarpImageFilter.h>
//#include "InverseDisplacementImageFilter.h"
#include <itkNearestNeighborInterpolateImageFunction.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkBSplineInterpolateImageFunction.h>
#include "itkLabelImageGenericInterpolateImageFunction.h"

#include <itkComposeDisplacementFieldsImageFilter.h>

#include <itkAbsoluteValueDifferenceImageFilter.h>
#include <itkStatisticsImageFilter.h>
#include <itkMultiplyImageFilter.h>

static char help[] = "Solves AdLem model. Equations solved: "
    " --------------------------------\n"
    " mu div(u) + grad(p)	= (mu + lambda) grad(a)\n"
    " div(u) + k.p		= -a\n"
    " --------------------------------\n"
    " k = 0 in brain tissue. Value of k is either 0 in CSF or non-zero, can be chosen by the user.\n"
    " Choosing non-zero k relaxes the Incompressibility Constraint (IC) in CSF.\n"
    "Arguments: \n"
    "-muFile			: Lame parameter mu image file. Use this if mu is spatially varying\n"
    "         If piecewise constant in tissue and in CSF, use -parameters instead.\n\n"
    "-parameters		: String that provides Lame parameters separated by a space in the "
    "following order		:\n"
    "             muBrain muCsf lambdaBrain lambdaCsf. E.g. -parameters "
    "'2.4 4.5 1.2 3'\n\n"
    "-boundary_condition	: Possible values: dirichlet_at_walls dirichlet_at_skull.\n\n"
    "--relax_ic_in_csf		: If given, relaxes IC, that is non-zero k. If not given, modifies"
    "\natrophy map distributing uniform volume change in CSF to compensate global volume change in"
    "\n other regions. \n\n"
    "-relax_ic_coeff		: value of k. If not given when using --relax_ic_in_csf, reciprocal of lambda is used.\n\n"
    "-atrophyFile		: Filename of a valid existing atrophy file that prescribes desired volume change.\n\n"
    "-maskFile			: Segmentation file that segments the image into CSF, tissue and non-brain regions.\n\n"
    "-imageFile			: Input image filename.\n\n"
    "-domainRegion		: Origin (in image coordinate => integer values) and size (in image coord => integer values) "
    "of the image region selected as computational domain.\n"
    "    x y z sx sy sz e.g. '0 0 0 30 40 50' Selects the region with origin at (0, 0, 0) and size (30, 40, 50) \n"
    "    If not provided uses full image regions.\n\n"
    "--useTensorLambda		: true or false. If true must provide a DTI image for lame parameter lambda.\n\n"
    "-lambdaFile		: filename of the DTI lambda-value image. Used when -useTensorlambda is true.\n\n"
    "-numOfTimeSteps		: number of time-steps to run the model.\n\n"
    "-resPath			: Path where all the results are to be placed.\n\n"
    "-resultsFilenamesPrefix	: Prefix to be added to all output files.\n\n"
    "--writePressure		: true or false; write/not write the pressure image file output.\n\n"
    "--writeForce		: true or false; write/not write the force image file output.\n\n"
    "--writeResidual		: true or false; write/not write the residual image file output.\n\n"
    ;

struct UserOptions {
    std::string atrophyFileName, maskFileName, lambdaFileName, muFileName;
    std::string baselineImageFileName;	//used only when debug priority is highest.

    unsigned int	domainOrigin[3], domainSize[3]; //Currently not taken from commaind line!
    bool		isDomainFullSize;

    std::string boundaryCondition;
    float	lameParas[4];	//muBrain, muCsf, lambdaBrain, lambdaCsf
    bool	relaxIcInCsf;
    float	relaxIcCoeff;	//compressibility coefficient k for CSF region.
    bool        useTensorLambda, isMuConstant;
    int         numOfTimeSteps;

    std::string resultsPath;    // Directory where all the results will be stored.
    std::string resultsFilenamesPrefix;	// Prefix for all the filenames of the results to be stored in the resultsPath.
    bool        writePressure, writeForce, writeResidual;
};


#undef __FUNCT__
#define __FUNCT_	"opsParser"
int opsParser(UserOptions &ops) {
/*
  Parse the options provided by the user and set relevant UserOptions variables.
 */
        PetscErrorCode	ierr;
        PetscBool	optionFlag = PETSC_FALSE;
        char		optionString[PETSC_MAX_PATH_LEN];
	ierr = PetscOptionsGetString(NULL,"-muFile",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
        if(optionFlag) {	//Use image option only when mu is not even piecewise constant.
	    ops.muFileName	   = optionString;
	    ops.isMuConstant	   = false;
	}
	else{//Provide parameters from options when mu is piecewise consant.
	    ops.muFileName = "";
	    ops.isMuConstant = true;
	}

	ierr = PetscOptionsGetString(NULL,"-parameters",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
        if(!optionFlag){ //If parameters not given.
	    if(ops.muFileName.empty()) throw "Must provide -parameters when -muFile not given\n";
	    //Set all paras to 1 but this isn't supposed to be used by the program.
	    //rather values should be used from muImageFileName.
	    for(int i=0; i<4; ++i){
		ops.lameParas[i] = 1.;
	    }
	}
	else {//Set values that will be used instead of the image.
	    std::stringstream parStream(optionString);
	    for(int i=0; i<4; ++i){
		parStream >> ops.lameParas[i];
	    }
	}

	// ---------- Set boundary condition option
	ierr = PetscOptionsGetString(NULL,"-boundary_condition",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
        if(!optionFlag) throw "Must provide a valid boundary condition. e.g. dirichlet_at_skull";
        ops.boundaryCondition = optionString;

	ierr = PetscOptionsGetString(NULL,"--relax_ic_in_csf",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
	ops.relaxIcInCsf = (bool)optionFlag;

	PetscReal optionReal;
	ierr = PetscOptionsGetReal(NULL, "-relax_ic_coeff", &optionReal, &optionFlag);CHKERRQ(ierr);
	if(optionFlag) ops.relaxIcCoeff = (float)optionReal;
	else ops.relaxIcCoeff = 0.;


	// ---------- Set input image files, computational region and other options.
	ierr = PetscOptionsGetString(NULL,"-atrophyFile",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
        if(optionFlag) ops.atrophyFileName = optionString;

        ierr = PetscOptionsGetString(NULL,"-maskFile",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
        if(optionFlag) ops.maskFileName = optionString;

        ierr = PetscOptionsGetString(NULL,"-imageFile",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
        if(optionFlag) ops.baselineImageFileName = optionString;

	ierr = PetscOptionsGetString(NULL,"-domainRegion",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
        if(optionFlag) {
	    ops.isDomainFullSize = false;
	    std::stringstream regionStream(optionString);
	    for(int i=0; i<3; ++i) regionStream >> ops.domainOrigin[i];
	    for(int i=0; i<3; ++i) regionStream >> ops.domainSize[i];
	}else ops.isDomainFullSize = true;

        ierr = PetscOptionsGetString(NULL,"--useTensorLambda",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
	ops.useTensorLambda = (bool)optionFlag;

        ierr = PetscOptionsGetInt(NULL,"-numOfTimeSteps",&ops.numOfTimeSteps,&optionFlag);CHKERRQ(ierr);
        if(!optionFlag) {
            ops.numOfTimeSteps = 1;
            PetscSynchronizedPrintf(PETSC_COMM_WORLD,"\n Using default number of steps: 1 since -numOfTimeSteps option was not used.\n");
        } else {
            PetscSynchronizedPrintf(PETSC_COMM_WORLD,"\n Model will be run for %d time steps\n", ops.numOfTimeSteps);
        }

	ierr = PetscOptionsGetString(NULL,"-lambdaFile",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
	if (ops.useTensorLambda) {
	    if(!optionFlag) throw "Must provide valid tensor image filename when using --useTensorLambda.\n";
	    ops.lambdaFileName = optionString;
	}
	else
	    if(optionFlag) throw "-lambdaFile option can be used only when --useTensorLambda is provided.\n";

	// ---------- Set output path and prefixes.
	ierr = PetscOptionsGetString(NULL,"-resPath",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
        if(!optionFlag) throw "Must provide a valid path with -resPath option: e.g. -resPath ~/results";
        ops.resultsPath = optionString;

        ierr = PetscOptionsGetString(NULL,"-resultsFilenamesPrefix",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
        if(!optionFlag) throw "Must provide a prefix for output filenames: e.g. -resultsFilenamesPrefix step1";
	ops.resultsFilenamesPrefix = optionString;

	// ---------- Set the choice of the output files to be written.
        ierr = PetscOptionsGetString(NULL,"--writePressure",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
	ops.writePressure = (bool)optionFlag;
        ierr = PetscOptionsGetString(NULL,"--writeForce",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
	ops.writeForce = (bool)optionFlag;
        ierr = PetscOptionsGetString(NULL,"--writeResidual",optionString,PETSC_MAX_PATH_LEN,&optionFlag);CHKERRQ(ierr);
	ops.writeResidual = (bool)optionFlag;
	return 0;

}

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{

    const unsigned int dim = 3;
    std::vector<double> wallVelocities(18);
    //TODO: Set from the user when dirichlet_at_walls boundary condition is used.
        /*0,1,2,		//south wall
       3,4,5,			//west wall
       6,7,8,			//north wall
       9,10,11,			//east wall
       12,13,14,		//front wall
       15,16,17			//back wall*/
    //    unsigned int wallPos		 = 6;
    //        wallVelocities.at(wallPos) = 1;

    PetscInitialize(&argc,&argv,(char*)0,help);
    {
	// ---------- Get user inputs
	UserOptions	ops;
	try {
	    opsParser(ops);
	}
	catch (const char* msg){
	    std::cerr<<msg<<std::endl;
	    return EXIT_FAILURE;
	}
        // ---------- Read input baseline image
        AdLem3D<dim>::ScalarImageType::Pointer baselineImage = AdLem3D<dim>::ScalarImageType::New();
        {
            AdLem3D<dim>::ScalarImageReaderType::Pointer   imageReader = AdLem3D<dim>::ScalarImageReaderType::New();
            imageReader->SetFileName(ops.baselineImageFileName);
            imageReader->Update();
            baselineImage = imageReader->GetOutput();
        }
        // ---------- Read input baseline brainMask image
        AdLem3D<dim>::IntegerImageType::Pointer baselineBrainMask = AdLem3D<dim>::IntegerImageType::New();
        {
            AdLem3D<dim>::IntegerImageReaderType::Pointer   imageReader = AdLem3D<dim>::IntegerImageReaderType::New();
            imageReader->SetFileName(ops.maskFileName);
            imageReader->Update();
            baselineBrainMask = imageReader->GetOutput();
        }
        // ---------- Read input atrophy map
        AdLem3D<dim>::ScalarImageType::Pointer baselineAtrophy = AdLem3D<dim>::ScalarImageType::New();
        {
            AdLem3D<dim>::ScalarImageReaderType::Pointer   imageReader = AdLem3D<dim>::ScalarImageReaderType::New();
            imageReader->SetFileName(ops.atrophyFileName);
            imageReader->Update();
            baselineAtrophy = imageReader->GetOutput();
        }

	// ---------- Set up output prefix with proper path
	std::string filesPref(ops.resultsPath+ops.resultsFilenamesPrefix);

	AdLem3D<dim>		AdLemModel;
	try { // ---------- Set up the model parameters
	    AdLemModel.setBoundaryConditions(ops.boundaryCondition, ops.relaxIcInCsf, ops.relaxIcCoeff);
	    if(AdLemModel.getBcType() == AdLem3D<dim>::DIRICHLET_AT_WALLS)
		AdLemModel.setWallVelocities(wallVelocities);
	    AdLemModel.setLameParameters(
		ops.isMuConstant, ops.useTensorLambda, ops.lameParas[0], ops.lameParas[1], ops.lameParas[2],
		ops.lameParas[3], ops.lambdaFileName, ops.muFileName);
	    AdLemModel.setBrainMask(baselineBrainMask, maskLabels::NBR, maskLabels::CSF);
	    // ---------- Set up the atrophy map
	    AdLemModel.setAtrophy(baselineAtrophy);

	    // ---------- Set the computational region (Can be set only after setting all required images!)
	    if(ops.isDomainFullSize)
		AdLemModel.setDomainRegionFullImage();
	    else
		AdLemModel.setDomainRegion(ops.domainOrigin, ops.domainSize);
	    if (!ops.relaxIcInCsf) {
		AdLemModel.prescribeUniformExpansionInCsf();
		baselineAtrophy = AdLemModel.getAtrophyImage();
		AdLemModel.writeAtrophyToFile(filesPref + "T0AtrophyModified.nii.gz");

	    }
	} catch(const char* msg) {
	    std::cerr<<msg<<std::endl;
	    return EXIT_FAILURE;
	}


        // ---------- Define itk types required for the warping of the mask and atrophy map:
        //typedef InverseDisplacementImageFilter<AdLem3D<dim>::VectorImageType> FPInverseType;
        typedef itk::WarpImageFilter<AdLem3D<dim>::ScalarImageType,AdLem3D<dim>::ScalarImageType,AdLem3D<dim>::VectorImageType> WarpFilterType;
	typedef itk::WarpImageFilter<AdLem3D<dim>::IntegerImageType,AdLem3D<dim>::IntegerImageType,AdLem3D<dim>::VectorImageType> IntegerWarpFilterType;
        typedef itk::NearestNeighborInterpolateImageFunction<AdLem3D<dim>::IntegerImageType> InterpolatorFilterNnType;
	typedef itk::LabelImageGenericInterpolateImageFunction<AdLem3D<dim>::ScalarImageType, itk::LinearInterpolateImageFunction> InterpolatorGllType; //General Label interpolator with linear interpolation.
        typedef itk::AbsoluteValueDifferenceImageFilter<AdLem3D<dim>::IntegerImageType,AdLem3D<dim>::IntegerImageType,AdLem3D<dim>::ScalarImageType> DiffImageFilterType;
        typedef itk::StatisticsImageFilter<AdLem3D<dim>::ScalarImageType> StatisticsImageFilterType;
        typedef itk::ComposeDisplacementFieldsImageFilter<AdLem3D<dim>::VectorImageType, AdLem3D<dim>::VectorImageType> VectorComposerType;
        AdLem3D<dim>::VectorImageType::Pointer composedDisplacementField; //declared outside loop because we need this for two different iteration steps.


	bool		isMaskChanged(true); //tracker flag to see if the brain mask is changed or not after the previous warp and NN interpolation.
        for (int t=1; t<=ops.numOfTimeSteps; ++t) {
            //-------------- Get the string for the current time step and add it to the prefix of all the files to be saved -----//
            std::stringstream timeStep;
            timeStep << t;
            std::string stepString("T"+timeStep.str());
            //------------- Modify atrophy map to adapt to the provided mask. ----------------//
	    // ---------- do the modification after the first step. That means I expect the atrophy map to be valid
	    // ---------- when input by the user. i.e. only GM/WM has atrophy and 0 on CSF and NBR regions.
            // ---------- Solve the system of equations
            AdLemModel.solveModel(isMaskChanged);
            // ---------- Write the solutions and residuals
            AdLemModel.writeVelocityImage(filesPref+stepString+"vel.nii.gz");
            AdLemModel.writeDivergenceImage(filesPref+stepString+"div.nii.gz");
            if (ops.writeForce) AdLemModel.writeForceImage(filesPref+stepString+"force.nii.gz");
            if (ops.writePressure) AdLemModel.writePressureImage(filesPref+stepString+"press.nii.gz");
            if (ops.writeResidual) AdLemModel.writeResidual(filesPref+stepString);

            // ---------- Compose velocity fields before inversion and the subsequent warping
            if(t == 1) { //Warp baseline image and write because won't be written later if ops.numOfTimeSteps=1.
                composedDisplacementField = AdLemModel.getVelocityImage();
		typedef itk::BSplineInterpolateImageFunction<AdLem3D<dim>::ScalarImageType> InterpolatorFilterType;
		InterpolatorFilterType::Pointer interpolatorFilter = InterpolatorFilterType::New();
		interpolatorFilter->SetSplineOrder(3);
                WarpFilterType::Pointer warper3 = WarpFilterType::New();
                warper3->SetDisplacementField(composedDisplacementField);
		warper3->SetInterpolator(interpolatorFilter);
                warper3->SetInput(baselineImage);
                warper3->SetOutputSpacing(baselineImage->GetSpacing());
                warper3->SetOutputOrigin(baselineImage->GetOrigin());
                warper3->SetOutputDirection(baselineImage->GetDirection());
                warper3->Update();
                AdLem3D<dim>::ScalarImageWriterType::Pointer imageWriter1 = AdLem3D<dim>::ScalarImageWriterType::New();
                imageWriter1->SetFileName(filesPref + "WarpedImageBspline" + stepString+ ".nii.gz");
                imageWriter1->SetInput(warper3->GetOutput());
                imageWriter1->Update();
            }
            else { // Compose the velocity field.
                VectorComposerType::Pointer vectorComposer = VectorComposerType::New();
                vectorComposer->SetDisplacementField(AdLemModel.getVelocityImage());
                vectorComposer->SetWarpingField(composedDisplacementField);
                vectorComposer->Update();
                composedDisplacementField = vectorComposer->GetOutput();
            }

            if(ops.numOfTimeSteps > 1) {

                //------------*** Invert the current displacement field to create warping field *** -------------//
//                FPInverseType::Pointer inverter1 = FPInverseType::New();
//                inverter1->SetInput(composedDisplacementField);
//                inverter1->SetErrorTolerance(1e-1);
//                inverter1->SetMaximumNumberOfIterations(50);
//                inverter1->Update();
//                std::cout<<"tolerance not reached for "<<inverter1->GetNumberOfErrorToleranceFailures()<<" pixels"<<std::endl;
//                AdLem3D<dim>::VectorImageType::Pointer warperField = inverter1->GetOutput();
                //----------------*** Let's not invert the field, rather assume the atrophy is provided to be negative
                //---------------- so that we can use the field obtained from the model itself as being already inverted.//
                AdLem3D<dim>::VectorImageType::Pointer warperField = composedDisplacementField;

                // ---------- Warp baseline brain mask with an itk warpFilter, nearest neighbor.
                // ---------- Using the inverted composed field.
                IntegerWarpFilterType::Pointer brainMaskWarper = IntegerWarpFilterType::New();
                brainMaskWarper->SetDisplacementField(warperField);
                brainMaskWarper->SetInput(baselineBrainMask);
                brainMaskWarper->SetOutputSpacing(baselineBrainMask->GetSpacing());
                brainMaskWarper->SetOutputOrigin(baselineBrainMask->GetOrigin());
                brainMaskWarper->SetOutputDirection(baselineBrainMask->GetDirection());

                InterpolatorFilterNnType::Pointer nnInterpolatorFilter = InterpolatorFilterNnType::New();
		InterpolatorGllType::Pointer gllInterpolator = InterpolatorGllType::New();
                brainMaskWarper->SetInterpolator(nnInterpolatorFilter);
		//brainMaskWarper->SetInterpolator(gllInterpolator);
                brainMaskWarper->Update();

                // ---------- Compare warped mask with the previous mask
                DiffImageFilterType::Pointer diffImageFilter = DiffImageFilterType::New();
                diffImageFilter->SetInput1(AdLemModel.getBrainMaskImage());
                diffImageFilter->SetInput2(brainMaskWarper->GetOutput());
                diffImageFilter->Update();

                StatisticsImageFilterType::Pointer statisticsImageFilter = StatisticsImageFilterType::New();
                statisticsImageFilter->SetInput(diffImageFilter->GetOutput());
                statisticsImageFilter->Update();
                if(statisticsImageFilter->GetSum() < 1) {
                    isMaskChanged = false;
                } else {
                    isMaskChanged = true;
                    AdLemModel.setBrainMask(brainMaskWarper->GetOutput(), maskLabels::NBR, maskLabels::CSF);
                    AdLemModel.writeBrainMaskToFile(filesPref+stepString+"Mask.nii.gz");
                }
                // ---------- Warp baseline atrophy with an itk WarpFilter, linear interpolation; using inverted composed field.
                WarpFilterType::Pointer atrophyWarper = WarpFilterType::New();
                atrophyWarper->SetDisplacementField(warperField);
                atrophyWarper->SetInput(baselineAtrophy);
                atrophyWarper->SetOutputSpacing(baselineAtrophy->GetSpacing());
                atrophyWarper->SetOutputOrigin(baselineAtrophy->GetOrigin());
                atrophyWarper->SetOutputDirection(baselineAtrophy->GetDirection());
                atrophyWarper->Update();
                AdLemModel.setAtrophy(atrophyWarper->GetOutput());
                //Atrophy present at the newly created CSF regions are redistributed to the nearest GM/WM tissues voxels.
		// And in CSF put the values as the ops.relaxIcInCsf dictates.
	        AdLemModel.modifyAtrophy(maskLabels::CSF, 0, true, ops.relaxIcInCsf);
		//AdLemModel.modifyAtrophy(maskLabels::CSF,0,false, ops.relaxIcInCsf); //no redistribution.
		AdLemModel.modifyAtrophy(maskLabels::NBR,0);  //set zero atrophy at non-brain region., don't change values elsewhere.
		AdLemModel.writeAtrophyToFile(filesPref+stepString+"AtrophyModified.nii.gz");

                // ---------- Warp the baseline image with the inverted composed field with BSpline interpolation
                typedef itk::BSplineInterpolateImageFunction<AdLem3D<dim>::ScalarImageType> InterpolatorFilterType;
		InterpolatorFilterType::Pointer interpolatorFilter = InterpolatorFilterType::New();
		interpolatorFilter->SetSplineOrder(3);
		WarpFilterType::Pointer baselineWarper = WarpFilterType::New();
                baselineWarper->SetDisplacementField(warperField);
		baselineWarper->SetInterpolator(interpolatorFilter);
                baselineWarper->SetInput(baselineImage);
                baselineWarper->SetOutputSpacing(baselineImage->GetSpacing());
                baselineWarper->SetOutputOrigin(baselineImage->GetOrigin());
                baselineWarper->SetOutputDirection(baselineImage->GetDirection());
                baselineWarper->Update();
                AdLem3D<dim>::ScalarImageWriterType::Pointer imageWriter = AdLem3D<dim>::ScalarImageWriterType::New();
                //Write with the stepString lying at the end of the filename so that other tool can combine all the images
                //into 4D by using the number 1, 2, 3 ... at the end.
                imageWriter->SetFileName(filesPref + "WarpedImageBspline" + stepString+ ".nii.gz");
                imageWriter->SetInput(baselineWarper->GetOutput());
                imageWriter->Update();
            }
        }
        AdLem3D<dim>::VectorImageWriterType::Pointer   displacementWriter = AdLem3D<dim>::VectorImageWriterType::New();
        displacementWriter->SetFileName(filesPref+"ComposedField.nii.gz");
        displacementWriter->SetInput(composedDisplacementField);
        displacementWriter->Update();
    }
    PetscErrorCode ierr;
    ierr = PetscFinalize();CHKERRQ(ierr);
    return 0;
}
