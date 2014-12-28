/*!\defgroup AUTOLEVELLER*/
/******************************************************************************/
/*!
 \file       autoleveller.hpp
 \brief
 Autoleveller functions. Idea and hints taken from the output gcode of
 http://www.autoleveller.co.uk/ by James Hawthorne PhD

 \version
 18.12.2014 - Nicola Corna - nicola@corna.info\n
 
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

 \ingroup    AUTOLEVELLER
 */
/******************************************************************************/

#ifndef AUTOLEVELLER_H
#define AUTOLEVELLER_H

// Define this if you want named parameters (only in linuxcnc, easier debug)
#define AUTOLEVELLER_NAMED_PARAMETERS

#include <string>
using std::string;

#include <fstream>
using std::endl;

#include <vector>
using std::vector;
using std::pair;

#include <boost/exception/all.hpp>
class autoleveller_exception: virtual std::exception, virtual boost::exception {
};

/******************************************************************************/
/*
 */
/******************************************************************************/
class autoleveller {
protected: typedef pair<double,double> xycoord;
public:
	enum Software { LINUXCNC = 0, MACH3 = 1, TURBOCNC = 2 };

	autoleveller( double xmin, double ymin, double xmax, double ymax, double XProbeDist, double YProbeDist, Software software );
	void probeHeader( std::ofstream &of, double zprobe, double zsafe, double zfail, int feedrate, std::string probeOn = "", std::string probeOff = "" );
	void setMillingParameters ( double zdepth, double zsafe, int feedrate );
	
	void addChainPoint ( std::ofstream &of, double x, double y );
	inline void newChain () {
		startNewChain = true;
	}
	
	const double boardLenX;
	const double boardLenY;
	const xycoord startPoint;
	const unsigned int numXPoints;
	const unsigned int numYPoints;
	const double XProbeDist;
	const double YProbeDist;
	const Software software;

protected:
	enum Axis { XAXIS, YAXIS, NOAXIS };

	double zdepth;
	double zsafe;
	int feedrate;
	double cfactor;

	xycoord lastChainPoint;
	bool startNewChain;

	vector< pair <xycoord, Axis> > splitLine ( xycoord startPoint, xycoord endPoint );
	string gcodeInterpolateAlignedPoint ( pair <xycoord, Axis> point, unsigned int variableNum );	
	string gcodeInterpolateGenericPoint ( xycoord point, unsigned int variableNum );	
	string getVarName( unsigned int i, unsigned int j );
	void correctLine( std::ofstream &of, xycoord startPoint, xycoord endPoint );

	static void mergeLinePoints (vector<xycoord> vector1, vector<xycoord> vector2, vector< pair <xycoord, Axis> > &result);
	static double manhattanDistance (xycoord a, xycoord b);
};

#endif // NGCEXPORTER_H
