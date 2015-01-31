/*!\defgroup NGC_EXPORTER*/
/******************************************************************************/
/*!
 \file       ngc_exporter.cpp
 \brief

 \version
 09.01.2015 - Nicola Corna - nicola@corna.info\n
 - Added zero-start option

 20.11.2014 - Nicola Corna - nicola@corna.info\n
 - Added bridge height option
 - Enabled bridges when bOptimise=false
 
 \version
 19.12.2013 - Erik Schuster - erik@muenchen-ist-toll.de\n
 - added option for optimised g-code output (reduces file size up to 40%).
 - added option to add four bridges to the outline cut (clumsy code, but it works)
 - added methods optimise_Path,add_Bridge,get_SlopeOfLine,get_D_PointToLine,get_D_PointToPoint,get_Y_onLine,get_X_onLine

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

 \ingroup    NGC_EXPORTER
 */
/******************************************************************************/

#include "ngc_exporter.hpp"
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
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

   autoleveller::Software autolevellerSoftware;
   bMetricinput = options["metric"].as<bool>();      //set flag for metric input
   bMetricoutput = options["metricoutput"].as<bool>();      //set flag for metric output
   bOptimise = options["optimise"].as<bool>();       //set flag for optimisation
   bMirrored = options["mirror-absolute"].as<bool>();      //set flag
   bCutfront = options["cut-front"].as<bool>();      //set flag
   bFrontAutoleveller = options["al-front"].as<bool>();
   bBackAutoleveller = options["al-back"].as<bool>();
   probeOnCommands = options["al-probe-on"].as<string>();
   probeOffCommands = options["al-probe-off"].as<string>();
   string outputdir = options["output-dir"].as<string>();

   //set imperial/metric conversion factor for output coordinates depending on metricoutput option
   cfactor = bMetricoutput ? 25.4 : 1;
   
#ifdef __unix__
   if ( !outputdir.empty() && *outputdir.rbegin() != '/' )
      outputdir += '/';
#else
   if ( !outputdir.empty() && *outputdir.rbegin() != '/' && *outputdir.rbegin() != '\\' )
      outputdir += '\\';
