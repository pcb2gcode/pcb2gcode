/*!\defgroup NGC_EXPORTER*/
/******************************************************************************/
/*!
 \file       ngc_exporter.cpp
 \brief

 \version
 04.08.2013 - Erik Schuster - erik@muenchen-ist-toll.de\n
 - Optimised time for export by approx. 5%. ("\n" instead of endl).
 - Added metricoutput option.
 - Added g64 option.
 - Added ondrill option.
 - Started documenting the code for doxygen processing.
 - Formatted the code with the Eclipse code styler (Style: K&R).

 \version
 1.1.4 - 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net> and others

 \copyright  pcb2gcode is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 pcb2gcode is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.

 \todo
 re-think g64+get_tolerance

 \ingroup    NGC_EXPORTER
 */
/******************************************************************************/

#include "ngc_exporter.hpp"
#include <boost/foreach.hpp>
#include <iostream>
#include <iomanip>
using namespace std;

/******************************************************************************/
/*
 */
/******************************************************************************/
NGC_Exporter::NGC_Exporter(shared_ptr<Board> board)
         : Exporter(board) {
   this->board = board;
   bDoSVG = false;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::set_svg_exporter(shared_ptr<SVG_Exporter> svgexpo) {
   this->svgexpo = svgexpo;
   bDoSVG = true;
}
/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::add_header(string header) {
   this->header.push_back(header);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::export_all(boost::program_options::variables_map& options) {

   g64 = options.count("g64") ? options["g64"].as<double>() : 1;        //set g64 value
   bMetricinput = options["metric"].as<bool>();                         //set flag for metric input
   bMetricoutput = options["metricoutput"].as<bool>();                  //set flag for metric output
   bOptimise = options["optimise"].as<bool>();                          //set flag for optimisation

   //set imperial/metric conversion factor for output coordinates depending on metricoutput option
   cfactor = bMetricoutput ? 25.4 : 1;

   BOOST_FOREACH( string layername, board->list_layers() ) {
      std::stringstream option_name;
      option_name << layername << "-output";
      string of_name = options[option_name.str()].as<string>();
      cerr << "Exporting " << layername << "... ";
      export_layer(board->get_layer(layername), of_name);
      cerr << "DONE." << " (Height: " << board->get_height() * cfactor
           << (bMetricoutput ? "mm" : "in") << " Width: "
           << board->get_width() * cfactor << (bMetricoutput ? "mm" : "in")
           << ")" << endl;
   }
}

/******************************************************************************/
/*
 \brief  Returns the maximum allowed tolerance for the commanded toolpath
 \retval Maximum allowed tolerance in Inches.
 */
/******************************************************************************/
double NGC_Exporter::get_tolerance(void) {
   if (g64 < 1) {
      return g64;      // return g64 value if plausible value (more or less) is given.
   } else {
      return 5.0 / this->board->get_dpi();      // set maximum deviation to 5 pixels to ensure smooth movement
   }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::export_layer(shared_ptr<Layer> layer, string of_name) {

   string layername = layer->get_name();
   shared_ptr<RoutingMill> mill = layer->get_manufacturer();

   bool bSvgOnce = TRUE;
   double tolerance = get_tolerance();
   //double xcenter = (board->get_max_x() - board->get_min_x()) / 2;
   //double ycenter = (board->get_max_y() - board->get_min_y()) / 2;

   // open output file
   std::ofstream of;
   of.open(of_name.c_str());

   // write header to .ngc file
   BOOST_FOREACH( string s, header ) {
      of << "( " << s << " )\n";
   }

   of.setf(ios_base::fixed);      //write floating-point values in fixed-point notation
   of.precision(5);           //Set floating-point decimal precision
   of << setw(7);        //Sets the field width to be used on output operations.

   of << "\n" << preamble;      //insert external preamble

   if (bMetricoutput) {
      of << "G94     ( Millimeters per minute feed rate. )\n"
         << "G21     ( Units == Millimeters. )\n\n";
   } else {
      of << "G94     ( Inches per minute feed rate. )\n"
         << "G20     ( Units == INCHES. )\n\n";
   }

   of << "G90     ( Absolute coordinates.        )\n" << "S" << left
      << mill->speed << "  ( RPM spindle speed.           )\n"
      << "F" << mill->feed * cfactor << "\n"
      << "M3      ( Spindle on clockwise.        )\n\n";

   of << "G64 P" << tolerance
      << " ( set maximum deviation from commanded toolpath )\n\n";

   //SVG EXPORTER
   if (bDoSVG) {
      //choose a color
      svgexpo->set_rand_color();
   }

   // contours
   BOOST_FOREACH( shared_ptr<icoords> path, layer->get_toolpaths() ) {
      // retract, move to the starting point of the next contour
      of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";
      of << "G00 Z" << mill->zsafe * cfactor << " ( retract )\n\n";
      of << "G00 X" << path->begin()->first * cfactor << " Y"
         << path->begin()->second * cfactor << " ( rapid move to begin. )\n";

      //SVG EXPORTER
      if (bDoSVG) {
         svgexpo->move_to(path->begin()->first, path->begin()->second);
         bSvgOnce = TRUE;
      }

      /* if we're cutting, perhaps do it in multiple steps, but do isolations just once.
       * i know this is partially repetitive, but this way it's easier to read
       */
      shared_ptr<Cutter> cutter = boost::dynamic_pointer_cast<Cutter>(mill);

      if (cutter && cutter->do_steps) {

         //--------------------------------------------------------------------
         //cutting (outline)

         double z_step = cutter->stepsize;
         double z = mill->zwork + z_step * abs(int(mill->zwork / z_step));

         while (z >= mill->zwork) {
            /*
            of << "G01 Z" << z * cfactor << " F" << mill->feed * cfactor
               << " ( plunge. )\n";*/
            of << "G01 Z" << z * cfactor << "\n";
            of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";

            icoords::iterator iter = path->begin();
            icoords::iterator last = path->end();      // initializing to quick & dirty sentinel value
            icoords::iterator peek;

            while (iter != path->end()) {
               peek = iter + 1;

               //iter->first = x-coorinate (top view)
               //iter->second =y-coordinate (top view)

               /* it's necessary to write the coordinates if...it's the beginning or it's the end*/
               /* or if neither of the axis align */
               /* x axis aligns */
               /* y axis aligns */
               /* no need to check for "they are on one axis but iter is outside of last and peek" because that's impossible from how they are generated */

               if (last == path->end()
                   || peek == path->end()
                   || !((last->first == iter->first
                         && iter->first == peek->first)
                        || (last->second == iter->second
                            && iter->second == peek->second))) {

                  of << "X" << iter->first * cfactor << " Y" << iter->second * cfactor << "\n";

                  //SVG EXPORTER
                  if (bDoSVG) {
                     if (bSvgOnce)
                        svgexpo->line_to(iter->first, iter->second);
                  }
               }
               last = iter;
               ++iter;
            }

            //SVG EXPORTER
            if (bDoSVG) {
               svgexpo->close_path();
               bSvgOnce = FALSE;
            }

            z -= z_step;
         }
      } else {

         //--------------------------------------------------------------------
         // isolating (front/backside)
         /*
         of << "G01 Z" << mill->zwork * cfactor << " F" << mill->feed * cfactor
            << " ( plunge. )\n";
            */
         of << "G01 Z" << mill->zwork * cfactor << "\n";
         of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";

         icoords::iterator iter = path->begin();
         icoords::iterator last = path->end();      //initializing to quick & dirty sentinel value
         icoords::iterator peek;

         while (iter != path->end()) {
            peek = iter + 1;
            if ( /* it's necessary to write the coordinates if... */
            last == path->end() || /* it's the beginning */
            peek == path->end()
            || /* it's the end */
            !( /* or if neither of the axis align */
            (last->first == iter->first && iter->first == peek->first) || /* x axis aligns */
            (last->second == iter->second && iter->second == peek->second) /* y axis aligns */
            )
            /* no need to check for "they are on one axis but iter is outside of last and peek" becaus that's impossible from how they are generated */
            ) {
               of << "X" << iter->first * cfactor << " Y"
                  << iter->second * cfactor << "\n";
               //SVG EXPORTER
               if (bDoSVG)
                  if (bSvgOnce)
                     svgexpo->line_to(iter->first, iter->second);
            }
            last = iter;
            ++iter;
         }
         //SVG EXPORTER
         if (bDoSVG) {
            svgexpo->close_path();
            bSvgOnce = FALSE;
         }
      }
   }

   of << "\n";
   // retract
   of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n"
      << "G00 Z" << mill->zchange * cfactor << " ( retract )\n\n"
      << postamble
      << "M9 ( Coolant off. )\n"
      << "M2 ( Program end. )" << endl;
   of.close();

   //SVG EXPORTER
   if (bDoSVG) {
      svgexpo->stroke();
   }
}

/******************************************************************************/
/*
 \brief   Calculates the slope of the line between point A and B
 */
/******************************************************************************/
double NGC_Exporter::calcSlope(icoords::iterator a, icoords::iterator b) {
   double x;
   double y;
   x = b->first - a->first;
   y = b->second - a->second;
   return y / x;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::set_preamble(string _preamble) {
   preamble = _preamble;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::set_postamble(string _postamble) {
   postamble = _postamble;
}

