/*!\defgroup DRILL*/
/******************************************************************************/
/*!
 \file   drill.cpp
 \brief  Generate the g-code file for drilling.

 \version
  09.01.2015 - Nicola Corna - nicola@corna.info\n
 - Added zero-start option

 1.1.4dev 2013 - Erik Schuster - erik@muenchen-ist-toll.de\n
 - Bugfix for postamble: The postamble is now inserted in the output file.
 - Preamble: The optional external file does not substitute the internal preamble anymore.
 - Changed x-coordinate calculation.
 - Added onedrill option.
 - Added metricoutput option.
 - Prepared documenting the code with doxygen.
 - Formatted the code.\n

 \version
 1.1.4 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net> and others

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

 \bug The milldrill option does not produce usable output.

 \bug Milldrill + mirror_absolute returns error #134.

 \todo Sort the drill coordinates if the onedrill option is given to reduce the
 time for the drilling process.

 \ingroup    DRILL
 */
/******************************************************************************/

#include <iostream>
using std::cout;
using std::endl;

#include <fstream>
#include <iomanip>
using namespace std;

#include "drill.hpp"

#include <cstring>
#include <boost/scoped_array.hpp>

#include <boost/foreach.hpp>

using std::pair;

/******************************************************************************/
/*
 \brief  Constructor
 \param  metricoutput : if true, ngc output in metric units
 */
/******************************************************************************/
ExcellonProcessor::ExcellonProcessor(string drillfile,
                                     const ivalue_t board_width,
                                     const ivalue_t board_center,
                                     bool _metricoutput,
                                     double xoffset,
                                     double yoffset)
         : board_center(board_center), board_width(board_width),
           bMetricOutput(_metricoutput), xoffset(xoffset),
           yoffset(yoffset) {

   bDoSVG = false;      //clear flag for SVG export
   project = gerbv_create_project();

   const char* cfilename = drillfile.c_str();
   boost::scoped_array<char> filename(new char[strlen(cfilename) + 1]);
   strcpy(filename.get(), cfilename);

   gerbv_open_layer_from_filename(project, filename.get());

   if (project->file[0] == NULL) {
      throw drill_exception();
   }

   //set imperial/metric conversion factor for output coordinates depending on metricoutput option
   cfactor = bMetricOutput ? 25.4 : 1;

   calc_dimensions();      //calculate board dimensions

   //set metric or imperial preambles
   if (bMetricOutput) {
      preamble = string("G94       (Millimeters per minute feed rate.)\n")
                 + "G21       (Units == Millimeters.)\n";
   } else {
      preamble = string("G94       (Inches per minute feed rate.)\n")
                 + "G20       (Units == INCHES.)\n";
   }
   preamble += "G90       (Absolute coordinates.)\n";

   //set postamble
   postamble = string("M5      (Spindle off.)\n") +
                      "M9      (Coolant off.)\n" + 
                      "M2      (Program end.)\n";
}

/******************************************************************************/
/*
 \brief  Destructor
 */
