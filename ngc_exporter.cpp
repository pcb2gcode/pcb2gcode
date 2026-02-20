#include "ngc_exporter.hpp"
#include "common.hpp"
#include <fstream>
#include <iostream>

NGC_Exporter::NGC_Exporter(std::shared_ptr<Board> board) 
    : board(board), ocodes(0), globalVars(0) {} 

void NGC_Exporter::export_all(boost::program_options::variables_map& options) {
    std::vector<std::string> layer_names = board->list_layers();
    std::string out_dir = options.count("output") ? options["output"].as<std::string>() : ".";

    for (const auto& name : layer_names) {
        std::shared_ptr<Layer> layer = board->get_layer(name);
        std::string of_name = build_filename(out_dir, name + ".ngc");
        boost::optional<autoleveller> leveller; 
        export_layer(layer, of_name, leveller);
    }
}

void NGC_Exporter::export_layer(std::shared_ptr<Layer> layer, std::string of_name, boost::optional<autoleveller> leveller) {
    std::ofstream of(of_name);
    if (!of.is_open()) return;

    if (leveller) leveller->header(of);
    for (const auto& h : header) { of << "( " << h << " )\n"; }
    
    if (!preamble.empty()) of << preamble << "\n";
    of << "( Multithreaded G-code Export )\n";
    of << "G21 G90\n";
    
    auto toolpaths = board->get_toolpath(layer->get_name());
    for (const auto& path_pair : toolpaths) {
        for (const auto& linestring : path_pair.second) {
            if (linestring.empty()) continue;
            of << "G0 X" << linestring[0].x() << " Y" << linestring[0].y() << "\n";
            for (size_t i = 1; i < linestring.size(); ++i) {
                of << "G1 X" << linestring[i].x() << " Y" << linestring[i].y() << "\n";
            }
        }
    }

    if (!postamble.empty()) of << postamble << "\n";
    if (leveller) leveller->footer(of); 
    of << "M2\n";
    of.close();
}

void NGC_Exporter::add_header(std::string h) { header.push_back(h); }
void NGC_Exporter::set_preamble(std::string p) { preamble = p; }
void NGC_Exporter::set_postamble(std::string p) { postamble = p; }


