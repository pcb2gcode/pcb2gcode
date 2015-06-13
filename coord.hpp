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

typedef std::pair<int, int> coordpair;
typedef std::vector<coordpair> coords;

//#include "Fixed.hpp"
//typedef numeric::Fixed<16,16> ivalue_t;
typedef double ivalue_t;

typedef std::pair<ivalue_t, ivalue_t> icoordpair;
typedef std::vector<icoordpair> icoords;

//Adaptation of icoordpair to Boost Geometry (point)
namespace boost {
    namespace geometry {
        namespace traits {

            template<> struct tag<icoordpair>
            { typedef point_tag type; };

            template<> struct coordinate_type<icoordpair>
            { typedef ivalue_t type; };

            template<> struct coordinate_system<icoordpair>
            { typedef cs::cartesian type; };

            template<> struct dimension<icoordpair> : boost::mpl::int_<2> {};

            template<> struct access<icoordpair, 0> {
                static ivalue_t get(icoordpair const& p)
                { return p.first; }

                static void set(icoordpair& p, ivalue_t const& value)
                { p.first = value; }
            };

            template<> struct access<icoordpair, 1> {
                static ivalue_t get(icoordpair const& p)
                { return p.second; }

                static void set(icoordpair& p, ivalue_t const& value)
                { p.second = value; }
            };
        }
    }
}

// Adaptation of icoords to Boost Geometry (ring)
namespace boost {
    namespace geometry {
        namespace traits {
            template <> struct tag< icoords > { typedef ring_tag type; };
        }
    }
}

#endif // COORD_H
