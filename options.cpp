/*
 * This file is part of pcb2gcode.
 *
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
 * Copyright (C) 2010 Bernhard Kubicek <kubicek@gmx.at>
 * Copyright (C) 2013 Erik Schuster <erik@muenchen-ist-toll.de>
 * Copyright (C) 2014-2017 Nicola Corna <nicola@corna.info>
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

#include "options.hpp"
#include "config.h"

#include <fstream>
#include <list>
#include <boost/exception/all.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/variant.hpp>
#include "units.hpp"
#include "available_drills.hpp"

#include <string>
using std::string;
using std::to_string;

#include <iostream>
using std::cerr;
using std::endl;

/******************************************************************************/
/*
 */
/******************************************************************************/
options& options::instance() {
    static options singleton;
    return singleton;
}

void options::maybe_throw(const std::string& what, ErrorCodes error_code) {
  if (instance().vm["ignore-warnings"].as<bool>()) {
    cerr << "Ignoring error code " << error_code << ": " << what << endl;
  } else {
    throw pcb2gcode_parse_exception(what, error_code);
  }
}

/* Adjusts the processed varibles map in place to fix for deprecated variables,
 * etc.
 */
void fix_variables_map(po::variables_map& vm) {
  // Deal with the deprecated milldrill option.
  if (vm["min-milldrill-hole-diameter"].defaulted() && vm["milldrill"].as<bool>()) {
    vm.at("min-milldrill-hole-diameter").value() = Length(0);
  }
  // Deal with deprecated offset option.
  if (vm.count("offset") && vm.at("mill-diameters").defaulted()) {
    vm.at("mill-diameters").as<std::vector<CommaSeparated<Length>>>().clear();
    vm.at("mill-diameters").as<std::vector<CommaSeparated<Length>>>().push_back({vm["offset"].as<Length>()*2.0});
    vm.at("offset").value() = Length(0);
  }

  if (vm["bridgesnum"].as<unsigned int>() > 0 && vm["bridges"].as<Length>().asInch(1) <= 0) {
    vm.at("bridgesnum").value() = (unsigned int) 0;
  }
}

/* parse options, both command line and from the millproject file if it exists.
 * Throws on error.
 */
void options::parse(int argc, const char** argv) {
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
      throw pcb2gcode_parse_exception(std::string("Error: You've supplied an invalid parameter.\n"
                                                  "Details: ")
                                      + e.what(), ERR_UNKNOWNPARAMETER);
    }

    po::notify(instance().vm);

    if (!instance().vm["noconfigfile"].as<bool>()) {
      parse_files(
          flatten(
              instance().vm["config"].as<std::vector<CommaSeparated<string>>>()),
          instance().vm["config"].defaulted());
    }

    if (instance().vm.count("basename")) {
      /*
       * this needs to be an extra step, as --basename modifies the default
       * values of the --...-output parameters
       */
      string basename = "";
      basename = instance().vm["basename"].as<string>() + "_";
      instance().vm.at("front-output").value() = basename + "front.ngc";
      instance().vm.at("back-output").value() = basename + "back.ngc";
      instance().vm.at("drill-output").value() = basename + "drill.ngc";
      instance().vm.at("outline-output").value() = basename + "outline.ngc";
      instance().vm.at("milldrill-output").value() = basename + "milldrill.ngc";
    }

    if (instance().vm.count("tolerance")) {
      if (instance().vm.count("g64")) {
        maybe_throw("You can't specify both tolerance and g64!", ERR_BOTHTOLERANCEG64);
      }
    } else {
        const double cfactor = instance().vm["metric"].as<bool>() ? 25.4 : 1;
        double tolerance;

        if (instance().vm.count("g64"))
        {
            tolerance = instance().vm["g64"].as<double>();
        }
        else {
          tolerance = 0.0004 * cfactor;
        }

        string tolerance_str = "--tolerance=" + to_string(tolerance);

        const char *fake_tolerance_command_line[] = { "",
                                            tolerance_str.c_str() };

        po::store(po::parse_command_line(2,
                        (char**) fake_tolerance_command_line,
                        generic, style), instance().vm);
    }
    fix_variables_map(instance().vm);

    po::notify(instance().vm);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
