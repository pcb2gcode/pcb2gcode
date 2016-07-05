/*
 * This file is part of pcb2gcode.
 * 
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

#ifndef AUTOLEVELLER_H
#define AUTOLEVELLER_H

//Fixed probe fail depth (in inches, string)
#define FIXED_FAIL_DEPTH_IN "-0.1"

//Fixed probe fail depth (in mm, string)
#define FIXED_FAIL_DEPTH_MM "-3"

#include <string>
using std::string;
using std::to_string;

#include <fstream>
using std::endl;

#include <vector>
using std::vector;

#include <memory>
using std::shared_ptr;

#include <boost/program_options.hpp>

#include "geometry.hpp"
#include "unique_codes.hpp"
#include "common.hpp"
#include "tile.hpp"

class autoleveller
{
public:
    // The constructor just initialize the common parameters variables (parameters are in inches)
    autoleveller( const boost::program_options::variables_map &options, uniqueCodes *ocodes, 
                  uniqueCodes *globalVars, double quantization_error, double xoffset, double yoffset,
                  const struct Tiling::TileInfo tileInfo );

    // prepareWorkarea computes the area of the milling project and computes the required number of probe
    // points; if it exceeds the maximum number of probe point it return false, otherwise it returns true
    // All the arguments must be in inches
    bool prepareWorkarea( vector<shared_ptr<icoords> > &toolpaths );

    // header prints in of the header required for the probing (subroutines and probe calls for LinuxCNC,
    // only the probe calls for the other softwares)
    void header( std::ofstream &of );

    // setMillingParameters sets the milling parameters
    void setMillingParameters ( double zwork, double zsafe, int feedrate );

    // autoleveller doesn't just interpolate a point, it also checks that the distance between the
    // previous point and the new point is not too high. If the distance is too high, it creates the
    // required number of points between the previous and the current point and it interpolates them too.
    // This function adds a new chain point. Always call setLastChainPoint before starting a new chain
    // (call it also for the 1st chain)
    string addChainPoint ( icoordpair point );

    // g01Corrected interpolates only one point (without adding it to the chain), and it prints a G01 to that
    // position
    string g01Corrected ( icoordpair point );

    // Set lastPoint as the last chain point. You can use this function when you want to start a new chain
    inline void setLastChainPoint ( icoordpair lastPoint )
    {
        this->lastPoint = lastPoint;
    }

    // This function returns the maximum number of probe points
    inline unsigned int maxProbePoints()
    {
        return software == LINUXCNC ? 4501 : 500;
    }

    // This function returns the required number of probe points
    inline unsigned int requiredProbePoints()
    {
        return numXPoints * numYPoints;
    }

    // Since Mach3/4 require the subroutine body to be written at the end of the file, footer writes them
    // if software != LinuxCNC
    inline void footer( std::ofstream &of )
    {
        if( software != LINUXCNC )
            footerNoIf( of );
    }

    // Some parameters are just placed as string in the output, saving them as strings saves a lot of
    // string to double conversion. These parameters are probably not useful for the user, but we can
    // safely define them as public as they are const
    const double unitconv;
    const double cfactor;
    const string probeCodeCustom;
    const string zProbeResultVarCustom;
    const string setZZeroCustom;
    const double XProbeDistRequired;
    const double YProbeDistRequired;
    const string zwork;
    const string zprobe;
    const string zsafe;
    const string zfail;
    const string feedrate;
    const string probeOn;
    const string probeOff;
    const Software software;
    const double quantization_error;
    const double xoffset;
    const double yoffset;

    //Number of the g01 interpolated macro
    const unsigned int g01InterpolatedNum;
    
    //Number of the X/Y probe subroutines
    const unsigned int yProbeNum;
    const unsigned int xProbeNum;
    
    // Global variables
    const string returnVar;
    const string globalVar0;
    const string globalVar1;
    const string globalVar2;
    const string globalVar3;
    const string globalVar4;
    const string globalVar5;
    
    const struct Tiling::TileInfo tileInfo;
    const unsigned int initialXOffsetVar;
    const unsigned int initialYOffsetVar;

    static const string callSubRepeat[];
    static const string probeCode[];
    static const string zProbeResultVar[];
    static const string setZZero[];

protected:
    double startPointX;
    double startPointY;
    unsigned int numXPoints;
    unsigned int numYPoints;
    double XProbeDist;
    double YProbeDist;
    double averageProbeDist;
    uniqueCodes *ocodes;

    string callSub2[3];

    icoordpair lastPoint;

    //computeWorkarea computes the occupied rectangule of toolpaths
    box_type_fp computeWorkarea( vector<shared_ptr<icoords> > &toolpaths );

    // footerNoIf prints the footer, regardless of the software
    void footerNoIf( std::ofstream &of );

    // getVarName returns the string containing the variable name associated with the probe point with
    // the indexes i and j
    string getVarName( int i, int j );

    // interpolatePoint finds the correct 4 probed points and computes a bilinear interpolation of point.
    // The result of the interpolation is saved in the parameter number RESULT_VAR
    string interpolatePoint ( icoordpair point );

    // numOfSubsegments returns the right number of subsegments in order to approximate the line in the
    // best way possible
    unsigned int numOfSubsegments ( icoordpair point );

    // splitSegment splits the segment between lastPoint and point in n subsegments, and returns the
    // icoords (aka vector<icoordpair>) containing them
    icoords splitSegment ( const icoordpair point, const unsigned int n );
};

#endif // AUTOLEVELLER_H
