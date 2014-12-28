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

#include "autoleveller.hpp"

#include <sstream>
#include <cmath>
#include <boost/algorithm/string/replace.hpp>

/******************************************************************************/
/*	
 *  Constructor, throws an exception if the number of required variables (probe
 *  points) exceeds the maximum number of variables (unlimited for linuxCNC)
 */
/******************************************************************************/
autoleveller::autoleveller(double xmin, double ymin, double xmax, double ymax, double XProbeDist, double YProbeDist, Software software) :
 boardLenX( xmax - xmin ),
 boardLenY( ymax - ymin ),
 startPoint( xycoord(xmin, ymin) ),
 numXPoints( round ( boardLenX / XProbeDist ) > 1 ? round ( boardLenX / XProbeDist ) + 1 : 2 ),		//We need at least 2 probe points 
 numYPoints( round ( boardLenY / YProbeDist ) > 1 ? round ( boardLenY / YProbeDist ) + 1 : 2 ),
 XProbeDist( boardLenX / ( numXPoints - 1 ) ),
 YProbeDist( boardLenY / ( numYPoints - 1 ) ),
 software( software ),
 startNewChain( true )
{
	if( numXPoints * numYPoints > 500 )
		throw autoleveller_exception();
}

/******************************************************************************/
/*
 *  Returns the ij-th variable name (based on software)
 */
/******************************************************************************/
string autoleveller::getVarName( unsigned int i, unsigned int j ) {
	std::stringstream ss;
#ifdef AUTOLEVELLER_NAMED_PARAMETERS
	if ( software == MACH3 || software == TURBOCNC )
		ss << '#' << i * numYPoints + j + 500;	//getVarName(10,8) returns (numYPoints=10) #180
	else
		ss << "#<_" << i << '_' << j << '>';	//getVarName(10,8) returns #<_10_8>	
#else		
	ss << '#' << i * numYPoints + j + 500;	
#endif

	return ss.str();
}

/******************************************************************************/
/*
 *  Generate the gcode probe header
 */
/******************************************************************************/
void autoleveller::probeHeader( std::ofstream &of, double zprobe, double zsafe, double zfail, int feedrate, std::string probeOn, std::string probeOff ) {
	const char *probeCode[] = { "G38.2", "G31", "G31" };
	const char *setZero[] = { "G10 L20 P0 Z0", "G92 Z0", "G92 Z0" };
	const char *zProbeResultVar[] = { "#5063", "#2002", "#2002" };
	const char *logFileOpenAndComment[] =
	 { "(PROBEOPEN RawProbeLog.txt) ( Record all probes in RawProbeLog.txt )",
	   "M40 (Begins a probe log file, when the window appears, enter a name for the log file such as \"RawProbeLog.txt\")",
	   "( No probe log function available in turboCNC )" };
	const char *logFileClose[] = { "(PROBECLOSE)" , "M41", "( No probe log function available in turboCNC )" };
	const char *parameterForm[] = { "parameter in the form #<_xprobenumber_yprobenumber>", "numbered parameter", "numbered parameter" };

	int i;
	int j = 0;
	int incr_decr = 1;

	boost::replace_all(probeOn, "$", "\n");
	boost::replace_all(probeOff, "$", "\n");

	if( !probeOn.empty() )
		of << probeOn << endl;
	of << "G0 Z" << zsafe << " ( Move Z to safe height )"<< endl;
	of << "G0 X" << startPoint.first << " Y" << startPoint.second << " ( Move XY to start point )" << endl;
	of << "G0 Z" << zprobe << " ( Move Z to probe height )" << endl;
	of << probeCode[software] << " Z" << zfail << " F" << feedrate << " ( Z-probe )" << endl;
	of << setZero[software] << " ( Set the current Z as zero-value )" << endl;
	of << "G0 Z" << zprobe << " ( Move Z to probe height )" << endl;
	of << probeCode[software] << " Z" << zfail << " F" << feedrate / 2 << " ( Z-probe again, half speed )" << endl;
	of << setZero[software] << " ( Set the current Z as zero-value )" << endl;
	of << logFileOpenAndComment[software] << endl;
	of << endl;
	of << "( We now start the real probing: move the Z axis to the probing height, move to )" << endl;
	of << "( the probing XY position, probe it and save the result, parameter " << zProbeResultVar[software] << ", )" << endl;
	of << "( in a " << parameterForm[software] << " )" << endl;
	of << "( We will make " << numXPoints << " probes on the X-axis and " << numYPoints << " probes on the Y-axis, )" << endl;
	of << "( for a total of " << numXPoints * numYPoints << " probes )" << endl;
	of << endl;

	for( int i = 0; i < numXPoints; i++ ) {
		while( j >= 0 && j < numYPoints ) {
			of << "G0 Z" << zprobe << endl;			//Move Z to probe height
			of << "X" << i * XProbeDist + startPoint.first << " Y" << j * YProbeDist + startPoint.second << endl;		//Move to the XY coordinate
			of << probeCode[software] << " Z" << zfail << " F" << feedrate << endl;	//Z-probe
			of << getVarName(i, j) << "=" << zProbeResultVar[software] << endl;	//Save the Z-value

			j += incr_decr ;
		}
		incr_decr *= -1;
		j += incr_decr ;
	}

	of << "G0 Z" << zsafe << " ( Move Z to safe height )"<< endl;
	of << logFileClose[software] << " ( Close the probe log file )" << endl;
	of << "( Probing has ended, each Z-coordinate will be corrected with a bilinear interpolation )" << endl;
	if( probeOff.empty() )
		of << "M0 ( Remove the probe tool )" << endl;
	else
		of << probeOff << endl;
	of << endl;
}

