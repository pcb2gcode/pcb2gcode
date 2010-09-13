#ifndef COORD_H
#define COORD_H

#include <vector>

typedef std::pair<int,int>     coordpair;
typedef std::vector<coordpair> coords;


#include "Fixed.hpp"
//typedef numeric::Fixed<16,16> ivalue_t;
typedef double ivalue_t;

typedef std::pair<ivalue_t, ivalue_t> icoordpair;
typedef std::vector<icoordpair>   icoords;

#endif // COORD_H
