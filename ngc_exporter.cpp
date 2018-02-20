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
#include <boost/algorithm/string.hpp>
#include <iostream>
using std::cerr;
using std::flush;
using std::ios_base;
using std::left;
using std::to_string;

#include <cmath>
using std::ceil;

#include <iomanip>

#include <glibmm/miscutils.h>
using Glib::build_filename;

#include <boost/format.hpp>
using boost::format;

/******************************************************************************/
/*
 */
/******************************************************************************/
NGC_Exporter::NGC_Exporter(shared_ptr<Board> board)
    : Exporter(board), dpi(board->get_dpi()), 
      quantization_error( 2.0 / dpi ), ocodes(1), globalVars(100)
{
    this->board = board;
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
    bZchangeG53 = options["zchange-absolute"].as<bool>();
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
    
    tileInfo = Tiling::generateTileInfo( options, ocodes, board->get_height(), board->get_width() );

    if( bFrontAutoleveller || bBackAutoleveller )
        leveller = new autoleveller ( options, &ocodes, &globalVars, quantization_error,
                                      xoffset, yoffset, tileInfo );

    if (options["bridges"].as<double>() > 0 && options["bridgesnum"].as<unsigned int>() > 0)
        bBridges = true;
    else
        bBridges = false;

    for ( string layername : board->list_layers() )
    {
      if( options["zero-start"].as<bool>() ) {
        if (layername == "back" ||
            (layername == "outline" && !workSide(options, "cut"))) {
          xoffset = -board->get_min_x();
          yoffset = board->get_min_y();
        } else {
          xoffset = board->get_min_x();
          yoffset = board->get_min_y();
        }
      } else {
        xoffset = 0;
        yoffset = 0;
      }
        std::stringstream option_name;
        option_name << layername << "-output";
        string of_name = build_filename(outputdir, options[option_name.str()].as<string>());
        cout << "Exporting " << layername << "... " << flush;
        export_layer(board->get_layer(layername), of_name);
        cout << "DONE." << " (Height: " << board->get_height() * cfactor
             << (bMetricoutput ? "mm" : "in") << " Width: "
             << board->get_width() * cfactor << (bMetricoutput ? "mm" : "in")
             << ")";
        if (layername == "outline")
            cout << " The board should be cut from the " << ( workSide(options, "cut") ? "FRONT" : "BACK" ) << " side. ";
        cout << endl;
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
    bool bAutolevelNow;
    vector<shared_ptr<icoords> > toolpaths = layer->get_toolpaths();
    vector<unsigned int> bridges;
    vector<unsigned int>::const_iterator currentBridge;

    double xoffsetTot;
    double yoffsetTot;
    Tiling tiling( tileInfo, cfactor );
    tiling.setGCodeEnd(string("\nG04 P0 ( dwell for no time -- G64 should not smooth over this point )\n")
        + (bZchangeG53 ? "G53 " : "") + "G00 Z" + str( format("%.3f") % ( mill->zchange * cfactor ) ) + 
        " ( retract )\n\n" + postamble + "M5 ( Spindle off. )\nG04 P" +
        to_string(mill->spindown_time) +
        "M9 ( Coolant off. )\n"
        "M2 ( Program end. )\n\n" );

    tiling.initialXOffsetVar = globalVars.getUniqueCode();
    tiling.initialYOffsetVar = globalVars.getUniqueCode();

    // open output file
    std::ofstream of;
    of.open(of_name.c_str());

    // write header to .ngc file
    for ( string s : header )
    {
        of << "( " << s << " )\n";
    }

    if( ( bFrontAutoleveller && layername == "front" ) ||
        ( bBackAutoleveller && layername == "back" ) )
        bAutolevelNow = true;
    else
        bAutolevelNow = false;

    if( bAutolevelNow || ( tileInfo.enabled && tileInfo.software != CUSTOM ) )
        of << "( Gcode for " << getSoftwareString(tileInfo.software) << " )\n";
    else
        of << "( Software-independent Gcode )\n";

    of.setf(ios_base::fixed);      //write floating-point values in fixed-point notation
    of.precision(5);              //Set floating-point decimal precision

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
       << "S" << left << mill->speed << " ( RPM spindle speed. )\n";

    if (mill->explicit_tolerance)
        of << "G64 P" << mill->tolerance * cfactor << " ( set maximum deviation from commanded toolpath )\n";

    of << "F" << mill->feed * cfactor << " ( Feedrate. )\n\n";

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
       << "M3 ( Spindle on clockwise. )\n"
       << "G04 P" << mill->spinup_time << "\n";
    
    tiling.header( of );

    for( unsigned int i = 0; i < tileInfo.forYNum; i++ )
    {
        yoffsetTot = yoffset - i * tileInfo.boardHeight;
        
        for( unsigned int j = 0; j < tileInfo.forXNum; j++ )
        {
            xoffsetTot = xoffset - ( i % 2 ? tileInfo.forXNum - j - 1 : j ) * tileInfo.boardWidth;

            if( tileInfo.enabled && tileInfo.software == CUSTOM )
                of << "( Piece #" << j + 1 + i * tileInfo.forXNum << ", position [" << j << ";" << i << "] )\n\n";

            // contours
            for ( shared_ptr<icoords> path : toolpaths )
            {
                // retract, move to the starting point of the next contour
                of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";
                of << "G00 Z" << mill->zsafe * cfactor << " ( retract )\n\n";
                of << "G00 X" << ( path->begin()->first - xoffsetTot ) * cfactor << " Y"
                   << ( path->begin()->second - yoffsetTot ) * cfactor << " ( rapid move to begin. )\n";

                /* if we're cutting, perhaps do it in multiple steps, but do isolations just once.
                 * i know this is partially repetitive, but this way it's easier to read
                 */
                shared_ptr<Cutter> cutter = dynamic_pointer_cast<Cutter>(mill);

                if (cutter && cutter->do_steps)
                {

                    //--------------------------------------------------------------------
                    //cutting (outline)

                    const unsigned int steps_num = ceil(-mill->zwork / cutter->stepsize);

                    if( bBridges )
                        if( i == 0 && j == 0 )  //Compute the bridges only the 1st time
                            bridges = layer->get_bridges( path );

                    for (unsigned int i = 0; i < steps_num; i++)
                    {
                        const double z = mill->zwork / steps_num * (i + 1);

                        of << "G01 Z" << z * cfactor << " F" << mill->vertfeed * cfactor << " ( plunge. )\n";
                        of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";
                        of << "F" << mill->feed * cfactor << "\n";
                        of << "G01 ";

                        icoords::iterator iter = path->begin();
                        icoords::iterator last = path->end();      // initializing to quick & dirty sentinel value

                        if (bBridges)
                            currentBridge = bridges.begin();

                        while (iter != path->end())
                        {

                            of << "X" << ( iter->first - xoffsetTot ) * cfactor << " Y"
                               << ( iter->second - yoffsetTot ) * cfactor << '\n';

                            if (bBridges && currentBridge != bridges.end())
                            {
                                if (z < cutter->bridges_height)
                                {
                                    if (*currentBridge == iter - path->begin())
                                        of << "Z" << cutter->bridges_height * cfactor << '\n';
                                    else if (*currentBridge == last - path->begin())
                                    {
                                        of << "Z" << z * cfactor << " F" << cutter->vertfeed * cfactor << '\n';
                                        of << "F" << cutter->feed * cfactor << '\n';
                                        of << "G01 ";
                                    }
                                }

                                if (*currentBridge == last - path->begin())
                                    ++currentBridge;
                            }

                            last = iter;
                            ++iter;
                        }
                    }
                }
                else
                {
                    //--------------------------------------------------------------------
                    // isolating (front/backside)
                    of << "F" << mill->vertfeed * cfactor << '\n';

                    if( bAutolevelNow )
                    {
                        leveller->setLastChainPoint( icoordpair( ( path->begin()->first - xoffsetTot ) * cfactor,
                                                     ( path->begin()->second - yoffsetTot ) * cfactor ) );
                        of << leveller->g01Corrected( icoordpair( ( path->begin()->first - xoffsetTot ) * cfactor,
                                                      ( path->begin()->second - yoffsetTot ) * cfactor ) );
                    }
                    else
                        of << "G01 Z" << mill->zwork * cfactor << "\n";

                    of << "G04 P0 ( dwell for no time -- G64 should not smooth over this point )\n";
                    of << "F" << mill->feed * cfactor << '\n';

                    if (!bAutolevelNow)
                        of << "G01 ";

                    icoords::iterator iter = path->begin();

                    while (iter != path->end())
                    {
                        if( bAutolevelNow )
                            of << leveller->addChainPoint( icoordpair( ( iter->first - xoffsetTot ) * cfactor,
                                                                           ( iter->second - yoffsetTot ) * cfactor ) );
                        else
                            of << "X" << ( iter->first - xoffsetTot ) * cfactor << " Y"
                               << ( iter->second - yoffsetTot ) * cfactor << '\n';
                        ++iter;
                    }
                }
            }
        }
    }
    
    tiling.footer( of );

    if( bAutolevelNow )
    {
        leveller->footer( of );
    }

    of.close();
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
