#ifndef _RUNGEKUTTA2_H_
#define _RUNGEKUTTA2_H_

#include "TimeStepper.h"
#include "State.h"

namespace ibpm {

/*!
    \file RungeKutta2.h
    \class RungeKutta2

    \brief Timestepper using RK2 for nonlinear terms
    
    Uses Crank-Nicolson for linear terms.  Uses the scheme given by Peyret, p. 148[3], for alpha=1, beta=1/2:  
    \f{align}
    (1 - \frac{h}{2}L)\omega_1 + hBf_1 &=
        (1+\frac{h}{2}L)\omega^n + h N(q^n)\\
    C\omega_1 &= b_{n+1} \\
    (1 - \frac{h}{2}L)\omega^{n+1} + hBf^{n+1} &=
        (1+\frac{h}{2}L)\omega^n + \frac{h}{2}(N(q^n)+N(q_1))\\
    C\omega^{n+1} &= b_{n+1}
    \f}  

    \author Steve Brunton
    \author $LastChangedBy$
    \date  28 Aug 2008
    \date $LastChangedDate$
    \version $Revision$
*/

class RungeKutta2 : public TimeStepper {
public:

    /// Instantiate an RK2 solver
    RungeKutta2( Grid& grid, Model& model, double timestep );

    /// Destructor
    ~RungeKutta2();

    void init();
    bool load(const std::string& basename);
    bool save(const std::string& basename);

    /// Advance the state forward one step, using RK2
    void advance(State& x);
private:
    ProjectionSolver* _solver;
    State _x1;
};

} // namespace ibpm

#endif /* _RUNGEKUTTA2_H_ */

