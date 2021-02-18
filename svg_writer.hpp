#ifndef SVG_WRITER_HPP
#define SVG_WRITER_HPP

#include <fstream>

class svg_writer {
 public:
  svg_writer(std::string filename, box_type_fp bounding_box);
  template <typename multi_polygon_type_t>
  void add(multi_polygon_type_t geometry, double opacity, bool stroke);
  void add(multi_linestring_type_fp mls, coordinate_type_fp width, bool stroke);
  void add(const linestring_type_fp& path, coordinate_type_fp width, unsigned int r, unsigned int g, unsigned int b);
  void add(multi_linestring_type_fp paths, coordinate_type_fp width, unsigned int r, unsigned int g, unsigned int b);

 protected:
  std::ofstream output_file;
  const box_type_fp bounding_box;
  std::unique_ptr<bg::svg_mapper<point_type_fp> > mapper;
};

#endif //SVG_WRITER_HPP
