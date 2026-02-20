#include "board.hpp"
#include <future>
#include <vector>
#include <iostream>
#include <thread>

Board::Board(bool fill_outline, std::string outputdir, bool tsp_2opt, 
             MillFeedDirection::MillFeedDirection mill_feed_direction, 
             bool invert_gerbers, bool render_paths_to_shapes)
    : fill_outline(fill_outline), outputdir(outputdir), tsp_2opt(tsp_2opt), 
      mill_feed_direction(mill_feed_direction), invert_gerbers(invert_gerbers), 
      render_paths_to_shapes(render_paths_to_shapes) 
{}

void Board::prepareLayer(std::string layername, std::shared_ptr<GerberImporter> importer, 
                         std::shared_ptr<RoutingMill> manufacturer, bool backside, bool ymirror) {
    prepared_layers[layername] = std::make_tuple(importer, manufacturer, backside, ymirror);
}

void Board::createLayers() {
    unsigned int n_threads = std::thread::hardware_concurrency();
    if (n_threads == 0) n_threads = 2; 
    std::cout << "Perf: System detected " << n_threads << " threads. Parallelizing by layer..." << std::endl;

    for (const auto& prep : prepared_layers) {
        auto importer = std::get<0>(prep.second);
        boost::geometry::expand(bounding_box, importer->get_bounding_box());
    }

    std::vector<std::future<std::shared_ptr<Layer>>> futures;
    for (const auto& prep : prepared_layers) {
        futures.push_back(std::async(std::launch::async, [this, prep]() {
            std::string name = prep.first;
            auto importer = std::get<0>(prep.second);
            auto mill = std::get<1>(prep.second);

            std::cerr << "PERF_LOG: START_RENDER_" << name << std::endl;

            auto surface = std::make_shared<Surface_vectorial>(bounding_box, name, outputdir, tsp_2opt,
                                                        mill_feed_direction, invert_gerbers,
                                                        render_paths_to_shapes || (name == "outline"));
            
            surface->render(importer, mill->tolerance); 
            
            auto layer_ptr = std::make_shared<Layer>(name, surface, mill, std::get<2>(prep.second), std::get<3>(prep.second));
            
            std::cerr << "PERF_LOG: FINISH_RENDER_" << name << std::endl;
            return layer_ptr;
        }));
    }

    for (auto& f : futures) {
        auto layer = f.get();
        layers[layer->get_name()] = layer;
    }
}

std::vector<std::string> Board::list_layers() {
    std::vector<std::string> names;
    for (auto const& [name, layer] : layers) { names.push_back(name); }
    return names;
}

std::shared_ptr<Layer> Board::get_layer(std::string layername) {
    return layers.at(layername);
}

std::vector<std::pair<coordinate_type_fp, multi_linestring_type_fp>> Board::get_toolpath(std::string layername) {
    return layers.at(layername)->get_toolpaths();
}


