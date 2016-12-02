//
//  var.cpp
//  Gravity
//
//  Created by Hijazi, Hassan on 21/10/16.
//
//

#include <Gravity/var.h>



template<typename type> var<type>::var():param<type>(){
    param<type>::set_type(var_c);
};

template<typename type> var<type>::var(const char* name):param<type>(name){
    param<type>::set_type(var_c);
    _lb = make_shared<vector<type>>();
    _ub = make_shared<vector<type>>();
};

template<typename type> var<type>::var(const var<type>& v):param<type>(v){
    param<type>::set_type(var_c);
    _lb = v._lb;
    _ub = v._ub;
};

template<typename type> var<type>::var(const char* name, type lb, type ub):var(name){
    _lb->push_back(lb);
    _ub->push_back(ub);
};


/* Modifiers */
template<typename type> void   var<type>::set_size(int s){
    param<type>::set_size(s);
    _lb->reserve(s);
    _ub->reserve(s);
};


template<typename type> void   var<type>::add_bounds(type lb, type ub){
    if (ub<lb) {
        throw invalid_argument("add_bounds(lb, ub): Invalid bounds!");
    }
    _lb->push_back(lb);
    _ub->push_back(ub);
}

template<typename type> void   var<type>::add_lb_only(type val){
    _lb->push_back(val);
    _ub->push_back(std::numeric_limits<type>::max());
}

template<typename type> void   var<type>::add_ub_only(type val){
    _lb->push_back(std::numeric_limits<type>::min());
    _ub->push_back(val);
}

template<typename type> void   var<type>::set_lb(int i, type val){
    if (_lb->size() <= i) {
        throw out_of_range("set_lb(int i, type val)");
    }
    _lb->at(i) = val;
}

template<typename type> void   var<type>::set_ub(int i, type val){
    if (_ub->size() <= i) {
        throw out_of_range("set_lb(int i, type val)");
    }
    _ub->at(i) = val;
}


/* Operators */
template<typename type> bool var<type>::operator==(const var& v) const{
    return (param<type>::operator==(v) && *_lb==*v._lb && *_ub==*v._ub);
};

template<typename type> bool var<type>::operator!=(const var& v) const{
    return !(*this==v);
}

/* Output */
template<typename type> void var<type>::print(bool bounds) const{
    param<type>::print(false);
    if (!bounds) {
        return;
    }
    cout << " : " << endl;
    if(_lb==nullptr && _ub!=nullptr){
        for(int i = 0 ; i < _lb->size(); i++){
            cout << "(" << i << ") = ";
            cout << " [ -inf , " << (*_ub)[i] << " ]\n";
        }
        cout << ";\n";
    }
    if(_lb!=nullptr && _ub==nullptr){
        for(int i = 0 ; i < _lb->size(); i++){
            cout << "(" << i << ") = ";
            cout << " [ " << (*_lb)[i] << ", inf ]\n";
        }
        cout << ";\n";
    }
    if(_lb!=nullptr && _ub!=nullptr){
        for(int i = 0 ; i < _lb->size(); i++){
            cout << "(" << i << ") = ";
            cout << " [ " << (*_lb)[i] << ", " << (*_ub)[i] << "]\n";
        }
        cout << ";\n";
    }    
};


template class var<bool>;
template class var<short>;
template class var<int>;
template class var<float>;
template class var<double>;
template class var<long double>;
