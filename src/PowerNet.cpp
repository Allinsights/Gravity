//
//  PowerNet.cpp
//
//
//  Created by Guanglei Wang on 03/06/2017.
//

#include <gravity/PowerNet.h>
#include <algorithm>
#include <map>
#define _USE_MATH_DEFINES
#include <cmath>
#include <list>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <queue>
#include <time.h>
//#define USEDEBUG
#ifdef USEDEBUG
#define Debug(x) cout << x
#else
#define Debug(x)
#endif
#define DebugOn(x) cout << x
#define DebugOff(x)

using namespace std;


static int max_line_len;
static char* line = nullptr;

PowerNet::PowerNet(){
    bMVA=0;
}

PowerNet::~PowerNet(){
    if(!gens.empty()){
        for (Gen* g:gens){
            delete g;
        }
        gens.clear();
    }
}
//
//Bus* Net::get_bus(string name) {
//    return nodeID.find(name)->second;
//}
//
// Read a grid
// @discussion line with delimiter ";"

int PowerNet::readgrid(const char* fname) {
    
    string name;
    double pl = 0, ql = 0, gs = 0, bs = 0, kvb = 0, vmin = 0, vmax = 0, vs = 0;
    int id = 0;
    cout << "Loading file " << fname << endl;
    ifstream file(fname, std::ifstream::in);
    if(!file.is_open()){
        cout << "Could not open file\n";
        return -1;
    }
    
    string word;
    while (word.compare("function")){
        file >> word;
    }
    
    file.ignore(6);
    file >> word;
    _name = word;
    
//  cout << _name << endl;
    while (word.compare("mpc.baseMVA")){
        file >> word;
    }
    
    file.ignore(3);
    getline(file, word,';');
    bMVA = atoi(word.c_str());
    
//  cout << "BaseMVA = " << bMVA << endl;
    
    
    /* Nodes data */
    while (word.compare("mpc.bus")){
        file >> word;
    }
    
    getline(file, word);
    Bus* bus = NULL;
    Bus* bus_clone= NULL;
    file >> word;
    
    while(word.compare("];")){
        name = word.c_str();
        id = atoi(name.c_str());
        file >> ws >> word >> ws >> word;
        pl = atof(word.c_str())/bMVA;
        file >> word;
        ql = atof(word.c_str())/bMVA;
        file >> word;
        gs = atof(word.c_str())/bMVA;
        file >> word;
        bs = atof(word.c_str())/bMVA;
        file >> ws >> word >> ws >> word;
        vs = atof(word.c_str());
        file >> ws >> word >> ws >> word;
        kvb = atof(word.c_str());
        file >> ws >> word >> ws >> word;
        vmax = atof(word.c_str());
        getline(file, word,';');
        vmin = atof(word.c_str());
        // single phase
        bus = new Bus(name, pl, ql, gs, bs, vmin, vmax, kvb, 1);
        bus_clone = new Bus(name, pl, ql, gs, bs, vmin, vmax, kvb, 1);
        bus->vs = vs;
        bus_clone->vs = vs;
        
        this->Net::add_node(bus);
        file >> word;
    }
    file.seekg (0, file.beg);
    
    
    /* Generator data */
    while (word.compare("mpc.gen")){
        file >> word;
    }
    double qmin = 0, qmax = 0, pmin = 0, pmax = 0, ps = 0, qs = 0;
    int status = 0;
    getline(file, word);
    
    
    file >> word;
    std::vector<bool> gen_status;
    
    while(word.compare("];")){
        name = word.c_str();
        // name -> node. 
        bus = (Bus*)(Net::get_node(name));
        file >> word;
        ps = atof(word.c_str())/bMVA;
        file >> word;
        qs = atof(word.c_str())/bMVA;
        file >> word;
        qmax = atof(word.c_str())/bMVA;
        file >> word;
        qmin = atof(word.c_str())/bMVA;
        
        file >> ws >> word >> ws >> word >> ws >> word;
        status = atof(word.c_str());
        file >> word;
        pmax = atof(word.c_str())/bMVA;
        
        file >> word;
        pmin = atof(word.c_str())/bMVA;
        
        getline(file, word,'\n');
        gen_status.push_back(status==1);
        
        if(status==1){
            bus->_has_gen = true;
            /** generator name, ID */
            Gen* g = new Gen(bus, to_string(gens.size()+1), pmin, pmax, qmin, qmax);
            g->ID = gens.size();
            g->ps = ps;
            g->qs = qs;
            gens.push_back(g);
            bus->_gen.push_back(g);
        }
//        getline(file, word);
        file >> word;
    }
    
    
    file.seekg (0, file.beg);
    
    /* Generator costs */
    while (word.compare("mpc.gencost")){
        file >> word;
    }
    double c0 = 0, c1 = 0,c2 = 0;
    getline(file, word);
    
    int gen_counter = 0;
    for (int i = 0; i < gen_status.size(); ++i) {
        file >> ws >> word >> ws >> word >> ws >> word >> ws >> word >> ws >> word;
        c2 = atof(word.c_str());
        file >> word;
        c1 = atof(word.c_str());
        file >> word;
        c0 = atof(word.c_str());
        if (gen_status[i]) {
          gens[gen_counter]->set_costs(c0, c1, c2);
          gen_counter++;
        }
        getline(file, word);
    }
    file.seekg (0, file.beg);
    
    /* Lines data */
    m_theta_lb = 0;
    m_theta_ub = 0;
    while (word.compare("mpc.branch")){
        file >> word;
    }
    getline(file, word);
    double res = 0;
    
    Line* arc = NULL;
    string src,dest;
    file >> word;
    while(word.compare("];")){
        src = word;
        file >> dest;
        id = (int)arcs.size() + 1;
        
        arc = new Line(to_string(id));
        
        arc->id = id;
        
        arc->src = get_node(src);
        arc->dest= get_node(dest);
        
        
        file >> word;
        arc->r = atof(word.c_str());
        file >> word;
        arc->x = atof(word.c_str());
        res = pow(arc->r,2) + pow(arc->x,2);
        
        if (res==0) {
            cerr << " line with r = x = 0" << endl;
            exit(-1);
        }
        // define g and b for each conductor.
        arc->g = arc->r/res;
        arc->b = -arc->x/res;
        
        file >> word;
        arc->ch = atof(word.c_str());
        file >> word;
        arc->limit = atof(word.c_str())/bMVA;
        
        file >> ws >> word >> ws >> word >> ws >> word;
        if(atof(word.c_str()) == 0)
            arc->tr = 1.0;
        else
            arc->tr = atof(word.c_str());
        file >> word;
        arc->as = atof(word.c_str())*M_PI/180;
        file >> word;
        
        arc->cc = arc->tr*cos(arc->as);
        arc->dd = arc->tr*sin(arc->as);
        arc->status = atof(word.c_str());
        file >> word;
        
        arc->tbound.min = atof(word.c_str())*M_PI/180;
        m_theta_lb += arc->tbound.min;
        file >> word;
        
        arc->tbound.max = atof(word.c_str())*M_PI/180;
        m_theta_ub += arc->tbound.max;
        
        Bus* bus_s = (Bus*)(arc->src);
        Bus* bus_d = (Bus*)(arc->dest);

    
        
        /**
         arc->smax = max(pow((Bus*)(arc->src)->vbound.max,2)*(arc->g*arc->g+arc->b*arc->b)*(pow(arc->src->vbound.max,2)+ pow(arc->dest->vbound.max,2)),
         pow(arc->dest->vbound.max,2)*(arc->g*arc->g+arc->b*arc->b)*(pow(arc->dest->vbound.max,2) + pow((Bus*)(arc->src)->vbound.max,2)));
        */
        
        arc->smax = max(
                        pow(bus_s->vbound.max,2)*(arc->g*arc->g + arc->b*arc->b)*(pow(bus_s->vbound.max,2) + pow(bus_d->vbound.max,2)),
                        pow(bus_d->vbound.max,2)*(arc->g*arc->g+arc->b*arc->b)*(pow(bus_d->vbound.max,2) + pow(bus_s->vbound.max,2))
                        );
        
        
        if(arc->status == 1){
            arc->connect();
        }
        else {
            delete arc;
        }

        getline(file, word,'\n');
        file >> word;
    }
    file.close();
    return 0;
}
