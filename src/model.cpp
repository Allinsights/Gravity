//
//  model.cpp
//  Gravity
//
//  Created by Hijazi, Hassan (Data61, Canberra City) on 6/5/17.
//
//

#include <Gravity/model.h>
//#define USEDEBUG
#ifdef USEDEBUG
#define Debug(x) cout << x
#else
#define Debug(x)
#endif
#define DebugOn(x) cout << x
#define DebugOff(x)


using namespace std;

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
    for (auto &cp:_cons) {
        delete cp.second;
    }
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



size_t Model::get_nb_nnz_h() const{
    size_t idx=0;
    /* return the structure of the hessian */
    for(auto& v: _vars)
    {
//        vid = v->get_idx();
//        //        v->print();
//        //        cout << "hessian link :\n";
//        for(auto itv = v->_hess.cbegin(); itv != v->_hess.cend(); ++itv)
//        {
//            vjd = *itv;
//            if (vjd <= vid) { // This is a symmetric matrix, fill the lower left triangle only.
//                //                vj = this->_vars.find(vjd)->second;
//                //                vj->print();
//                //                cout << " , ";
//                idx++;
//            }
//        }
        //        cout << "\n";
    }
    //        cout << "Hessian nnz = " << idx << endl;
    //    exit(-1);
    return idx;
};

//

Constraint* Model::get_constraint(const string& cname) const{
    return (Constraint*)&_cons.at(cname);
}

param_* Model::get_var(const string& vname) const{
        return _vars.at(vname);
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
    if (_vars.count(v->get_name())==0) {
        _nb_vars += v->get_dim();
        v->set_id(_vars.size());
        _vars[v->get_name()] = v;
    }
};

void Model::add_var(param_& v){
    if (_vars.count(v.get_name())==0) {
        _nb_vars += v.get_dim();
        auto newv = (param_*)copy(v);
        v.set_id(_vars.size());
        newv->set_id(_vars.size());
        _vars[v.get_name()] = newv;
    }
};


void Model::del_var(const param_& v){
    auto it = _vars.find(v.get_name());
    if (it!=_vars.end()) {
        _nb_vars -= v.get_dim();
        delete it->second;
        _vars.erase(it);
    }
};


void Model::add_param(param_* v){
    if (_params.count(v->get_name())==0) {
        _nb_params += v->get_dim();
        v->set_id(_params.size());
        _params[v->get_name()] = v;
    }
};

void Model::add_param(param_& v){
    if (_params.count(v.get_name())==0) {
        _nb_params += v.get_dim();
        auto newv = (param_*)copy(v);
        v.set_id(_params.size());
        newv->set_id(_params.size());
        _params[v.get_name()] = newv;
    }
};


void Model::del_param(const param_& v){
    auto it = _params.find(v.get_name());
    if (it!=_params.end()) {
        _nb_params -= v.get_dim();
        delete it->second;
        _params.erase(it);
    }
};

void Model::add_constraint(const Constraint& c){
    _v_in_cons.resize(_nb_vars);
    _nb_cons += c.get_nb_instances();
    if (_cons.count(c.get_name())==0) {
        auto newc = new Constraint(c);
//        embed(*newc);
        if (newc->is_nonlinear()) {
            newc->compute_derivatives();
        }
        _cons[c.get_name()] = newc;
    }
    else {
        throw invalid_argument("rename constraint as this name has been used by another one: " + c.to_str());
    }
};