#endif

   if( options["zero-start"].as<bool>() )
   {
      xoffset = board->get_min_x();
      yoffset = board->get_min_y();
   }
   else
   {
      xoffset = 0;
      yoffset = 0;
   }

   if( options.count("g64") )
       g64 = options["g64"].as<double>();
   else
	   g64 = ( 5.0 / this->board->get_dpi() ) * cfactor;      // set maximum deviation to 5 pixels to ensure smooth movement
   
   if( bFrontAutoleveller || bBackAutoleveller ) {
      autolevellerFeed = options["al-probefeed"].as<double>();
	  autolevellerFailDepth = AUTOLEVELLER_FIXED_FAIL_DEPTH * cfactor;	//Fixed (by now) 
       
	  if( boost::iequals( options["software"].as<string>(), "turbocnc" ) )
	     autolevellerSoftware = autoleveller::TURBOCNC;
	  else if( boost::iequals( options["software"].as<string>(), "mach3" ) )
	     autolevellerSoftware = autoleveller::MACH3;
   	  else if( boost::iequals( options["software"].as<string>(), "mach4" ) )
	     autolevellerSoftware = autoleveller::MACH4;
	  else
	     autolevellerSoftware = autoleveller::LINUXCNC;

      try {
      	 leveller = new autoleveller ( ( board->get_min_x() - xoffset ) * cfactor, ( board->get_min_y() - yoffset ) * cfactor,
      								 ( board->get_max_x() - xoffset ) * cfactor, ( board->get_max_y() - yoffset ) * cfactor,
      								 options["al-x"].as<double>(),
      								 options["al-y"].as<double>(),
      								 options["zwork"].as<double>(),
      								 autolevellerSoftware );
      } catch (autoleveller_exception &exc) {
      	 std::cerr << "Number of probe points exceeds the maximum value (500). Reduce either al-x or al-y" << std::endl;
         exit(EXIT_FAILURE);
      }
	}

   if (options["bridges"].as<double>() == 0) {
      bBridges = false;
      dBridgewidth = 0;
      bridgesZ = options["zsafe"].as<double>();
   } else {
      bBridges = true;
      dBridgewidth = options["bridges"].as<double>() / cfactor;
      if (options.count("zbridges"))
        bridgesZ = options["zbridges"].as<double>();
      else
        bridgesZ = options["zsafe"].as<double>();	//Use zsafe as default value
   }

   BOOST_FOREACH( string layername, board->list_layers() ) {
      std::stringstream option_name;
      option_name << layername << "-output";
      string of_name = outputdir + options[option_name.str()].as<string>();
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
 */
/******************************************************************************/
void NGC_Exporter::export_layer(shared_ptr<Layer> layer, string of_name) {

   string layername = layer->get_name();
   shared_ptr<RoutingMill> mill = layer->get_manufacturer();
   bool bSvgOnce = TRUE;
   bool bAutolevelNow;

   // open output file
   std::ofstream of;
   of.open(of_name.c_str());

   // write header to .ngc file
   BOOST_FOREACH( string s, header ) {
      of << "( " << s << " )\n";
   }

   if( ( bFrontAutoleveller && layername == "front" ) ||
   	   ( bBackAutoleveller && layername == "back" ) ) {
   	  bAutolevelNow = true;
   }
   else
      bAutolevelNow = false;

   of.setf(ios_base::fixed);      //write floating-point values in fixed-point notation
   of.precision(5);              //Set floating-point decimal precision
   of << setw(7);        //Sets the field width to be used on output operations.

   of << "\n" << preamble;       //insert external preamble

   if (bMetricoutput) {
      of << "G94 ( Millimeters per minute feed rate. )\n"
         << "G21 ( Units == Millimeters. )\n\n";
   } else {
      of << "G94 ( Inches per minute feed rate. )\n"
         << "G20 ( Units == INCHES. )\n\n";
   }

   of << "G90 ( Absolute coordinates. )\n" << "S" << left
      << mill->speed << " ( RPM spindle speed. )\n" << "F"
      << mill->feed * cfactor << " ( Feedrate. )\n";

   of << "G64 P" << g64
      << " ( set maximum deviation from commanded toolpath )\n\n";

   if( ( layername == "front" && bFrontAutoleveller ) || ( layername == "back" && bBackAutoleveller ) )
      leveller->probeHeader( of, mill->zsafe * cfactor, mill->zsafe * cfactor, autolevellerFailDepth,
      						  autolevellerFeed, probeOnCommands, probeOffCommands );

	of << "M3 ( Spindle on clockwise. )\n";

   //SVG EXPORTER
   if (bDoSVG) {
      //choose a color
      svgexpo->set_rand_color();
   }

   // contours
   BOOST_FOREACH( shared_ptr<icoords> path, layer->get_toolpaths() ) {

      if (bOptimise) {
         optimise_Path(path);
      }

      // retract, move to the starting point of the next contour
      of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";
      of << "G00 Z" << mill->zsafe * cfactor << " ( retract )\n\n";
      of << "G00 X" << ( path->begin()->first - xoffset ) * cfactor << " Y"
         << ( path->begin()->second - yoffset ) * cfactor << " ( rapid move to begin. )\n";

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

         if( bBridges ) {
            dBridgewidth += mill->tool_diameter;
            if (!bMirrored) {
               dBridgexmin = (board->get_width() / 2 + board->get_min_x())
                             - dBridgewidth / 2;
               dBridgexmax = dBridgexmin + dBridgewidth;
               dBridgeymin = (board->get_height() / 2 + board->get_min_y())
                             - dBridgewidth / 2;
               dBridgeymax = dBridgeymin + dBridgewidth;
            } else {
               dBridgexmax = (board->get_width() / 2 + board->get_min_x())
                             - dBridgewidth / 2;
               dBridgexmax *= -1;
               dBridgexmin = dBridgexmax - dBridgewidth;
               dBridgeymin = (board->get_height() / 2 + board->get_min_y())
                             - dBridgewidth / 2;
               dBridgeymax = dBridgeymin + dBridgewidth;
            }
         }

         //--------------------------------------------------------------------
         //cutting (outline)

         double z_step = cutter->stepsize;
         double z = mill->zwork + z_step * abs(int(mill->zwork / z_step));

         while (z >= mill->zwork) {
            of << "G01 Z" << z * cfactor << " F" << mill->feed * cfactor / 2
               << " ( plunge. )\n";
            of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";
            of << "F" << mill->feed * cfactor << "\n";

            icoords::iterator iter = path->begin();
            icoords::iterator last = path->end();      // initializing to quick & dirty sentinel value
            icoords::iterator peek;
            while (iter != path->end()) {
               peek = iter + 1;

               if (!bOptimise
                   && (last == path->end()
                       || peek == path->end()
                       || !((last->first == iter->first
                             && iter->first == peek->first)
                            || (last->second == iter->second
                                && iter->second == peek->second)))) {
                  of << "X" << ( iter->first - xoffset ) * cfactor << " Y"
                     << ( iter->second - yoffset ) * cfactor << endl;
                  if (bDoSVG) {
                     if (bSvgOnce)
                        svgexpo->line_to(iter->first, iter->second);
                  }
               }
               if (bBridges && last != path->end() && peek != path->end()) {
                  add_Bridge(of, bridgesZ, double(z * cfactor),
                           path->at(distance(path->begin(), last)),
                           path->at(distance(path->begin(), iter)));
                  of << "X" << ( iter->first - xoffset ) * cfactor << " Y"
                     << ( iter->second - yoffset ) * cfactor << endl;
               } else {
                  of << "X" << ( iter->first - xoffset ) * cfactor << " Y"
                     << ( iter->second - yoffset ) * cfactor << endl;
               }
               if (bDoSVG && bOptimise && bSvgOnce) {
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
            z -= z_step;
         }
      } else {
         //optimise_Path(path,"test_iso.ngc");
         //--------------------------------------------------------------------
         // isolating (front/backside)
         of << "G01 Z" << mill->zwork * cfactor << "\n";
         of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";

		 if( bAutolevelNow )
		 	leveller->startNewChain();

         icoords::iterator iter = path->begin();
         icoords::iterator last = path->end();      // initializing to quick & dirty sentinel value
         icoords::iterator peek;
         while (iter != path->end()) {
            peek = iter + 1;
            if (!bOptimise
                && (last == path->end()
                    || peek == path->end()
                    || !((last->first == iter->first
                          && iter->first == peek->first)
                         || (last->second == iter->second
                             && iter->second == peek->second)))
                /* no need to check for "they are on one axis but iter is outside of last and peek" because that's impossible from how they are generated */
                ) {
               if( bAutolevelNow )
		          of << leveller->addChainPoint( icoordpair( ( iter->first - xoffset ) * cfactor, ( iter->second - yoffset ) * cfactor ) );
               else 
	              of << "X" << ( iter->first - xoffset ) * cfactor << " Y"
	                 << ( iter->second - yoffset ) * cfactor << endl;
               //SVG EXPORTER
               if (bDoSVG)
                  if (bSvgOnce)
                     svgexpo->line_to(iter->first, iter->second);
            }
            if (bOptimise) {
		       if( bAutolevelNow )
		          of << leveller->addChainPoint( icoordpair( ( iter->first - xoffset ) * cfactor, ( iter->second - yoffset ) * cfactor ) );
		       else 
		          of << "X" << ( iter->first - xoffset ) * cfactor << " Y"
		          << ( iter->second - yoffset ) * cfactor;
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
      << "G00 Z" << mill->zchange * cfactor << " ( retract )\n\n" << postamble
      << "M5 ( Spindle off. )\n" << "M9 ( Coolant off. )\n" << "M2 ( Program end. )"
	  << endl << endl;
   of.close();

   //SVG EXPORTER
   if (bDoSVG) {
      svgexpo->stroke();
   }
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

/******************************************************************************/
/*
 \brief        Optimise the tool path
 \description  Simple path optimisation (no fancy algorithm).
 Reduces the file size by approx. 20-40%.
 Loss of precision is less than the dpi value.
 */
/******************************************************************************/
void NGC_Exporter::optimise_Path(shared_ptr<icoords> path) {

   icoords::iterator p0;
   icoords::iterator p1;
   icoords::iterator p2;
   double smax, s02, m01, m02, m12;

   p0 = path->begin();
   smax = 1.01 / board->get_dpi() * sqrt(2);
   while (p0 < path->end() - 2) {
      p1 = p0 + 1;
      p2 = p1 + 1;
      m02 = get_SlopeOfLine(path->at(distance(path->begin(), p0)),
                   path->at(distance(path->begin(), p2)));
      m02 = fabs(m02);
      s02 = get_D_PointToPoint(path->at(distance(path->begin(), p0)),
                            path->at(distance(path->begin(), p2)));
      if ((m02 > 0.95 && m02 < 1.05 && s02 <= smax)) {
         path->erase(p1);
      } else {
         p0++;
      }
   }

   p0 = path->begin();
   while (p0 < path->end() - 2) {
      p1 = p0 + 1;
      p2 = p1 + 1;
      m01 = get_SlopeOfLine(path->at(distance(path->begin(), p0)),
                   path->at(distance(path->begin(), p1)));
      m12 = get_SlopeOfLine(path->at(distance(path->begin(), p1)),
                   path->at(distance(path->begin(), p2)));
      if (m01 == m12) {
         path->erase(p1);
      } else {
         p0++;
      }
   }
}

/******************************************************************************/
/*
 \brief        Add ridges to the outline cut
 \description  Works for "normal" shaped pcb only.
 \param        of = output file
 \param        zsh = saftey height
 \param        zcut = cut depth
 \param        p0 = last point
 \param        p1 = current point
 */
/******************************************************************************/
void NGC_Exporter::add_Bridge(std::ofstream& of, double zsh, double zcut,
                            icoordpair p0, icoordpair p1) {
   static unsigned char state = 0;      //state of statemachine
   static bool cutting = true;      //if true, cutting, else on saftey height
   double x0 = p0.first;
   double x1 = p1.first;
   double y0 = p0.second;
   double y1 = p1.second;
   double x, y;

   switch (state) {

      case 0:
         //top to bottom
         if (cutting && y0 >= dBridgeymax && y1 <= dBridgeymax) {
            x = get_X_onLine(dBridgeymax, p0, p1);
            of << "X" << ( x - xoffset ) * cfactor << " Y" << ( dBridgeymax - yoffset ) * cfactor << "\n";
            of << "Z" << zsh << "\n";
            cutting = false;      //set flag
         }
         if (!cutting && y0 >= dBridgeymin && y1 <= dBridgeymin) {
            x = get_X_onLine(dBridgeymin, p0, p1);
            of << "X" << ( x - xoffset ) * cfactor << " Y" << ( dBridgeymin - yoffset ) * cfactor << "\n";
            of << "Z" << zcut << "\n";
            cutting = true;      //set flag
            state = bCutfront ? 3 : 1;      //set next state
         }
         break;

      case 1:
         //left to right
         if (cutting && x0 <= dBridgexmin && x1 >= dBridgexmin) {
            y = get_Y_onLine(dBridgexmax, p0, p1);
            of << "X" << ( dBridgexmin - xoffset ) * cfactor << " Y" << ( y - yoffset ) * cfactor << "\n";
            of << "Z" << zsh << "\n";
            cutting = false;      //set flag
         }
         if (!cutting && x0 <= dBridgexmax && x1 >= dBridgexmax) {
            y = get_Y_onLine(dBridgexmax, p0, p1);
            of << "X" << ( dBridgexmax - xoffset ) * cfactor << " Y" << ( y - yoffset ) * cfactor << "\n";
            of << "Z" << zcut << "\n";
            cutting = true;      //set flag
            state = bCutfront ? 0 : 2;      //set next state
         }
         break;

      case 2:
         //bottom to top
         if (cutting && y0 <= dBridgeymin && y1 >= dBridgeymin) {
            x = get_X_onLine(dBridgeymin, p0, p1);
            of << "X" << ( x - xoffset ) * cfactor << " Y" << ( dBridgeymin - yoffset ) * cfactor << "\n";
            of << "Z" << zsh << "\n";
            cutting = false;      //set flag
         }
         if (!cutting && y0 <= dBridgeymax && y1 >= dBridgeymax) {
            x = get_X_onLine(dBridgeymax, p0, p1);
            of << "X" << ( x - xoffset ) * cfactor << " Y" << ( dBridgeymax - yoffset ) * cfactor << "\n";
            of << "Z" << zcut << "\n";
            cutting = true;      //set flag
            state = bCutfront ? 1 : 3;      //set next state
         }
         break;

      case 3:
         //right to left
         if (cutting && x0 >= dBridgexmax && x1 <= dBridgexmax) {
            y = get_Y_onLine(dBridgexmax, p0, p1);
            of << "X" << ( dBridgexmax - xoffset ) * cfactor << " Y" << ( y - yoffset ) * cfactor << "\n";
            of << "Z" << zsh << "\n";
            cutting = false;      //set flag
         }
         if (!cutting && x0 >= dBridgexmin && x1 <= dBridgexmin) {
            y = get_Y_onLine(dBridgexmax, p0, p1);
            of << "X" << ( dBridgexmin - xoffset ) * cfactor << " Y" << ( y - yoffset ) * cfactor << "\n";
            of << "Z" << zcut << "\n";
            cutting = true;      //set flag
            state = bCutfront ? 2 : 0;      //set next state
         }
         break;
   }
}

/******************************************************************************/
/*
 \brief   Get the y value with given x value and a line between p1 and p2
 \param   x = x-value on the line
 \param   p1,p2 = points of the line
 \retval  y-value
 */
/******************************************************************************/
double NGC_Exporter::get_Y_onLine(double x, icoordpair p1, icoordpair p2) {
   double x1 = p1.first;
   double y1 = p1.second;
   double x2 = p2.first;
   double y2 = p2.second;
   return (y2 - y1) / (x2 - x1) * x + (x2 * y1 - x1 * y2) / (x2 - x1);
}

/******************************************************************************/
/*
 \brief   Get the x value with given y value and a line between p1 and p2
 \param   y = y-value on the line
 \param   p1,p2 = points of the line
 \retval  x-value
 */
/******************************************************************************/
double NGC_Exporter::get_X_onLine(double y, icoordpair p1, icoordpair p2) {
   double x1 = p1.first;
   double y1 = p1.second;
   double x2 = p2.first;
   double y2 = p2.second;
   return (y * (x1 - x1) - (x2 * y1 - x1 * y2)) / (y2 - y1);
}

/******************************************************************************/
/*
 \brief   Calculates the slope of the line between point A and B
 \param   p1,p2 = Points of the line
 \retval  Slope
 */
/******************************************************************************/
double NGC_Exporter::get_SlopeOfLine(icoordpair p1, icoordpair p2) {
   return (p2.second - p1.second) / (p2.first - p1.first);
}

/******************************************************************************/
/*
 \brief  Calculates the distance of a point from a line
 \param  p1 = point
 \param  p2 = point 1 of line
 \param  p3 = point 2 of line
 \reval  d  = distance of point p1 from line p2-p3
 */
/******************************************************************************/
double NGC_Exporter::get_D_PointToLine(icoordpair p1, icoordpair p2,
                                    icoordpair p3) {
   double a = fabs(
            (p2.first * p3.second + p3.first * p1.second + p1.first * p2.second
             - p3.first * p2.second - p1.first * p3.second
             - p2.first * p1.second));
   double b = sqrt(
            pow((p2.first - p3.first), 2) + pow((p2.second - p3.second), 2));
   return a / b;
}

/******************************************************************************/
/*
 \brief   Calculates the distance between two points
 \param   p1,p2 = points
 \retval  Distance
 */
/******************************************************************************/
double NGC_Exporter::get_D_PointToPoint(icoordpair p1, icoordpair p2) {
   return sqrt(pow((p1.first - p2.first), 2) + pow((p1.second - p2.second), 2));
}
