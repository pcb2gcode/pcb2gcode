/*!\defgroup EXPORTER*/
/******************************************************************************/
/*!
 \file       exporter.hpp
 \brief

 \version
 04.08.2013 - Erik Schuster - erik@muenchen-ist-toll.de\n
 - Formatted the code with the Eclipse code styler (Style: K&R).
 - Prepared commenting the code

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

 \ingroup    EXPORTER
 */
/******************************************************************************/

#ifndef EXPORTER_H
#define EXPORTER_H

#include <vector>
using std::vector;

#include <string>
using std::string;
using std::pair;

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <boost/program_options.hpp>

#include "coord.hpp"
#include "board.hpp"

/******************************************************************************/
/*
 */
/******************************************************************************/
class Exporter: public boost::noncopyable {
public:
	Exporter(shared_ptr<Board> board) {
	}
	;

	virtual void export_all(boost::program_options::variables_map&) = 0;
};

#endif // EXPORTER_H
