// Euler.cc
//
// Description:
// Implementation of the Euler class
//
// Author(s):
// Clancy Rowley
//
// Date: 2 Aug 2008
//
// $Revision$
// $LastChangedDate$
// $LastChangedBy$
// $HeadURL:// $Header$

#include "BoundaryVector.h"
#include "Geometry.h"
#include "State.h"
#include "NavierStokesModel.h"
#include "ProjectionSolver.h"
#include "TimeStepper.h"
#include "Euler.h"

Euler::Euler(const NavierStokesModel& model, double timestep) :
    TimeStepper(model, timestep),
    _linearTermEigenvalues( *(_model->getLambda()) ) {
    // compute eigenvalues of linear term on RHS
    _linearTermEigenvalues *= timestep/2.;
    _linearTermEigenvalues += 1;
    
    _solver = createSolver(timestep);
}

void Euler::advance(State& x) {
    // If the body is moving, update the positions of the bodies
    const Geometry* geom = _model->getGeometry();
    if ( ! geom->isStationary() ) {
        geom->moveBodies(x.time);
    }

    // Evaluate Right-Hand-Side (a) for first equation of ProjectionSolver
    Scalar a = _model->S( x.gamma );
    a *= _linearTermEigenvalues;
    a = _model->Sinv( a );

    Scalar nonlinear = _model->nonlinear( x );
    a += _timestep * nonlinear;
    
    // Evaluate Right-Hand-Side (b) for second equation of ProjecitonSolver
    BoundaryVector b = geom->getVelocities();
    
    // Call the ProjectionSolver to determine the circulation and forces
    _solver->solve( a, b, x.gamma, x.f );
    
    // Compute the corresponding flux
    _model->computeFlux( x.gamma, x.q );
    
    // Update the value of the time
    x.time += _timestep;
}