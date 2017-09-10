//
//  Node.h
//
#ifndef Node_h
#define Node_h
#include <vector>
#include <string>

/** A Node has:
    a name
    an ID.
    a set of branches.
    Name is the unique lable of a node. 
    ID is determined if it is within a container of nodes.
 */

class Arc;

class Node{
    
public:
    std::string _name;
    int ID;
    
    std::vector<Arc*> branches;
    /* the number of edges needed to make the subgraph formed by adjacent nodes a clique */
    int fill_in;

    // constructions
    Node();
    Node(std::string name, int id= -1);
    ~Node();
    Node* clone();
    
      /*
     @brief Adds a to the list of incident arcs
     */
    void addArc(Arc* a);

     /*
     @brief Find and remove incident arc from list of branches
     @return 0 if a was found and removed, -1 otherwise
     */
    int removeArc(Arc* a);

    void update_fill_in(Node* n);
    
    /*
     @brief Returns true if n is an adjacent node.
     */
    bool is_connected(Node* n);
};

#endif
