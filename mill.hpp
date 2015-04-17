/*!\defgroup MILL*/
/******************************************************************************/
/*!
 \file       mill.hpp
 \brief

 \version
 04.08.2013 - Erik Schuster - erik@muenchen-ist-toll.de\n
 - Started documenting the code for doxygen processing.
 - Formatted the code with the Eclipse code styler (Style: K&R).

 \version
 1.1.4 - 2009, 2010, Patrick Birnzain <pbirnzain@users.sourceforge.net> and others

 \copyright
 pcb2gcode is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 pcb2gcode is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.

 \ingroup    MILL
 */
/******************************************************************************/

#ifndef MILL_H
#define MILL_H

#include <stdint.h>

/******************************************************************************/
/*
 */
/******************************************************************************/
class Mill {
public:
	virtual ~Mill() {
	}
	;

	double feed;
	int speed;
	double zchange;
	double zsafe;
	double zwork;

	//double  g64;                //!< maximum deviation from commanded toolpath
	//bool    metricoutput;       //!< if true, g-code output in metric units
	//bool    metricinput;        //!< if true, input parameters are in metric units
};

/******************************************************************************/
/*
 */
/******************************************************************************/
class RoutingMill: public Mill {
public:
	double tool_diameter;
    bool optimise;
};

/******************************************************************************/
/*
 */
/******************************************************************************/
class Isolator: public RoutingMill {
public:
	int extra_passes;
};

/******************************************************************************/
/*
 */
/******************************************************************************/
class Cutter: public RoutingMill {
public:
	bool do_steps;
	double stepsize;
	unsigned int bridges_num;
	double bridges_height;
	double bridges_width;
};

/******************************************************************************/
/*
 */
/******************************************************************************/
class Driller: public Mill {
};

#endif // MILL_H
