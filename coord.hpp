/*!\defgroup COORD*/
/******************************************************************************/
/*!
 \file       coord.hpp
 \brief

 \version
 04.08.2013 - Erik Schuster - erik@muenchen-ist-toll.de\n
 - Formatted the code with the Eclipse code styler (Style: K&R).
 - Prepared commenting the code

 \ingroup    COORD
 */
/******************************************************************************/

#ifndef COORD_H
#define COORD_H

#include <vector>

typedef std::pair<int, int> coordpair;
typedef std::vector<coordpair> coords;

//#include "Fixed.hpp"
//typedef numeric::Fixed<16,16> ivalue_t;
typedef double ivalue_t;

typedef std::pair<ivalue_t, ivalue_t> icoordpair;
typedef std::vector<icoordpair> icoords;

#endif // COORD_H
