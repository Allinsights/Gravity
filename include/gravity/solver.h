//
//  PTSolver.h
//  PowerTools++
//
//  Created by Hassan on 30/01/2015.
//  Copyright (c) 2015 NICTA. All rights reserved.
//

#ifndef __PowerTools____Solver__
#define __PowerTools____Solver__

#include <stdio.h>

#include <Gravity/GravityConfig.h>

#include <Gravity/model.h>
#ifdef USE_IPOPT
#include <Gravity/IpoptProgram.h>
#endif
#ifdef USE_GUROBI
#include <Gravity/GurobiProgram.h>
#endif
#ifdef USE_BONMIN
#include <Gravity/BonminProgram.h>
#endif

class PTSolver {
    
protected:
    union {
#ifdef USE_IPOPT
        IpoptProgram* ipopt_prog;
#endif
#ifdef USE_GUROBI
        GurobiProgram* grb_prog;
#endif
#ifdef USE_BONMIN
        BonminProgram* bonmin_prog;
#endif
    } prog;

public:
    SolverType                      _stype;
    /** Constructor */
    //@{
    PTSolver();

    PTSolver(Model* model, SolverType stype);
    //@}
    void set_model(Model* m);
    
    /* Destructor */
    ~PTSolver();
    
    int run(int output, bool relax);
};
#endif /* defined(__PowerTools____Solver__) */
