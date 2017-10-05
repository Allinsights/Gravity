//
//  SOCOPF.cpp
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
#include <gravity/solver.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;
using namespace gravity;

// codes for generating vertices of a hypercube (not necessarily unit).
void box(vector<double>* V, double l, double u, unsigned dim){
    if (dim == 1){
        V[0].push_back(l);
        V[1].push_back(u);
    }
    else if (dim < 1)
        cerr << "Dim should be as least 1!!" << endl;
    else{
        box(V, l, u, dim -1);
        unsigned n = pow(2, dim-1);
        for (unsigned i = 0; i < n; i++){
            V[n+i] = V[i];
            V[i].push_back(l);
            V[n+i].push_back(u);
        }
    }
}

int main (int argc, const char * argv[])
{
    const char* fname;
    if (argc >= 2) {
        fname = argv[1];
    }
    else {
        fname = "../../data_sets/Power/nesta_case5_pjm.m";
    }
    PowerNet* grid = new PowerNet();
    grid->readgrid(fname);
    grid->get_tree_decomp_bags(true);
    
    // Grid Parameters
    unsigned nb_gen = grid->gens.size();
    unsigned nb_lines = grid->arcs.size();
    unsigned nb_buses = grid->nodes.size();

    /** build model */
    Model SOCP("SOCP Model");

    /** Variables */
    // power generation
    var<Real> Pg("Pg", grid->pg_min.in(grid->gens), grid->pg_max.in(grid->gens));
    var<Real> Qg ("Qg", grid->qg_min.in(grid->gens), grid->qg_max.in(grid->gens));
    SOCP.add_var(Pg^(nb_gen));
    SOCP.add_var(Qg^(nb_gen));
    
    // power flow
    var<Real> Pf_from("Pf_from");
    var<Real> Qf_from("Qf_from");
    var<Real> Pf_to("Pf_to");
    var<Real> Qf_to("Qf_to");
    SOCP.add_var(Pf_from^(nb_lines));
    SOCP.add_var(Qf_from^(nb_lines));
    SOCP.add_var(Pf_to^(nb_lines));
    SOCP.add_var(Qf_to^(nb_lines));
    
    // Lifted variables.
    var<Real>  R_Wij("R_Wij"); // real part of Wij
    var<Real>  Im_Wij("Im_Wij"); // imaginary part of Wij.
    //var<Real>  Wii("Wii", 0, 100000);
    var<Real>  Wii("Wii", grid->w_min, grid->w_max);// strange! leads to two different answers
    SOCP.add_var(Wii^nb_buses);
    SOCP.add_var(R_Wij^nb_lines);
    SOCP.add_var(Im_Wij^nb_lines);
    
    /** Construct the objective function*/
    func_ obj;
    for (auto g:grid->gens) {
        if (g->_active) {
            obj += grid->c1(g->_name)*Pg(g->_name) + grid->c2(g->_name)*Pg(g->_name)*Pg(g->_name) + grid->c0(g->_name);
        }
    }
    //func_ obj = sum(grid->c0) +sum(grid->c1.in(grid->gens),Pg.in(grid->gens)) + sum(grid->c2.in(grid->gens), power(Pg.in(grid->gens),2));
    SOCP.set_objective(min(obj));
    
    /** Define constraints */
    /* SOCP constraints */
    Constraint SOC("SOC");
    cout << "nb_arcs: " << nb_lines << endl;
    SOC =  power(R_Wij.in(grid->arcs), 2) + power(Im_Wij.in(grid->arcs), 2) - Wii.from(grid->arcs)*Wii.to(grid->arcs) ;
    SOCP.add_constraint(SOC <= 0);
    
    //KCL
    for (auto b: grid->nodes) {
        Bus* bus = (Bus*) b;
        Constraint KCL_P("KCL_P"+bus->_name);
        Constraint KCL_Q("KCL_Q"+bus->_name);

        /* Power Conservation */
        KCL_P  = sum(Pf_from.in(b->get_out())) + sum(Pf_to.in(b->get_in())) + bus->pl()- sum(Pg.in(bus->_gen));
        KCL_Q  = sum(Qf_from.in(b->get_out())) + sum(Qf_to.in(b->get_in())) + bus->ql()- sum(Qg.in(bus->_gen));

        /* Shunts */
        KCL_P +=  bus->gs()*(Wii(bus->_name));
        KCL_Q +=  bus->bs()*(Wii(bus->_name));
        
        SOCP.add_constraint(KCL_P = 0);
        SOCP.add_constraint(KCL_Q = 0);
    }

    //AC Power Flow
    Constraint Flow_P_From("Flow_P_From");
    Flow_P_From += Pf_from.in(grid->arcs);
    Flow_P_From -= grid->g_ff.in(grid->arcs)*Wii.from(grid->arcs);
    Flow_P_From -= grid->g_ft.in(grid->arcs)*R_Wij.in(grid->arcs);
    Flow_P_From -= grid->b_ft.in(grid->arcs)*Im_Wij.in(grid->arcs);
    SOCP.add_constraint(Flow_P_From = 0);
    
    Constraint Flow_P_To("Flow_P_To");
    Flow_P_To += Pf_to.in(grid->arcs);
    Flow_P_To -= grid->g_tt.in(grid->arcs)*Wii.to(grid->arcs);
    Flow_P_To -= grid->g_tf.in(grid->arcs)*R_Wij.in(grid->arcs);
    Flow_P_To += grid->b_tf.in(grid->arcs)*Im_Wij.in(grid->arcs);
    SOCP.add_constraint(Flow_P_To = 0);
    
    Constraint Flow_Q_From("Flow_Q_From");
    Flow_Q_From += Qf_from.in(grid->arcs);
    Flow_Q_From += grid->b_ff.in(grid->arcs)*Wii.from(grid->arcs);
    Flow_Q_From += grid->b_ft.in(grid->arcs)*R_Wij.in(grid->arcs);
    Flow_Q_From += grid->g_ft.in(grid->arcs)*Im_Wij.in(grid->arcs);
    SOCP.add_constraint(Flow_Q_From = 0);
    
    Constraint Flow_Q_To("Flow_Q_To");
    Flow_Q_To += Qf_to.in(grid->arcs);
    Flow_Q_To += grid->b_tt.in(grid->arcs)*Wii.to(grid->arcs);
    Flow_Q_To += grid->b_tf.in(grid->arcs)*R_Wij.in(grid->arcs);
    Flow_Q_To -= grid->g_tf.in(grid->arcs)*Im_Wij.in(grid->arcs);
    SOCP.add_constraint(Flow_Q_To = 0);
    //    // AC voltage limit constraints.
//    Constraint Vol_limit_UB("Vol_limit_UB");
//    Vol_limit_UB = Wii.in(grid->nodes);
//    Vol_limit_UB -= power(grid->v_max.in(grid->nodes), 2);
//    SOCP.add_constraint(Vol_limit_UB <= 0);
//
//    Constraint Vol_limit_LB("Vol_limit_LB");
//    Vol_limit_LB = Wii.in(grid->nodes);
//    Vol_limit_LB -= power(grid->v_min.in(grid->nodes),2);
//    SOCP.add_constraint(Vol_limit_LB >= 0);
    
    /* Phase Angle Bounds constraints */
    Constraint PAD_UB("PAD_UB");
    PAD_UB = Im_Wij.in(grid->arcs);
    PAD_UB -= (grid->tan_th_max).in(grid->arcs)*R_Wij.in(grid->arcs);
    SOCP.add_constraint(PAD_UB <= 0);
    
    Constraint PAD_LB("PAD_LB");
    PAD_LB =  Im_Wij.in(grid->arcs);
    PAD_LB -= grid->tan_th_min.in(grid->arcs)*R_Wij.in(grid->arcs);
    SOCP.add_constraint(PAD_LB >= 0);
    
    /* Thermal Limit Constraints */
    Constraint Thermal_Limit_from("Thermal_Limit_from");
    Thermal_Limit_from += power(Pf_from.in(grid->arcs), 2) + power(Qf_from.in(grid->arcs), 2);
    Thermal_Limit_from -= power(grid->S_max.in(grid->arcs),2);
    SOCP.add_constraint(Thermal_Limit_from <= 0);
    
    Constraint Thermal_Limit_to("Thermal_Limit_to");
    Thermal_Limit_to += power(Pf_to.in(grid->arcs), 2) + power(Qf_to.in(grid->arcs), 2);
    Thermal_Limit_to -= power(grid->S_max.in(grid->arcs),2);
    SOCP.add_constraint(Thermal_Limit_to <= 0);
    
    /* Strengthen relaxation using cover estimators */
    /* Clique tree decomposition */
    /* Cover estimators */
    //generate vertices of a box using a recursive algorithm
//    unsigned dim = nb_buses;
//    unsigned Num_points = pow(2, dim);
//    vector<double> pmatrix[Num_points];
//    var<double> lambda_R("lambda_R", 0, 1);
//    var<double> lambda_Im("lambda_Im", 0, 1);
//    // check this carefully.
//    SOCP.add_var(lambda_R^(Num_points));
//    SOCP.add_var(lambda_Im^(Num_points));
//    box(pmatrix, grid->v_min.getvalue(), grid->v_max.getvalue(), dim);
//    for (int i = 0; i < Num_points; i++)
//        for (int j = 0; j < dim; j++)
//            cout << "P[" << i <<", " << j << "] =" << pmatrix[i][j] << endl;
//
//    for (auto a: grid->arcs){
//        auto s = a->src;
//        auto d = a->dest;
//        Constraint Lin("Cover_Wij_" + a->_name);
//        Lin = R_Wij(a->_name);
//        cout << "s->ID: " << s->ID << endl;
//        cout << "d->ID: " << d->ID << endl;
//        for (int i = 0; i < Num_points; i++){
//            Lin -= lambda_R(i)*pmatrix[i][s->ID]*pmatrix[i][d->ID];
//            Lin -= lambda_Im(i)*pmatrix[i][s->ID]*pmatrix[i][d->ID] ;
//        }
//         SOCP.add_constraint(Lin = 0);
//    }
//
//    Constraint Convex_comb_R("Convex_comb_R");
//    Convex_comb_R = sum(lambda_R);
//    SOCP.add_constraint(Convex_comb_R = 1);
//
//    Constraint Convex_comb_Im("Convex_comb_Im");
//    Convex_comb_Im = sum(lambda_Im);
//    SOCP.add_constraint(Convex_comb_Im = 1);
    solver SCOPF(SOCP,ipopt);
    SCOPF.run();
    return 0;
}
