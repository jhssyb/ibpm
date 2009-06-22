/*!
\file ibpm.cc

\brief Sample main routine for IBFS code

\author Clancy Rowley
\date  3 Jul 2008

$Revision$
$Date$
$Author$
$HeadURL$
*/

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include "ibpm.h"

using namespace std;
using namespace ibpm;

enum ModelType { LINEAR, NONLINEAR, ADJOINT, LINEARPERIODIC, INVALID };

// Return a solver of the appropriate type (e.g. Euler, RK2 )
TimeStepper* GetSolver(
    Grid& grid,
    NavierStokesModel& model,
    double dt,
    string solverType
);

// Return the type of model specified in the string modelName
ModelType str2model( string modelName );

/*! \brief Main routine for IBFS code
 *  Set up a timestepper and advance the flow in time.
 */
int main(int argc, char* argv[]) {
    cout << "Immersed Boundary Projection Method (IBPM), version "
        << IBPM_VERSION << endl;

    // Get parameters
    ParmParser parser( argc, argv );
    bool helpFlag = parser.getFlag( "h", "print this help message and exit" );
    string name = parser.getString( "name", "run name", "ibpm" );
    int nx = parser.getInt(
        "nx", "number of gridpoints in x-direction", 200 );
    int ny = parser.getInt(
        "ny", "number of gridpoints in y-direction", 200 );
    int ngrid = parser.getInt(
        "ngrid", "number of grid levels for multi-domain scheme", 1 );
    double length = parser.getDouble(
        "length", "length of finest domain in x-dir", 4.0 );
    double xOffset = parser.getDouble(
        "xoffset", "x-coordinate of left edge of finest domain", -2. );
    double yOffset = parser.getDouble(
        "yoffset", "y-coordinate of bottom edge of finest domain", -2. );
	double xShift = parser.getDouble( 
		"xshift", "percentage offset between grid levels in x-direction", 0. );
    double yShift = parser.getDouble(
		"yshift", "percentage offset between grid levels in y-direction", 0. );
    string geomFile = parser.getString(
        "geom", "filename for reading geometry", name + ".geom" );
    double Reynolds = parser.getDouble("Re", "Reynolds number", 100.);
    double dt = parser.getDouble( "dt", "timestep", 0.01 );
    string modelName = parser.getString(
        "model", "type of model (linear, nonlinear, adjoint, linearperiodic)", 
		"nonlinear" );
    string baseFlow = parser.getString(
        "baseflow", "base flow for linear/adjoint model", "" );
    string integratorType = parser.getString(
        "scheme", "timestepping scheme (euler,ab2,rk2,rk3)", "rk2" );
    string icFile = parser.getString( "ic", "initial condition filename", "");
    string outdir = parser.getString(
        "outdir", "directory for saving output", "." );
    int iTecplot = parser.getInt(
        "tecplot", "if >0, write a Tecplot file every n timesteps", 100);
    int iRestart = parser.getInt(
        "restart", "if >0, write a restart file every n timesteps", 100);
    int iForce = parser.getInt(
        "force", "if >0, write forces every n timesteps", 1);
    int numSteps = parser.getInt(
        "nsteps", "number of timesteps to compute", 250 );
	int period = parser.getInt(
		"period", "period of periodic baseflow", 1);
	int periodStart = parser.getInt(
		"periodstart", "start time of periodic baseflow", 0);
	string periodBaseFlowName = parser.getString(
		"pbaseflowname", "name of periodic baseflow, e.g. 'flow/ibpmperiodic%05d.bin', with '%05d' as time, decided by periodstart/period", "" );
    bool subtractBaseflow = parser.getBool(
		"subbaseflow", "Subtract ic by baseflow (1/0(true/false))", false);
	string numDigitInFileName = parser.getString(
		"numdigfilename", "number of digits for time representation in filename", "%05d");
	ModelType modelType = str2model( modelName );
    
    if ( ! parser.inputIsValid() || modelType == INVALID || helpFlag ) {
        parser.printUsage( cerr );
        exit(1);
    }
    
    if ( modelType != NONLINEAR ) {
		if (modelType != LINEARPERIODIC && baseFlow == "" ){
			cout << "ERROR: for linear or adjoint models, "
            "must specify a base flow" << endl;
			exit(1);
		}
		else if (modelType != LINEARPERIODIC && periodBaseFlowName != ""){
			cout << "WARNING: for linear or adjoint models, "
            "a periodic base flow is not needed" << endl;
			exit(1);
		}
		else if (modelType == LINEARPERIODIC && periodBaseFlowName == "" ) {
			cout << "ERROR: for linear periodic model, "
			"must specify a periodic base flow" << endl;
			exit(1);
		}
		else if (modelType == LINEARPERIODIC && baseFlow != "" ) {
			cout << "WARNING: for linear periodic model, "
            "a single baseflow is not needed" << endl;
			exit(1);
		}		
    }
	
    // create output directory if not already present
    AddSlashToPath( outdir );
    mkdir( outdir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO );

    // output command line arguments
    string cmd = parser.getParameters();
    cout << "Command:" << endl << cmd << endl;
    parser.saveParameters( outdir + name + ".cmd" );

    // Name of this run
    cout << "Run name: " << name << endl;

    // Setup grid
    cout << "Grid parameters:" << endl
        << "  nx      " << nx << endl
        << "  ny      " << ny << endl
        << "  ngrid   " << ngrid << endl
        << "  length  " << length << endl
        << "  xoffset " << xOffset << endl
		<< "  yoffset " << yOffset << endl
		<< "  xshift  " << xShift << endl
		<< "  yshift  " << yShift << endl;
    Grid grid( nx, ny, ngrid, length, xOffset, yOffset );
	grid.setXShift( xShift );
	grid.setYShift( yShift );

    // Setup geometry
    Geometry geom;
    cout << "Reading geometry from file " << geomFile << endl;
    if ( geom.load( geomFile ) ) {
        cout << "  " << geom.getNumPoints() << " points on the boundary" << endl;
    }
    else {
        exit(-1);
    }

    // Setup equations to solve
    cout << "Reynolds number = " << Reynolds << endl;
    double magnitude = 1;
    double alpha = 0;  // angle of background flow
    Flux q_potential = Flux::UniformFlow( grid, magnitude, alpha );
	
    cout << "Setting up Navier Stokes model..." << flush;
    NavierStokesModel* model = NULL;
	State x00( grid, geom.getNumPoints() );	
	switch (modelType){
		case NONLINEAR:		
			model = new NonlinearNavierStokes( grid, geom, Reynolds, q_potential);
			break;
		case LINEAR:
			x00.load( baseFlow );
			model = new LinearizedNavierStokes( grid, geom, Reynolds, x00 );
			break;			
		case ADJOINT:
			x00.load( baseFlow );
			model = new AdjointNavierStokes( grid, geom, Reynolds, x00 );
			break;			
		case LINEARPERIODIC:{
			// load periodic baseflow files
			vector<State> x0(period, x00);	
			char pbffilename[256];
			string pbf = periodBaseFlowName;
			for (int i=0; i < period; i++) {
				cout << "loading the " << i << "-th periodic baseflow:" << endl;
				sprintf(pbffilename, pbf.c_str(), i + periodStart);
				x0[i].load(pbffilename);					  
			}
			x00 = x0[0];
			model = new LinearizedPeriodicNavierStokes( grid, geom, Reynolds, x0, period);		
			break;
			}
		case INVALID:
			cout << "ERROR: must specify a valid modelType" << endl;
			exit(1);
			break;
	}
	assert( model != NULL );
    model->init();
	cout << "done" << endl;
	
    // Setup timestepper
    TimeStepper* solver = GetSolver( grid, *model, dt, integratorType );
    cout << "Using " << solver->getName() << " timestepper" << endl;
    cout << "  dt = " << dt << endl;
    if ( ! solver->load( outdir + name ) ) {
        solver->init();
        solver->save( outdir + name );
    }

    // Load initial condition
    State x( grid, geom.getNumPoints() );
    x.omega = 0.;
    x.f = 0.;
    x.q = 0.;
	if (icFile != "") {	
	    cout << "Loading initial condition from file: " << icFile << endl;
        if ( ! x.load(icFile) ) {
            cout << "  (failed: using zero initial condition)" << endl;
        }
		if ( subtractBaseflow == true ) {
            cout << "  Subtract initial condition by baseflow to form a linear initial perturbation" << endl;
			if (modelType != NONLINEAR) {
				assert((x.q).Ngrid() == (x00.q).Ngrid());
				assert((x.omega).Ngrid() == (x00.omega).Ngrid());
				x.q -= x00.q;
				x.omega -= x00.omega;
				x.f = 0;
			}
			else {
				cout << "Flag subbaseflow should be true only for linear cases"<< endl;
				exit(1);				
			}
		}
    }
    else {
        cout << "Using zero initial condition" << endl;
    }
	cout << "initial time = " << x.timestep << endl;
	
    // Setup output routines
    OutputTecplot tecplot( outdir + name + numDigitInFileName + ".plt", "Test run, step" +  numDigitInFileName);
    OutputRestart restart( outdir + name + numDigitInFileName + ".bin" );
    OutputForce force( outdir + name + ".force" ); 

    Logger logger;
    // Output Tecplot file every timestep
    if ( iTecplot > 0 ) {
        cout << "Writing Tecplot file every " << iTecplot << " steps" << endl;
        logger.addOutput( &tecplot, iTecplot );
    }
    if ( iRestart > 0 ) {
        cout << "Writing restart file every " << iRestart << " steps" << endl;
        logger.addOutput( &restart, iRestart );
    }
    if ( iForce > 0 ) {
        cout << "Writing forces every " << iForce << " steps" << endl;
        logger.addOutput( &force, iForce );
    }
    logger.init();
    logger.doOutput( x );
    cout << "Integrating for " << numSteps << " steps" << endl;

    for(int i=1; i <= numSteps; ++i) {
        cout << "step " << i << endl; 
		solver->advance( x );
        double lift;
        double drag;
        x.computeNetForce( drag, lift );
        cout << "x force : " << setw(16) << drag*2 << " , y force : "
            << setw(16) << lift*2 << "\n";
        logger.doOutput( x );
    }
    logger.cleanup();
    delete solver;
    delete model;
    return 0;
}

ModelType str2model( string modelName ) {
    ModelType type;
    MakeLowercase( modelName );
    if ( modelName == "nonlinear" ) {
        type = NONLINEAR;
    }
    else if ( modelName == "linear" ) {
        type = LINEAR;
    }
    else if ( modelName == "adjoint" ) {
        type = ADJOINT;
    }
	else if ( modelName == "linearperiodic" ) {
        type = LINEARPERIODIC;
    }
    else {
        cerr << "Unrecognized model: " << modelName << endl;
        type = INVALID;
    }
    return type;
}

TimeStepper* GetSolver(
    Grid& grid,
    NavierStokesModel& model,
    double dt,
    string solverType
    ) {
    MakeLowercase( solverType );
    if ( solverType == "euler" ) {
        return new Euler( grid, model, dt );
    }
    else if ( solverType == "ab2" ) {
        return new AdamsBashforth( grid, model, dt );
    }
    else if ( solverType == "rk2" ) {
        return new RungeKutta2( grid, model, dt );
    }
    else if ( solverType == "rk3" ) {
        return new RungeKutta3( grid, model, dt );
    }
    else {
        cerr << "ERROR: unrecognized solver: " << solverType << endl;
        exit(1);
    }
}

