//
//  Decompose.cpp
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

#ifdef USE_BOOST
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/kruskal_min_spanning_tree.hpp>
#include <deque>
#include <iterator>
#endif
using namespace std;
using namespace gravity;

/** INITIALISE SUBPROBLEM MODEL */
// it returns a outer-approximation function object 
double  subproblem(PowerNet* grid,Net* chordal, unsigned T, unsigned c, Net* cliquetree,
                  vector<Bus*> bag_bus, vector<Gen*> bag_gens, vector<Arc*> bag_arcs, 
                  vector<param<Real>>& lambda_sep, vector<param<Real>>& mu_sep, 
                  vector<param<Real>>& kappa_sep, vector<param<Real>>& eta_sep,
                  param<Real>& rate_ramp, param<Real>& rate_switch, param<Real>& min_up, param<Real>& min_down,
                  param<Real>& cost_up, param<Real>& cost_down) 
{
//    func_  OA;
    cout << "Solving subproblem associated with maximal clique .........." << c << endl;
    if (bag_arcs.size() == 0){
        //OA += 0;
        //return OA;
        return 0;
    }
    Model Subr("Subr");
    // POWER FLOW
    DebugOn("bag_arcs " << c << " has " << bag_arcs.size() << " lines." << endl);
    var<Real> Pf_from("Pf_from", grid->S_max.in(bag_arcs, T));
    var<Real> Qf_from("Qf_from", grid->S_max.in(bag_arcs, T));
    var<Real> Pf_to("Pf_to", grid->S_max.in(bag_arcs, T));
    var<Real> Qf_to("Qf_to", grid->S_max.in(bag_arcs, T));
    Subr.add_var(Pf_from^(T*bag_arcs.size()));
    Subr.add_var(Qf_from^(T*bag_arcs.size()));
    Subr.add_var(Pf_to^(T*bag_arcs.size()));
    Subr.add_var(Qf_to^(T*bag_arcs.size()));

    // Lifted variables.
    //var<Real>  R_Wij("R_Wij", grid->wr_min.in(bus_pairs), grid->wr_max.in(bus_pairs)); // real part of Wij
    //var<Real>  Im_Wij("Im_Wij", grid->wi_min.in(bus_pairs), grid->wi_max.in(bus_pairs)); // imaginary part of Wij.
    var<Real>  R_Wij("R_Wij");
    var<Real>  Im_Wij("Im_Wij");
    var<Real>  Wii("Wii", grid->w_min.in(bag_bus, T), grid->w_max.in(bag_bus, T));
    Subr.add_var(Wii^(T*bag_bus.size()));
    Subr.add_var(R_Wij^(T*grid->_bags.size()*(bag_bus.size() - 1)/2));
    Subr.add_var(Im_Wij^(T*grid->_bags.size()*(bag_bus.size() - 1)/2));

    // Commitment variables
    var<bool>  On_off("On_off", 0, 1);
    var<Real>  Start_up("Start_up", 0, 1);
    var<Real>  Shut_down("Shut_down", 0, 1);
    Subr.add_var(On_off^(T*bag_gens.size()));
    Subr.add_var(Start_up^(T*bag_gens.size()));
    Subr.add_var(Shut_down^(T*bag_gens.size()));

    /* Construct the objective function*/
    func_ obj;
    Node* Cr = cliquetree->get_node(to_string(c));
    Arc* arc = nullptr;
    Bus* node = nullptr;
    for (auto a: Cr->get_out()){
        Debug("a->_intersection.size " << a->_intersection.size() << endl);
        for (int i = 0; i < a->_intersection.size(); i ++){
            node = (Bus*)a->_intersection.at(i);
            for (int j = i + 1; j < a->_intersection.size(); j ++){
                arc = chordal->get_arc(node, a->_intersection.at(j)); 
                obj += lambda_sep[a->_id](arc->_name)*R_Wij(arc->_name);
            }
        }
    }

    for (auto a: Cr->get_in()){
        for (int i = 0; i < a->_intersection.size(); i ++){
            node = (Bus*) a->_intersection.at(i);
            for (int j = i + 1; j < a->_intersection.size(); j ++){
                arc = chordal->get_arc(node, a->_intersection.at(j));
                obj += lambda_sep[a->_id](arc->_name)*R_Wij(arc->_name);
            }
        }
    }
    // power generation
    if (bag_gens.size() > 0) {
        var<Real> Pg("Pg", grid->pg_min.in(bag_gens, T), grid->pg_max.in(bag_gens, T));
        var<Real> Qg("Qg", grid->qg_min.in(bag_gens, T), grid->qg_max.in(bag_gens, T));
        Subr.add_var(Pg^(T*bag_gens.size()));
        Subr.add_var(Qg^(T*bag_gens.size()));

        for (auto g: bag_gens) {
            DebugOn("number of generators: " << bag_gens.size() << endl);
            if (g->_active) {
                DebugOn("generator name: " << g->_name << endl);
                for (int t = 0; t < T; t++) {
                    if (t > 1) {
                        string l = to_string(t);
                        obj += grid->c1(g->_name, l)*Pg(g->_name, l) + grid->c2(g->_name, l)*Pg(g->_name, l)*Pg(g->_name, l) + grid->c0(g->_name, l);
                        //obj += cost_up.getvalue()*Start_up(g->_name, l)+ cost_down.getvalue()*Shut_down(g->_name, l);
                    }
                    else {
                        obj += grid->c1(g->_name)*Pg(g->_name) + grid->c2(g->_name)*Pg(g->_name)*Pg(g->_name) + grid->c0(g->_name);
                        //obj += cost_up.getvalue()*Start_up(g->_name)+ cost_down.getvalue()*Shut_down(g->_name);
                    }
                }
            }
            obj.print(true);
        }
        for (auto a: Cr->get_out()){
            for (int i = 0; i < a->_intersection.size(); i ++){
            node = (Bus*) a->_intersection.at(i);
                for (auto g: node->_gen){
                    if (g->_active){
                        obj += mu_sep[a->_id](g->_name)*Pg(g->_name);
                        obj += kappa_sep[a->_id](g->_name)*Qg(g->_name);
                        obj += eta_sep[a->_id](g->_name)*On_off(g->_name);
                    }
                }
            }
        }

        for (auto a: Cr->get_in()){
            for (int i = 0; i < a->_intersection.size(); i ++){
            node = (Bus*) a->_intersection.at(i);
                for (auto g: node->_gen){
                    if (g->_active){
                        obj -= mu_sep[a->_id](g->_name)*Pg(g->_name);
                        obj -= kappa_sep[a->_id](g->_name)*Qg(g->_name);
                        obj -= eta_sep[a->_id](g->_name)*On_off(g->_name);
                    }
                }
            }
        }
        Subr.set_objective(min(obj));
        //KCL
        for (int t = 0; t < T; t++) {
            for (auto bus: bag_bus) {
                if (!bus->_active) {
                    continue;
                }
                Constraint KCL_P("KCL_P"+bus->_name+ "time_" + to_string(t));
                Constraint KCL_Q("KCL_Q"+bus->_name+ "time_" + to_string(t));

                /* Power Conservation */
                vector<Arc*> out;
                set_intersection(bus->get_out().begin(), bus->get_out().end(), bag_arcs.begin(), bag_arcs.end(), back_inserter(out));
                vector<Arc*> in;
                set_intersection(bus->get_in().begin(), bus->get_in().end(), bag_arcs.begin(), bag_arcs.end(), back_inserter(in));
                KCL_P  = sum(Pf_from.in_at(out, t))+ sum(Pf_to.in_at(in, t)) + bus->pl() - sum(Pg.in_at(bus->_gen, t));
                KCL_Q  = sum(Qf_from.in_at(out, t)) + sum(Qf_to.in_at(in, t))+ bus->ql() - sum(Qg.in_at(bus->_gen, t));

                /* Shunts */
                KCL_P +=  bus->gs()*Wii(bus->_name);
                KCL_Q -=  bus->bs()*Wii(bus->_name);

                Subr.add_constraint(KCL_P = 0);
                Subr.add_constraint(KCL_Q = 0);
            }
        }
        /* Commitment constraints */
        // Inter-temporal constraints
        for (int t = 1; t < T; t++) {
            Constraint MC1("MC1_"+ to_string(t));
            Constraint MC2("MC2_"+ to_string(t));
            MC1 = On_off.in_at(bag_gens, t)- On_off.in_at(bag_gens, t-1)-  Start_up.in_at(bag_gens, t);
            MC2 = On_off.in_at(bag_gens, t-1) - On_off.in_at(bag_gens, t) - Shut_down.in_at(bag_gens, t);
            Subr.add_constraint(MC1 <= 0);
            Subr.add_constraint(MC2 <= 0);
        }

        // Min-up constraints
        for (int t = 1; t < T; t++) {
            Constraint Min_up1("Min_up1_"+ to_string(t));
            Min_up1 = On_off.in_at(bag_gens, t) - On_off.in_at(bag_gens, t-1) - Start_up.in_at(bag_gens, t) + Shut_down.in_at(bag_gens, t);
            Subr.add_constraint(Min_up1 = 0);
        }

        for (int t = min_up.getvalue(); t < T; t++) {
            Constraint Min_Up("Min_Up_constraint" + to_string(t));
            for (int l = t-min_up.getvalue()+1; l < t +1; l++) {
                Min_Up   += Start_up.in_at(bag_gens, l);
            }
            Min_Up -= On_off.in_at(bag_gens, t);
            Subr.add_constraint(Min_Up <= 0);
        }

        for (int t = min_down.getvalue(); t < T; t++) {
            Constraint Min_Down("Min_Down_constraint" + to_string(t));
            for (int l = t-min_down.getvalue()+1; l < t +1; l++) {
                Min_Down   += Shut_down.in_at(bag_gens, l);
            }
            Min_Down -= 1 - On_off.in_at(bag_gens, t);
            Subr.add_constraint(Min_Down <= 0);
        }

        //Ramp rate
        Constraint Production_P_LB("Production_P_LB");
        Constraint Production_P_UB("Production_P_UB");
        Constraint Production_Q_LB("Production_Q_LB");
        Constraint Production_Q_UB("Production_Q_UB");

        Production_P_UB = Pg.in(bag_gens, T) - grid->pg_max.in(bag_gens, T)*On_off.in(bag_gens,T);
        Production_P_LB = Pg.in(bag_gens, T) - grid->pg_min.in(bag_gens, T)*On_off.in(bag_gens,T);
        Subr.add_constraint(Production_P_UB <=0);
        Subr.add_constraint(Production_P_LB >= 0);

        //grid->qg_max.print(true);
        //grid->qg_min.print(true);

        Production_Q_UB = Qg.in(bag_gens, T) - grid->qg_max.in(bag_gens, T)*On_off.in(bag_gens,T);
        Production_Q_LB = Qg.in(bag_gens, T) - grid->qg_min.in(bag_gens, T)*On_off.in(bag_gens,T);
        Subr.add_constraint(Production_Q_UB <= 0);
        Subr.add_constraint(Production_Q_LB >= 0);

        for (int t = 1; t < T; t++) {
            Constraint Ramp_up("Ramp_up_constraint" + to_string(t));
            Constraint Ramp_down("Ramp_down_constraint" + to_string(t));

            Ramp_up = Pg.in_at(bag_gens, t);
            Ramp_up -= Pg.in_at(bag_gens, t-1);
            Ramp_up -= rate_ramp*On_off.in_at(bag_gens, t-1);
            Ramp_up -= rate_switch*(1 - On_off.in_at(bag_gens, t));

            Ramp_down = Pg.in_at(bag_gens, t-1);
            Ramp_down -= Pg.in_at(bag_gens, t);
            Ramp_down -= rate_ramp*On_off.in_at(bag_gens, t);
            Ramp_down -= rate_switch*(1 - On_off.in_at(bag_gens, t-1));

            Subr.add_constraint(Ramp_up <= 0);
            Subr.add_constraint(Ramp_down <= 0);
        }
    }
    else {
        obj += 0;
        Subr.set_objective(min(obj));
        //KCL
        for (int t = 0; t < T; t++)
            for (auto bus: bag_bus) {
                if (!bus->_active) {
                    continue;
                }
                Constraint KCL_P("KCL_P"+bus->_name+ "time_" + to_string(t));
                Constraint KCL_Q("KCL_Q"+bus->_name+ "time_" + to_string(t));

                /* Power Conservation */
                vector<Arc*> out;
                set_intersection(bus->get_out().begin(), bus->get_out().end(), bag_arcs.begin(), bag_arcs.end(), back_inserter(out));
                vector<Arc*> in;
                set_intersection(bus->get_in().begin(), bus->get_in().end(), bag_arcs.begin(), bag_arcs.end(), back_inserter(in));
                KCL_P  = sum(Pf_from.in_at(out, t))+ sum(Pf_to.in_at(in, t)) + bus->pl();
                KCL_Q  = sum(Qf_from.in_at(out, t)) + sum(Qf_to.in_at(in, t))+ bus->ql();

                /* Shunts */
                KCL_P +=  bus->gs()*Wii(bus->_name);
                KCL_Q -=  bus->bs()*Wii(bus->_name);

                Subr.add_constraint(KCL_P = 0);
                Subr.add_constraint(KCL_Q = 0);
            }
    }
    /* SOCP constraints */
    Constraint SOC("SOC");
    SOC =  power(R_Wij.in(bag_arcs, T), 2) + power(Im_Wij.in(bag_arcs, T), 2) - Wii.from(bag_arcs, T)*Wii.to(bag_arcs, T) ;
    Subr.add_constraint(SOC <= 0);

    //AC Power Flow.
    Constraint Flow_P_From("Flow_P_From");
    Flow_P_From += Pf_from.in(bag_arcs, T);
    Flow_P_From -= grid->g_ff.in(bag_arcs, T)*Wii.from(bag_arcs, T);
    Flow_P_From -= grid->g_ft.in(bag_arcs, T)*R_Wij.in(bag_arcs, T);
    Flow_P_From -= grid->b_ft.in(bag_arcs, T)*Im_Wij.in(bag_arcs, T);
    Flow_P_From = 0;
    Subr.add_constraint(Flow_P_From);

    Constraint Flow_P_To("Flow_P_To");
    Flow_P_To += Pf_to.in(bag_arcs, T);
    Flow_P_To -= grid->g_tt.in(bag_arcs, T)*Wii.to(bag_arcs, T);
    Flow_P_To -= grid->g_tf.in(bag_arcs, T)*R_Wij.in(bag_arcs, T);
    Flow_P_To += grid->b_tf.in(bag_arcs, T)*Im_Wij.in(bag_arcs, T);
    Flow_P_To = 0;
    Subr.add_constraint(Flow_P_To);

    Constraint Flow_Q_From("Flow_Q_From");
    Flow_Q_From += Qf_from.in(bag_arcs, T);
    Flow_Q_From += grid->b_ff.in(bag_arcs, T)*Wii.from(bag_arcs, T);
    Flow_Q_From += grid->b_ft.in(bag_arcs, T)*R_Wij.in(bag_arcs, T);
    Flow_Q_From += grid->g_ft.in(bag_arcs, T)*Im_Wij.in(bag_arcs, T);
    Flow_Q_From = 0;
    Subr.add_constraint(Flow_Q_From);

    Constraint Flow_Q_To("Flow_Q_To");
    Flow_Q_To += Qf_to.in(bag_arcs, T);
    Flow_Q_To += grid->b_tt.in(bag_arcs, T)*Wii.to(bag_arcs, T);
    Flow_Q_To += grid->b_tf.in(bag_arcs, T)*R_Wij.in(bag_arcs, T);
    Flow_Q_To -= grid->g_tf.in(bag_arcs, T)*Im_Wij.in(bag_arcs, T);
    Flow_Q_To = 0;
    Subr.add_constraint(Flow_Q_To);

    //Phase Angle Bounds constraints */
    //NOTE THAT WE SHOULD USE BUS PAIRS!!!
    Constraint PAD_UB("PAD_UB");
    PAD_UB = Im_Wij.in(bag_arcs, T);
    PAD_UB -= (grid->tan_th_max).in(bag_arcs, T)*R_Wij.in(bag_arcs, T);
    Subr.add_constraint(PAD_UB <= 0);

    Constraint PAD_LB("PAD_LB");
    PAD_LB =  Im_Wij.in(bag_arcs, T);
    PAD_LB -= grid->tan_th_min.in(bag_arcs, T)*R_Wij.in(bag_arcs, T);
    Subr.add_constraint(PAD_LB >= 0);

    //Thermal Limit Constraints */
    Constraint Thermal_Limit_from("Thermal_Limit_from");
    Thermal_Limit_from += power(Pf_from.in(bag_arcs, T),  2) + power(Qf_from.in(bag_arcs, T), 2);
    Thermal_Limit_from -= power(grid->S_max.in(bag_arcs, T), 2);
    Subr.add_constraint(Thermal_Limit_from <= 0);

    Constraint Thermal_Limit_to("Thermal_Limit_to");
    Thermal_Limit_to += power(Pf_to.in(bag_arcs, T), 2) + power(Qf_to.in(bag_arcs, T), 2);
    Thermal_Limit_to -= power(grid->S_max.in(bag_arcs, T),2);
    Subr.add_constraint(Thermal_Limit_to <= 0);

    /* Resolve it! */
    solver solve_Subr(Subr,ipopt);
    solve_Subr.run();

    obj.print(true);
    
   // OA = obj.get_outer_app();
    return Subr._obj_val;
}