string options::help()
{
    std::stringstream msg;
    msg << PACKAGE_STRING << "\n\n";
    msg << instance().cli_options << instance().cfg_options;
    return msg.str();
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void options::parse_files(const std::vector<std::string>& config_files,
                          const bool defaulted) {
  // We do it in serve order so that the latter config file in the
  // list overrides the former.
  for (const auto& file : boost::adaptors::reverse(config_files)) {
    try {
      std::ifstream stream(file);
      if (!defaulted && stream.fail()) {
        // If the user explicitly specified the config files to use
        // then we will throw an error if any of those files are not
        // found.
        maybe_throw("Missing configuration file \"" + file + "\"",
                    ERR_INVALIDPARAMETER);
      }
      po::store(po::parse_config_file(stream, instance().cfg_options),
                instance().vm);
    } catch (std::exception& e) {
      maybe_throw("Error parsing configuration file \"" + file + "\": " +
                  e.what(), ERR_INVALIDPARAMETER);
    }
  }
  po::notify(instance().vm);
}

/******************************************************************************/
/*
 */
/******************************************************************************/
options::options()
         : cli_options("command line only options"), cfg_options("Generic options (CLI and config files)") {

   cli_options.add_options()
       ("noconfigfile", po::value<bool>()->default_value(false)->implicit_value(true), "ignore any configuration file")
       ("config",
        po::value<std::vector<CommaSeparated<string>>>()->
        default_value(std::vector<CommaSeparated<string>>{{"millproject"}})->multitoken(),
        "list of comma-separated config files")
       ("help,?", "produce help message")
       ("version,V", "show the current software version");
   po::options_description drilling_options("Drilling options, for making holes in the PCB");

   drilling_options.add_options()
       ("drill", po::value<string>(), "Excellon drill file")
       ("milldrill", po::value<bool>()->default_value(false)->implicit_value(true), "[DEPRECATED] Use min-milldrill-hole-diameter=0 instead")
       ("milldrill-diameter", po::value<Length>(), "diameter of the end mill used for drilling with --milldrill")
       ("min-milldrill-hole-diameter", po::value<Length>()->default_value(Length(std::numeric_limits<double>::infinity())),
        "minimum hole width or milldrilling.  Holes smaller than this are drilled.  This implies milldrill")
       ("zdrill", po::value<Length>(), "drilling depth")
       ("zmilldrill", po::value<Length>(), "milldrilling depth")
       ("drill-feed", po::value<Velocity>(), "drill feed in [i/m] or [mm/m]")
       ("drill-speed", po::value<Rpm>(), "spindle rpm when drilling")
       ("drill-front", po::value<bool>()->implicit_value(true), "[DEPRECATED, use drill-side instead] drill through the front side of board")
       ("drill-side", po::value<BoardSide::BoardSide>()->default_value(BoardSide::AUTO),
        "drill side; valid choices are front, back or auto (default)")
       ("drills-available", po::value<std::vector<AvailableDrills>>()
        ->default_value(std::vector<AvailableDrills>{})
        ->multitoken(), "list of drills available")
       ("onedrill", po::value<bool>()->default_value(false)->implicit_value(true), "use only one drill bit size")
       ("drill-output", po::value<string>()->default_value("drill.ngc"), "output file for drilling")
       ("nog91-1", po::value<bool>()->default_value(false)->implicit_value(true), "do not explicitly set G91.1 in drill headers")
       ("nog81", po::value<bool>()->default_value(false)->implicit_value(true), "replace G81 with G0+G1")
       ("nom6", po::value<bool>()->default_value(false)->implicit_value(true), "do not emit M6 on tool changes")
       ("milldrill-output", po::value<string>()->default_value("milldrill.ngc"), "output file for milldrilling");
   cfg_options.add(drilling_options);

   po::options_description milling_options("Milling options, for milling traces into the PCB");
   milling_options.add_options()
       ("front", po::value<string>(),"front side RS274-X .gbr")
       ("back", po::value<string>(), "back side RS274-X .gbr")
       ("voronoi", po::value<bool>()->default_value(false)->implicit_value(true), "generate voronoi regions")
       ("offset", po::value<Length>()->default_value(0), "Note: Prefer to use --mill-diameters and --milling-overlap if you just that's what you mean."
        "  An optional offset to add to all traces, useful if the bit has a little slop that you want to keep out of the trace.")
       ("mill-diameters", po::value<std::vector<CommaSeparated<Length>>>()->default_value({{Length(0)}})
        ->multitoken(), "Diameters of mill bits, used in the order that they are provided.")
       ("milling-overlap", po::value<boost::variant<Length, Percent>>()->default_value(parse_unit<Percent>("50%")),
        "How much to overlap milling passes, from 0% to 100% or an absolute length")
       ("isolation-width", po::value<Length>()->default_value(Length(0)),
        "Minimum isolation width between copper surfaces")
       ("extra-passes", po::value<int>()->default_value(0), "[DEPRECATED] use --isolation-width instead. "
        "Specify the the number of extra isolation passes, increasing the isolation width half the tool diameter with each pass")
       ("pre-milling-gcode", po::value<std::vector<string>>()->default_value(std::vector<string>{}, ""),
        "custom gcode inserted before the start of milling each trace (used to activate pump or fan or laser connected to fan)")
       ("post-milling-gcode", po::value<std::vector<string>>()->default_value(std::vector<string>{}, ""),
        "custom gcode inserted after the end of milling each trace (used to deactivate pump or fan or laser connected to fan)")
       ("zwork", po::value<Length>(), "milling depth in inches (Z-coordinate while engraving)")
       ("mill-feed", po::value<Velocity>(), "feed while isolating in [i/m] or [mm/m]")
       ("mill-vertfeed", po::value<Velocity>(), "vertical feed while isolating in [i/m] or [mm/m]")
       ("mill-infeed", po::value<Length>(), "maximum milling depth; PCB may be cut in multiple passes")
       ("mill-speed", po::value<Rpm>(), "spindle rpm when milling")
       ("mill-feed-direction", po::value<MillFeedDirection::MillFeedDirection>()->default_value(MillFeedDirection::ANY),
        "In which direction should all milling occur")
       ("invert-gerbers", po::value<bool>()->default_value(false)->implicit_value(true),
        "Invert polarity of front and back gerbers, causing the milling to occur inside the shapes")
       ("draw-gerber-lines", po::value<bool>()->default_value(false)->implicit_value(true),
        "Draw lines in the gerber file as just lines and not as filled in shapes")
       ("preserve-thermal-reliefs", po::value<bool>()->default_value(true)->implicit_value(true), "generate mill paths for thermal reliefs in voronoi mode")
       ("front-output", po::value<string>()->default_value("front.ngc"), "output file for front layer")
       ("back-output", po::value<string>()->default_value("back.ngc"), "output file for back layer");
   cfg_options.add(milling_options);

   po::options_description outline_options("Outline options, for cutting the PCB out of the FR4");
   outline_options.add_options()
       ("outline", po::value<string>(), "pcb outline polygon RS274-X .gbr")
       ("fill-outline", po::value<bool>()->default_value(true)->implicit_value(true), "accept a contour instead of a polygon as outline (enabled by default)")
       ("cutter-diameter", po::value<Length>(), "diameter of the end mill used for cutting out the PCB")
       ("zcut", po::value<Length>(), "PCB cutting depth in inches")
       ("cut-feed", po::value<Velocity>(), "PCB cutting feed in [i/m] or [mm/m]")
       ("cut-vertfeed", po::value<Velocity>(), "PCB vertical cutting feed in [i/m] or [mm/m]")
       ("cut-speed", po::value<Rpm>(), "spindle rpm when cutting")
       ("cut-infeed", po::value<Length>(), "maximum cutting depth; PCB may be cut in multiple passes")
       ("cut-front", po::value<bool>()->implicit_value(true), "[DEPRECATED, use cut-side instead] cut from front side. ")
       ("cut-side", po::value<BoardSide::BoardSide>()->default_value(BoardSide::AUTO), "cut side; valid choices are front, back or auto (default)")
       ("bridges", po::value<Length>()->default_value(Length(0)), "add bridges with the given width to the outline cut")
       ("bridgesnum", po::value<unsigned int>()->default_value(2), "specify how many bridges should be created")
       ("zbridges", po::value<Length>(), "bridges height (Z-coordinates while engraving bridges, default to zsafe) ")
       ("outline-output", po::value<string>()->default_value("outline.ngc"), "output file for outline");
   cfg_options.add(outline_options);

   po::options_description optimization_options("Optimization options, for faster PCB creation, smaller output files, and different algorithms.");
   optimization_options.add_options()
       ("optimise", po::value<Length>()->default_value(parse_unit<Length>("0.0001in"))->implicit_value(parse_unit<Length>("0.0001in")),
        "Reduce output file size by up to 40% while accepting a little loss of precision.  Larger values reduce file sizes and processing time even further.  Set to 0 to disable.")
       ("eulerian-paths", po::value<bool>()->default_value(true)->implicit_value(true), "Don't mill the same path twice if milling loops overlap.  This can save up to 50% of milling time.  Enabled by default.")
       ("vectorial", po::value<bool>()->default_value(true)->implicit_value(true), "enable or disable the vectorial rendering engine")
       ("tsp-2opt", po::value<bool>()->default_value(true)->implicit_value(true), "use TSP 2OPT to find a faster toolpath (but slows down gcode generation)")
       ("path-finding-limit", po::value<size_t>()->default_value(1), "Use path finding for up to this many steps in the search (more is slower but makes a faster gcode path)")
       ("g0-vertical-speed", po::value<Velocity>()->default_value(parse_unit<Velocity>("50in/min")), "speed of vertical G0 movements, for use in path-finding")
       ("g0-horizontal-speed", po::value<Velocity>()->default_value(parse_unit<Velocity>("100in/min")), "speed of horizontal G0 movements, for use in path-finding")
       ("backtrack", po::value<Velocity>()->default_value(std::numeric_limits<double>::infinity()), "allow retracing a milled path if it's faster than retract-move-lower.  For example, set to 5in/s if you are willing to remill 5 inches of trace in order to save 1 second of milling time.")
       ("min-path-length", po::value<Length>()->default_value(Length(0)), "Skip milling paths shorter than this length.  Useful for removing tiny Voronoi artifacts.  Set to 0.1mm to remove paths shorter than 0.1mm.");
   cfg_options.add(optimization_options);

   po::options_description autolevelling_options("Autolevelling options, for generating gcode to automatically probe the board and adjust milling depth to the actual board height");
   autolevelling_options.add_options()
       ("al-front", po::value<bool>()->default_value(false)->implicit_value(true), "enable the z autoleveller for the front layer")
       ("al-back", po::value<bool>()->default_value(false)->implicit_value(true),
        "enable the z autoleveller for the back layer")
       ("software", po::value<Software::Software>(),
        "choose the destination software (useful only with the autoleveller). Supported programs are linuxcnc, mach3, mach4 and custom")
       ("al-x", po::value<Length>(), "max x distance between probes")
       ("al-y", po::value<Length>(), "max y distance bewteen probes")
       ("al-probefeed", po::value<Velocity>(), "speed during the probing")
       ("al-probe-on", po::value<string>()->default_value("(MSG, Attach the probe tool)@M0 ( Temporary machine stop. )"),
        "execute this commands to enable the probe tool (default is M0)")
       ("al-probe-off", po::value<string>()->default_value("(MSG, Detach the probe tool)@M0 ( Temporary machine stop. )"),
        "execute this commands to disable the probe tool (default is M0)")
       ("al-probecode", po::value<string>()->default_value("G31"), "custom probe code (default is G31)")
       ("al-probevar", po::value<unsigned int>()->default_value(2002), "number of the variable where the result of the probing is saved (default is 2002)")
       ("al-setzzero", po::value<string>()->default_value("G92 Z0"), "gcode for setting the actual position as zero (default is G92 Z0)");
   cfg_options.add(autolevelling_options);

   po::options_description alignment_options("Alignment options, useful for aligning the milling on opposite sides of the PCB");
   alignment_options.add_options()
       ("x-offset", po::value<Length>()->default_value(0), "offset the origin in the x-axis by this length")
       ("y-offset", po::value<Length>()->default_value(0), "offset the origin in the y-axis by this length")
       ("zero-start", po::value<bool>()->default_value(false)->implicit_value(true), "set the starting point of the project at (0,0)")
       ("mirror-absolute", po::value<bool>()->default_value(true)->implicit_value(true),
        "[DEPRECATED, must always be true] mirror back side along absolute zero instead of board center")
       ("mirror-axis", po::value<Length>()->default_value(Length(0)), "For two-sided boards, the PCB needs to be flipped along the axis x=VALUE")
       ("mirror-yaxis", po::value<bool>()->default_value(false), "For two-sided boards, the PCB needs to be flipped along the y axis instead");
   cfg_options.add(alignment_options);

   po::options_description cnc_options("CNC options, common to all the milling, drilling, and cutting");
   cnc_options.add_options()
       ("zsafe", po::value<Length>(), "safety height (Z-coordinate during rapid moves)")
       ("spinup-time", po::value<Time>()->default_value(parse_unit<Time>("1 ms")), "time required to the spindle to reach the correct speed")
       ("spindown-time", po::value<Time>(), "time required to the spindle to return to 0 rpm")
       ("zchange", po::value<Length>(), "tool changing height")
       ("zchange-absolute", po::value<bool>()->default_value(false)->implicit_value(true), "use zchange as a machine coordinates height (G53)")
       ("tile-x", po::value<int>()->default_value(1), "number of tiling columns. Default value is 1")
       ("tile-y", po::value<int>()->default_value(1), "number of tiling rows. Default value is 1");
   cfg_options.add(cnc_options);

   cfg_options.add_options()
       ("ignore-warnings", po::value<bool>()->default_value(false)->implicit_value(true), "Ignore warnings")
       ("svg", po::value<string>(), "[DEPRECATED] use --vectorial, SVGs will be generated automatically; this option has no effect")
       ("metric", po::value<bool>()->default_value(false)->implicit_value(true), "use metric units for parameters. does not affect gcode output")
       ("metricoutput", po::value<bool>()->default_value(false)->implicit_value(true), "use metric units for output")
       ("g64", po::value<double>(), "[DEPRECATED, use tolerance instead] maximum deviation from toolpath, overrides internal calculation")
       ("tolerance", po::value<double>(), "maximum toolpath tolerance")
       ("nog64", po::value<bool>()->default_value(false)->implicit_value(true), "do not set an explicit g64")
       ("output-dir", po::value<string>()->default_value(""), "output directory")
       ("basename", po::value<string>(), "prefix for default output file names")
       ("preamble-text", po::value<string>(), "preamble text file, inserted at the very beginning as a comment.")
       ("preamble", po::value<string>(), "gcode preamble file, inserted at the very beginning.")
       ("postamble", po::value<string>(), "gcode postamble file, inserted before M9 and M2.")
       ("no-export", po::value<bool>()->default_value(false)->implicit_value(true), "skip the exporting process");
}

/******************************************************************************/
/*
 */
/******************************************************************************/
static void check_generic_parameters(po::variables_map const& vm)
{
    double unit;      //factor for imperial/metric conversion

    unit = vm["metric"].as<bool>() ? (1. / 25.4) : 1;

    //---------------------------------------------------------------------------
    //Check spinup(down)-time parameters:

    if (vm["spinup-time"].as<Time>().asMillisecond(1) < 0) {
      options::maybe_throw("spinup-time can't be negative!", ERR_NEGATIVESPINUP);
    }

    if (vm.count("spindown-time") && vm["spindown-time"].as<Time>().asMillisecond(1) < 0) {
      options::maybe_throw("spindown-time can't be negative!", ERR_NEGATIVESPINDOWN);
    }

    //---------------------------------------------------------------------------
    //Check g64 parameter:

    if (vm.count("g64"))
    {
        cerr << "g64 is deprecated, use tolerance.\n";
    }

    //---------------------------------------------------------------------------
    //Check mirror-absolute parameter:

    if (!vm["mirror-absolute"].as<bool>()) {
      options::maybe_throw("mirror-absolute is deprecated, it must be true.", ERR_FALSEMIRRORABSOLUTE);
    }

    //---------------------------------------------------------------------------
    //Check tolerance parameter:

    //Upper threshold value of tolerance parameter for warning
    double tolerance_th = vm["metric"].as<bool>() ? 0.2 : 0.008;

    if (vm.count("tolerance"))              //tolerance parameter is given
    {
        if (vm["tolerance"].as<double>() > tolerance_th)
            cerr << "Warning: high tolerance value (allowed deviation from toolpath) given.\n"
                 << endl;

        else if (vm["tolerance"].as<double>() == 0)
            cerr << "Warning: Deviation from commanded toolpath set to 0 (tolerance=0). No smooth milling is most likely!\n"
                 << endl;

        else if (vm["tolerance"].as<double>() < 0) {
          options::maybe_throw("tolerance can't be negative!", ERR_NEGATIVETOLERANCE);
        }
    }

    //---------------------------------------------------------------------------
    //Check svg parameter:

    if (vm.count("svg")) {
      cerr << "--svg is deprecated and has no effect anymore, use --vectorial to generate SVGs.\n";
    }

    //---------------------------------------------------------------------------
    //Check for available board dimensions:

    if (vm.count("drill")
            && !(vm.count("front") || vm.count("back") || vm.count("outline")))
    {
        cerr << "Warning: Board dimensions unknown. Gcode for drilling will be probably misaligned.\n";
    }

    //---------------------------------------------------------------------------
    //Check for tile parameters

    if (vm["tile-x"].as<int>() < 1) {
      options::maybe_throw("tile-x can't be negative!", ERR_NEGATIVETILEX);
    }

    if (vm["tile-y"].as<int>() < 1) {
      options::maybe_throw("tile-y can't be negative!", ERR_NEGATIVETILEY);
    }

    //---------------------------------------------------------------------------
    //Check for safety height parameter:

    if (!vm.count("zsafe")) {
      options::maybe_throw("Error: Safety height not specified.", ERR_NOZSAFE);
    }

    //---------------------------------------------------------------------------
    //Check for zchange parameter parameter:

    if (!vm.count("zchange")) {
      options::maybe_throw("Error: Tool changing height not specified.", ERR_NOZCHANGE);
    }

    //---------------------------------------------------------------------------
    //Check for autoleveller parameters

    if (vm["al-front"].as<bool>() || vm["al-back"].as<bool>())
    {
        if (!vm.count("software")) {
          options::maybe_throw("Error: unspecified or unsupported software, please specify a supported software (linuxcnc, mach3, mach4 or custom).", ERR_NOSOFTWARE);
        }

        if (!vm.count("al-x")) {
          options::maybe_throw("Error: autoleveller probe width x not specified.", ERR_NOALX);
        } else if (vm["al-x"].as<Length>().asInch(unit) <= 0) {
          options::maybe_throw("Error: al-x < 0!", ERR_NEGATIVEALX);
        }

        if (!vm.count("al-y")) {
          options::maybe_throw("Error: autoleveller probe width y not specified.", ERR_NOALY);
        } else if (vm["al-y"].as<Length>().asInch(unit) <= 0) {
          options::maybe_throw("Error: al-y < 0!", ERR_NEGATIVEALY);
        }

        if (!vm.count("al-probefeed")) {
          options::maybe_throw("Error: autoleveller probe feed rate not specified.", ERR_NOALPROBEFEED);
        } else if (vm["al-probefeed"].as<Velocity>().asInchPerMinute(unit) <= 0) {
          options::maybe_throw("Error: al-probefeed < 0!", ERR_NEGATIVEPROBEFEED);
        }
    }
    if (vm["mill-feed-direction"].as<MillFeedDirection::MillFeedDirection>() != MillFeedDirection::ANY &&
        vm["tsp-2opt"].as<bool>()) {
      options::maybe_throw("Error: Can't use tsp-2opt together with mill-feed-direction", ERR_INVALIDPARAMETER);
    }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
static void check_milling_parameters(po::variables_map const& vm)
{

    double unit;      //factor for imperial/metric conversion

    unit = vm["metric"].as<bool>() ? (1. / 25.4) : 1;

    if (vm.count("front") || vm.count("back"))
    {

      if (!vm.count("zwork")) {
        options::maybe_throw("Error: --zwork not specified.", ERR_NOZWORK);
      } else if (vm["zwork"].as<Length>().asDouble() > 0){
        cerr << "Warning: Engraving depth (--zwork) is greater than zero!\n";
      }

      if (!vm["vectorial"].as<bool>()) {
        options::maybe_throw("Error: --vectorial is mandatory", ERR_INVALIDPARAMETER);
      }
      if (!vm.count("mill-diameters")) {
        options::maybe_throw("Error: no --mill-diameters specified.", ERR_NOOFFSET);
      }

      if (!vm.count("mill-feed")) {
        options::maybe_throw("Error: Milling feed [i/m or mm/m] not specified.", ERR_NOMILLFEED);
      }

      if (!vm.count("mill-speed")) {
        options::maybe_throw("Error: Milling speed [rpm] not specified.", ERR_NOMILLSPEED);
      }

        // required parameters present. check for validity.
      if (vm["zsafe"].as<Length>().asInch(unit) <= vm["zwork"].as<Length>().asInch(unit)) {
        options::maybe_throw("Error: The safety height --zsafe is lower than the milling "
                             "height --zwork. Are you sure this is correct?", ERR_ZSAFELOWERZWORK);
      }

      if (vm["mill-feed"].as<Velocity>().asDouble() <= 0) {
        options::maybe_throw("Error: Negative or equal to 0 milling feed (--mill-feed).", ERR_NEGATIVEMILLFEED);
      }

      if (vm.count("mill-vertfeed") && vm["mill-vertfeed"].as<Velocity>().asInchPerMinute(unit) <= 0) {
        options::maybe_throw("Error: Negative or equal to 0 vertical milling feed (--mill-vertfeed).", ERR_NEGATIVEMILLVERTFEED);
      }

      if (vm.count("mill-infeed")>0 && vm["mill-infeed"].as<Length>().asInch(unit) <= 0.0) {
        options::maybe_throw("Error: The milling infeed --mill-infeed. seems too low.", ERR_LOWMILLINFEED);
      }

      if (vm["mill-speed"].as<Rpm>().asDouble() < 0) {
        options::maybe_throw("Error: --mill-speed < 0.", ERR_NEGATIVEMILLSPEED);
      }
    }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
static void check_drilling_parameters(po::variables_map const& vm)
{

    double unit;      //factor for imperial/metric conversion

    unit = vm["metric"].as<bool>() ? (1. / 25.4) : 1;

    //only check the parameters if a drill file is given
    if (vm.count("drill"))
    {

        if (!vm.count("zdrill")) {
          options::maybe_throw("Error: Drilling depth (--zdrill) not specified.\n", ERR_NOZDRILL);
        }

        if (vm["zsafe"].as<Length>().asInch(unit) <= vm["zdrill"].as<Length>().asInch(unit)) {
          options::maybe_throw("Error: The safety height --zsafe is lower than the drilling "
                               "height --zdrill!\n", ERR_ZSAFELOWERZDRILL);
        }

        if (!vm.count("zchange")) {
          options::maybe_throw("Error: Drill bit changing height (--zchange) not specified.", ERR_NOZCHANGE);
        } else if (!vm["zchange-absolute"].as<bool>() && vm["zchange"].as<Length>().asInch(unit) <= vm["zdrill"].as<Length>().asInch(unit)) {
          options::maybe_throw("Error: The safety height --zsafe is lower than the tool "
                               "change height --zchange!", ERR_ZSAFELOWERZCHANGE);
        }

        if (!vm.count("drill-feed")) {
          options::maybe_throw("Error:: Drilling feed (--drill-feed) not specified.", ERR_NODRILLFEED);
        } else if (vm["drill-feed"].as<Velocity>().asInchPerMinute(unit) <= 0) {
          options::maybe_throw("Error: The drilling feed --drill-feed is <= 0.", ERR_NEGATIVEDRILLFEED);
        }

        if (!vm.count("drill-speed")) {
          options::maybe_throw("Error: Drilling spindle RPM (--drill-speed) not specified.", ERR_NODRILLSPEED);
        } else if (vm["drill-speed"].as<Rpm>().asRpm(1) < 0) {        //no need to support both directions?
          options::maybe_throw("Error: --drill-speed < 0.", ERR_NEGATIVEDRILLSPEED);
        }

        if (vm.count("drill-front")) {
          cerr << "drill-front is deprecated, use drill-side.\n";

          if (!vm["drill-side"].defaulted()) {
            options::maybe_throw("You can't specify both drill-front and drill-side!", ERR_BOTHDRILLFRONTSIDE);
          }
        }
    }

}

/******************************************************************************/
/*
 */
/******************************************************************************/
static void check_cutting_parameters(po::variables_map const& vm) {

  double unit;      //factor for imperial/metric conversion

  unit = vm["metric"].as<bool>() ? (1. / 25.4) : 1;

  //only check the parameters if an outline file is given or milldrill is enabled
  if (vm.count("outline") ||
      (vm.count("drill") &&
       (vm["min-milldrill-hole-diameter"].as<Length>() < Length(std::numeric_limits<double>::infinity())))) {
    if (!vm.count("zcut")) {
      options::maybe_throw("Error: Board cutting depth (--zcut) not specified.", ERR_NOZCUT);
    } else if (vm["zcut"].as<Length>().asInch(unit) > 0) {
      options::maybe_throw("Error: Cutting depth (--zcut) is greater than zero!", ERR_NEGATIVEZWORK);
    }

    if (!vm.count("cutter-diameter")) {
      options::maybe_throw("Error: Cutter diameter not specified.", ERR_NOCUTTERDIAMETER);
    }

    if (!vm.count("cut-feed")) {
      options::maybe_throw("Error: Board cutting feed (--cut-feed) not specified.", ERR_NOCUTFEED);
    }

    if (!vm.count("cut-speed")) {
      options::maybe_throw("Error: Board cutting spindle RPM (--cut-speed) not specified.", ERR_NOCUTSPEED);
    }

    if (!vm.count("cut-infeed")) {
      options::maybe_throw("Error: Board cutting infeed (--cut-infeed) not specified.", ERR_NOCUTINFEED);
    }

    if (vm["zsafe"].as<Length>().asInch(unit) <= vm["zcut"].as<Length>().asInch(unit)) {
      options::maybe_throw("Error: The safety height --zsafe is lower than the cutting "
                           "height --zcut!", ERR_ZSAFELOWERZCUT);
    }

    if (vm["cut-feed"].as<Velocity>().asInchPerMinute(unit) <= 0) {
      options::maybe_throw("Error: The cutting feed --cut-feed is <= 0.", ERR_NEGATIVECUTFEED);
    }

    if (vm.count("cut-vertfeed") && vm["cut-vertfeed"].as<Velocity>().asInchPerMinute(unit) <= 0) {
      options::maybe_throw("Error: The cutting vertical feed --cut-vertfeed is <= 0.", ERR_NEGATIVECUTVERTFEED);
    }

    if (vm["cut-speed"].as<Rpm>().asRpm(1) < 0) {        //no need to support both directions?
      options::maybe_throw("Error: The cutting spindle speed --cut-speed is lower than 0.", ERR_NEGATIVESPINDLESPEED);
    }

    if (vm["cut-infeed"].as<Length>().asInch(unit) < 0.001) {
      options::maybe_throw("Error: The cutting infeed --cut-infeed. seems too low.", ERR_LOWCUTINFEED);
    }

    if (vm["bridges"].as<Length>().asInch(unit) < 0) {
      options::maybe_throw("Error: negative bridge value.", ERR_NEGATIVEBRIDGE);
    }

    if (vm.count("cut-front")) {
      cerr << "cut-front is deprecated, use cut-side.\n";
      if (!vm["cut-side"].defaulted()) {
        options::maybe_throw("You can't specify both cut-front and cut-side!\n", ERR_BOTHCUTFRONTSIDE);
      }
    }
  }
}

/******************************************************************************/
/*
 */
/******************************************************************************/
void options::check_parameters()
{
  po::variables_map const& vm = instance().vm;

  try {
    check_generic_parameters(vm);
    check_milling_parameters(vm);
    check_cutting_parameters(vm);
    check_drilling_parameters(vm);
  } catch (std::runtime_error& re) {
    maybe_throw("Error: Invalid parameter. :-(", ERR_INVALIDPARAMETER);
  }
}
