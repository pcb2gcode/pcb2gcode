/*!\defgroup M4_EXPORTER*/
/******************************************************************************/
/*!
 \file       m4_exporter.cpp
 \brief

 \version
 based on 1.1.4 - 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net> 

 \ingroup    M4_EXPORTER
 */
/******************************************************************************/

#include "m4_exporter.hpp"
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <iomanip>
using namespace std;

#include <glibmm/miscutils.h>
using Glib::build_filename;

/******************************************************************************/
/*
 */
/******************************************************************************/
M4_Exporter::M4_Exporter(shared_ptr<Board> board)
         : Exporter(board), dpi(board->get_dpi()), quantization_error( 2.0 / dpi ) {
   this->board = board;
   bDoSVG = false;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void M4_Exporter::set_svg_exporter(shared_ptr<SVG_Exporter> svgexpo) {
   this->svgexpo = svgexpo;
   bDoSVG = true;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void M4_Exporter::add_header(string header) {
   this->header.push_back(header);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void M4_Exporter::export_all(boost::program_options::variables_map& options) {

   bMetricinput = options["metric"].as<bool>();      //set flag for metric input
   bMetricoutput = options["metricoutput"].as<bool>();      //set flag for metric output
   bMirrored = options["mirror-absolute"].as<bool>();      //set flag
   bCutfront = options["cut-front"].as<bool>();      //set flag
   bFrontAutoleveller = options["al-front"].as<bool>();
   bBackAutoleveller = options["al-back"].as<bool>();
   string outputdir = options["output-dir"].as<string>();

   //set imperial/metric conversion factor for output coordinates depending on metricoutput option
   cfactor = bMetricoutput ? 25.4 : 1;

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
	   g64 = quantization_error * cfactor;      // set maximum deviation to 2 pixels to ensure smooth movement
   
   if( bFrontAutoleveller || bBackAutoleveller )
   	  leveller = new autoleveller ( options, quantization_error, xoffset, yoffset );

   if (options["bridges"].as<double>() > 0 && options["bridgesnum"].as<unsigned int>() != 0)
      bBridges = true;
   else
      bBridges = false;

   BOOST_FOREACH( string layername, board->list_layers() ) {
      std::stringstream option_name;
      option_name << layername << "-output";
      string of_name = build_filename(outputdir, options[option_name.str()].as<string>());
      of_name += ".m4";
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
/*
/******************************************************************************/
void M4_Exporter::export_layer(shared_ptr<Layer> layer, string of_name) {

   string layername = layer->get_name();
   shared_ptr<RoutingMill> mill = layer->get_manufacturer();
   bool bSvgOnce = TRUE;
   bool bAutolevelNow;
   vector<shared_ptr<icoords> > toolpaths = layer->get_toolpaths();
   vector<unsigned int> bridges;
   vector<unsigned int>::const_iterator currentBridge;
   
   // open output file
   std::ofstream of;
   of.open(of_name.c_str());

   // write header to .m4 file
   BOOST_FOREACH( string s, header ) {
      of << "m4_header(`" << s << "')\n";
   }

   if( ( bFrontAutoleveller && layername == "front" ) ||
   	   ( bBackAutoleveller && layername == "back" ) ) {
      of << "define(m4_autoleveller)dnl\n";
  	  bAutolevelNow = true;
   }
   else {
      of << "define(m4_noautoleveller)dnl\n";
      bAutolevelNow = false;
    }

   of.setf(ios_base::fixed);      //write floating-point values in fixed-point notation
   of.precision(5);              //Set floating-point decimal precision
   of << setw(7);        //Sets the field width to be used on output operations.
   of << "define(m4_digitdot," << 2 << ")dnl\n";
   of << "define(m4_dotdigit," << 5 << ")dnl\n";

   if (bMetricoutput) {
      of << "define(m4_metric)dnl\n";
   } else {
      of << "define(m4_imperial)dnl\n";
   }

   of << "define(m4_spindlespeed," << mill->speed << ")dnl\n";
   of << "define(m4_maxdeviation," << g64 << ")dnl\n";
   of << "define(m4_feedrate," << mill->feed * cfactor << ")dnl\n";
   of << "define(m4_plungerate," << mill->feed/2 * cfactor << ")dnl\n";
   of << "define(m4_zsafe," << mill->zsafe * cfactor << ")dnl\n";
   of << "define(m4_zwork," << mill->zwork * cfactor << ")dnl\n";

    if( bAutolevelNow ) {
      if( !leveller->prepareWorkarea( toolpaths ) ) {
         std::cerr << "Required number of probe points (" << leveller->requiredProbePoints() <<
                      ") exceeds the maximum number (" << leveller->maxProbePoints() << "). "
                      "Reduce either al-x or al-y." << std::endl;
         exit(EXIT_FAILURE);
      }

      leveller->header( of );
    }

   //SVG EXPORTER
   if (bDoSVG) {
      //choose a color
      svgexpo->set_rand_color();
   }

   of << "m4_preamble" << "\n";       //insert external preamble
   
   // contours
   BOOST_FOREACH( shared_ptr<icoords> path, toolpaths ) {

      of << "m4_move(" << ( path->begin()->first - xoffset ) * cfactor << "," << ( path->begin()->second - yoffset ) * cfactor << ")\n";

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

         if( bBridges ) {
            bridges = layer->get_bridges( path );
            currentBridge = bridges.begin();
         }

         while (z >= mill->zwork) {
            of << "m4_plunge("<< z << ")\n";

            icoords::iterator iter = path->begin();
            icoords::iterator last = path->end();      // initializing to quick & dirty sentinel value
            icoords::iterator peek;
            while (iter != path->end()) {
               peek = iter + 1;

               if (mill->optimise //Already optimised (also includes the bridge case)
                       || last == path->end()  //First
                           || peek == path->end()   //Last
                               || !aligned(last, iter, peek) ) {    //Not aligned
                  of << "m4_mill(" << ( iter->first - xoffset ) * cfactor << "," << ( iter->second - yoffset ) * cfactor << ")\n";
                  if (bDoSVG) {
                     if (bSvgOnce)
                        svgexpo->line_to(iter->first, iter->second);
                  }

                  if( bBridges && currentBridge != bridges.end() ) {
                      if( *currentBridge == iter - path->begin() )
                         of << "m4_Z(" << cutter->bridges_height * cfactor << ")\n";
                      else if( *currentBridge == last - path->begin() ) {
                         of << "m4_Z(" << z * cfactor << ")\n";
                         ++currentBridge;
                      }
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
            leveller->setLastChainPoint( icoordpair( ( path->begin()->first - xoffset ) * cfactor, ( path->begin()->second - yoffset ) * cfactor ) );
            of << "m4_G01corr(" << ( path->begin()->first - xoffset ) * cfactor << "," << ( path->begin()->second - yoffset ) * cfactor << ")\n";
         }

         icoords::iterator iter = path->begin();
         icoords::iterator last = path->end();      // initializing to quick & dirty sentinel value
         icoords::iterator peek;
         while (iter != path->end()) {
            peek = iter + 1;
            if (mill->optimise //When simplifypath is performed, no further optimisation is required
                    || last == path->end()  //First
                        || peek == path->end()   //Last
                            || !aligned(last, iter, peek) ) {    //Not aligned
                /* no need to check for "they are on one axis but iter is outside of last and peek" because that's impossible from how they are generated */
               if( bAutolevelNow )
		          of << "m4_addChainPoint(" << ( iter->first - xoffset ) * cfactor << "," << ( iter->second - yoffset ) * cfactor << ")\n";
               else 
                      of << "m4_mill(" << ( iter->first - xoffset ) * cfactor << "," << ( iter->second - yoffset ) * cfactor << ")\n";
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

   of << "m4_postamble" << "\n";

   if( bAutolevelNow ) {
      leveller->footer( of );
   }

   of.close();

   //SVG EXPORTER
   if (bDoSVG) {
      svgexpo->stroke();
   }
}