int main (int argc, const char * argv[])
{
    // Decompose
    PowerNet* grid = new PowerNet();
    const char* fname;
    //fname = "../../data_sets/Power/nesta_case5_pjm.m";
    //fname = "../../data_sets/Power/nesta_case2383wp_mp.m";
    fname = "../../data_sets/Power/nesta_case14_ieee.m";
    //fname = "../../data_sets/Power/nesta_case1354_pegase.m";

    grid->readgrid(fname);
    // Grid Parameters
    unsigned nb_gen = grid->get_nb_active_gens();
    unsigned nb_lines = grid->get_nb_active_arcs();
    unsigned nb_buses = grid->get_nb_active_nodes();

    DebugOn("nb gens = " << nb_gen << endl);
    DebugOn("nb lines = " << 2*nb_lines << endl);
    DebugOn("nb buses = " << nb_buses << endl);

    // Schedule
    unsigned T = 1;
    param<Real> rate_ramp("rate_ramp");
    param<Real> rate_switch("rate_switch");
    param<Real> min_up("min_up");
    param<Real> min_down("min_down");
    param<Real> cost_up("cost_up");
    param<Real> cost_down("cost_down");

    for (auto g: grid->gens) {
        rate_ramp(g->_name) = max(grid->pg_min(g->_name).getvalue(), 0.25*grid->pg_max(g->_name).getvalue());
        rate_ramp._dim++;
        rate_switch(g->_name) = max(grid->pg_min(g->_name).getvalue(), 0.25*grid->pg_max(g->_name).getvalue());
        rate_switch._dim++;
    }
    min_up = 1;
    min_down = 1;
    cost_up = 50;
    cost_down = 30;

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
    rate_ramp.time_expand(T);
    rate_switch.time_expand(T);

    /** Clique tree decomposition **/
    Net* chordal = grid->get_chordal_extension();
    grid->get_clique_tree();
    const unsigned nb_cliques = grid->_bags.size();
    vector<Bus*> bag_bus[nb_cliques]; // Note that each clique contains just nodes, not buses!
    vector<Gen*> bag_gens[nb_cliques];
    vector<Arc*> bag_arcs[nb_cliques]; //bag_arcs contains the arcs of the power grid while variables associated with W are defined on chordal graph.

    for (int c = 0; c < nb_cliques; c++) {
        for (int i = 0; i < grid->_bags[c].size(); i++) {
            Bus* bus = (Bus*) grid->get_node(grid->_bags[c].at(i)->_name);
            if (bus !=nullptr) {
                bag_bus[c].push_back(bus);
            }
            if (bus->_has_gen) {
                bag_gens[c].insert(bag_gens[c].end(), bus->_gen.begin(), bus->_gen.end());
            }
            for (int j = i+1; j < grid->_bags[c].size(); j++) {
                Arc* a = (Arc*)grid->get_arc(bus, grid->get_node(grid->_bags[c].at(j)->_name));
                if (a != nullptr)
                    bag_arcs[c].push_back(a);
            }
        }
        DebugOn("bag " << c << " has " << bag_gens[c].size() << " generators. " << endl;)
        DebugOn("bag " << c << " has " << bag_arcs[c].size() << " line " << endl;)
    }


#ifdef USE_BOOST
    /** Note that we also need the edge information of the clique tree **/
    /** boost graph library or implement the expanded version of MCS algorithm by Blair and Peyton */
    typedef boost::adjacency_list <boost::vecS,
            boost::vecS,
            boost::undirectedS,
            boost::no_property,
            boost::property < boost::edge_weight_t, int >
            > Graph;
    typedef boost::graph_traits <Graph>::edge_descriptor Edge;
    typedef boost::graph_traits <Graph>::vertex_descriptor Vertex;

    // BUILD THE INTERSECTION GRAPH OF THE CLIQUES
    typedef std::pair<int, int> E;
    std::vector<E> edges;
    std::vector<int> weights;
    for (int i = 0; i < nb_cliques; i++) {
        cout << "bag " << i << " has " << grid->_bags[i].size() << " vertices." <<endl;
        sort(grid->_bags[i].begin(), grid->_bags[i].end());
        for (int j = i +1; j < nb_cliques; j++) {
            vector<Node*> v3;
            sort(grid->_bags[j].begin(), grid->_bags[j].end());
            set_intersection(grid->_bags[i].begin(), grid->_bags[i].end(), grid->_bags[j].begin(), grid->_bags[j].end(), back_inserter(v3));
            if (v3.size() > 0) {
                edges.push_back(E(i, j));
                weights.push_back(-v3.size());
            }
        }
    }
    size_t num_edges = edges.size();

#if defined(BOOST_MSVC) && BOOST_MSVC <= 1300
    Graph g(num_nodes);
    boost::property_map<Graph, edge_weight_t>::type weightmap = get(edge_weight, g);
    for (std::size_t j = 0; j < num_edges; ++j) {
        Edge e;
        bool inserted;
        boost::tie(e, inserted) = boost::add_edge(edges[j].first, edges[j].second, g);
        boost::weightmap[e] = weights[j];
    }
#else
    Graph g(edges.begin(), edges.end(), weights.begin(), nb_cliques);
#endif
    boost::property_map < Graph, boost::edge_weight_t >::type weight = get(boost::edge_weight, g);
    std::vector < Edge > spanning_tree;
    boost::kruskal_minimum_spanning_tree(g, std::back_inserter(spanning_tree));

    DebugOn("Print the total " << spanning_tree.size() << " edges in the clique tree:" << endl);

    //////////CLIQUE TREE /////////////////////////////
    Net* cliquetree = new Net();
    Node* node = nullptr;
    Arc*  a = nullptr;
    string name;
    for (int i = 0; i < nb_cliques; i++) {
        node= new Node(to_string(i), i);
        cliquetree->add_node(node);
    }

    for (std::vector < Edge >::iterator ei = spanning_tree.begin();
            ei != spanning_tree.end(); ++ei) {
        int u = source(*ei, g);
        int v = target(*ei, g);
        DebugOn(u << " <--> " << v
                << " with weight of " << -weight[*ei]
                << endl);
        name = (int) cliquetree->arcs.size();
        a = new Arc(name);
        a->_id = cliquetree->arcs.size();

        // intersection
        vector<Node*> v3;
        sort(grid->_bags[u].begin(), grid->_bags[u].end());
        sort(grid->_bags[v].begin(), grid->_bags[v].end());
        set_intersection(grid->_bags[u].begin(), grid->_bags[u].end(),
                         grid->_bags[v].begin(), grid->_bags[v].end(),
                         back_inserter(v3));
        
        a->_src = cliquetree->get_node(to_string(u));
        a->_dest = cliquetree->get_node(to_string(v));
        a->_weight = -weight[*ei];
        a->_intersection = v3;
        cliquetree->add_arc(a);
        a->connect();
    }
    const unsigned nb_clt_edges = spanning_tree.size();


///////////////// DEFINE LAGRANGE MULTIPLIERS  ////////////////////////////////
    vector<param<Real>> lambda_in;
    vector<param<Real>> mu_in;
    vector<param<Real>> kappa_in;
    vector<param<Real>> eta_in;

    vector<param<Real>> lambda_out;
    vector<param<Real>> mu_out;
    vector<param<Real>> kappa_out;
    vector<param<Real>> eta_out;

    vector<param<Real>> lambda_sep;
    vector<param<Real>> mu_sep;
    vector<param<Real>> kappa_sep;
    vector<param<Real>> eta_sep;

    vector<param<Real>> lambda_grad;
    vector<param<Real>> mu_grad;
    vector<param<Real>> kappa_grad;
    vector<param<Real>> eta_grad;

    for (auto a: cliquetree->arcs) {
        int l = a->_id;
        param<Real> lambda_C_in("lambda_C_in" + to_string(l));
        param<Real> mu_C_in("lambda_C_in" + to_string(l));
        param<Real> kappa_C_in("lambda_C_in" + to_string(l));
        param<Real> eta_C_in("lambda_C_in" + to_string(l));
        lambda_C_in^(a->_weight*(a->_weight-1)/2);
        mu_C_in^(a->_weight);
        kappa_C_in^(a->_weight);
        eta_C_in^(a->_weight);

        param<Real> lambda_C_out("lambda_C_out" + to_string(l));
        param<Real> mu_C_out("lambda_C_out" + to_string(l));
        param<Real> kappa_C_out("lambda_C_out" + to_string(l));
        param<Real> eta_C_out("lambda_C_out" + to_string(l));
        lambda_C_out^(a->_weight*(a->_weight-1)/2);
        mu_C_out^(a->_weight);
        kappa_C_out^(a->_weight);
        eta_C_out^(a->_weight);

        param<Real> lambda_C_sep("lambda_C_sep" + to_string(l));
        param<Real> mu_C_sep("lambda_C_sep" + to_string(l));
        param<Real> kappa_C_sep("lambda_C_sep" + to_string(l));
        param<Real> eta_C_sep("lambda_C_sep" + to_string(l));
        lambda_C_sep^(a->_weight*(a->_weight-1)/2);
        mu_C_sep^(a->_weight);
        kappa_C_sep^(a->_weight);
        eta_C_sep^(a->_weight);

        param<Real> lambda_C_grad("lambda_C_grad" + to_string(l));
        param<Real> mu_C_grad("lambda_C_grad" + to_string(l));
        param<Real> kappa_C_grad("lambda_C_grad" + to_string(l));
        param<Real> eta_C_grad("lambda_C_grad" + to_string(l));

        for (int i = 0; i < a->_weight; i++) {
            mu_C_sep(i) = 0;
        }

        lambda_in.push_back(lambda_C_in);
        mu_in.push_back(mu_C_in);
        kappa_in.push_back(kappa_C_in);
        eta_in.push_back(eta_C_in);

        lambda_out.push_back(lambda_C_out);
        mu_out.push_back(mu_C_out);
        kappa_out.push_back(kappa_C_out);
        eta_out.push_back(eta_C_out);

        lambda_sep.push_back(lambda_C_sep);
        mu_sep.push_back(mu_C_sep);
        kappa_sep.push_back(kappa_C_sep);
        eta_sep.push_back(eta_C_sep);

        lambda_grad.push_back(lambda_C_grad);
        mu_grad.push_back(mu_C_grad);
        kappa_grad.push_back(kappa_C_grad);
        eta_grad.push_back(eta_C_grad);
    }

    for (auto a: cliquetree->arcs) {
        int l = a->_id;
        for (int i = 0; i < a->_weight; i++) {
            mu_in[l](i) = 0;
            mu_sep[l](i) = 0;
            mu_out[l](i) = 0;
            mu_grad[l](i) = 0;

            kappa_in[l](i) = 0;
            kappa_sep[l](i) = 0;
            kappa_out[l](i) = 0;
            kappa_grad[l](i) = 0;

            eta_in[l](i) = 0;
            eta_sep[l](i) = 0;
            eta_out[l](i) = 0;
            eta_grad[l](i) = 0;

            for (int j = i+1; j < a->_weight; j++) {
                lambda_in[l](i, j) = 0;
                lambda_sep[l](i, j) = 0;
                lambda_out[l](i, j) = 0;
                lambda_grad[l](i, j) = 0;
            }
        }
    }

/////////////////////////////////// INITIALISE MAIN ///////////////////////////////////
    Model Master("Master");
    /** param **/
    param<Real> gamma_in("gamma_C_in");
    param<Real> gamma_out("gamma_C_out");
    param<Real> gamma_sep("gamma_C_sep");
    gamma_in^nb_cliques;
    gamma_out^nb_cliques;
    gamma_sep^nb_cliques;
    
    /** Variables  */
    var<Real> gamma_C("gamma_C");
    vector<var<Real>> lambda_var;
    vector<var<Real>> mu_var;
    vector<var<Real>> kappa_var;
    vector<var<Real>> eta_var;

    Master.add_var(gamma_C^nb_cliques);
    for (auto a: cliquetree->arcs) {
        var<Real> lambda("lambda_C" + to_string(a->_id));
        var<Real> mu("lambda_C" + to_string(a->_id));
        var<Real> kappa("lambda_C" + to_string(a->_id));
        var<Real> eta("lambda_C" + to_string(a->_id));

        lambda_var.push_back(lambda);
        mu_var.push_back(mu);
        kappa_var.push_back(kappa);
        eta_var.push_back(eta);

        Master.add_var(lambda^(a->_weight*(a->_weight -1)/2));
        Master.add_var(mu^a->_weight);
        Master.add_var(kappa^a->_weight);
        Master.add_var(eta^a->_weight);
    }
    /////////** OBJ*//////////////
    func_ master_obj = sum(gamma_C);
    Master.set_objective(master_obj);
    double bound = 100000000;

    Constraint UB;
    UB = sum(gamma_C) - bound;
    Master.add_constraint(UB <= 0);

////////////////  CONVERGENCE INFORMATION /////////////////////////
    unsigned iter_limit;
    cout << "Enter the limit of the number of iterations: ";
    cin >> iter_limit;
    cout << endl;

    double LBlog[iter_limit];
    double UBlog[iter_limit];

    for(int iter = 0; iter < iter_limit; iter++) {
        LBlog[iter] = 0.0;
    }

    double LDlog[nb_cliques];

    // log of solutions
    vector<param<Real>> lambda_log;
    vector<param<Real>> mu_log;
    vector<param<Real>> kappa_log;
    vector<param<Real>> eta_log;

    for (auto a: cliquetree->arcs) {
        int l = a->_id;
        param<Real> lambda_C_log("lambda_C_log" + to_string(l));
        param<Real> mu_C_log("lambda_C_log" + to_string(l));
        param<Real> kappa_C_log("lambda_C_log" + to_string(l));
        param<Real> eta_C_log("lambda_C_log" + to_string(l));
        lambda_C_log^(a->_weight*(a->_weight-1)/2);
        mu_C_log^(a->_weight);
        kappa_C_log^(a->_weight);
        eta_C_log^(a->_weight);
    }


    //param<Real> R_Wij_log("R_Wij_log");
    //param<Real> Im_Wij_log("Im_Wij_log");
    //param<Real> Wii_log("Wii_log");
    //param<Real> Pg_log("Pg_log");
    //param<Real> Qg_log("Qg_log");
    //param<Real> On_off_log("On_off_log");


///////////////////////////////// INITIALIZATION ///////////////////////////////////////////
    double wall0 = get_wall_time();
    double cpu0  = get_cpu_time();
    double value_dual = 0;
    //func_ OA;
    for (int c = 0; c < nb_cliques; c++) {
        value_dual += subproblem(grid, chordal, T, c, cliquetree,
                bag_bus[c], bag_gens[c], bag_arcs[c],
                lambda_sep, mu_sep, kappa_sep, eta_sep,  
                rate_ramp, rate_switch, min_up, min_down, cost_up, cost_down);
     //       OA.print(true);
    }


cout << "................  Initialization value:  " << value_dual <<endl;

/////////////////// APPEND MORE CONSTRAINTS TO MAIN //////////////////////////////////
        //if (iter_limit > 0) {
        //    for (c = 0; c < nb_cliques; c++)
        //    {
        //        Constraint Concavity;  
        //        Master.add_constraint(gamma_C[c] <= value_dual[r]
        //                      + IloScalProd(w_cpulog[r], lambda_var) - IloScalProd(w_cpulog[r], lambdasep)
        //                      + IloScalProd(z_meomlog[r], mu_var) - IloScalProd(z_meomlog[r], musep)
        //    }
        //}

////////////////////////// BEGIN LAGRANGE ITERATIONS HERE /////////////////////////////////////
    cout << "<<<<<<<<<<< Lagrangean decomposition algorithm >>>>>>>>>"<< endl;
    cout<< setw(15) << left <<"ITERATION" << setw(15) << "LB" << setw(15)  << "UB" << endl;
    //for(int itcount = 0; itcount < iter_limit; itcount++) {
    //    double value_dual = 0;
    //    for (int c = 0; c < nb_cliques; c++) {
    //        DebugOn("bag_arc " << c << "has " << bag_arcs[c].size() << " lines" << endl);
    //        value_dual += subproblem(grid, T, c, bag_bus[c], bag_gens[c], bag_arcs[c], rate_ramp, rate_switch, min_up, min_down, cost_up, cost_down);
    //    }
    //}
    return 0;
#endif
}