void Model::add_on_off(const Constraint& c, var<bool>& on){
    if (c.get_ftype() != lin_) {
        cerr << "Nonlinear constraint.\n";
        exit(-1);
    }
    Constraint res(c.get_name() + "_on/off");
    double b;
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
    int vid = 0;
    //    param_* v = NULL;
    var<>* var = NULL;
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
    size_t idx = 0;
    param_* v;
    for(auto& v_p: _vars)
    {
        v = v_p.second;
        switch (v->get_intype()) {
            case float_: {
                auto real_var = (var<float>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    x_l[idx] = (double)real_var->get_lb(i);
                    x_u[idx] = (double)real_var->get_ub(i);
                    idx++;
                }
                break;
            }
            case long_:{
                auto real_var = (var<long double>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    x_l[idx] = (double)real_var->get_lb(i);
                    x_u[idx] = (double)real_var->get_ub(i);
                    idx++;
                }
                break;
            }
            case double_:{
                auto real_var = (var<double>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    x_l[idx] = (double)real_var->get_lb(i);
                    x_u[idx] = (double)real_var->get_ub(i);
                    idx++;
                }
                break;
            }
            case integer_:{
                auto real_var = (var<int>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    x_l[idx] = (double)real_var->get_lb(i);
                    x_u[idx] = (double)real_var->get_ub(i);
                    idx++;
                }
                break;
            }
            case short_:{
                auto real_var = (var<short>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    x_l[idx] = (double)real_var->get_lb(i);
                    x_u[idx] = (double)real_var->get_ub(i);
                    idx++;
                }
                break;
            }
            case binary_:{
                auto real_var = (var<bool>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    x_l[idx] = (double)real_var->get_lb(i);
                    x_u[idx] = (double)real_var->get_ub(i);
                    idx++;
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
    size_t idx = 0;
    param_* v;
    for(auto& v_p: _vars)
    {
        v = v_p.second;
        switch (v->get_intype()) {
            case float_: {
                auto real_var = (var<float>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    real_var->set_val(i, x[idx]);
                    idx++;
                }
                break;
            }
            case long_:{
                auto real_var = (var<long double>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    real_var->set_val(i, x[idx]);
                    idx++;
                }
                break;
            }
            case double_:{
                auto real_var = (var<double>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    real_var->set_val(i, x[idx]);
                    idx++;
                }
                break;
            }
            case integer_:{
                auto real_var = (var<int>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    real_var->set_val(i, x[idx]);
                    idx++;
                }
                break;
            }
            case short_:{
                auto real_var = (var<short>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    real_var->set_val(i, x[idx]);
                    idx++;
                }
                break;
            }
            case binary_:{
                auto real_var = (var<bool>*)v;
                for (int i = 0; i < real_var->get_dim(); i++) {
                    real_var->set_val(i, x[idx]);
                    idx++;
                }
                break;
            }
            default:
                break;
        } ;
    }
}

void Model::fill_in_obj(const double* x , double& res, bool new_x){
    if (new_x) {
        set_x(x);
    }
    res = _obj.eval();
}

void Model::fill_in_cstr(const double* x , double* res, bool new_x){
    size_t idx=0;
    if (new_x) {
        set_x(x);
    }
    Constraint* c = nullptr;
    for(auto& c_p: _cons)
    {
        c = c_p.second;
        auto nb_ins = c->get_nb_instances();
        for (int i = 0; i< nb_ins; i++){
            res[idx] = c->eval(i);
            idx++;
        }
    }
}

void Model::fill_in_jac(const double* x , double* res, bool new_x){
    if (new_x) {
        set_x(x);
    }
    size_t idx=0, inst = 0;
    size_t cid = 0;
    size_t vid = 0;
    Constraint* c = NULL;
    param_* v = NULL;
    /* return the structure of the jacobian */
    for(auto& c_p :_cons)
    {
        c = c_p.second;
        auto nb_ins = c->get_nb_instances();
        inst = 0;
        for (int i = 0; i< nb_ins; i++){
            for (auto &v_p: c->get_vars()){
                v = v_p.second.first;
                vid = v->get_id();
                if (v->_is_transposed) {
                    for (int j = 0; j<v->get_dim(); j++) {
                        res[idx] = (c->get_stored_derivative(*v))->eval(inst);
                        idx++;
                    }

                }
                else {
                    res[idx] = (c->get_stored_derivative(*v))->eval(inst);
                    idx++;
                }
            }
            cid++;
            inst++;
        }
    }
}


void Model::fill_in_jac_nnz(int* iRow , int* jCol){
    size_t idx=0, inst = 0;
    size_t cid = 0;
    size_t vid = 0;
    Constraint* c = NULL;
    param_* v = NULL;
    /* return the structure of the jacobian */
    for(auto& c_p :_cons)
    {
        c = c_p.second;
        auto nb_ins = c->get_nb_instances();
        inst = 0;
        for (int i = 0; i< nb_ins; i++){
            for (auto &v_p: c->get_vars()){
                v = v_p.second.first;
                vid = v->get_id();
                if (v->_is_transposed) {
                    for (int j = 0; j<v->get_dim(); j++) {
                        iRow[idx] = cid;
                        jCol[idx] = vid + j;
                        idx++;
                    }
                }
                else {
                    if (v->_is_indexed) {
                        iRow[idx] = cid;
                        jCol[idx] = vid + v->get_id_inst();
                        idx++;
                    }                    
                    else {
                        iRow[idx] = cid;
                        jCol[idx] = vid + inst;
                        idx++;
                    }
                }
            }
            cid++;
            inst++;
        }
    }
}

void Model::fill_in_hess_nnz(int* iRow , int* jCol){
    int idx=0;
    int vid = 0, vjd = 0;
    //    param_* v = NULL;
    /* return the structure of the hessian */
    for(auto& v: _vars)
    {
//        vid = v->get_idx();
//        for(auto itv = v->_hess.cbegin(); itv != v->_hess.cend(); ++itv)
//        {
//            vjd = *itv;
//            if (vjd <= vid) { // This is a symmetric matrix, fill the lower left triangle only.
//                iRow[idx] = vid;
//                jCol[idx] = vjd;
//                v->_hess_id.insert(pair<int, int>(vjd, idx));
//                getVar(vjd)->_hess_id.insert(pair<int, int>(vid, idx));
//                //                _hess_index.insert(pair<string, int>(to_string(vid)+','+to_string(vjd), idx));
//                idx++;
//            }
//        }
    }
}

#ifdef USE_IPOPT
void Model::fill_in_var_linearity(Ipopt::TNLP::LinearityType* param_types){
    size_t vid = 0;
    bool linear = true;
    for(auto& vi: _vars)
    {
        vid = vi.second->get_id();
        linear = true;
        for(auto &c: _v_in_cons[vid])
        {
            if (!c->is_linear()) {
                linear=false;
            }
        }
        if (linear) param_types[vid]=Ipopt::TNLP::LINEAR;
        else param_types[vid] = Ipopt::TNLP::NON_LINEAR;
    }
}


void Model::fill_in_cstr_linearity(Ipopt::TNLP::LinearityType* const_types){
    Constraint* c = nullptr;
    bool lin = false;
    size_t cid = 0;
    for(auto& c_p :_cons)
    {
        c = c_p.second;
        if (c->is_linear() || c->is_constant()) {
            lin = true;
        }
        else {
            lin = false;
        }
        auto nb_ins = c->get_nb_instances();
        for (int i = 0; i< nb_ins; i++){
            if (lin) {
                const_types[cid]=Ipopt::TNLP::LINEAR;
            }
            else {
                const_types[cid] = Ipopt::TNLP::NON_LINEAR;
            }
            cid++;
        }
    }
}
#endif


void Model::fill_in_hess(const double* x , double obj_factor, const double* lambda, double* res, bool new_x){
    int vid = 0, vjd = 0, cid = 0, idx = 0;
    param_* vi = NULL;
    func_* obj_dfdx = NULL;
    func_* dfdx = NULL;
    double hess = 0;
    for(auto& v: _vars)
    {
//        vid = v->get_idx();
//        
//        for(auto itv = v->_hess.cbegin(); itv != v->_hess.cend(); ++itv)
//        {
//            vjd = *itv;
//            if (vjd <= vid) { // This is a symmetric matrix, fill the lower left triangle only.
//                res[idx] = 0;
//                idx++;
//            }
//        }
    }
    
//    for (auto &it1:_obj->_vars){
//        vid = it1.first;
//        vi = it1.second;
//        if (_obj->get_type()==nlin_) {
//            //            obj_dfdx = _obj->get_dfdx(vi);
//            //            if (obj_dfdx && !_obj->has_dfdx(vid)) {
//            //                _obj->set_dfdx(vid, obj_dfdx);
//            //            }
//            //        }
//            _obj->compute_dfdx(vi);
//        }
//        for (auto &it2:_obj->_vars){
//            vjd = it2.first;
//            if (vjd<=vid) {
//                if(_obj->_hess.count(vid)==1 && _obj->_hess.count(vjd)==1) {
//                    if(_obj->get_type()==nlin_ && obj_dfdx){
//                        if(obj_dfdx->has_var(vjd))
//                            hess = obj_dfdx->eval_dfdx(vjd, x);
//                        else
//                            hess = 0;
//                    }
//                    else{
//                        hess = _obj->get_q_coeff(vid, vjd);
//                        if (vid==vjd)
//                            hess *= 2;
//                    }
//                    if(hess != 0)
//                        res[get_hess_index(vi, vjd)] = obj_factor * hess;
//                }
//                else
//                    res[get_hess_index(vi, vjd)] = 0;
//                //                else
//                //                    res[get_hess_index(vid, vjd)] = 0;
//            }
//        }
//    }

    for (auto& c:_cons) {
        
//        for (auto it1:c->_vars){
//            vid = it1.first;
//            if(c->_hess.count(vid)==0)
//                continue;
//            vi = it1.second;
//            for (auto& it2:*c->_hess[vid]){
//                vjd = it2;
//                hess = c->get_q_coeff(vid, vjd);
//                
//                if (vid==vjd) {
//                        hess *= 2;
//                }
//                if(hess != 0)
//                    res[get_hess_index(vi, vjd)] += lambda[cid] * hess;
//            }
//        }
    }
}






void Model::fill_in_grad_obj(const double* x , double* res, bool new_x){
    if (new_x) {
        set_x(x);
    }
    int idx=0;
    for(auto& vi_p: _vars)
    {
        res[idx] = 0;
        if (_obj.get_vars().count(vi_p.first)!=0) {
            res[idx] = _obj.get_stored_derivative(*vi_p.second)->eval();
        }
        idx++;
    }
}

void Model::fill_in_maps() {
    Constraint* c = NULL;
    for(auto& c_p :_cons)
    {
        c = c_p.second;
        for (auto &v_p: c->get_vars()) {
            _v_in_cons[v_p.second.first->get_id()+v_p.second.first->get_id_inst()].insert(c);
        }
        for (auto &vec: c->get_hessian_link()) {
            for (auto &set: vec.second) {
//                _hess_link[vec.first].insert(set);
            }
        }
    }
}


void Model::fill_in_var_init(double* x) {
    set_x(x);
}

void Model::fill_in_cstr_bounds(double* g_l ,double* g_u) {
    size_t cid = 0;
    Constraint* c = NULL;
    for(auto& c_p :_cons)
    {
        c = c_p.second;
        switch (c->get_type()) {
            case eq:{
                auto nb_ins = c->get_nb_instances();
                for (int i = 0; i< nb_ins; i++){
                    g_l[cid] = c->_rhs;
                    g_u[cid] = c->_rhs;
                    cid++;
                }
                break;
            }
            case leq:{
                auto nb_ins = c->get_nb_instances();
                for (int i = 0; i< nb_ins; i++){
                    g_l[cid] = numeric_limits<double>::lowest();
                    g_u[cid] = c->_rhs;
                    cid++;
                }
                break;
            }
            case geq:{
                auto nb_ins = c->get_nb_instances();
                for (int i = 0; i< nb_ins; i++){
                    g_l[cid] = c->_rhs;
                    g_u[cid] = numeric_limits<double>::max();
                    cid++;
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
    size_t idx = 0;
    param_* v;
    for(auto& v_p: _vars)
    {
        v = v_p.second;
        if (v->get_intype()== short_ || v->get_intype() == integer_) {
            for (int i = 0; i < real_var->get_dim(); i++) {
                param_types[idx] = Bonmin::TMINLP::INTEGER;
                idx++;
            }
            
        }
        else if (v->get_intype()== binary_) {
            for (int i = 0; i < real_var->get_dim(); i++) {
                param_types[idx] = Bonmin::TMINLP::BINARY;
                idx++;
            }
        }
        else {
            for (int i = 0; i < real_var->get_dim(); i++) {
                param_types[idx] = Bonmin::TMINLP::CONTINUOUS;
                idx++;
            }
        }
    }
}

#endif


void Model::embed(expr& e){
    switch (e.get_type()) {
        case uexp_c:{
            auto ue = (uexpr*)&e;
            if (ue->_son->is_function()) {
                auto f = (func_*)ue->_son;
                embed(*f);
            }
            else if(ue->_son->is_expr()){
                embed(*(expr*)ue->_son);
            }
            else if (ue->_son->is_var()){
                if (_vars.count(((param_*)ue->_son)->get_name())==0) {
                    add_var((param_*)copy(*ue->_son));
                }
            }
            break;
        }
        case bexp_c:{
            auto be = (bexpr*)&e;
            if (be->_lson->is_function()) {
                auto f = (func_*)be->_lson;
                embed(*f);
            }
            else if(be->_lson->is_expr()){
                embed(*(expr*)be->_lson);
            }
            else if (be->_lson->is_var()){
                if (_vars.count(((param_*)be->_lson)->get_name())==0) {
                    add_var((param_*)copy(*be->_lson));
                }
            }
            if (be->_rson->is_function()) {
                auto f = (func_*)be->_rson;
                embed(*f);
            }
            else if(be->_rson->is_expr()){
                embed(*(expr*)be->_rson);
            }
            else if (be->_rson->is_var()){
                if (_vars.count(((param_*)be->_rson)->get_name())==0) {
                    add_var((param_*)copy(*be->_rson));
                }
            }
            break;
        }
        default:
            break;
    }
}

void Model::embed(func_& f){
    f._embedded = true;
    param_* p = nullptr;
    param_* p1 = nullptr;
    param_* p2 = nullptr;
    for (auto &pair:f.get_lterms()) {
        p = pair.second._p;
        if (p->is_var()) {
            auto it = _vars.find(p->get_name());
            if (it==_vars.end()) {
                add_var(p);
            }
            else{
                p = it->second;
                pair.second._p = p;                
            }
        }
        else {
            auto it = _params.find(p->get_name());
            if (it==_params.end()) {
                add_param(p);
            }
            else{
                p = it->second;
                pair.second._p = p;
            }
        }
    }
    for (auto &pair:f.get_qterms()) {
        p1 = pair.second._p->first;
        p2 = pair.second._p->second;
        if (p1->is_var()) {
            auto it1 = _vars.find(p1->get_name());
            if (it1==_vars.end()) {
                add_var(p1);
            }
            else{
                p1 = it1->second;
                pair.second._p->first = p1;
            }
            auto it2 = _vars.find(p2->get_name());
            if (it2==_vars.end()) {
                add_var(p2);
            }
            else{
                p2 = it2->second;
                pair.second._p->second = p2;
            }
        }
        else {
            auto it1 = _params.find(p1->get_name());
            if (it1==_params.end()) {
                add_param(p1);
            }
            else{
                p1 = it1->second;
                pair.second._p->first = p1;
            }
            auto it2 = _params.find(p2->get_name());
            if (it2==_params.end()) {
                add_param(p2);
            }
            else{
                p2 = it2->second;
                pair.second._p->second = p2;
            }
        }
    }
    for (auto &pair:f.get_pterms()) {
        auto list = pair.second._l;
        for (auto &ppi: *list) {
            p = ppi.first;
            if (p->is_var()) {
                auto it = _vars.find(p->get_name());
                if (it==_vars.end()) {
                    add_var(p);
                }
                else{
                    p = it->second;
                    ppi.first = p;
                }
            }
            else {
                auto it = _params.find(p->get_name());
                if (it==_params.end()) {
                    add_param(p);
                }
                else{
                    p = it->second;
                    ppi.first = p;                    
                }
            }
        }
    }
    if (f.is_nonlinear()) {
        embed(f.get_expr());
    }
    auto old_vars = f.get_vars();
    for (auto &vp: old_vars) {
        auto vv = _vars[vp.first];
        if (vv != vp.second.first) {
            delete vp.second.first;
            f.delete_var(vp.first);
            f.add_var(vv);
        }
    }
    auto old_params = f.get_params();
    for (auto &pp: old_params) {
        auto p = _params[pp.first];
        if (p != pp.second.first) {
            delete pp.second.first;
            f.delete_param(pp.first);
            f.add_param(p);
        }
    }
    
    if (f.is_nonlinear()) {
        f.compute_derivatives();
    }
//    f.embed_derivatives();
    
}



void Model::print_functions() const{
    cout << "Number of atomic functions = " << _functions.size();
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

pair<func_*, ObjectiveType> max(const func_& f){
    return make_pair<>((func_*)&f,maximize);
};

pair<func_*, ObjectiveType> min(const func_& f){
    return make_pair<>((func_*)&f,minimize);
};
