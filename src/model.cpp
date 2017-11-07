//
//  model.cpp
//  Gravity
//
//  Created by Hijazi, Hassan (Data61, Canberra City) on 6/5/17.
//
//

#include <gravity/model.h>


using namespace std;
using namespace gravity;

/** Constructor */
//@{
Model::Model(){
    _nnz_g = 0;
    _nnz_h = 0;
};
//@}

/* Destructor */
Model::~Model(){
    for (auto &vp:_vars) {
        delete vp.second;
    }
//    for (auto &cp:_cons) {
//        delete cp.second;
//    }
};


/* Accessors */
size_t Model::get_nb_vars() const{
    return _nb_vars;
};

size_t Model::get_nb_cons() const{
    size_t n = 0;
    for (auto &cp:_cons) {
        n += cp.second->get_nb_instances();
    }
    return n;
};


size_t Model::get_nb_nnz_g() const{    
    return _nnz_g;
};

//Split "mem" into "parts", e.g. if mem = 10 and parts = 4 you will have: 0,2,4,6,10
//if possible the function will split mem into equal chuncks, if not
//the last chunck will be slightly larger

std::vector<int> bounds(int parts, int mem) {
    std::vector<int>bnd;
    int delta = mem / parts;
    int reminder = mem % parts;
    int N1 = 0, N2 = 0;
    bnd.push_back(N1);
    for (int i = 0; i < parts; ++i) {
        N2 = N1 + delta;
        if (i == parts - 1)
            N2 += reminder;
        bnd.push_back(N2);
        N1 = N2;
    }
    return bnd;
}


/* Return the number of nonzeros in the lower left part of the hessian */
size_t Model::get_nb_nnz_h() const{    
    size_t nnz = 0;
    string vi_name;
    shared_ptr<param_> vi;
    set<pair<func_*,func_*>> s;
    for (auto &pairs: _hess_link) {
        vi_name = pairs.first.first;
        s = pairs.second;
        vi = (*s.begin()).first->get_var(vi_name);
        nnz += (*s.begin()).first->get_nb_instances();
    }
    return nnz;
};



Constraint* Model::get_constraint(const string& cname) const{
    return (Constraint*)&_cons_name.at(cname);
}

param_* Model::get_var(const string& vname) const{
        return _vars_name.at(vname);
}



/* Modifiers */

void Model::init_indices(){// Initialize the indices of all variables involved in the model
    param_* v= nullptr;
    size_t idx = 0;
    for(auto& v_p: _vars)
    {
        v = v_p.second;
        v->set_id(idx);
        for (int i = 0; i < v->get_dim(); i++) {
            idx++;
        }
    }
}

void Model::add_var(param_* v){
    if (v->_is_indexed) {
        return;
    }

    if (_vars_name.count(v->get_name()) == 0) {
        v->set_id(_nb_vars);
        v->set_vec_id(_vars.size());
        _vars_name[v->get_name()] = v;
        _vars[v->get_vec_id()] = v;
        _nb_vars += v->get_dim();
    }
};

void Model::add_var(param_& v){
    if (v._is_indexed) { 
        return;
    }
    if (_vars_name.count(v.get_name())==0) {
        v.set_id(_nb_vars);
        v.set_vec_id(_vars.size());
        auto newv = (param_*)copy(v);
        _vars_name[v.get_name()] = newv;
        _vars[v.get_vec_id()] = newv;
        _nb_vars += v.get_dim();
    }
};


void Model::del_var(const param_& v){
    auto it = _vars.find(v.get_id());
    if (it!=_vars.end()) {
        _nb_vars -= v.get_dim();
        delete it->second;
        _vars.erase(it);
    }
};


void Model::add_param(param_* v){
    if (_params.count(v->get_id())==0) {
        _nb_params += v->get_dim();
        v->set_id(_params.size());
        v->set_vec_id(_vars.size());
        _params_name[v->get_name()] = v;
        _params[v->get_vec_id()] = v;
    }
};

void Model::add_param(param_& v){
    if (_params.count(v.get_id())==0) {
        _nb_params += v.get_dim();
        auto newv = (param_*)copy(v);
        v.set_id(_params.size());
        v.set_vec_id(_vars.size());
        newv->set_id(_params.size());
        newv->set_vec_id(_vars.size());
        _params_name[v.get_name()] = newv;
        _params[v.get_vec_id()] = newv;
    }
};


void Model::del_param(const param_& v){
    auto it = _params.find(v.get_id());
    if (it!=_params.end()) {
        _nb_params -= v.get_dim();
        delete it->second;
        _params.erase(it);
    }
};

void Model::add_constraint(const Constraint& c){
    if (_cons_name.count(c.get_name())==0) {
        auto newc = make_shared<Constraint>(c);
        if (newc->is_constant()) {
            switch (newc->_ctype) {
                case leq:
                    if (newc->eval()>c.get_rhs()) {
                        throw invalid_argument("Adding violated constant constraint!\n");
                    }
                    break;
                case geq:
                    if (newc->eval()<c.get_rhs()) {
                        throw invalid_argument("Adding violated constant constraint!\n");
                    }
                    break;
                case eq:
                    if (newc->eval()!=c.get_rhs()) {
                        throw invalid_argument("Adding violated constant constraint!\n");
                    }
                    break;
                default:
                    break;
            }
            DebugOn("WARNING: Adding redundant constant constraint, Gravity will be ignoring it.\n");
            return;
        }

        newc->update_to_str();
//        newc->print();
        if (newc->is_nonlinear()) {
            embed(*newc->get_expr());
        }
        newc->_id = _nb_cons;
        _cons_name[c.get_name()] = newc;
        _cons[newc->_id] = newc;
    }
    else {
        throw invalid_argument("rename constraint as this name has been used by another one: " + c.to_str());
    }
    int nb_inst = c.get_nb_instances();
    _nb_cons += nb_inst;
    _nnz_g += c.get_nb_vars()*nb_inst;
    if (_type==lin_m && c.is_quadratic()) {
        _type = quad_m;
    }
    if ((_type==lin_m || _type==quad_m) && (c.is_nonlinear() || c.is_polynomial())) {
        _type = nlin_m;
    }
};