/******************************************************************************/
/*
 *  This function spits each straight line into smaller pieces. Each segment is
 *  between one X or Y intersection with the original straight line. This splitting
 *  permit a good linear approximation of the milling surface
 */
/******************************************************************************/
vector< pair <autoleveller::xycoord, autoleveller::Axis> > autoleveller::splitLine ( autoleveller::xycoord startPoint, autoleveller::xycoord endPoint ) {
	
	vector< pair <xycoord, Axis> > splittedLine;
	vector<xycoord> xpoints;
	vector<xycoord> ypoints;		
	double xdistancesquare;
	double ydistancesquare;
	double inclination;
	double current;
	
	//Intersections with the X-axes
	if( endPoint.second != startPoint.second )	//Don't do if it is horizontal
	{
		inclination = ( endPoint.first - startPoint.first ) / ( endPoint.second - startPoint.second );

		if( startPoint.second < endPoint.second )
			for( current = ceil( startPoint.second / YProbeDist ) * YProbeDist; current < endPoint.second; current += YProbeDist )
				xpoints.push_back( xycoord( startPoint.first + ( current - startPoint.second ) * inclination, current ) );
		else
			for( current = floor( startPoint.first / YProbeDist ) * YProbeDist; current > endPoint.first; current -= YProbeDist )
				xpoints.push_back( xycoord( startPoint.first + ( current - startPoint.second ) * inclination, current ) );
	}
	
	//Intersections with the Y-axes
	if( endPoint.first != startPoint.first )	//Don't do if it is vertical
	{
		inclination = ( endPoint.second - startPoint.second ) / ( endPoint.first - startPoint.first );
	
		if( startPoint.first < endPoint.first )
			for( current = ceil( startPoint.first / XProbeDist ) * XProbeDist; current < endPoint.first; current += XProbeDist )
				ypoints.push_back( xycoord( current, startPoint.second + ( current - startPoint.first ) * inclination ) );
		else
			for( current = floor( startPoint.first / XProbeDist ) * XProbeDist; current > endPoint.first; current -= XProbeDist )
				ypoints.push_back( xycoord( current, startPoint.second + ( current - startPoint.first ) * inclination ) );
	}
	
	//We already know which vector capacity will be required, therefore we reserve now that space in order to prevent
	//useless reallocations
	splittedLine.reserve ( xpoints.size() + ypoints.size() + 2 );
	
	splittedLine.push_back( pair<xycoord,Axis>( startPoint, NOAXIS ) );
	
	//Now we merge each vector, keeping them ordered. We use the Manhattan distance because it is easier to compute than the
	//Euclidean distance and, for our needs, it doesn't have any drawback
	mergeLinePoints( xpoints, ypoints, splittedLine );
	
	splittedLine.push_back( pair<xycoord,Axis>( endPoint, NOAXIS ) );
	
	return splittedLine;
}

/******************************************************************************/
/*
 *  Merge two ordered vectors into an ordered vector. The result vector must have 
 *  at least one element. The first element of result is used as "base point" 
 *  for the distance computation
 */
/******************************************************************************/
void autoleveller::mergeLinePoints (vector<autoleveller::xycoord> vector1, vector<autoleveller::xycoord> vector2, vector< pair <autoleveller::xycoord, autoleveller::Axis> > &result)
{
	vector<xycoord>::iterator iter1 = vector1.begin();
	vector<xycoord>::iterator iter2 = vector2.begin();	
	vector< pair <xycoord, Axis> >::iterator resultIter = result.end();

	while (true) {
		
		if ( iter1 == vector1.end() ) {
			while ( iter2 <= vector2.end() && vector2.size() != 0 )
				result.push_back ( pair<xycoord,Axis>( *iter2++, YAXIS ) );
			return;	
		}
		
		if ( iter2 == vector2.end() && vector1.size() != 0 ) {
			while ( iter1 <= vector1.end() )
				result.push_back ( pair<xycoord,Axis>( *iter1++, XAXIS ) );		
			return;
		}
		
		if ( manhattanDistance ( *iter2, result[0].first ) < manhattanDistance ( *iter1, result[0].first ) )	//TODO: don't merge points too close
			*resultIter++ = pair<xycoord,Axis>( *iter2++, YAXIS );
		else
			*resultIter++ = pair<xycoord,Axis>( *iter1++, XAXIS );
	}
}

