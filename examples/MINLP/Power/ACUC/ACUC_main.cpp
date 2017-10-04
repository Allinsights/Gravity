//
//  ACUC.cpp
//  Gravity
//
//  Created by Guanglei Wang on 6/9/17.
//
//
#include <stdio.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <cstring>
#include <fstream>
#include "../PowerNet.h"
#include <gravity/model.h>
#include <gravity/solver.h>
#include <stdio.h>
#include <stdlib.h>
using namespace std;
using namespace gravity;

int main (int argc, const char * argv[])
{
    // ACUC
    PowerNet* grid = new PowerNet();
    const char* fname;
    fname = "../../data_sets/Power/nesta_case5_pjm.m";
    grid->readgrid(fname);

    // Grid Parameters
    int nb_gen = grid->gens.size();
    int nb_lines = grid->arcs.size();
    int nb_buses = grid->nodes.size();

    // Schedule periods 
    unsigned T = 1;
    grid->c0.time_expand(T);
    grid->c1.time_expand(T);
    grid->c2.time_expand(T);
    grid->S_max.time_expand(T);
    grid->tan_th_min.time_expand(T);
    grid->tan_th_max.time_expand(T);
    grid->g_tt.time_expand(T);
    grid->g_ff.time_expand(T);
    grid->g_ft.time_expand(T);
    grid->g_tf.time_expand(T);
    grid->b_tt.time_expand(T);
    grid->b_ff.time_expand(T);
    grid->b_ft.time_expand(T);
    grid->b_tf.time_expand(T);
    grid->pg_min.time_expand(T);
    grid->pg_max.time_expand(T);
    grid->qg_min.time_expand(T);
    grid->qg_max.time_expand(T);
    grid->w_min.time_expand(T);
    grid->w_max.time_expand(T);

    grid->w_min.print(true);
    
    /** build model */
    Model ACUC("ACUC Model");

    /** Variables */
    // power generation
    //var<Real> Pg("Pg", grid->pg_min, grid->pg_max);
    var<Real> Pg("Pg", grid->pg_min.in(grid->gens, T), grid->pg_max.in(grid->gens, T));
    var<Real> Qg ("Qg",grid->qg_min.in(grid->gens, T), grid->qg_max.in(grid->gens, T));
    
    ACUC.add_var(Pg^(T*nb_gen));
    ACUC.add_var(Qg^(T*nb_gen));

    // power flow
    var<double> Pf_from("Pf_from");
    var<double> Qf_from("Qf_from");
    var<double> Pf_to("Pf_to");
    var<double> Qf_to("Qf_to");
    ACUC.add_var(Pf_from^(T*nb_lines));
    ACUC.add_var(Qf_from^(T*nb_lines));
    ACUC.add_var(Pf_to^(T*nb_lines));
    ACUC.add_var(Qf_to^(T*nb_lines));

    // Lifted variables.
    var<Real>  R_Wij("R_Wij");   // real part of Wij
    var<Real>  Im_Wij("Im_Wij"); // imaginary part of Wij.
    var<Real>  Wii("Wii", grid->w_min, grid->w_max);
    ACUC.add_var(Wii^(T*nb_buses));
    ACUC.add_var(R_Wij^(T*nb_lines));
    ACUC.add_var(Im_Wij^(T*nb_lines));
    
    /* Construct the objective function*/
    func_ obj;
    obj  = sum(grid->c0.in(grid->gens, T));
    obj += sum(grid->c1.in(grid->gens, T), Pg.in(grid->gens, T));
    obj += sum(grid->c2.in(grid->gens, T), power(Pg.in(grid->gens, T), 2));
    ACUC.set_objective(min(obj));
    
    grid->c1.in(grid->gens, T).print(true);

    /** Define constraints */
    /* SOCP constraints */
    Constraint SOC("SOC");
    SOC =  power(R_Wij.in(grid->arcs, T), 2) + power(Im_Wij.in(grid->arcs, T), 2) - Wii.from(grid->arcs, T)*Wii.to(grid->arcs, T) ;
    ACUC.add_constraint(SOC <= 0);
    //
    //KCL
    for (auto b: grid->nodes) {
        Bus* bus = (Bus*) b;
        Constraint KCL_P("KCL_P"+bus->_name);
        Constraint KCL_Q("KCL_Q"+bus->_name);

        /* Power Conservation */
        KCL_P  = sum(Pf_from.in(b->get_out(), T)) + sum(Pf_to.in(b->get_in(), T)) + bus->pl()- sum(Pg.in(bus->_gen, T));
        KCL_Q  = sum(Qf_from.in(b->get_out(), T)) + sum(Qf_to.in(b->get_in(), T)) + bus->ql()- sum(Qg.in(bus->_gen, T));

        /* Shunts */
        //KCL_P +=  bus->gs()*Wii(bus->_name);
        //KCL_Q -=  bus->bs()*Wii(bus->_name);

        ACUC.add_constraint(KCL_P = 0);
        ACUC.add_constraint(KCL_Q = 0);
    }

    //AC Power Flow.
    Constraint Flow_P_From("Flow_P_From");
    Flow_P_From += Pf_from.in(grid->arcs, T);
    Flow_P_From -= grid->g_ff.in(grid->arcs, T)*Wii.from(grid->arcs, T);
    Flow_P_From -= grid->g_ft.in(grid->arcs, T)*R_Wij.in(grid->arcs, T);
    Flow_P_From -= grid->b_ft.in(grid->arcs, T)*Im_Wij.in(grid->arcs, T);
    Flow_P_From = 0;
    ACUC.add_constraint(Flow_P_From);

    Constraint Flow_P_To("Flow_P_To");
    Flow_P_To += Pf_to.in(grid->arcs, T);
    Flow_P_To -= grid->g_tt.in(grid->arcs, T)*Wii.to(grid->arcs, T);
    Flow_P_To -= grid->g_tf.in(grid->arcs, T)*R_Wij.in(grid->arcs, T);
    Flow_P_To += grid->b_tf.in(grid->arcs, T)*Im_Wij.in(grid->arcs, T);
    Flow_P_To = 0;
    ACUC.add_constraint(Flow_P_To);

    Constraint Flow_Q_From("Flow_Q_From");
    Flow_Q_From += Qf_from.in(grid->arcs, T);
    Flow_Q_From += grid->b_ff.in(grid->arcs, T)*Wii.from(grid->arcs, T);
    Flow_Q_From += grid->b_ft.in(grid->arcs, T)*R_Wij.in(grid->arcs, T);
    Flow_Q_From += grid->g_ft.in(grid->arcs, T)*Im_Wij.in(grid->arcs, T);
    Flow_Q_From = 0;
    ACUC.add_constraint(Flow_Q_From);

    Constraint Flow_Q_To("Flow_Q_To");
    Flow_Q_To += Qf_to.in(grid->arcs, T);
    Flow_Q_To += grid->b_tt.in(grid->arcs, T)*Wii.to(grid->arcs, T);
    Flow_Q_To += grid->b_tf.in(grid->arcs, T)*R_Wij.in(grid->arcs, T);
    Flow_Q_To -= grid->g_tf.in(grid->arcs, T)*Im_Wij.in(grid->arcs, T);
    Flow_Q_To = 0;
    ACUC.add_constraint(Flow_Q_To);
    //
    //    // AC voltage limit constraints.
    Constraint Vol_limit_UB("Vol_limit_UB");
    Vol_limit_UB = Wii.in(grid->nodes, T);
    Vol_limit_UB -= grid->w_max.in(grid->nodes, T);
    ACUC.add_constraint(Vol_limit_UB <= 0);

    Constraint Vol_limit_LB("Vol_limit_LB");
    Vol_limit_LB = Wii.in(grid->nodes, T);
    Vol_limit_UB -= grid->w_min.in(grid->nodes, T);
    ACUC.add_constraint(Vol_limit_LB >= 0);

    /* Phase Angle Bounds constraints */
    Constraint PAD_UB("PAD_UB");
    PAD_UB = Im_Wij.in(grid->arcs, T);
    PAD_UB -= (grid->tan_th_max).in(grid->arcs, T)*R_Wij.in(grid->arcs, T);
    ACUC.add_constraint(PAD_UB <= 0);

    Constraint PAD_LB("PAD_LB");
    PAD_LB =  Im_Wij.in(grid->arcs, T);
    PAD_LB -= grid->tan_th_min.in(grid->arcs, T)*R_Wij.in(grid->arcs, T);
    ACUC.add_constraint(PAD_LB >= 0);

    /* Thermal Limit Constraints */
    Constraint Thermal_Limit_from("Thermal_Limit_from");
    Thermal_Limit_from += power(Pf_from.in(grid->arcs, T),  2) + power(Qf_from.in(grid->arcs, T), 2);
    Thermal_Limit_from -= power(grid->S_max.in(grid->arcs, T), 2);
    ACUC.add_constraint(Thermal_Limit_from <= 0);

    Constraint Thermal_Limit_to("Thermal_Limit_to");
    Thermal_Limit_to += power(Pf_to.in(grid->arcs, T), 2) + power(Qf_to.in(grid->arcs, T), 2);
    Thermal_Limit_to -= power(grid->S_max.in(grid->arcs, T),2);
    ACUC.add_constraint(Thermal_Limit_to <= 0);
    
    
    /* Pg */
    Constraint PG_UB("PG_UB");
    PG_UB = Pg.in(grid->gens, T) -grid->pg_max.in(grid->gens, T);
    ACUC.add_constraint(PG_UB <= 0);

    Constraint PG_LB("PG_LB");
    PG_LB += Pg.in(grid->gens, T);
    PG_LB -= grid->pg_min.in(grid->gens, T);
    ACUC.add_constraint(PG_LB >= 0);


    solver OPF(ACUC,ipopt);
    OPF.run();
    return 0;
}