void Model::del_constraint(const Constraint& c){
    //    _cons.erase(c->get_idx());
    assert(false);
};

void Model::set_objective(const func_& f, ObjectiveType t) {
    _obj = f;
    _objt = t;
//    embed(_obj);
}

void Model::set_objective(pair<func_*, ObjectiveType> p){
    _obj = *p.first;
    _objt = p.second;
}

void Model::set_objective_type(ObjectiveType t) {
    _objt = t;
}


void Model::check_feasible(const double* x){
//    int vid = 0;
    //    param_* v = NULL;
//    var<>* var = NULL;
    /* return the structure of the hessian */
//    for(auto& v: _vars)
//    {
//        vid = v->get_idx();
//        var = getparam_<double>(vid);
//        if ((x[vid] - var->get_ub())>1e-6) {
//            cerr << "violated upper bound constraint: ";
//            var->print();
//        }
//        if ((x[vid] - var->get_lb())<-1e-6) {
//            cerr << "violated lower bound constraint: ";
//            var->print();
//        }
//    }
//    int cid = 0;
//    for(auto& c: _cons)
//    {
//        cid = c->get_idx();
//        switch (c->get_type()) {
//            case eq:
//                if(fabs(c->eval(x)-c->_rhs) > 1e-6) {
//                    cerr << "violated constraint: ";
//                    c->print();
//                    printf ("violation = %.10f;\n",(c->eval(x)-c->_rhs));
//                }
//                break;
//            case leq:
//                if((c->eval(x)-c->_rhs) > 1e-6) {
//                    cerr << "violated constraint: ";
//                    c->print();
//                    printf ("violation = %.10f;\n",(c->eval(x)-c->_rhs));
//                }
//                break;
//            case geq:
//                if((c->eval(x)-c->_rhs) < -1e-6) {
//                    cerr << "violated constraint: ";
//                    c->print();
//                    printf ("violation = %.10f;\n",(c->eval(x)-c->_rhs));
//                }
//                break;
//                
//            default:
//                break;
//        }
//    }
}


