/*!\defgroup OPTIONS*/
/******************************************************************************/
/*!
 \file       options.cpp
 \brief

  \version
 09.01.2015 - Nicola Corna - nicola@corna.info\n
 - Added zero-start option
 - Added noconfigfile option
  
 04.12.2014 - Nicola Corna - nicola@corna.info\n
 - added preamble text option
 
 \version
 20.11.2014 - Nicola Corna - nicola@corna.info\n
 - added bridge height option

 \version
 19.12.2013 - Erik Schuster - erik@muenchen-ist-toll.de\n
 - added options "bridges" and "optimise".

 \version
 2013 - Erik Schuster - erik@muenchen-ist-toll.de\n
 - Added a check and warning if a drill file is given, but no board file or the
 absolute-mirror option.
 - Added cut-front option.
 - Added metricoutput option.
 - Added g64 option.
 - Added onedrill option.
 - Prepared documenting the code with doxygen.
 - Formatted the code.

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

 \ingroup    OPTIONS
 */
/******************************************************************************/

#include "options.hpp"
#include "config.h"

#include <fstream>
#include <list>
#include <boost/foreach.hpp>
#include <boost/exception/all.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>
using std::cerr;
using std::endl;

/******************************************************************************/
/*
 */
/******************************************************************************/
options&
options::instance() {
   static options singleton;
   return singleton;
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void options::parse(int argc, char** argv) {
   // guessing causes problems when one option is the start of another
   // (--drill, --drill-diameter); see bug 3089930
   int style = po::command_line_style::default_style
               & ~po::command_line_style::allow_guessing;

   po::options_description generic;
   generic.add(instance().cli_options).add(instance().cfg_options);

   try {
      po::store(po::parse_command_line(argc, argv, generic, style),
                instance().vm);
   } catch (std::logic_error& e) {
      cerr << "Error: You've supplied an invalid parameter.\n"
           << "Note that spindle speeds are integers!\n" << "Details: "
           << e.what() << endl;
      exit(ERR_UNKNOWNPARAMETER);
   }

   po::notify(instance().vm);

   if( !instance().vm["noconfigfile"].as<bool>() )
	   parse_files();

   /*
    * this needs to be an extra step, as --basename modifies the default
    * values of the --...-output parameters
    */
   string basename = "";

   if (instance().vm.count("basename")) {
      basename = instance().vm["basename"].as<string>() + "_";
   }

   string front_output = "--front-output=" + basename + "front.ngc";
   string back_output = "--back-output=" + basename + "back.ngc";
   string outline_output = "--outline-output=" + basename + "outline.ngc";
   string drill_output = "--drill-output=" + basename + "drill.ngc";

   const char *fake_basename_command_line[] = { "", front_output.c_str(),
                                                back_output.c_str(),
                                                outline_output.c_str(),
                                                drill_output.c_str() };

   po::store(
            po::parse_command_line(5, (char**) fake_basename_command_line,
                                   generic, style),
            instance().vm);
   po::notify(instance().vm);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
string options::help() {
   std::stringstream msg;
   msg << PACKAGE_STRING << "\n\n";
   msg << instance().cli_options << instance().cfg_options;
   return msg.str();
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void options::parse_files() {

   std::string file("millproject");

   try {
      std::ifstream stream;

      try {
         stream.open(file.c_str());
         po::store(po::parse_config_file(stream, instance().cfg_options),
                   instance().vm);
      } catch (std::exception& e) {
         cerr << "Error parsing configuration file \"" << file << "\": "
              << e.what() << endl;
      }

      stream.close();
   } catch (std::exception& e) {
      cerr << "Error reading configuration file \"" << file << "\": "
           << e.what() << endl;
   }

   po::notify(instance().vm);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
options::options()
         : cli_options("command line only options"), cfg_options("generic options (CLI and config files)") {

   cli_options.add_options()(
			"noconfigfile", po::value<bool>()->default_value(false)->zero_tokens()->implicit_value(true), "ignore any configuration file")(
			"help,?", "produce help message")(
			"version", "show the current software version");

   cfg_options.add_options()(
			"front", po::value<string>(),"front side RS274-X .gbr")(
            "back", po::value<string>(), "back side RS274-X .gbr")(
            "outline", po::value<string>(), "pcb outline polygon RS274-X .gbr")(
            "drill", po::value<string>(), "Excellon drill file")(
            "svg", po::value<string>(), "SVG output file. EXPERIMENTAL")(
            "zwork", po::value<double>(),
            "milling depth in inches (Z-coordinate while engraving)")(
            "zsafe", po::value<double>(), "safety height (Z-coordinate during rapid moves)")(
            "offset", po::value<double>(), "distance between the PCB traces and the end mill path in inches; usually half the isolation width")(
            "mill-feed", po::value<double>(), "feed while isolating in [i/m] or [mm/m]")(
            "mill-speed", po::value<int>(), "spindle rpm when milling")(
            "milldrill", po::value<bool>()->default_value(false)->zero_tokens()->implicit_value(true), "drill using the mill head")(
            "extra-passes", po::value<int>()->default_value(0), "specify the the number of extra isolation passes, increasing the isolation width half the tool diameter with each pass")(
            "fill-outline", po::value<bool>()->default_value(false)->zero_tokens()->implicit_value(true), "accept a contour instead of a polygon as outline (you likely want to enable this one)")(
            "outline-width", po::value<double>(), "width of the outline")(
            "cutter-diameter", po::value<double>(), "diameter of the end mill used for cutting out the PCB")(
            "zcut", po::value<double>(), "PCB cutting depth in inches")(
            "cut-feed", po::value<double>(), "PCB cutting feed in [i/m] or [mm/m]")(
			"cut-speed", po::value<int>(), "spindle rpm when cutting")(
            "cut-infeed", po::value<double>(), "maximum cutting depth; PCB may be cut in multiple passes")(
            "cut-front", po::value<bool>()->default_value(false)->zero_tokens()->implicit_value(true), "cut from front side. Default is back side.")(
			"zdrill", po::value<double>(), "drill depth")(
            "zchange", po::value<double>(), "tool changing height")(
            "drill-feed", po::value<double>(), "drill feed in [i/m] or [mm/m]")(
            "drill-speed", po::value<int>(), "spindle rpm when drilling")(
            "drill-front", po::value<bool>()->default_value(false)->zero_tokens()->implicit_value(true), "drill through the front side of board")(
            "onedrill", po::value<bool>()->default_value(false)->zero_tokens()->implicit_value(true), "use only one drill bit size")(
            "metric", po::value<bool>()->default_value(false)->zero_tokens()->implicit_value(true), "use metric units for parameters. does not affect gcode output")(
            "metricoutput", po::value<bool>()->default_value(false)->zero_tokens()->implicit_value(true), "use metric units for output")(
            "optimise", po::value<bool>()->default_value(false)->zero_tokens()->implicit_value(true), "Reduce output file size by up to 40% while accepting a little loss of precision.")(
            "bridges", po::value<double>()->default_value(0), "add four bridges with the given width to the outline cut")(
            "zbridges", po::value<double>(), "bridges heigth (Z-coordinates while engraving bridges, default to zsafe) ")(
			"al-front", po::value<bool>()->default_value(false)->zero_tokens()->implicit_value(true),
            "enable the z autoleveller for the front layer")(
			"al-back", po::value<bool>()->default_value(false)->zero_tokens()->implicit_value(true),
            "enable the z autoleveller for the back layer")(
			"software", po::value<string>(), "choose the destination software (useful only with the autoleveller). Supported softwares are linuxcnc, mach3, mach4 and turbocnc")(
			"al-x", po::value<double>(), "width of the x probes")( 
			"al-y", po::value<double>(), "width of the y probes")(           
			"al-probefeed", po::value<double>(), "speed during the probing")(
			"al-probe-on", po::value<string>()->default_value("(MSG, attach the probe tool)$M0 ( Temporary machine stop. )"), "execute this commands to enable the probe tool (default is M0)")(
			"al-probe-off", po::value<string>()->default_value("(MSG, detach the probe tool)$M0 ( Temporary machine stop. )"), "execute this commands to disable the probe tool (default is M0)")(
            "dpi", po::value<int>()->default_value(1000), "virtual photoplot resolution")(
            "zero-start", po::value<bool>()->default_value(false)->zero_tokens()->implicit_value(true), "set the starting point of the project at (0,0)")(
            "g64", po::value<double>()->default_value(1), "maximum deviation from toolpath, overrides internal calculation")(
            "mirror-absolute", po::value<bool>()->default_value(false)->zero_tokens()->implicit_value(true), "mirror back side along absolute zero instead of board center\n")(
            "output-dir", po::value<string>()->default_value(""), "output directory")(
            "basename", po::value<string>(), "prefix for default output file names")(
            "front-output", po::value<string>()->default_value("front.ngc"), "output file for front layer")(
            "back-output", po::value<string>()->default_value("back.ngc"), "output file for back layer")(
            "outline-output", po::value<string>()->default_value("outline.ngc"), "output file for outline")(
            "drill-output", po::value<string>()->default_value("drill.ngc"), "output file for drilling")(
            "preamble-text", po::value<string>(), "preamble text file, inserted at the very beginning as a comment.")(
            "preamble", po::value<string>(), "gcode preamble file, inserted at the very beginning.")(
            "postamble", po::value<string>(), "gcode postamble file, inserted before M9 and M2.");
}

/******************************************************************************/
/*
 */
/******************************************************************************/
static void check_generic_parameters(po::variables_map const& vm) {

   //---------------------------------------------------------------------------
   //Check dpi parameter:

   if (vm["dpi"].as<int>() < 100) {
      cerr << "Warning: very low DPI value." << endl;
   } else
      if (vm["dpi"].as<int>() > 10000) {
         cerr << "Warning: very high DPI value, processing may take extremely long"
              << endl;
      }

   //---------------------------------------------------------------------------
   //Check g64 parameter:

   double g64th;      //!< Upper threshold value of g64 parameter for warning

   if (vm.count("g64")) {                  //g64 parameter is given
      if (vm["metric"].as<bool>()) {       //metric input
         g64th = 0.1;                      //set threshold value
      } else {                             //imperial input
         g64th = 0.003937008;              //set threshold value
      }

      if (vm["g64"].as<double>() > g64th)
         cerr << "Warning: high G64 value (allowed deviation from toolpath) given.\n"
              << endl;

      if (vm["g64"].as<double>() == 0)
         cerr << "Warning: Deviation from commanded toolpath set to 0 (g64=0). No smooth milling is most likely!\n"
              << endl;
   }

   //---------------------------------------------------------------------------
   //Check for available board dimensions:

   if (vm.count("drill")
       && !(vm.count("front") || vm.count("back") || vm.count("outline"))
       && !vm.count("mirror-absolute")) {
      cerr << "Warning: Board dimensions unknown. Gcode for drilling probably misaligned on Y-axis.\n";
   }

   //---------------------------------------------------------------------------
   //Check for safety height parameter:

   if (!vm.count("zsafe")) {
      cerr << "Error: Safety height not specified.\n";
      exit(ERR_NOZSAFE);
   }

   //---------------------------------------------------------------------------
   //Check for zchange parameter parameter:

   if (!vm.count("zchange")) {
      cerr << "Error: Tool changing height not specified.\n";
      exit(ERR_NOZCHANGE);
   }
   
   //---------------------------------------------------------------------------
   //Check for autoleveller parameters

   if (vm["al-front"].as<bool>() || vm["al-back"].as<bool>()) {
   	  if (!vm.count("software") || 
   	  		( boost::iequals( vm["software"].as<string>(), "linuxcnc" ) &&	//boost::iequals is case insensitive
   	  		  boost::iequals( vm["software"].as<string>(), "mach3" ) &&
   	  		  boost::iequals( vm["software"].as<string>(), "mach4" ) &&
   	  		  boost::iequals( vm["software"].as<string>(), "turbocnc" ) ) ) {
         cerr << "Error: Unknown software, please specify a software (linuxcnc, mach3, mach4 or turbocnc).\n";
      	 exit(ERR_NOSOFTWARE);
      }
      
      if (!vm.count("al-x")) {
      	 cerr << "Error: autoleveller probe width x not specified.\n";
         exit(ERR_NOALX);
      }
       
      if (!vm.count("al-y")) {
      	 cerr << "Error: autoleveller probe width y not specified.\n";
         exit(ERR_NOALY);
      }
      
      if (!vm.count("al-probefeed")) {
      	 cerr << "Error: autoleveller probe feed rate not specified.\n";
         exit(ERR_NOALPROBEFEED);
      }
   }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
static void check_milling_parameters(po::variables_map const& vm) {

   if (vm.count("front") || vm.count("back")) {

      if (!vm.count("zwork")) {
         cerr << "Error: --zwork not specified.\n";
         exit(ERR_NOZWORK);
      } else
         if (vm["zwork"].as<double>() > 0) {
            cerr << "Warning: Engraving depth (--zwork) is greater than zero!\n";
         }

      if (!vm.count("offset")) {
         cerr << "Error: Engraving --offset not specified.\n";
         exit(ERR_NOOFFSET);
      }

      if (!vm.count("mill-feed")) {
         cerr << "Error: Milling feed [ipm] not specified.\n";
         exit(ERR_NOMILLFEED);
      }

      if (!vm.count("mill-speed")) {
         cerr << "Error: Milling speed [rpm] not specified.\n";
         exit(ERR_NOMILLSPEED);
      }

      // required parameters present. check for validity.
      if (vm["zsafe"].as<double>() <= vm["zwork"].as<double>()) {
         cerr << "Error: The safety height --zsafe is lower than the milling "
              << "height --zwork. Are you sure this is correct?\n";
         exit(ERR_ZSAFELOWERZWORK);
      }

      if (vm["mill-feed"].as<double>() < 0) {
         cerr << "Error: Negative milling feed (--mill-feed).\n";
         exit(ERR_NEGATIVEMILLFEED);
      }

      if (vm["mill-speed"].as<int>() < 0) {
         cerr << "Error: --mill-speed < 0.\n";
         exit(ERR_NEGATIVEMILLSPEED);
      }
   }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
static void check_drilling_parameters(po::variables_map const& vm) {

   //only check the parameters if a drill file is given
   if (vm.count("drill")) {

      if (!vm.count("zdrill")) {
         cerr << "Error: Drilling depth (--zdrill) not specified.\n";
         exit(ERR_NOZDRILL);
      }

      if (vm["zsafe"].as<double>() <= vm["zdrill"].as<double>()) {
         cerr << "Error: The safety height --zsafe is lower than the drilling "
              << "height --zdrill!\n";
         exit(ERR_ZSAFELOWERZDRILL);
      }

      if (!vm.count("zchange")) {
         cerr << "Error: Drill bit changing height (--zchange) not specified.\n";
         exit(ERR_NOZCHANGE);
      } else
         if (vm["zchange"].as<double>() <= vm["zdrill"].as<double>()) {
            cerr << "Error: The safety height --zsafe is lower than the tool "
                 << "change height --zchange!\n";
            exit(ERR_ZSAFELOWERZCHANGE);
         }

      if (!vm.count("drill-feed")) {
         cerr << "Error:: Drilling feed (--drill-feed) not specified.\n";
         exit(ERR_NODRILLFEED);
      } else
         if (vm["drill-feed"].as<double>() <= 0) {
            cerr << "Error: The drilling feed --drill-feed is <= 0.\n";
            exit(ERR_NEGATIVEDRILLFEED);
         }

      if (!vm.count("drill-speed")) {
         cerr << "Error: Drilling spindle RPM (--drill-speed) not specified.\n";
         exit(ERR_NODRILLSPEED);
      } else
         if (vm["drill-speed"].as<int>() < 0) {      //no need to support both directions?
            cerr << "Error: --drill-speed < 0.\n";
            exit(ERR_NEGATIVEDRILLSPEED);
         }

   }

}

/******************************************************************************/
/*
 */
/******************************************************************************/
static void check_cutting_parameters(po::variables_map const& vm) {

   //only check the parameters if an outline file is given
   if (vm.count("outline")) {
      if ((vm.count("drill") || vm.count("milldrill"))) {
         if (vm["fill-outline"].as<bool>()) {
            if (!vm.count("outline-width")) {
               cerr << "Error: For outline filling, a width (--outline-width) has to be specified.\n";
               exit(ERR_NOOUTLINEWIDTH);
            } else {
               double outline_width = vm["outline-width"].as<double>();
               if (outline_width < 0) {
                  cerr << "Error: Specified outline width is less than zero!\n";
                  exit(ERR_NEGATIVEOUTLINEWIDTH);
               } else
                  if (outline_width == 0) {
                     cerr << "Error. Specified outline width is zero!\n";
                     exit(ERR_ZEROOUTLINEWIDTH);
                  } else {
                     std::stringstream width_sb;
                     if ((vm["metric"].as<bool>() && outline_width >= 10)
                         || (!vm["metric"].as<bool>() && outline_width >= 0.4)) {
                        width_sb << outline_width
                                 << (vm["metric"].as<bool>() ? " mm" : " inch");
                        cerr << "Warning: You specified an outline-width of "
                             << width_sb.str() << "!\n";
                     }
                  }
            }
         }
      }

      if (!vm.count("zcut")) {
         cerr << "Error: Board cutting depth (--zcut) not specified.\n";
         exit(ERR_NOZCUT);
      }

      if (!vm.count("cutter-diameter")) {
         cerr << "Error: Cutter diameter not specified.\n";
         exit(ERR_NOCUTTERDIAMETER);
      }

      if (!vm.count("cut-feed")) {
         cerr << "Error: Board cutting feed (--cut-feed) not specified.\n";
         exit(ERR_NOCUTFEED);
      }

      if (!vm.count("cut-speed")) {
         cerr << "Error: Board cutting spindle RPM (--cut-speed) not specified.\n";
         exit(ERR_NOCUTSPEED);
      }

      if (!vm.count("cut-infeed")) {
         cerr << "Error: Board cutting infeed (--cut-infeed) not specified.\n";
         exit(ERR_NOCUTINFEED);
      }

      if (vm["zsafe"].as<double>() <= vm["zcut"].as<double>()) {
         cerr << "Error: The safety height --zsafe is lower than the cutting "
              << "height --zcut!\n";
         exit(ERR_ZSAFELOWERZCUT);
      }

      if (vm["cut-feed"].as<double>() <= 0) {
         cerr << "Error: The cutting feed --cut-feed is <= 0.\n";
         exit(ERR_NEGATIVECUTFEED);
      }

      if (vm["cut-speed"].as<int>() < 0) {      //no need to support both directions?
         cerr << "Error: The cutting spindle speed --cut-speed is lower than 0.\n";
         exit(ERR_NEGATIVESPINDLESPEED);
      }

      if (vm["cut-infeed"].as<double>() < 0.001) {
         cerr << "Error: The cutting infeed --cut-infeed. seems too low.\n";
         exit(ERR_LOWCUTINFEED);
      }
   }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void options::check_parameters() {

   po::variables_map const& vm = instance().vm;

   try {
      check_generic_parameters(vm);
      check_milling_parameters(vm);
      check_cutting_parameters(vm);
      check_drilling_parameters(vm);
   } catch (std::runtime_error& re) {
      cerr << "Error: Invalid parameter. :-(\n";
      exit(ERR_INVALIDPARAMETER);
   }
}
