//
//  ACOPF.cpp
//  Gravity
//
//  Created by Guanglei Wang on 6/9/17.
//
//
#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include "../PowerNet.h"
#include <gravity/solver.h>
#include <stdlib.h>

using namespace std;
using namespace gravity;


int main (int argc, const char * argv[])
{
    const char* fname;
    if (argc >= 2) {
        fname = argv[1];
    }
    else {
//            fname = "../../data_sets/Power/nesta_case5_pjm.m";
           //fname = "../../data_sets/Power/nesta_case14_ieee.m";
           //fname = "../../data_sets/Power/nesta_case9241_pegase.m";
           //fname = "../../data_sets/Power/nesta_case2383wp_mp.m";
           //fname = "../../data_sets/Power/nesta_case1354_pegase_api.m";
           //fname = "../../data_sets/Power/nesta_case118_ieee.m";
//           fname = "/Users/hh/Dropbox/Work/Dev/pglib-opf/pglib_opf_case2383wp_k.m";
        fname = "/Users/hh/Dropbox/Work/Dev/pglib-opf/pglib_opf_case5_pjm.m";
//        fname = "/Users/hh/Dropbox/Work/Dev/nesta-0.7.0/opf/nesta_case3_lmbd.m";
//        fname = "/Users/hh/Dropbox/Work/Dev/nesta-0.7.0/opf/nesta_case5_pjm.m";
    }
    // ACOPF
    PowerNet grid;
//    fname = "../../data_sets/Power/nesta_case3_lmbd.m";
//    fname = "../../data_sets/Power/nesta_case14_ieee.m";
//    fname = "../../data_sets/Power/nesta_case9241_pegase.m";
//    fname = "/Users/hh/Dropbox/Work/Dev/nesta-0.7.0/opf/nesta_case3375wp_mp.m";
//    fname = "../../data_sets/Power/nesta_case300_ieee.m";
//    fname = "../../data_sets/Power/nesta_case2383wp_mp.m";
    grid.readgrid(fname);

    // Grid Parameters
    unsigned nb_gen = grid.get_nb_active_gens();
    unsigned nb_lines = grid.get_nb_active_arcs();
    unsigned nb_buses = grid.get_nb_active_nodes();


    DebugOn("nb gens = " << nb_gen << endl);
    DebugOn("nb lines = " << 2*nb_lines << endl);
    DebugOn("nb buses = " << nb_buses << endl);

    PowerModelType pmt = ACPOL;
    /** build model */
    if (argc >= 3) {
        if(!strcmp(argv[2],"ACPOL")) pmt = ACPOL;
        else if(!strcmp(argv[2],"ACRECT")) pmt = ACRECT;
        else if(!strcmp(argv[2],"QC")) pmt = QC;
        else if(!strcmp(argv[2],"QC_SDP")) pmt = QC_SDP;
        else if(!strcmp(argv[2],"OTS")) pmt = OTS;
        else if(!strcmp(argv[2],"SOCP")) pmt = SOCP;
        else if(!strcmp(argv[2],"SDP")) pmt = SDP;
        else if(!strcmp(argv[2],"DC")) pmt = DC;
        else if(!strcmp(argv[2],"QC_OTS_O")) pmt = QC_OTS_O;
        else if(!strcmp(argv[2],"QC_OTS_N")) pmt = QC_OTS_N;
        else if(!strcmp(argv[2],"QC_OTS_L")) pmt = QC_OTS_L;
        else if(!strcmp(argv[2],"SOCP_OTS")) pmt = SOCP_OTS;
        else{
            throw invalid_argument("Unknown model type.\n");            
        }
    }
    bool polar = (pmt==ACPOL);
    if (polar) {
        DebugOn("Using polar model\n");
    }
    Model ACOPF("AC-OPF Model");
    /** Variables */
    // power generation
    var<Real> Pg("Pg", grid.pg_min.in(grid.gens), grid.pg_max.in(grid.gens));
    var<Real> Qg ("Qg", grid.qg_min.in(grid.gens), grid.qg_max.in(grid.gens));
    ACOPF.add_var(Pg^(nb_gen));
    ACOPF.add_var(Qg^(nb_gen));

    // power flow
    var<Real> Pf_from("Pf_from", grid.S_max.in(grid.arcs));
    var<Real> Qf_from("Qf_from", grid.S_max.in(grid.arcs));
    var<Real> Pf_to("Pf_to", grid.S_max.in(grid.arcs));
    var<Real> Qf_to("Qf_to", grid.S_max.in(grid.arcs));

    ACOPF.add_var(Pf_from^(nb_lines));
    ACOPF.add_var(Qf_from^(nb_lines));
    ACOPF.add_var(Pf_to^(nb_lines));
    ACOPF.add_var(Qf_to^(nb_lines));

    // voltage related variables.
        var<Real> theta("theta");
        var<Real> v("|V|", grid.v_min.in(grid.nodes), grid.v_max.in(grid.nodes));
        var<Real> vr("vr", grid.v_max.in(grid.nodes));
        var<Real> vi("vi", grid.v_max.in(grid.nodes));
    
    if (polar) {
        ACOPF.add_var(v^(nb_buses));
        ACOPF.add_var(theta^(nb_buses));
        v.initialize_all(1);
    }
    else {
        ACOPF.add_var(vr^(nb_buses));
        ACOPF.add_var(vi^(nb_buses));
        vr.initialize_all(1);
    }

    /** Construct the objective function */
    func_ obj;
    for (auto g:grid.gens) {
        if (g->_active) {
            obj += grid.c1(g->_name)*Pg(g->_name) + grid.c2(g->_name)*Pg(g->_name)*Pg(g->_name) + grid.c0(g->_name);
        }
    }

    ACOPF.set_objective(min(obj));
    /** Define constraints */
    
    /* REF BUS */
    Constraint Ref_Bus("Ref_Bus");
    if (polar) {
        Ref_Bus = theta(grid.get_ref_bus());
    }
    else {
        Ref_Bus = vi(grid.get_ref_bus());
    }
    ACOPF.add_constraint(Ref_Bus = 0);
    
    
    //Generation Limit
//    Constraint PG_UB("PG_UB");
//    PG_UB = Pg.in(grid.gens) - grid.pg_max.in(grid.gens);
//    ACOPF.add_constraint(PG_UB <= 0);
//    
//    Constraint PG_LB("PG_LB");
//    PG_LB = Pg.in(grid.gens) - grid.pg_min.in(grid.gens);
//    ACOPF.add_constraint(PG_LB >= 0);
//
//    Constraint QG_UB("QG_UB");
//    QG_UB = Qg.in(grid.gens) - grid.qg_max.in(grid.gens);
//    ACOPF.add_constraint(QG_UB <= 0);
//    
//    Constraint QG_LB("QG_LB");
//    QG_LB = Qg.in(grid.gens) - grid.qg_min.in(grid.gens);
//    ACOPF.add_constraint(QG_LB >= 0);

    //Voltage Limit
//    Constraint Vi_UB("Vi_UB");
//    Vi_UB = vi.in(grid.nodes) - grid.v_max.in(grid.nodes);
//    ACOPF.add_constraint(Vi_UB <= 0);
//    
//    Constraint Vi_LB("Vi_LB");
//    Vi_LB = vi.in(grid.nodes) + grid.v_max.in(grid.nodes);
//    ACOPF.add_constraint(Vi_LB >= 0);
//    
//    Constraint Vr_UB("Vr_UB");
//    Vr_UB = vr.in(grid.nodes) - grid.v_max.in(grid.nodes);
//    ACOPF.add_constraint(Vr_UB <= 0);
//    
//    Constraint Vr_LB("Vr_LB");
//    Vr_LB = vr.in(grid.nodes) + grid.v_max.in(grid.nodes);
//    ACOPF.add_constraint(Vr_LB >= 0);
    
    //Power Limit
//    
//    Constraint Pf_UB("P_UB_From");
//    Pf_UB = Pf_from.in(grid.arcs) - grid.S_max.in(grid.arcs);
//    ACOPF.add_constraint(Pf_UB <= 0);
//    Constraint Pf_LB("P_LB_From");
//    Pf_LB = Pf_from.in(grid.arcs) + grid.S_max.in(grid.arcs);
//    ACOPF.add_constraint(Pf_LB >= 0);
//    
//    Constraint Pt_UB("P_UB_To");
//    Pt_UB = Pf_to.in(grid.arcs) - grid.S_max.in(grid.arcs);
//    ACOPF.add_constraint(Pt_UB <= 0);
//    Constraint Pt_LB("P_LB_To");
//    Pt_LB = Pf_to.in(grid.arcs) + grid.S_max.in(grid.arcs);
//    ACOPF.add_constraint(Pt_LB >= 0);
//    
//    Constraint Qf_UB("Q_UB_From");
//    Qf_UB = Qf_from.in(grid.arcs) - grid.S_max.in(grid.arcs);
//    ACOPF.add_constraint(Qf_UB <= 0);
//    Constraint Qf_LB("Q_LB_From");
//    Qf_LB = Qf_from.in(grid.arcs) + grid.S_max.in(grid.arcs);
//    ACOPF.add_constraint(Qf_LB >= 0);
//    
//    Constraint Qt_UB("Q_UB_To");
//    Qt_UB = Qf_to.in(grid.arcs) - grid.S_max.in(grid.arcs);
//    ACOPF.add_constraint(Qt_UB <= 0);
//    Constraint Qt_LB("Q_LB_To");
//    Qt_LB = Qf_to.in(grid.arcs) + grid.S_max.in(grid.arcs);
//    ACOPF.add_constraint(Qt_LB >= 0);
    
    //KCL
    for (auto b: grid.nodes) {
        if (!b->_active) {
            continue;
        }
        Bus* bus = (Bus*) b;
        Constraint KCL_P("KCL_P"+bus->_name);
        Constraint KCL_Q("KCL_Q"+bus->_name);

        /* Power Conservation */
        KCL_P  = sum(Pf_from.in(b->get_out())) + sum(Pf_to.in(b->get_in())) + bus->pl()- sum(Pg.in(bus->_gen));
        KCL_Q  = sum(Qf_from.in(b->get_out())) + sum(Qf_to.in(b->get_in())) + bus->ql()- sum(Qg.in(bus->_gen));
        
        /* Shunts */
        if (bus->gs()!=0) {
            if (polar) {
                KCL_P +=  bus->gs()*(power(v(bus->_name), 2));
            }
            else {
                KCL_P +=  bus->gs()*(power(vr(bus->_name), 2) + power(vi(bus->_name), 2));
            }
            DebugOff("Bus" << bus->_name << " : Shunt gs = " << bus->gs() << endl);
        }
        if (bus->bs()!=0) {
            if (polar) {
                KCL_Q -=  bus->bs()*(power(v(bus->_name), 2));
            }
            else {
                KCL_Q -=  bus->bs()*(power(vr(bus->_name), 2) + power(vi(bus->_name), 2));
            }
            DebugOff("Bus" << bus->_name << " : Shunt bs = " << bus->bs() << endl);
        }
        KCL_P = 0;
        KCL_Q = 0;
//        if (!KCL_P.get_vars().empty()) {
            ACOPF.add_constraint(KCL_P);
//        }
//        if (!KCL_Q.get_vars().empty()) {
            ACOPF.add_constraint(KCL_Q);
//        }
    }
    //AC Power Flow.
//
//    for (auto a:grid.arcs) {
//        auto arc = (Line*)a;
//        if (arc->_active) {
//            Constraint Flow_P_From("Flow_P_From"+arc->_name);
//            Flow_P_From += Pf_from(arc->_name);
//            Flow_P_From -= arc->g/(pow(arc->cc, 2) + pow(arc->dd, 2))*(power(vr(a->src->_name), 2) + power(vi(a->src->_name), 2));
//            Flow_P_From -= (-arc->g*arc->cc + arc->b*arc->dd)/(pow(arc->cc, 2) + pow(arc->dd, 2))*(vr(a->src->_name)*vr(a->dest->_name) + vi(a->src->_name)*vi(a->dest->_name));
//            Flow_P_From -= (-arc->b*arc->cc - arc->g*arc->dd)/(pow(arc->cc, 2) + pow(arc->dd, 2))*(vi(a->src->_name)*vr(a->dest->_name) - vr(a->src->_name)*vi(a->dest->_name));
//            Flow_P_From = 0;
////            Flow_P_From.print();
//            ACOPF.add_constraint(Flow_P_From);
//        
//            Constraint Flow_P_To("Flow_P_To"+arc->_name);
//            Flow_P_To += Pf_to(arc->_name);
//            Flow_P_To -= arc->g*(power(vr(a->dest->_name), 2) + power(vi(a->dest->_name), 2));
//            Flow_P_To -= (-arc->g*arc->cc - arc->b*arc->dd)/(pow(arc->cc, 2) + pow(arc->dd, 2))*(vr(a->src->_name)*vr(a->dest->_name) + vi(a->src->_name)*vi(a->dest->_name));
//            Flow_P_To -= (-arc->b*arc->cc + arc->g*arc->dd)/(pow(arc->cc, 2) + pow(arc->dd, 2))*(vi(a->dest->_name)*vr(a->src->_name) - vr(a->dest->_name)*vi(a->src->_name));
//            Flow_P_To = 0;
////            Flow_P_To.print();
//            ACOPF.add_constraint(Flow_P_To);
//
//        }
//    }
     /** TODO write the constraints in Complex form */
    Constraint Flow_P_From("Flow_P_From");
    Flow_P_From += Pf_from.in(grid.arcs);
    if (polar) {
//        Flow_P_From -= grid.Y_charge.in(grid.arcs)*cos(-1*grid.Y_charge_t.in(grid.arcs))/power(grid.tr.in(grid.arcs),2)*power(v.from(grid.arcs),2);
//        Flow_P_From += grid.Y.in(grid.arcs)/grid.tr.in(grid.arcs)*(v.from(grid.arcs)*v.to(grid.arcs)*cos(theta.from(grid.arcs) - theta.to(grid.arcs) - grid.as.in(grid.arcs)));
//        Flow_P_From += grid.b.in(grid.arcs)/grid.tr.in(grid.arcs)*(v.from(grid.arcs)*v.to(grid.arcs)*sin(theta.from(grid.arcs) - theta.to(grid.arcs) - grid.as.in(grid.arcs)));
        Flow_P_From -= grid.g.in(grid.arcs)/power(grid.tr.in(grid.arcs),2)*power(v.from(grid.arcs),2);
        Flow_P_From += grid.g.in(grid.arcs)/grid.tr.in(grid.arcs)*(v.from(grid.arcs)*v.to(grid.arcs)*cos(theta.from(grid.arcs) - theta.to(grid.arcs) - grid.as.in(grid.arcs)));
        Flow_P_From += grid.b.in(grid.arcs)/grid.tr.in(grid.arcs)*(v.from(grid.arcs)*v.to(grid.arcs)*sin(theta.from(grid.arcs) - theta.to(grid.arcs) - grid.as.in(grid.arcs)));
    }
    else {
        Flow_P_From -= grid.g_ff.in(grid.arcs)*(power(vr.from(grid.arcs), 2) + power(vi.from(grid.arcs), 2));
        Flow_P_From -= grid.g_ft.in(grid.arcs)*(vr.from(grid.arcs)*vr.to(grid.arcs) + vi.from(grid.arcs)*vi.to(grid.arcs));
        Flow_P_From -= grid.b_ft.in(grid.arcs)*(vi.from(grid.arcs)*vr.to(grid.arcs) - vr.from(grid.arcs)*vi.to(grid.arcs));
    }
    ACOPF.add_constraint(Flow_P_From=0);

    Constraint Flow_P_To("Flow_P_To");
    Flow_P_To += Pf_to.in(grid.arcs);
    if (polar) {
        Flow_P_To -= grid.g.in(grid.arcs)*power(v.to(grid.arcs), 2);
        Flow_P_To += grid.g.in(grid.arcs)/grid.tr.in(grid.arcs)*(v.from(grid.arcs)*v.to(grid.arcs)*cos(theta.to(grid.arcs) - theta.from(grid.arcs) + grid.as.in(grid.arcs)));
        Flow_P_To += grid.b.in(grid.arcs)/grid.tr.in(grid.arcs)*(v.from(grid.arcs)*v.to(grid.arcs)*sin(theta.to(grid.arcs) - theta.from(grid.arcs) + grid.as.in(grid.arcs)));
    }
    else {
        Flow_P_To -= grid.g_tt.in(grid.arcs)*(power(vr.to(grid.arcs), 2) + power(vi.to(grid.arcs), 2));
        Flow_P_To -= grid.g_tf.in(grid.arcs)*(vr.from(grid.arcs)*vr.to(grid.arcs) + vi.from(grid.arcs)*vi.to(grid.arcs));
        Flow_P_To -= grid.b_tf.in(grid.arcs)*(vi.to(grid.arcs)*vr.from(grid.arcs) - vr.to(grid.arcs)*vi.from(grid.arcs));
    }
    ACOPF.add_constraint(Flow_P_To=0);

    Constraint Flow_Q_From("Flow_Q_From");
    Flow_Q_From += Qf_from.in(grid.arcs);
    if (polar) {
        Flow_Q_From += (0.5*grid.ch.in(grid.arcs)+grid.b.in(grid.arcs))/power(grid.tr.in(grid.arcs),2)*power(v.from(grid.arcs),2);
        Flow_Q_From -= grid.b.in(grid.arcs)/grid.tr.in(grid.arcs)*(v.from(grid.arcs)*v.to(grid.arcs)*cos(theta.from(grid.arcs) - theta.to(grid.arcs) - grid.as.in(grid.arcs)));
        Flow_Q_From += grid.g.in(grid.arcs)/grid.tr.in(grid.arcs)*(v.from(grid.arcs)*v.to(grid.arcs)*sin(theta.from(grid.arcs) - theta.to(grid.arcs) - grid.as.in(grid.arcs)));
    }
    else {
        Flow_Q_From += grid.b_ff.in(grid.arcs)*(power(vr.from(grid.arcs), 2) + power(vi.from(grid.arcs), 2));
        Flow_Q_From += grid.b_ft.in(grid.arcs)*(vr.from(grid.arcs)*vr.to(grid.arcs) + vi.from(grid.arcs)*vi.to(grid.arcs));
        Flow_Q_From -= grid.g_ft.in(grid.arcs)*(vi.from(grid.arcs)*vr.to(grid.arcs) - vr.from(grid.arcs)*vi.to(grid.arcs));
    }
    ACOPF.add_constraint(Flow_Q_From=0);
    Constraint Flow_Q_To("Flow_Q_To");
    Flow_Q_To += Qf_to.in(grid.arcs);
    if (polar) {
        Flow_Q_To += (0.5*grid.ch.in(grid.arcs)+grid.b.in(grid.arcs))*power(v.to(grid.arcs),2);
        Flow_Q_To -= grid.b.in(grid.arcs)/grid.tr.in(grid.arcs)*(v.from(grid.arcs)*v.to(grid.arcs)*cos(theta.to(grid.arcs) - theta.from(grid.arcs) + grid.as.in(grid.arcs)));
        Flow_Q_To += grid.g.in(grid.arcs)/grid.tr.in(grid.arcs)*(v.from(grid.arcs)*v.to(grid.arcs)*sin(theta.to(grid.arcs) - theta.from(grid.arcs) + grid.as.in(grid.arcs)));
//        Flow_Q_To += grid.b_tt.in(grid.arcs)*power(v.to(grid.arcs),2);
//        Flow_Q_To -= grid.b.in(grid.arcs)/grid.tr.in(grid.arcs)*(v.from(grid.arcs)*v.to(grid.arcs)*cos(theta.to(grid.arcs) - theta.from(grid.arcs) + grid.as.in(grid.arcs)));
//        Flow_Q_To += grid.g.in(grid.arcs)/grid.tr.in(grid.arcs)*(v.from(grid.arcs)*v.to(grid.arcs)*sin(theta.to(grid.arcs) - theta.from(grid.arcs) + grid.as.in(grid.arcs)));
    }
    else {
        Flow_Q_To += grid.b_tt.in(grid.arcs)*(power(vr.to(grid.arcs), 2) + power(vi.to(grid.arcs), 2));
        Flow_Q_To += grid.b_tf.in(grid.arcs)*(vr.from(grid.arcs)*vr.to(grid.arcs) + vi.from(grid.arcs)*vi.to(grid.arcs));
        Flow_Q_To -= grid.g_tf.in(grid.arcs)*(vi.to(grid.arcs)*vr.from(grid.arcs) - vr.to(grid.arcs)*vi.from(grid.arcs));
    }
    ACOPF.add_constraint(Flow_Q_To=0);
    
    // AC voltage limit constraints.
    if (!polar) {
        Constraint Vol_limit_UB("Vol_limit_UB");
        Vol_limit_UB = power(vr.in(grid.nodes), 2) + power(vi.in(grid.nodes), 2);
        Vol_limit_UB -= power(grid.v_max.in(grid.nodes), 2);
        ACOPF.add_constraint(Vol_limit_UB <= 0);

        Constraint Vol_limit_LB("Vol_limit_LB");
        Vol_limit_LB = power(vr.in(grid.nodes), 2) + power(vi.in(grid.nodes), 2);
        Vol_limit_LB -= power(grid.v_min.in(grid.nodes),2);    
        ACOPF.add_constraint(Vol_limit_LB >= 0);
    }

    
    /* Phase Angle Bounds constraints */
    Constraint PAD_UB("PAD_UB");
    Constraint PAD_LB("PAD_LB:");
    auto bus_pairs = grid.get_bus_pairs();
    if (polar) {
        PAD_UB = theta.from(bus_pairs) - theta.to(bus_pairs);
        PAD_UB -= grid.th_max.in(bus_pairs);
        PAD_LB = theta.from(bus_pairs) - theta.to(bus_pairs);
        PAD_LB -= grid.th_min.in(bus_pairs);
        DebugOff(grid.th_min.to_str(true) << endl);
        DebugOff(grid.th_max.to_str(true) << endl);
    }
    else {        
        DebugOn("Number of bus_pairs = " << bus_pairs.size() << endl);
        PAD_UB = vi.from(bus_pairs)*vr.to(bus_pairs) - vr.from(bus_pairs)*vi.to(bus_pairs);
        PAD_UB -= grid.tan_th_max.in(bus_pairs)*(vr.from(bus_pairs)*vr.to(bus_pairs) + vi.from(bus_pairs)*vi.to(bus_pairs));
        
        PAD_LB = vi.from(bus_pairs)*vr.to(bus_pairs) - vr.from(bus_pairs)*vi.to(bus_pairs);
        PAD_LB -= grid.tan_th_min.in(bus_pairs)*(vr.from(bus_pairs)*vr.to(bus_pairs) + vi.to(bus_pairs)*vi.from(bus_pairs));
    }
    ACOPF.add_constraint(PAD_UB <= 0);
    ACOPF.add_constraint(PAD_LB >= 0);


//  Thermal Limit Constraints 
    Constraint Thermal_Limit_from("Thermal_Limit_from");
    Thermal_Limit_from += power(Pf_from.in(grid.arcs), 2) + power(Qf_from.in(grid.arcs), 2);
    Thermal_Limit_from -= power(grid.S_max.in(grid.arcs), 2);
    ACOPF.add_constraint(Thermal_Limit_from <= 0);

    Constraint Thermal_Limit_to("Thermal_Limit_to");
    Thermal_Limit_to += power(Pf_to.in(grid.arcs), 2) + power(Qf_to.in(grid.arcs), 2);
    Thermal_Limit_to -= power(grid.S_max.in(grid.arcs),2);
    ACOPF.add_constraint(Thermal_Limit_to <= 0);

    //solver OPF(ACOPF,cplex);
    solver OPF(ACOPF,ipopt);
    OPF.run();
    return 0;
}
