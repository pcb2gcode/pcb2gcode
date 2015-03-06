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
	   g64 = ( 2.0 / this->board->get_dpi() ) * cfactor;      // set maximum deviation to 2 pixels to ensure smooth movement
   
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

   if (options["bridges"].as<double>() != 0 && options["bridgesnum"].as<unsigned int>() != 0) {
      bridges = new outline_bridges( options["bridgesnum"].as<unsigned int>(),
                                     ( options["bridges"].as<double>() + options["cutter-diameter"].as<double>() ) / cfactor,
                                     options.count("zbridges")? options["zbridges"].as<double>() : options["zsafe"].as<double>() );
   }
   else
       bridges = NULL;

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
   vector<unsigned int> bridgesIndexes;
   vector<unsigned int>::const_iterator currentBridge;

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

   of << "G90 ( Absolute coordinates. )\n"
      << "S" << left << mill->speed << " ( RPM spindle speed. )\n"
      << "G64 P" << g64 << " ( set maximum deviation from commanded toolpath )\n\n";

   if( ( layername == "front" && bFrontAutoleveller ) || ( layername == "back" && bBackAutoleveller ) )
      leveller->probeHeader( of, mill->zsafe * cfactor, mill->zsafe * cfactor, autolevellerFailDepth,
      						  autolevellerFeed, probeOnCommands, probeOffCommands );

	of << "F" << mill->feed * cfactor << " ( Feedrate. )\n"
	   << "M3 ( Spindle on clockwise. )\n";

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
         if( bridges ) {
            try {
                bridgesIndexes = bridges->makeBridges( path );
                currentBridge = bridgesIndexes.begin();
                if ( bridgesIndexes.size() != bridges->number )
                    cerr << "Can't create " << bridges->number << " bridges on this layer, only " << bridgesIndexes.size() << " will be created." << endl;
            } catch ( outline_bridges_exception &exc ) {
                cerr << "Error while adding outline bridges. Outline bridges on this path won't be created." << endl;
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

               if (last == path->end()
                       || peek == path->end()
                       || !((last->first == iter->first
                             && iter->first == peek->first)
                            || (last->second == iter->second
                                && iter->second == peek->second))) {
                  of << "X" << ( iter->first - xoffset ) * cfactor << " Y"
                     << ( iter->second - yoffset ) * cfactor << endl;
                  if (bDoSVG) {
                     if (bSvgOnce)
                        svgexpo->line_to(iter->first, iter->second);
                  }
                  if( bridges && currentBridge != bridgesIndexes.end() && *currentBridge == iter - path->begin() )
                     of << "Z" << bridges->height << endl;
                  else if( bridges && currentBridge != bridgesIndexes.end() && *currentBridge == last - path->begin() ) {
                     of << "Z" << z * cfactor << endl;
                     ++currentBridge;
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
         if( bAutolevelNow ) {
            leveller->setLastChainPoint( icoordpair( ( path->begin()->first - xoffset ) * cfactor,
										             ( path->begin()->second - yoffset ) * cfactor ) );
            of << leveller->g01Corrected( icoordpair( ( path->begin()->first - xoffset ) * cfactor,
										              ( path->begin()->second - yoffset ) * cfactor ) );
         }
		 else
            of << "G01 Z" << mill->zwork * cfactor << "\n";

         of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";

         icoords::iterator iter = path->begin();
         icoords::iterator last = path->end();      // initializing to quick & dirty sentinel value
         icoords::iterator peek;

         while (iter != path->end()) {
            peek = iter + 1;
            if (last == path->end()
                    || peek == path->end()
                    || !((last->first == iter->first
                          && iter->first == peek->first)
                         || (last->second == iter->second
                             && iter->second == peek->second))) {
                /* no need to check for "they are on one axis but iter is outside of last and peek" because that's impossible from how they are generated */
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
