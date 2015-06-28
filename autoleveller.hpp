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

//Number of the bilinear interpolation macro
#define BILINEAR_INTERPOLATION_MACRO_NUMBER 5

//Number of the correction factor subroutine
#define CORRECTION_FACTOR_SUB_NUMBER 6

//Number of the g01 interpolated macro (just a shortcut for CORRECTION_FACTOR_SUB_NUMBER + G01 X Y)
#define G01_INTERPOLATED_MACRO_NUMBER 7

//Number of the Y probe subroutine
#define YPROBE_SUB_NUMBER 8

//Number of the X probe subroutine
#define XPROBE_SUB_NUMBER 9

//Number of the 1st repeat O-code (only for LinuxCNC)
#define REPEAT_CODE_1 10

//Number of the 2nd repeat O-code (only for LinuxCNC)
#define REPEAT_CODE_2 11

//Name of the variable where the interpolation result is saved (string)
#define RESULT_VAR "100"

//Name of the 1st usable global variable (string)
#define GLOB_VAR_0 "110"

//Name of the 2nd usable global variable (string)
#define GLOB_VAR_1 "111"

//Name of the 3rd usable global variable (string)
#define GLOB_VAR_2 "112"

//Name of the 4th usable global variable (string)
#define GLOB_VAR_3 "113"

//Name of the 5th usable global variable (string)
#define GLOB_VAR_4 "114"

//Name of the 6th usable global variable (string)
#define GLOB_VAR_5 "115"

//Fixed probe fail depth (in inches, string)
#define FIXED_FAIL_DEPTH_IN "-0.2"

//Fixed probe fail depth (in mm, string)
#define FIXED_FAIL_DEPTH_MM "-5"

#include <string>
using std::string;

#include <fstream>
using std::endl;

#include <vector>
using std::vector;

#include <boost/program_options.hpp>

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include "coord.hpp"

class autoleveller
{
public:
    // The constructor just initialize the common parameters variables (parameters be in inches)
    autoleveller( const boost::program_options::variables_map &options, double quantization_error, double xoffset, double yoffset );

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

    // g01Corrected interpolates only point (without adding it to the chain), and it prints a G01 to that
    // position
    string g01Corrected ( icoordpair point );

    // Returns a string containing the software name
    string getSoftware();

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

    // This enum contains the software codes. Note that the codes must start from 0 and be consecutive,
    // as they are used as array indexes
    enum Software { LINUXCNC = 0, MACH4 = 1, MACH3 = 2, CUSTOM = 3 };

    // Some parameters are just placed as string in the output, saving them as strings saves a lot of
    // string to double conversion. These parameters are probably not useful for the user, but we can
    // safely define them as public as they are const
    const double unitconv;
    const double cfactor;
    const double XProbeDistRequired;
    const double YProbeDistRequired;
    const string zwork;
    const string zprobe;
    const string zsafe;
    const string zfail;
    const string feedrate;
    const string feedrate2nd;
    const string probeOn;
    const string probeOff;
    const Software software;
    const double quantization_error;
    const double xoffset;
    const double yoffset;

protected:
    double startPointX;
    double startPointY;
    unsigned int numXPoints;
    unsigned int numYPoints;
    double XProbeDist;
    double YProbeDist;
    double averageProbeDist;

    static const char *callSub2[];
    static const char *callInterpolationMacro[];
    static const char *callSubRepeat[];
    string probeCode[4];
    string zProbeResultVar[4];
    string setZZero[4];

    icoordpair lastPoint;

    //computeWorkarea computes the occupied rectangule of toolpaths
    std::pair<icoordpair, icoordpair> computeWorkarea( vector<shared_ptr<icoords> > &toolpaths );

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
