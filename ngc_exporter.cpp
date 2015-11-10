/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net> and others
 * Copyright (C) 2010 Bernhard Kubicek <kubicek@gmx.at>
 * Copyright (C) 2013 Erik Schuster <erik@muenchen-ist-toll.de>
 * Copyright (C) 2014, 2015 Nicola Corna <nicola@corna.info>
 *
 * pcb2gcode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * pcb2gcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ngc_exporter.hpp"
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
using std::cerr;
using std::ios_base;
using std::setw;
using std::left;

#include <iomanip>

#include <glibmm/miscutils.h>
using Glib::build_filename;

/******************************************************************************/
/*
 */
/******************************************************************************/
NGC_Exporter::NGC_Exporter(shared_ptr<Board> board)
    : Exporter(board), dpi(board->get_dpi()), 
      quantization_error( 2.0 / dpi ), ocodes(1), globalVars(100)
{
    this->board = board;
    bDoSVG = false;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::set_svg_exporter(shared_ptr<SVG_Exporter> svgexpo)
{
    this->svgexpo = svgexpo;
    bDoSVG = true;
}
/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::add_header(string header)
{
    this->header.push_back(header);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::export_all(boost::program_options::variables_map& options)
{

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
        leveller = new autoleveller ( options, &ocodes, &globalVars, quantization_error, xoffset, yoffset );

    if (options["bridges"].as<double>() > 0 && options["bridgesnum"].as<unsigned int>() > 0)
        bBridges = true;
    else
        bBridges = false;

    BOOST_FOREACH( string layername, board->list_layers() )
    {
        std::stringstream option_name;
        option_name << layername << "-output";
        string of_name = build_filename(outputdir, options[option_name.str()].as<string>());
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
void NGC_Exporter::export_layer(shared_ptr<Layer> layer, string of_name)
{

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

    // write header to .ngc file
    BOOST_FOREACH( string s, header )
    {
        of << "( " << s << " )\n";
    }

    if( ( bFrontAutoleveller && layername == "front" ) ||
            ( bBackAutoleveller && layername == "back" ) )
    {
        of << "( Gcode for " << leveller->getSoftware() << " )\n";
        bAutolevelNow = true;
    }
    else
    {
        of << "( Software-independent Gcode )\n";
        bAutolevelNow = false;
    }

    of.setf(ios_base::fixed);      //write floating-point values in fixed-point notation
    of.precision(5);              //Set floating-point decimal precision
    of << setw(7);        //Sets the field width to be used on output operations.

    of << "\n" << preamble;       //insert external preamble

    if (bMetricoutput)
    {
        of << "G94 ( Millimeters per minute feed rate. )\n"
           << "G21 ( Units == Millimeters. )\n\n";
    }
    else
    {
        of << "G94 ( Inches per minute feed rate. )\n"
           << "G20 ( Units == INCHES. )\n\n";
    }

    of << "G90 ( Absolute coordinates. )\n"
       << "S" << left << mill->speed << " ( RPM spindle speed. )\n"
       << "G64 P" << g64 << " ( set maximum deviation from commanded toolpath )\n"
       << "F" << mill->feed * cfactor << " ( Feedrate. )\n\n";

    if( bAutolevelNow )
    {
        if( !leveller->prepareWorkarea( toolpaths ) )
        {
            std::cerr << "Required number of probe points (" << leveller->requiredProbePoints() <<
                      ") exceeds the maximum number (" << leveller->maxProbePoints() << "). "
                      "Reduce either al-x or al-y." << std::endl;
            exit(EXIT_FAILURE);
        }

        leveller->header( of );
    }

    of << "F" << mill->feed * cfactor << " ( Feedrate. )\n"
       << "M3 ( Spindle on clockwise. )\n";

    //SVG EXPORTER
    if (bDoSVG)
    {
        //choose a color
        svgexpo->set_rand_color();
    }

    // contours
    BOOST_FOREACH( shared_ptr<icoords> path, toolpaths )
    {

        // retract, move to the starting point of the next contour
        of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";
        of << "G00 Z" << mill->zsafe * cfactor << " ( retract )\n\n";
        of << "G00 X" << ( path->begin()->first - xoffset ) * cfactor << " Y"
           << ( path->begin()->second - yoffset ) * cfactor << " ( rapid move to begin. )\n";

        //SVG EXPORTER
        if (bDoSVG)
        {
            svgexpo->move_to(path->begin()->first, path->begin()->second);
            bSvgOnce = TRUE;
        }

        /* if we're cutting, perhaps do it in multiple steps, but do isolations just once.
         * i know this is partially repetitive, but this way it's easier to read
         */
        shared_ptr<Cutter> cutter = boost::dynamic_pointer_cast<Cutter>(mill);

        if (cutter && cutter->do_steps)
        {

            //--------------------------------------------------------------------
            //cutting (outline)

            double z_step = cutter->stepsize;
            double z = mill->zwork + z_step * abs(int(mill->zwork / z_step));

            if( bBridges )
            {
                bridges = layer->get_bridges( path );
                currentBridge = bridges.begin();
            }

            while (z >= mill->zwork)
            {
                of << "G01 Z" << z * cfactor << " F" << mill->vertfeed * cfactor << " ( plunge. )\n";
                of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";
                of << "F" << mill->feed * cfactor << "\n";

                icoords::iterator iter = path->begin();
                icoords::iterator last = path->end();      // initializing to quick & dirty sentinel value
                icoords::iterator peek;
                while (iter != path->end())
                {
                    peek = iter + 1;

                    if (mill->optimise //Already optimised (also includes the bridge case)
                            || last == path->end()  //First
                            || peek == path->end()   //Last
                            || !aligned(last, iter, peek) )      //Not aligned
                    {
                        of << "X" << ( iter->first - xoffset ) * cfactor << " Y"
                           << ( iter->second - yoffset ) * cfactor << endl;
                        if (bDoSVG)
                        {
                            if (bSvgOnce)
                                svgexpo->line_to(iter->first, iter->second);
                        }

                        if( bBridges && currentBridge != bridges.end() )
                        {
                            if( *currentBridge == iter - path->begin() )
                                of << "Z" << cutter->bridges_height * cfactor << endl;
                            else if( *currentBridge == last - path->begin() )
                            {
                                of << "Z" << z * cfactor << " F" << cutter->vertfeed * cfactor << endl;
                                of << "F" << cutter->feed * cfactor;
                                ++currentBridge;
                            }
                        }
                    }

                    last = iter;
                    ++iter;
                }
                //SVG EXPORTER
                if (bDoSVG)
                {
                    svgexpo->close_path();
                    bSvgOnce = FALSE;
                }
                z -= z_step;
            }
        }
        else
        {
            //--------------------------------------------------------------------
            // isolating (front/backside)
            of << "F" << mill->vertfeed * cfactor << endl;

            if( bAutolevelNow )
            {
                leveller->setLastChainPoint( icoordpair( ( path->begin()->first - xoffset ) * cfactor,
                                             ( path->begin()->second - yoffset ) * cfactor ) );
                of << leveller->g01Corrected( icoordpair( ( path->begin()->first - xoffset ) * cfactor,
                                              ( path->begin()->second - yoffset ) * cfactor ) );
            }
            else
                of << "G01 Z" << mill->zwork * cfactor << "\n";

            of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";
            of << "F" << mill->feed * cfactor << endl;

            icoords::iterator iter = path->begin();
            icoords::iterator last = path->end();      // initializing to quick & dirty sentinel value
            icoords::iterator peek;

            while (iter != path->end())
            {
                peek = iter + 1;
                if (mill->optimise //When simplifypath is performed, no further optimisation is required
                        || last == path->end()  //First
                        || peek == path->end()   //Last
                        || !aligned(last, iter, peek) )      //Not aligned
                {
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
            if (bDoSVG)
            {
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

    if( bAutolevelNow )
    {
        leveller->footer( of );
    }

    of.close();

    //SVG EXPORTER
    if (bDoSVG)
    {
        svgexpo->stroke();
    }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::set_preamble(string _preamble)
{
    preamble = _preamble;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void NGC_Exporter::set_postamble(string _postamble)
{
    postamble = _postamble;
}