/******************************************************************************/
ExcellonProcessor::~ExcellonProcessor() {
   gerbv_destroy_project(project);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void ExcellonProcessor::set_svg_exporter(shared_ptr<SVG_Exporter> svgexpo) {
   this->svgexpo = svgexpo;
   bDoSVG = true;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void ExcellonProcessor::add_header(string header) {
   this->header.push_back(header);
}

/******************************************************************************/
/*
 \brief	Calculate the board dimensions based on the drill file only
 */
/******************************************************************************/
void ExcellonProcessor::calc_dimensions(void) {

   x_min = +INFINITY;
   x_max = -INFINITY;

   shared_ptr<const map<int, drillbit> > bits = get_bits();
   shared_ptr<const map<int, icoords> > holes = get_holes();

   for (map<int, drillbit>::const_iterator it = bits->begin();
            it != bits->end(); it++) {
      const icoords drill_coords = holes->at(it->first);
      icoords::const_iterator coord_iter = drill_coords.begin();
      x_min = (coord_iter->first < x_min) ? coord_iter->first : x_min;
      x_max = (coord_iter->first > x_max) ? coord_iter->first : x_max;
      ++coord_iter;
      while (coord_iter != drill_coords.end()) {
         x_min = (coord_iter->first < x_min) ? coord_iter->first : x_min;
         x_max = (coord_iter->first > x_max) ? coord_iter->first : x_max;
         ++coord_iter;
      }
   }
   width = x_max - x_min;
   x_center = x_min + width / 2;
}

/******************************************************************************/
/*
 \brief	Recalculates the x-coordinate based on drillfront and mirror_absolute
 \param	drillfront = drill from front side
 \param	mirror_absoulte = mirror back side on y-axis
 \param xvalue = x-coordinate
 \retval recalulated x-coordinate
 */
/******************************************************************************/
double ExcellonProcessor::get_xvalue(bool drillfront, bool mirror_absolute,
                                     double xvalue) {

   double retval;

   if (drillfront) {      //drill from the front, no calculation needed
      retval = xvalue;
   } else {
      if (mirror_absolute) {      //drill from back side, mirrored along y-axis
         retval = xvalue * -1;
      } else {      //drill from back side, mirrored along board center
         retval = (2 * board_center - xvalue);
      }
   }

   return retval;
}

/******************************************************************************/
/*
 \brief  Exports the ngc file for drilling
 \param  of_name = output filename
 \param  driller = ...
 \param  drillfront = if true, use the coordinates from the front side instead of the back side (drill-front=true)
 \param  mirror_absolute = if true, mirror along the y axis instead of the board center
 \param  onedrill : if true, only the first drill bit is used, the others are skipped
 \todo   Optimise the implementation of onedrill by modifying the bits and
 using the smallest bit only.
 */
/******************************************************************************/
void ExcellonProcessor::export_ngc(const string of_name,
                                   shared_ptr<Driller> driller, bool drillfront,
                                   bool mirror_absolute, bool onedrill, bool nog81) {

   ivalue_t double_mirror_axis = mirror_absolute ? 0 : board_width;

   //SVG EXPORTER
   int rad = 1.;

   cout << "Exporting drill... ";

   //open output file
   std::ofstream of;
   of.open(of_name.c_str());

   shared_ptr<const map<int, drillbit> > bits = get_bits();
   shared_ptr<const map<int, icoords> > holes = get_holes();

   //write header to .ngc file
   BOOST_FOREACH (string s, header) {
      of << "( " << s << " )" << "\n";
   }

   if (!onedrill) {
      of << "\n( This file uses " << bits->size() << " drill bit sizes. )\n";
      of << "( Bit sizes:";
      for (map<int, drillbit>::const_iterator it = bits->begin();
               it != bits->end(); it++) {
         of << " [" << it->second.diameter << it->second.unit << "]";
      }
      of << " )\n\n";
   } else {
      of << "\n( This file uses only one drill bit. Forced by 'onedrill' option )\n\n";
   }

   of.setf(ios_base::fixed);      //write floating-point values in fixed-point notation
   of.precision(5);           //Set floating-point decimal precision
   of << setw(7);        //Sets the field width to be used on output operations.

   of << preamble_ext;        //insert external preamble file
   of << preamble;            //insert internal preamble
   of << "S" << left << driller->speed << "     (RPM spindle speed.)\n" << "\n";

   for (map<int, drillbit>::const_iterator it = bits->begin();
            it != bits->end(); it++) {
      //if the command line option "onedrill" is given, allow only the inital toolchange
      if ((onedrill == true) && (it != bits->begin())) {
         of << "(Drill change skipped. Forced by 'onedrill' option.)\n" << "\n";
      } else {
         of << "G00 Z" << driller->zchange * cfactor << " (Retract)\n" << "T"
            << it->first << "\n" << "M5      (Spindle stop.)\n"
            << "(MSG, Change tool bit to drill size " << it->second.diameter
            << " " << it->second.unit << ")\n"
            << "M6      (Tool change.)\n"
            << "M0      (Temporary machine stop.)\n"
            << "M3      (Spindle on clockwise.)\n" << "\n";
      }

      const icoords drill_coords = holes->at(it->first);
      icoords::const_iterator coord_iter = drill_coords.begin();
      //coord_iter->first = x-coorinate (top view)
      //coord_iter->second =y-coordinate (top view)

      //SVG EXPORTER
      if (bDoSVG) {
         svgexpo->set_rand_color();      //set a random color
         svgexpo->circle((double_mirror_axis - coord_iter->first),
                         coord_iter->second, rad);      //draw first circle
         svgexpo->stroke();
      }

	  if( nog81 )
		 of << "F" << driller->feed * cfactor << endl;
	  else {
		 of << "G81 R" << driller->zsafe * cfactor << " Z"
			 << driller->zwork * cfactor << " F" << driller->feed * cfactor << " X"
			 << ( get_xvalue(drillfront, mirror_absolute, coord_iter->first) - xoffset ) * cfactor
			 << " Y" << ( ( coord_iter->second - yoffset ) * cfactor) << "\n";
		 ++coord_iter;
	  }

      while (coord_iter != drill_coords.end()) {
         if( nog81 ) {
			 of << "G0 X"
				 << ( get_xvalue(drillfront, mirror_absolute, coord_iter->first) - xoffset ) * cfactor
				 << " Y" << ( ( coord_iter->second - yoffset ) * cfactor) << "\n";
			 of << "G1 Z" << driller->zwork * cfactor << endl;
			 of << "G1 Z" << driller->zsafe * cfactor << endl;
		 }
		 else {
			 of << "X"
				<< ( get_xvalue(drillfront, mirror_absolute, coord_iter->first) - xoffset )
				   * cfactor
				<< " Y" << ( ( coord_iter->second - yoffset ) * cfactor) << "\n";
		 }
         //SVG EXPORTER
         if (bDoSVG) {
            svgexpo->circle((double_mirror_axis - coord_iter->first),
                            coord_iter->second, rad);      //make a whole
            svgexpo->stroke();
         }
         ++coord_iter;
      }
      of << "\n";
   }

   of << "G00 Z" << driller->zchange * cfactor << " (All done -- retract)\n\n";
   of << postamble_ext;          //insert external postamble file
   of << postamble << endl;      //add internal postamble
   of.close();
   cout << "DONE." << endl;
}

/******************************************************************************/
/*
 *  \brief  mill one circle, returns false if tool is bigger than the circle
 */
/******************************************************************************/
bool ExcellonProcessor::millhole(std::ofstream &of, double x, double y,
                                 shared_ptr<Cutter> cutter,
                                 double holediameter) {

   g_assert(cutter);
   double cutdiameter = cutter->tool_diameter;

   /*cout << fixed << setprecision(3) << " Cutter: " << cutdiameter * cfactor << " " << ( bMetricOutput ? "mm" : "in" ) <<
                                       "  Hole: " << holediameter * cfactor << " " << ( bMetricOutput ? "mm" : "in" ) <<
                                       "  Mill radius: " << (holediameter - cutdiameter) / 2. * cfactor << ( bMetricOutput ? "mm" : "in" ) << endl;*/

   if (cutdiameter * 1.001 >= holediameter) {       //In order to avoid a "zero radius arc" error
      of << "G0 X" << x * cfactor << " Y" << y * cfactor << endl;
      of << "G1 Z" << cutter->zwork * cfactor << endl;
      of << "G0 Z" << cutter->zsafe * cfactor << endl << endl;

      return false;
   } else {

      double millr = (holediameter - cutdiameter) / 2.;      //mill radius

      of << "G0 X" << ( x + millr ) * cfactor << " Y" << y * cfactor << endl;

      double z_step = cutter->stepsize;
      double z = cutter->zwork + z_step * abs(int(cutter->zwork / z_step));

      if (!cutter->do_steps) {
         z = cutter->zwork;
         z_step = 1;      //dummy to exit the loop
      }

      int stepcount = abs(int(cutter->zwork / z_step));

      while (z >= cutter->zwork) {
         of << "G1 Z[" << cutter->zwork * cfactor << "+" << stepcount << "*" << cutter->stepsize * cfactor << "]" << endl;
         of << "G2 I" << -millr * cfactor << " J0" << endl;
         z -= z_step;
         stepcount--;
      }

      of << "G0 Z" << cutter->zsafe * cfactor << endl << endl;

      return true;
   }
}

/******************************************************************************/
/*
 \brief  mill larger holes by using a smaller mill-head
 \bug    does not work properly.
 */
/******************************************************************************/
void ExcellonProcessor::export_ngc(const string outputname,
                                   shared_ptr<Cutter> target, bool drillfront,
                                   bool mirror_absolute, bool onedrill) {
   unsigned int badHoles = 0;

   //g_assert(drillfront == true);       //WHY?
   //g_assert(mirror_absolute == false); //WHY?
      cout << "Exporting drill... ";

   // open output file
   std::ofstream of;
   of.open(outputname.c_str());

   shared_ptr<const map<int, drillbit> > bits = get_bits();
   shared_ptr<const map<int, icoords> > holes = get_holes();

   // write header to .ngc file
   BOOST_FOREACH (string s, header) {
      of << "( " << s << " )" << "\n";
   }
   of << "\n";

   of.setf(ios_base::fixed);      //write floating-point values in fixed-point notation
   of.precision(5);              //Set floating-point decimal precision
   of << setw(7);        //Sets the field width to be used on output operations.

   of << "( This file uses a mill head of " << (bMetricOutput ? (target->tool_diameter * 25.4) : target->tool_diameter)
      << (bMetricOutput ? "mm" : "inch") << " to drill the " << bits->size()
      << "bit sizes. )" << "\n";

   of << "( Bit sizes:";
   for (map<int, drillbit>::const_iterator it = bits->begin();
            it != bits->end(); it++) {
      of << " [" << it->second.diameter << "]";
   }
   of << " )\n\n";

   //preamble
   of << preamble_ext << preamble << "S" << left << target->speed
      << "    (RPM spindle speed.)\n" << "F" << target->feed * cfactor
      << " (Feedrate)\n\n";

   of << "G00 Z" << target->zsafe * cfactor << endl;

   for (map<int, drillbit>::const_iterator it = bits->begin();
            it != bits->end(); it++) {

      double diameter = it->second.unit == "mm" ? it->second.diameter / 25.8 : it->second.diameter;

      const icoords drill_coords = holes->at(it->first);
      icoords::const_iterator coord_iter = drill_coords.begin();

      do {
         if( !millhole(of, get_xvalue(!drillfront, mirror_absolute, coord_iter->first) - xoffset,
         	   coord_iter->second - yoffset, target, diameter) )
            ++badHoles;

         ++coord_iter;
      } while (coord_iter != drill_coords.end());
   }

   // retract, end
   of << "G00 Z" << target->zchange * cfactor << " ( All done -- retract )\n";
   of << postamble_ext;
   of << postamble << endl;
   of.close();

   if( badHoles != 0 ) {
      cerr << "Warning: " << badHoles << ( badHoles == 1 ? " hole was" : " holes were" )
         << " bigger than the milling tool." << endl;
   }
   cout << "DONE." << endl;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void ExcellonProcessor::parse_bits() {
   bits = shared_ptr<map<int, drillbit> >(new map<int, drillbit>());

   for (gerbv_drill_list_t* currentDrill = project->file[0]->image->drill_stats
            ->drill_list; currentDrill; currentDrill = currentDrill->next) {
      drillbit curBit;
      curBit.diameter = currentDrill->drill_size;
      curBit.unit = string(currentDrill->drill_unit);
      curBit.drill_count = currentDrill->drill_count;

      bits->insert(pair<int, drillbit>(currentDrill->drill_num, curBit));
   }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void ExcellonProcessor::parse_holes() {
   if (!bits)
      parse_bits();

   holes = shared_ptr<map<int, icoords> >(new map<int, icoords>());

   for (gerbv_net_t* currentNet = project->file[0]->image->netlist; currentNet;
            currentNet = currentNet->next) {
      if (currentNet->aperture != 0)
         (*holes)[currentNet->aperture].push_back(
                  icoordpair(currentNet->start_x, currentNet->start_y));
   }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
shared_ptr<const map<int, drillbit> > ExcellonProcessor::get_bits() {
   if (!bits)
      parse_bits();

   return bits;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
shared_ptr<const map<int, icoords> > ExcellonProcessor::get_holes() {
   if (!holes)
      parse_holes();

   return holes;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void ExcellonProcessor::set_preamble(string _preamble) {
   preamble_ext = _preamble;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void ExcellonProcessor::set_postamble(string _postamble) {
   postamble_ext = _postamble;
}