double autoleveller::manhattanDistance (autoleveller::xycoord a, autoleveller::xycoord b) {
	return abs( a.first - b.first ) + abs( a.second - b.second );
}

string autoleveller::gcodeInterpolateAlignedPoint ( pair <autoleveller::xycoord, autoleveller::Axis> point, unsigned int variableNum ) {
	std::stringstream output;
	int pointbeforeindex;
	int pointafterindex;
	int alignedindex;
	double x_minus_x0;
	
	switch ( point.second ) {
		case XAXIS:
			pointbeforeindex = floor ( ( point.first.first - startPoint.first ) / XProbeDist );
			pointafterindex = ceil ( ( point.first.first - startPoint.first ) / XProbeDist );
			alignedindex = round ( ( point.first.second - startPoint.second ) / YProbeDist );
			
			x_minus_x0 = ( point.first.first - startPoint.first ) - ( pointbeforeindex * XProbeDist );
			
			output << '#' << variableNum << '=' << getVarName( pointbeforeindex, alignedindex ) << "+[" << 
						getVarName( pointafterindex, alignedindex ) << '-' << getVarName( pointbeforeindex, alignedindex ) << 
						"]*" << x_minus_x0 / XProbeDist << endl;			
			break;
			
		case YAXIS:
			pointbeforeindex = floor ( ( point.first.second - startPoint.second ) / YProbeDist );
			pointafterindex = ceil ( ( point.first.second - startPoint.second ) / YProbeDist );
			alignedindex = round ( ( point.first.first - startPoint.first ) / XProbeDist );
			
			x_minus_x0 = ( point.first.second - startPoint.second ) - ( pointbeforeindex * YProbeDist );
			
			output << '#' << variableNum << '=' << getVarName( alignedindex, pointbeforeindex ) << "+[" << 
						getVarName( alignedindex, pointafterindex ) << '-' << getVarName( alignedindex, pointbeforeindex ) << 
						"]*" << x_minus_x0 / YProbeDist << endl;			
			break;
	}
	
	return output.str();
}

string autoleveller::gcodeInterpolateGenericPoint ( autoleveller::xycoord point, unsigned int variableNum ) {
	string output;
	std::stringstream bilinear;
	xycoord temp;
	
	temp.first = ceil ( ( point.first - startPoint.first ) / XProbeDist ) * XProbeDist + startPoint.first;
	temp.second = point.second;
	output += gcodeInterpolateAlignedPoint( pair<xycoord,Axis> ( temp, YAXIS ), variableNum + 2 );
	
	temp.first -= XProbeDist;
	output += gcodeInterpolateAlignedPoint( pair<xycoord,Axis> ( temp, YAXIS ), variableNum + 1 );
	
	bilinear << '#' << variableNum << "=#" << variableNum + 1 << "+[#" << variableNum + 2 << "-#" <<
						variableNum + 1 << "]*" << ( point.first - temp.first ) / XProbeDist << endl;
	
	output += bilinear.str();
	
	return output;
}

void autoleveller::correctLine( std::ofstream &of, autoleveller::xycoord startPoint, autoleveller::xycoord endPoint ) {	//FIXME
	
	vector< pair <xycoord, Axis> > splittedLine = splitLine ( startPoint, endPoint );
	vector< pair <xycoord, Axis> >::iterator i;
	
	//of << gcodeInterpolateGenericPoint( splittedLine.begin()->first, 1 );
	//of << "X" << i->first.first << " Y" << i->first.second << " Z[" << zdepth << "+#1]" << endl;

	//for( i = splittedLine.begin() + 1 ; i < splittedLine.end(); i++ ) {	
	//	of << gcodeInterpolateAlignedPoint( *i, 1 );
	//	of << "X" << i->first.first << " Y" << i->first.second << " Z[" << zdepth << "+#1]" << endl;
	//}
	 i = splittedLine.begin() + 1; //DELETEME
	//of << gcodeInterpolateGenericPoint( splittedLine.end()->first, 1 );
	of << gcodeInterpolateGenericPoint( i->first, 1 ); //deleteme
	of << "X" << i->first.first << " Y" << i->first.second << " Z[" << zdepth << "+#1]" << endl;
	
}

void autoleveller::setMillingParameters ( double zdepth, double zsafe, int feedrate ) {
	this->zdepth = zdepth;
	this->zsafe = zsafe;
	this->feedrate = feedrate;
}

void autoleveller::addChainPoint ( std::ofstream &of, double x, double y ) {
	xycoord newPoint (x, y);

	if (startNewChain) {
		startNewChain = false;
		of << gcodeInterpolateGenericPoint( newPoint, 1 );
		of << "X" << x << " Y" << y << " Z[" << zdepth << "+#1]" << endl;
	}
	else
		correctLine( of, lastChainPoint, newPoint );	
	
	lastChainPoint = newPoint;
}