void Model::fill_in_var_bounds(double* x_l ,double* x_u) {
    unsigned vid;
    param_* v;
    for(auto& v_p: _vars)
    {
        v = v_p.second;
        vid = v->get_id();
        switch (v->get_intype()) {
            case float_: {
                auto real_var = (var<float>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    x_l[vid+i] = (double)real_var->get_lb(i);
                    x_u[vid+i] = (double)real_var->get_ub(i);
                }
                break;
            }
            case long_:{
                auto real_var = (var<long double>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    x_l[vid+i] = (double)real_var->get_lb(i);
                    x_u[vid+i] = (double)real_var->get_ub(i);
                }
                break;
            }
            case double_:{
                auto real_var = (var<double>*)v;
                DebugOff(real_var->get_name() << " in:" << endl);
                for (int i = 0; i < real_var->get_dim(); i++) {
                    x_l[vid+i] = real_var->get_lb(i);
                    x_u[vid+i] = real_var->get_ub(i);
                    DebugOff("(" << i << ")" << " : [" << x_l[vid+i] << "," << x_u[vid+i] << "]\n");
                }
                DebugOff(";" << endl);
                break;
            }
            case integer_:{
                auto real_var = (var<int>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    x_l[vid+i] = (double)real_var->get_lb(i);
                    x_u[vid+i] = (double)real_var->get_ub(i);
                }
                break;
            }
            case short_:{
                auto real_var = (var<short>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    x_l[vid+i] = (double)real_var->get_lb(i);
                    x_u[vid+i] = (double)real_var->get_ub(i);
                }
                break;
            }
            case binary_:{
                auto real_var = (var<bool>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {                    
                    x_l[vid+i] = (double)real_var->get_lb(i);
                    x_u[vid+i] = (double)real_var->get_ub(i);
                }
                break;
            }
            default:
                break;
        } ;
    }
    //    cout << "idx = " << idx << endl;
}

void Model::set_x(const double* x){
    unsigned vid;
    param_* v;
    for(auto& v_p: _vars)
    {
        v = v_p.second;
        vid = v->get_id();
        switch (v->get_intype()) {
            case float_: {
                auto real_var = (var<float>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {                    
                    real_var->set_val(i, x[vid+i]);
                }
                break;
            }
            case long_:{
                auto real_var = (var<long double>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    real_var->set_val(i, x[vid+i]);
                }
                break;
            }
            case double_:{
                auto real_var = (var<double>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    real_var->set_val(i, x[vid+i]);
                }
                break;
            }
            case integer_:{
                auto real_var = (var<int>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    real_var->set_val(i, x[vid+i]);
                }
                break;
            }
            case short_:{
                auto real_var = (var<short>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    real_var->set_val(i, x[vid+i]);
                }
                break;
            }
            case binary_:{
                auto real_var = (var<bool>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    real_var->set_val(i, x[vid+i]);
                }
                break;
            }
            default:
                break;
        } ;
    }
}

void Model::compute_funcs() {    
    auto it = _nl_funcs.begin();
    while (it!=_nl_funcs.end()) {
        auto f = (*it++);
        DebugOff(f->to_str() << endl);
        for (int inst = 0; inst < f->get_nb_instances(); inst++) {
            f->_val->at(inst) = f->eval(inst);
        }
    }
    for (auto &c_p:_cons) {
        auto c = c_p.second;
        for (int inst = 0; inst < c->get_nb_instances(); inst++) {
            c->_val->at(inst) = c->eval(inst);
        }
        for (auto &dfdx: *c->get_dfdx()) {
            for (int inst = 0; inst < dfdx.second->get_nb_instances(); inst++) {
                dfdx.second->_val->at(inst) = dfdx.second->eval(inst);
            }
            for (auto &dfd2x: *dfdx.second->get_dfdx()) {
                for (int inst = 0; inst < dfd2x.second->get_nb_instances(); inst++) {
                    dfd2x.second->_val->at(inst) = dfd2x.second->eval(inst);
                }
            }
        }
    }
}

void Model::fill_in_obj(const double* x , double& res, bool new_x){
//    if (!new_x) {// IPOPT SEEMS TO BE INCONSISTENT ON NEW_X HERE!!
    if (new_x) {
        set_x(x);
        compute_funcs();
    }
//        set_x(x);
        res = _obj.eval();
        _obj_val = res;
//    }
//    else {
//        res = _obj_val;
//    }
    DebugOff("Objective = " << to_string(res) << endl);
}

void Model::fill_in_grad_obj(const double* x , double* res, bool new_x){
    param_* v;
    shared_ptr<func_> df;
    unsigned vid, vid_inst, index = 0;
    unique_id v_unique;
    for (int i = 0; i<_nb_vars; i++) {
        res[i] = 0;
    }
    if (_first_call_gard_obj) {
        _obj_grad_vals.resize(_obj.get_nb_vars());
        _first_call_gard_obj = false;
    }
    else if (_obj.is_linear()) {
//    else if (false) { /* No need to recompute jacobian for linear objectives or if x is not new */
        for(auto& vi_p: _obj.get_vars())
        {
            v = vi_p.second.first.get();
            vid = v->get_id();
            if (v->_is_vector) {
                for (int i = 0; i < v->get_dim(); i++) {
                    vid_inst = vid + v->get_id_inst(i);
                    res[vid_inst] = _obj_grad_vals[index++];
                }
            }
            else {
                vid_inst = vid + v->get_id_inst();
                res[vid_inst] = _obj_grad_vals[index++];
            }
        }
        return;
    }
    if (new_x) {
        set_x(x);
        compute_funcs();
    }
    for(auto& vi_p: _obj.get_vars()) {
        v = vi_p.second.first.get();
        vid = v->get_id();
        v_unique = v->_unique_id;
        df = _obj.get_stored_derivative(v_unique);
        if (v->_is_vector) {
            for (int i = 0; i < v->get_dim(); i++) {
                vid_inst = vid + v->get_id_inst(i);
                res[vid_inst] = df->eval(i);
                _obj_grad_vals[index++] =res[vid_inst];
            }
        }
        else {
            vid_inst = vid + v->get_id_inst();
            res[vid_inst] = df->eval();
            _obj_grad_vals[index++] =res[vid_inst];
        }
    }
}

void Model::fill_in_cstr(const double* x , double* res, bool new_x){
    Constraint* c = nullptr;
//    unsigned index = 0;
    if (new_x) {
        set_x(x);
        compute_funcs();
    }
        for(auto& c_p: _cons)
        {
            c = c_p.second.get();
            auto nb_ins = c->get_nb_instances();
            for (int inst = 0; inst< nb_ins; inst++){
                res[c->_id+inst] = c->get_val(inst);
//                _cons_vals[index++] = res[c->_id+inst];
                DebugOff("g[" << to_string(c->_id+inst) << "] = " << to_string(res[c->_id+inst]) << endl);
            }
        }
//    }
//    else {
//        
//        for(auto& c_p: _cons) {
//            c = c_p.second;
//            auto nb_ins = c->get_nb_instances();
//            for (int inst = 0; inst< nb_ins; inst++){
//                res[c->_id+inst] = _cons_vals[index++];
//            }
//        }
//    }
}

/* Fill the nonzero values in the jacobian */
void Model::fill_in_jac(const double* x , double* res, bool new_x){
//    if (!_first_call_jac && (!new_x || _type==lin_m)) { /* No need to recompute jacobian for linear models */
    if (!_first_call_jac && (_type==lin_m)) { /* No need to recompute jacobian for linear models */
        for (int i = 0; i< _nnz_g; i++) {
            res[i] = _jac_vals[i];
        }
        return;
    }
    if (new_x) {
        set_x(x);
        compute_funcs();
    }
    size_t idx=0;
    size_t cid = 0;
    unique_id vid;
    Constraint* c = NULL;
    param_* v = NULL;
    shared_ptr<func_> dfdx;
    for(auto& c_p :_cons)
    {
        c = c_p.second.get();
        auto nb_ins = c->get_nb_instances();
        if (c->is_linear() && !_first_call_jac) {
            DebugOff("Linear constraint, using stored jacobian!\n");
            for (int i = 0; i<c->get_nb_vars()*nb_ins; i++) {
                res[idx] = _jac_vals[idx];
                idx++;
            }
        }
        else {
            for (auto &v_p: c->get_vars()){
                v = v_p.second.first.get();
                vid = v->_unique_id;
                dfdx = c->get_stored_derivative(vid);
                for (int inst = 0; inst< nb_ins; inst++){
                    cid = c->_id+inst;
                    if (v->_is_vector) {
                        for (int j = 0; j<v->get_dim(); j++) {
                            res[idx] = dfdx->get_val(j);
                            _jac_vals[idx] = res[idx];
                            idx++;
                        }
                    }
                    else {
                        res[idx] = dfdx->get_val(inst);
                        _jac_vals[idx] = res[idx];
                        idx++;
                    }
                }
            }
        }
    }
    _first_call_jac = false;
}


void Model::fill_in_jac_nnz(int* iRow , int* jCol){
    size_t idx=0;
    size_t cid = 0;
    size_t vid = 0;
    Constraint* c = NULL;
    param_* v = NULL;
    /* return the structure of the jacobian */
    for(auto& c_p :_cons)
    {
        c = c_p.second.get();
        auto nb_ins = c->get_nb_instances();
        for (auto &v_p: c->get_vars()){
            v = v_p.second.first.get();
            vid = v->get_id();
            for (int inst = 0; inst< nb_ins; inst++){
                cid = c->_id+inst;            
                if (v->_is_vector) {
                    for (int j = 0; j<v->get_dim(); j++) {
                        iRow[idx] = cid;
                        jCol[idx] = vid + v->get_id_inst(j);
                        idx++;
                    }
                }
                else {
                        iRow[idx] = cid;
                        jCol[idx] = vid + v->get_id_inst(inst);
                        idx++;
                }
            }
        }
    }
    if (idx!=_nnz_g) {
        throw invalid_argument("idx!=_nnz_g");
    }
}



#ifdef USE_IPOPT
void Model::fill_in_var_linearity(Ipopt::TNLP::LinearityType* param_types){
    size_t vid = 0;
    bool linear = true;
    for(auto& vi: _vars)
    {
        vid = vi.second->get_id();
        for (int i = 0; i < vi.second->get_dim(); i++) {
//            linear = true;
//            for(auto &c: _v_in_cons[vid + i])
//            {
//                if (!c->is_linear()) {
//                    linear=false;
//                }
//            }
            if (linear) param_types[vid + i]=Ipopt::TNLP::LINEAR;
            else param_types[vid + i] = Ipopt::TNLP::NON_LINEAR;
        }
    }
}


void Model::fill_in_cstr_linearity(Ipopt::TNLP::LinearityType* const_types){
    Constraint* c = nullptr;
    bool lin = false;
    size_t cid = 0;
    for(auto& c_p :_cons)
    {
        c = c_p.second.get();
        if (c->is_linear() || c->is_constant()) {
            lin = true;
        }
        else {
            lin = false;
        }
        auto nb_ins = c->get_nb_instances();
        for (int i = 0; i< nb_ins; i++){
            cid = c->_id +i;
            if (lin) {
                const_types[cid]=Ipopt::TNLP::LINEAR;
            }
            else {
                const_types[cid] = Ipopt::TNLP::NON_LINEAR;
            }
        }
    }
}
#endif


void Model::fill_in_hess_nnz(int* iRow , int* jCol){
    size_t idx = 0, idx_all=0, vid, vjd;
    string vi_name, vj_name;
    shared_ptr<param_> vi;
    shared_ptr<param_> vj;
    unique_id vi_unique, vj_unique;
    set<pair<func_*,func_*>> s;
    for (auto &pairs: _hess_link) {
        vi_name = pairs.first.first;
        vj_name = pairs.first.second;
        s = pairs.second;
        vi = (*s.begin()).first->get_var(vi_name);
        vj = (*s.begin()).first->get_var(vj_name);
        if (vi_name.compare(vj_name) > 0) {//ONLY STORE LOWER TRIANGULAR PART OF HESSIAN
//            temp = vi;
//            vi = vj;
//            vj = temp;
            throw invalid_argument("SHOULD BE SORTED CORRECTLY IN FILL_MAPS");
        }
        vid = vi->get_id();
        vjd = vj->get_id();
        for (unsigned i = 0; i<(*s.begin()).first->get_nb_instances(); i++) {
            idx_all += s.size();
            iRow[idx] = vid + vi->get_id_inst(i);
            jCol[idx] = vjd + vj->get_id_inst(i);
            idx++;
        }
    }
    _hess_vals.resize(idx_all);
}
 
void Model::fill_in_hess(const double* x , double obj_factor, const double* lambda, double* res, bool new_x){
    size_t idx = 0, idx_in = 0;
    set<pair<func_*,func_*>> s;
    Constraint* c;
    func_* df;
    double hess = 0;
    if (_first_call_hess) {
        if (new_x) {
            set_x(x);
            compute_funcs();
        }
        for (auto &pairs: _hess_link) {
            s = pairs.second;
            for (unsigned inst = 0; inst<(*s.begin()).first->get_nb_instances(); inst++) {
                res[idx] = 0;
                for (auto f_pair:s) {
                    if (f_pair.first->_is_constraint) {
                        c = (Constraint*)f_pair.first;
                        df = f_pair.second;
//                        for (unsigned inst = 0; inst < c->get_nb_instances(); inst++) {
                            hess = df->get_val(inst);
                            _hess_vals[idx_in++] = hess;
                            res[idx] += lambda[c->_id + inst] * hess;
//                        }
                    }
                    else {
                        hess = f_pair.second->eval();
                        _hess_vals[idx_in++] = hess;
                        res[idx] += obj_factor * hess;
                    }
                }
                idx++;
            }
        }
        _first_call_hess = false;
        return;
    }
    
    if ((_type==lin_m || _type==quad_m)) { /* No need to recompute Hessian for quadratic models or if already computed for that point */
        for (auto &pairs: _hess_link) {
            s = pairs.second;
            for (unsigned inst = 0; inst<(*s.begin()).first->get_nb_instances(); inst++) {
                res[idx] = 0;
                for (auto f_pair:s) {
                    if (f_pair.first->_is_constraint) {
                        c = (Constraint*)f_pair.first;
                        res[idx] += lambda[c->_id + inst] * _hess_vals[idx_in++];
                    }
                    else {
                        res[idx] += obj_factor * _hess_vals[idx_in++];
                    }
                }
                idx++;
            }
        }
        return;
    }
    if (new_x) {
        set_x(x);
        compute_funcs();
    }
    for (auto &pairs: _hess_link) {
        s = pairs.second;
        for (unsigned inst = 0; inst<(*s.begin()).first->get_nb_instances(); inst++) {
            res[idx] = 0;            
            for (auto f_pair:s) {
                if (f_pair.first->_is_constraint) {
                    c = (Constraint*)f_pair.first;
                    if (c->is_quadratic()) {
                        res[idx] += lambda[c->_id + inst] * _hess_vals[idx_in++];
                    }
                    else{
                        hess = f_pair.second->get_val(inst);
                        idx_in++;
                        res[idx] += lambda[c->_id + inst] * hess;
                    }
                    
                }
                else {
                    if (_obj.is_quadratic()) {
                        res[idx] += obj_factor * _hess_vals[idx_in++];
                    }
                    else {
                        hess = f_pair.second->eval();
                        _hess_vals[idx_in++] = hess;
                        res[idx] += obj_factor * hess;
                    }
                }
            }
            idx++;
        }
    }
}








/* Compute derivatives and nonzeros in Jacobian and Hessian */
void Model::fill_in_maps() {
//    unsigned vid, vjd, vid_inst, vjd_inst;
    string vi_name, vj_name;
    param_* vi;
    param_* vj;
//    param_* temp;
    

    _obj.compute_derivatives();
    if (!_obj.is_linear()) {
        for (auto &vi_p: _obj.get_vars()) {
            vi = vi_p.second.first.get();
            vi_name = vi->get_name();
//            vid = vi->get_id();
            auto df = _obj.get_stored_derivative(vi->_unique_id);
            for (auto &vj_p: df->get_vars()) {
                vj = vj_p.second.first.get();
                vj_name = vj->get_name();
//                vjd = vj->get_id();
                if (vi_name.compare(vj_name) < 0) {//ONLY STORE LOWER TRIANGULAR PART OF HESSIAN
                    _hess_link[make_pair<>(vi_name,vj_name)].insert(make_pair<>(&_obj,_obj.get_stored_derivative(vi->_unique_id)->get_stored_derivative(vj->_unique_id).get()));
                }
                else {
                    _hess_link[make_pair<>(vj_name,vi_name)].insert(make_pair<>(&_obj,_obj.get_stored_derivative(vj->_unique_id)->get_stored_derivative(vi->_unique_id).get()));
                }
//                for (int inst = 0; inst<vi->get_dim(); inst++) {
//                    vid = vi->get_id();
//                    vid_inst = vid + vi->get_id_inst(inst);
//                    vjd = vj->get_id();
//                    vjd_inst = vjd + vj->get_id_inst(inst);
//                    _hess.insert(make_pair<>(vid_inst, vjd_inst));
//                }
            }
        }
    }
    Constraint* c = NULL;
//    unsigned cid = 0;
    for(auto& c_p :_cons)
    {
        c = c_p.second.get();
        c->compute_derivatives();
        c->_val = make_shared<vector<double>>();
        c->_val->resize(c->get_nb_instances());
        if (!c->is_linear()) {
            for (auto &vi_p: c->get_vars()) {
                vi = vi_p.second.first.get();
                vi_name = vi->get_name();
//                vid = vi->get_id();
                auto df = c->get_stored_derivative(vi->_unique_id);
                if (df->is_nonlinear()) {
                    embed(df);
                }
                for (auto &vj_p: df->get_vars()) {
                    vj = vj_p.second.first.get();
                    vj_name = vj->get_name();
//                    vjd = vj->get_id();
                    if (vi_name.compare(vj_name) < 0) {//ONLY STORE LOWER TRIANGULAR PART OF HESSIAN
                        _hess_link[make_pair<>(vi_name,vj_name)].insert(make_pair<>(c, c->get_stored_derivative(vi->_unique_id)->get_stored_derivative(vj->_unique_id).get()));
                    }
                    else {
                        _hess_link[make_pair<>(vj_name,vi_name)].insert(make_pair<>(c, c->get_stored_derivative(vj->_unique_id)->get_stored_derivative(vi->_unique_id).get()));
                    }
//                    for (int inst = 0; inst<c->get_nb_instances(); inst++) {
//                        vid_inst = vid + vi->get_id_inst(inst);
//                        vjd_inst = vjd + vj->get_id_inst(inst);
//                        _hess.insert(make_pair<>(vid_inst, vjd_inst));
//                    }
                }
//                _v_in_cons
                
            }
        }
    }
    _nnz_h = _hess.size();
    DebugOn("Size of _hess_link = " << _hess_link.size() << endl);
}

//void Model::fill_in_maps() {
//    unsigned vid, vjd, expo, temp, vid_inst, vjd_inst;
//    unique_id vi_unique, vj_unique, temp_unique;
//    param_* vi;
//    param_* vj;
//    
//    _obj.compute_derivatives();
//    if (!_obj.is_linear()) {
//        for (auto &qt_p: _obj.get_qterms()) {
//            vi = qt_p.second._p->first;
//            vid = vi->get_id();
//            vj = qt_p.second._p->second;
//            vjd = vj->get_id();
//            if (vi->_is_vector) {
//                for (int i = 0; i<vi->get_dim(); i++) {
//                    vid_inst = vid + vi->get_id_inst(i);
//                    vi_unique = vi->_unique_id;
//                    vjd_inst = vjd + vj->get_id_inst(i);
//                    vj_unique = vj->_unique_id;
//                    if (vid_inst > vjd_inst) {
//                        temp = vid_inst;
//                        temp_unique = vi_unique;
//                        vid_inst = vjd_inst;
//                        vi_unique = vj_unique;
//                        vjd_inst = temp;
//                        vj_unique = temp_unique;
//                    }
//                    _hess_link[make_pair<>(vi_unique,vid_inst)][make_pair<>(vj_unique,vjd_inst)].insert(make_pair<>(-1,0));//"-1" indicates a hessian link in the objective
//                    _obj.get_hess_link()[vid_inst].insert(vjd_inst);
//                }
//            }
//            else {
//                vid_inst = vid + vi->get_id_inst();
//                vi_unique = vi->_unique_id;
//                vjd_inst = vjd + vj->get_id_inst();
//                vj_unique = vj->_unique_id;
//                if (vid_inst > vjd_inst) {
//                    temp = vid_inst;
//                    temp_unique = vi_unique;
//                    vid_inst = vjd_inst;
//                    vi_unique = vj_unique;
//                    vjd_inst = temp;
//                    vj_unique = temp_unique;
//                }
//                _hess_link[make_pair<>(vi_unique,vid_inst)][make_pair<>(vj_unique,vjd_inst)].insert(make_pair<>(-1,0));//"-1" indicates a hessian link in the objective
//                _obj.get_hess_link()[vid_inst].insert(vjd_inst);
//
//            }
//        }
//        
//        for (auto &pt_p: _obj.get_pterms()) {
//            for (auto v_it = pt_p.second._l->begin(); v_it != pt_p.second._l->end(); v_it++) {
//                vi = v_it->first;
//                vid = vi->get_id();
//                vid_inst = vid + vi->get_id_inst();
//                vi_unique = vi->_unique_id;
//                expo = v_it->second;
//                if (expo>1) {
//                    _hess_link[make_pair<>(vi_unique,vid_inst)][make_pair<>(vi_unique,vid_inst)].insert(make_pair<>(-1,0));//"-1" indicates objective
//                    _obj.get_hess_link()[vid_inst].insert(vid_inst);
//                }
//                for (auto v_jt = next(v_it); v_jt != pt_p.second._l->end(); v_jt++) {
//                    vi = v_it->first;
//                    vid = vi->get_id();
//                    vid_inst = vid + vi->get_id_inst();
//                    vi_unique = vi->_unique_id;
//                    vj = v_jt->first;
//                    vjd = vj->get_id();
//                    vjd_inst = vjd + vj->get_id_inst();
//                    vj_unique = vj->_unique_id;
//                    if (vid_inst > vjd_inst) {
//                        temp = vid_inst;
//                        temp_unique = vi_unique;
//                        vid_inst = vjd_inst;
//                        vi_unique = vj_unique;
//                        vjd_inst = temp;
//                        vj_unique = temp_unique;
//                    }
//                    _hess_link[make_pair<>(vi_unique,vid_inst)][make_pair<>(vj_unique,vjd_inst)].insert(make_pair<>(-1,0));//"-1" indicates objective
//                    _obj.get_hess_link()[vid_inst].insert(vjd_inst);
//                    
//                }
//            }
//        }
//    }
//    Constraint* c = NULL;
//    unsigned cid = 0;
//    for(auto& c_p :_cons)
//    {
//        c = c_p.second;
//        c->compute_derivatives();
//        if (c->is_linear()) {
//            continue;
//        }
//        for (int inst = 0; inst<c->get_nb_instances(); inst++) {
//            cid = c->_id + inst;
//            for (auto &qt_p: c->get_qterms()) {
//                vi = qt_p.second._p->first;
//                vid = vi->get_id();
//                vid_inst = vid + vi->get_id_inst(inst);
//                vi_unique = vi->_unique_id;
//                vj = qt_p.second._p->second;
//                vjd = vj->get_id();
//                vjd_inst = vjd + vj->get_id_inst(inst);
//                vj_unique = vj->_unique_id;
//                if (vi->_is_vector) {
//                    for (int i = 0; i<vi->get_dim(); i++) {
//                        vid_inst = vid + vi->get_id_inst(i);
//                        vi_unique = vi->_unique_id;
//                        vjd_inst = vjd + vj->get_id_inst(i);
//                        vj_unique = vj->_unique_id;
//                        if (vid_inst > vjd_inst) {
//                            temp = vid_inst;
//                            temp_unique = vi_unique;
//                            vid_inst = vjd_inst;
//                            vi_unique = vj_unique;
//                            vjd_inst = temp;
//                            vj_unique = temp_unique;
//                        }
//                        _hess_link[make_pair<>(vi_unique,vid_inst)][make_pair<>(vj_unique,vjd_inst)].insert(make_pair<>(c->_id,inst));
//                        c->get_hess_link()[vid_inst].insert(vjd_inst);                        
//                    }
//                }
//                else {
//                    if (vid_inst > vjd_inst) {
//                        temp = vid_inst;
//                        temp_unique = vi_unique;
//                        vid_inst = vjd_inst;
//                        vi_unique = vj_unique;
//                        vjd_inst = temp;
//                        vj_unique = temp_unique;
//                    }
//                    _hess_link[make_pair<>(vi_unique,vid_inst)][make_pair<>(vj_unique,vjd_inst)].insert(make_pair<>(c->_id,inst));
//                    c->get_hess_link()[vid_inst].insert(vjd_inst);
//                }
//            }
//            
//            for (auto &pt_p: c->get_pterms()) {
//                for (auto v_it = pt_p.second._l->begin(); v_it != pt_p.second._l->end(); v_it++) {
//                    vi = v_it->first;
//                    vid = vi->get_id();
//                    vid_inst = vid + vi->get_id_inst(inst);
//                    vi_unique = vi->_unique_id;
//                    expo = v_it->second;
//                    if (expo>1) {
//                        _hess_link[make_pair<>(vi_unique,vid_inst)][make_pair<>(vi_unique,vid_inst)].insert(make_pair<>(c->_id,inst));
//                        c->get_hess_link()[vid_inst].insert(vid_inst);
//                    }
//                    for (auto v_jt = next(v_it); v_jt != pt_p.second._l->end(); v_jt++) {
//                        vi = v_it->first;
//                        vid = vi->get_id();
//                        vid_inst = vid + vi->get_id_inst(inst);
//                        vi_unique = vi->_unique_id;
//                        vj = v_jt->first;
//                        vjd = vj->get_id();
//                        vjd_inst = vjd + vj->get_id_inst(inst);
//                        vj_unique = vj->_unique_id;
//                        if (vid_inst==vjd_inst) {
//                            continue;
//                        }
//                        if (vid_inst > vjd_inst) {
//                            temp = vid_inst;
//                            temp_unique = vi_unique;
//                            vid_inst = vjd_inst;
//                            vi_unique = vj_unique;
//                            vjd_inst = temp;
//                            vj_unique = temp_unique;
//                        }
//                        _hess_link[make_pair<>(vi_unique,vid_inst)][make_pair<>(vj_unique,vjd_inst)].insert(make_pair<>(c->_id,inst));
//                        c->get_hess_link()[vid_inst].insert(vjd_inst);
//                    }
//                }
//            }
//            if (c->is_nonlinear()) {
//                for (auto &vi_p: c->get_vars()) {
//                    vi = vi_p.second.first;
//                    vid = vi->get_id();
//                    vid_inst = vid + vi->get_id_inst(inst);
//                    vi_unique = vi->_unique_id;
//                    auto df = c->get_stored_derivative(vi_unique);
//                    for (auto &vj_p: df->get_vars()) {
//                        vj = vj_p.second.first;
//                        vjd = vj->get_id();
//                        vjd_inst = vjd + vj->get_id_inst(inst);
//                        vj_unique = vj->_unique_id;
//                        if (vid_inst > vjd_inst) {
//                            temp = vid_inst;
//                            temp_unique = vi_unique;
//                            vid_inst = vjd_inst;
//                            vi_unique = vj_unique;
//                            vjd_inst = temp;
//                            vj_unique = temp_unique;
//                        }
//                        _hess_link[make_pair<>(vi_unique,vid_inst)][make_pair<>(vj_unique,vjd_inst)].insert(make_pair<>(c->_id,inst));
//                        c->get_hess_link()[vid_inst].insert(vjd_inst);
//                    }
//                }
//            }
//        }
//        
//        //MISSING NONLINEAR PART!!!
//        for (auto &v_p: c->get_vars()) {
////            if(!v_p.second.first->_is_indexed){
//                for (int i = 0; i < v_p.second.first->get_dim(); i++) {
////                    _v_in_cons[v_p.second.first->get_id()+i].insert(c);
////                }
////            }
////            else {
////                auto ids = v_p.second.first->get_ids();
////                for (int i = 0; i < ids.size(); i++) {
//                    _v_in_cons[v_p.second.first->get_id() + v_p.second.first->get_id_inst(i)].insert(c);
////                }                
//            }
//        }
//    }
//    _nnz_h = 0;
//    for (auto &hess_i: _hess_link) {
//        _nnz_h += hess_i.second.size();
//    }    
//}


void Model::fill_in_var_init(double* x) {
    size_t vid;
    param_* v;
    for(auto& v_p: _vars)
    {
        v = v_p.second;
        vid = v->get_id();
        switch (v->get_intype()) {
            case float_: {
                auto real_var = (var<float>*)v;
                for (int i = 0; i < v->get_dim(); i++) {
//                    vid_inst = vid + v->get_id_inst(i);
                    x[vid+i] = (double)real_var->eval(i);
                }
                break;
            }
            case long_:{
                auto real_var = (var<long double>*)v;
                for (int i = 0; i < v->get_dim(); i++) {
//                    vid_inst = vid + v->get_id_inst(i);
                    x[vid+i] = (double)real_var->eval(i);
                }
                break;
            }
            case double_:{
                auto real_var = (var<double>*)v;
                for (int i = 0; i < v->get_dim(); i++) {
//                    vid_inst = vid + v->get_id_inst(i);
                    x[vid+i] = real_var->eval(i);
                }
                break;
            }
            case integer_:{
                auto real_var = (var<int>*)v;
                for (int i = 0; i < v->get_dim(); i++) {
//                    vid_inst = vid + v->get_id_inst(i);
                    x[vid+i] = (double)real_var->eval(i);
                }
                break;
            }
            case short_:{
                auto real_var = (var<short>*)v;
                for (int i = 0; i < v->get_dim(); i++) {
//                    vid_inst = vid + v->get_id_inst(i);
                    x[vid+i] = (double)real_var->eval(i);
                }
                break;
            }
            case binary_:{
                auto real_var = (var<bool>*)v;
                for (int i = 0; i < v->get_dim(); i++) {
//                    vid_inst = vid + v->get_id_inst(i);
                    x[vid+i] = (double)real_var->eval(i);
                }
                break;
            }
            default:
                break;
        } ;
    }
}

void Model::fill_in_cstr_bounds(double* g_l ,double* g_u) {
    size_t cid = 0;
    Constraint* c = NULL;
    for(auto& c_p :_cons)
    {
        c = c_p.second.get();
        switch (c->get_type()) {
            case eq:{
                auto nb_ins = c->get_nb_instances();
                for (int i = 0; i< nb_ins; i++){
                    cid = c->_id+i;
                    g_l[cid] = c->_rhs;
                    g_u[cid] = c->_rhs;
                }
                break;
            }
            case leq:{
                auto nb_ins = c->get_nb_instances();
                for (int i = 0; i< nb_ins; i++){
                    cid = c->_id+i;
                    g_l[cid] = numeric_limits<double>::lowest();
                    g_u[cid] = c->_rhs;
                }
                break;
            }
            case geq:{
                auto nb_ins = c->get_nb_instances();
                for (int i = 0; i< nb_ins; i++){
                    cid = c->_id+i;
                    g_l[cid] = c->_rhs;
                    g_u[cid] = numeric_limits<double>::max();
                }
                break;
            }
            default:
                throw invalid_argument("Undefined constraint type!\n");
                exit(-1);
                break;
        }
    }
}

#ifdef USE_BONMIN
void Model::fill_in_param_types(Bonmin::TMINLP::VariableType* param_types){
    size_t vid;
    param_* v;
    for(auto& v_p: _vars)
    {
        v = v_p.second;
        vid = v->get_id();
        if (v->get_intype()== short_ || v->get_intype() == integer_) {
            for (int i = 0; i < v->get_dim(); i++) {
                param_types[vid + i] = Bonmin::TMINLP::INTEGER;
            }
            
        }
        else if (v->get_intype()== binary_) {
            for (int i = 0; i < v->get_dim(); i++) {
                param_types[vid + i] = Bonmin::TMINLP::BINARY;
            }
        }
        else {
            for (int i = 0; i < v->get_dim(); i++) {
                param_types[vid + i] = Bonmin::TMINLP::CONTINUOUS;
            }
        }
    }
}

#endif


void Model::embed(expr& e){
    switch (e.get_type()) {
        case uexp_c:{
            auto ue = (uexpr*)&e;
            auto f = ue->_son;
            auto f_p = _nl_funcs_map.insert(make_pair<>(f->to_str(), f));
            if (f_p.second) {
                embed(f);
                _nl_funcs.push_back(f);
                f->_val = make_shared<vector<double>>();
                f->_val->resize(f->get_nb_instances());
            }
            else {
                ue->_son = f_p.first->second;
            }
            break;
        }
        case bexp_c:{
            auto be = (bexpr*)&e;
            auto f = be->_lson;
            auto f_p = _nl_funcs_map.insert(make_pair<>(f->to_str(), f));
            if (f_p.second) {
                embed(f);
                _nl_funcs.push_back(f);
                f->_val = make_shared<vector<double>>();
                f->_val->resize(f->get_nb_instances());
            }
            else {
                be->_lson = f_p.first->second;
            }
            f = be->_rson;
            f_p = _nl_funcs_map.insert(make_pair<>(f->to_str(), f));
            if (f_p.second) {
                embed(f);
                _nl_funcs.push_back(f);
                f->_val = make_shared<vector<double>>();
                f->_val->resize(f->get_nb_instances());
            }
            else {
                be->_rson = f_p.first->second;
            }
            break;
        }
        default:
            break;
    }
}

void Model::embed(shared_ptr<func_> f){
    if (f->get_expr()) {
        embed(*f->get_expr());
        for (auto &dfp:*f->get_dfdx()) {
            if (dfp.second->is_nonlinear()) {
                embed(dfp.second);
            }
        }
    }
    auto f_p = _nl_funcs_map.insert(make_pair<>(f->to_str(), f));
    if (f_p.second) {
        _nl_funcs.push_back(f_p.first->second);
        f_p.first->second->_val = make_shared<vector<double>>();
        f_p.first->second->_val->resize(f_p.first->second->get_nb_instances());
    }
}



void Model::print_nl_functions() const{
    cout << "Number of atomic functions = " << _nl_funcs.size();
    cout << endl;
    //    for (auto& f: _functions){
    //        f->print(false);
    //        cout << endl;
    //    }
    cout << endl;
}

void Model::print_solution() const{
    
}

void Model::print_constraints() const{
    for(auto& p: _cons){
        p.second->print();
    }
}

pair<func_*, ObjectiveType> gravity::max(const func_& f){
    return make_pair<>((func_*)&f,maximize);
};

pair<func_*, ObjectiveType> gravity::min(const func_& f){
    return make_pair<>((func_*)&f,minimize);
};


void Model::add_on_off(const Constraint& c, var<bool>& on){
    if (c.get_ftype() != lin_) {
        cerr << "Nonlinear constraint.\n";
        exit(-1);
    }
    Constraint res(c.get_name() + "_on/off");
//    double b;
    //    for(auto it: orig_q->_coefs) {
    //        v = getparam_<double>(it.first);
    //        if (!v->is_bounded_below() || !v->is_bounded_above()) {
    //            cerr << "Variable " << v->_name << " in constraint " << c._name << " does not have finite bounds.\n";
    //            exit(1);
    //        }
    //        if (c.get_type() == leq || c.get_type() == eq) {
    //            if (it.second < 0) res -= it.second*v->get_lb_off()*(1-on);
    //            else res -= it.second*v->get_ub_off()*(1-on);
    //        }
    //        else{ // geq
    //            if (it.second < 0) res -= it.second*v->get_ub_off()*(1-on);
    //            else res -= it.second*v->get_lb_off()*(1-on);
    //        }
    //    }
    //    if (c.get_type() == eq) {
    //        Constraint res2(c.get_name() + "_on/off2");
    //        for(auto it: orig_q->_coefs) {
    //            v = getparam_<double>(it.first);
    //            if (it.second < 0) res2 -= it.second*v->get_ub_off()*(1-on);
    //            else res2 -= it.second*v->get_lb_off()*(1-on);
    //        }
    //        res2 += *orig_q;
    //        res2 -= b*on;
    //        res2 >= 0;
    //        addConstraint(res2);
    //    }
    //    res += *orig_q;
    //    res -= orig_q->get_const();
    //    res -= b*on;
    //    if (c.get_type() == eq or c.get_type() == leq) res <= 0;
    //    else res >= 0;
    add_constraint(res);
}

void Model::add_on_off(var<>& v, var<bool>& on){
    //    if(v.get_ub() != v.get_ub_off()) {
    //        Constraint UB(v._name + "_UB_on/off");
    //        UB += v - v.get_ub() * on - (1 - on) * v.get_ub_off();
    //        UB <= 0;
    //        addConstraint(UB);
    //    }
    //    if(v.get_lb() != v.get_lb_off()) {
    //        Constraint LB(v._name + "_LB_on/off");
    //        LB += v - v.get_lb() * on - (1 - on) * v.get_lb_off();
    //        LB >= 0;
    //        addConstraint(LB);
    //    }
}

void Model::add_McCormick(std::string name, var<>& v, var<>& v1, var<>& v2) {
    //    Constraint MC1(name+"_McCormick1");
    //    MC1 += v;
    //    MC1 -= v1.get_lb()*v2 + v2.get_lb()*v1 - v1.get_lb()*v2.get_lb();
    //    MC1 >= 0;
    //    add_constraint(MC1);
    //    //    MC1.print();
    //    Constraint MC2(name+"_McCormick2");
    //    MC2 += v;
    //    MC2 -= v1.get_ub()*v2 + v2.get_ub()*v1 - v1.get_ub()*v2.get_ub();
    //    MC2 >= 0;
    //    add_constraint(MC2);
    //    //    MC2.print();
    //    Constraint MC3(name+"_McCormick3");
    //    MC3 += v;
    //    MC3 -= v1.get_lb()*v2 + v2.get_ub()*v1 - v1.get_lb()*v2.get_ub();
    //    MC3 <= 0;
    //    add_constraint(MC3);
    //    //    MC3.print();
    //    Constraint MC4(name+"_McCormick4");
    //    MC4 += v;
    //    MC4 -= v1.get_ub()*v2 + v2.get_lb()*v1 - v1.get_ub()*v2.get_lb();
    //    MC4 <= 0;
    //    add_constraint(MC4);
    //    MC4.print();
}


void Model::add_on_off_McCormick(std::string name, var<>& v, var<>& v1, var<>& v2, var<bool>& on) {
    //    Constraint MC1(name+"_McCormick1");
    //    MC1 += v;
    //    MC1 -= v1.get_lb()*v2 + v2.get_lb()*v1 - v1.get_lb()*v2.get_lb();
    //    MC1 >= 0;
    //    add_on_off(MC1, on);
    //    Constraint MC2(name+"_McCormick2");
    //    MC2 += v;
    //    MC2 -= v1.get_ub()*v2 + v2.get_ub()*v1 - v1.get_ub()*v2.get_ub();
    //    MC2 >= 0;
    //    add_on_off(MC2, on);
    //    Constraint MC3(name+"_McCormick3");
    //    MC3 += v;
    //    MC3 -= v1.get_lb()*v2 + v2.get_ub()*v1 - v1.get_lb()*v2.get_ub();
    //    MC3 <= 0;
    //    add_on_off(MC3, on);
    //    Constraint MC4(name+"_McCormick4");
    //    MC4 += v;
    //    MC4 -= v1.get_ub()*v2 + v2.get_lb()*v1 - v1.get_ub()*v2.get_lb();
    //    MC4 <= 0;
    //    add_on_off(MC4, on);
}
