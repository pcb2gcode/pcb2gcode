#ifndef FLATTEN_HPP
#define FLATTEN_HPP

template <typename T>
std::vector<T> flatten(const std::vector<std::vector<T>>& v) {
  std::size_t total_size = 0;
  for (const auto& sub : v)
    total_size += sub.size(); // I wish there was a transform_accumulate
  std::vector<T> result;
  result.reserve(total_size);
  for (const auto& sub : v)
    result.insert(result.end(), sub.begin(), sub.end());
  return result;
}

#endif // FLATTEN_HPP
