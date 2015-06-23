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
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/register/point.hpp>
#include <boost/geometry/geometries/register/ring.hpp>

typedef std::pair<int, int> coordpair;
typedef std::vector<coordpair> coords;

//#include "Fixed.hpp"
//typedef numeric::Fixed<16,16> ivalue_t;
typedef double ivalue_t;

typedef std::pair<ivalue_t, ivalue_t> icoordpair;
typedef std::vector<icoordpair> icoords;

//Adaptation of icoordpair to Boost Geometry (point)
BOOST_GEOMETRY_REGISTER_POINT_2D(icoordpair, ivalue_t, cs::cartesian, first, second)

// Adaptation of icoords to Boost Geometry (ring)
BOOST_GEOMETRY_REGISTER_RING(icoords)

#endif // COORD_H
