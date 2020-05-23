#include "CommandLineParser.h"
#include <algorithm>
#include <iostream>
// https://stackoverflow.com/questions/865668/how-to-parse-command-line-arguments-in-c

CommandLineParser::CommandLineParser(int &argc, char **argv) {
  for (int i = 1; i < argc; ++i)
    this->tokens.push_back(std::string(argv[i]));
  std::string paraName = getCmdOption("-w");
  if (!paraName.empty()) {
    width_ = (uint32_t)std::atoi(paraName.c_str());
  }
  paraName = getCmdOption("-h");
  if (!paraName.empty()) {
    height_ = (uint32_t)std::atoi(paraName.c_str());
  }
  paraName = getCmdOption("-wx");
  if (!paraName.empty()) {
    workgroup_size_x_ = (uint32_t)std::atoi(paraName.c_str());
  }
  paraName = getCmdOption("-wy");
  if (!paraName.empty()) {
    workgroup_size_y_ = (uint32_t)std::atoi(paraName.c_str());
  }
  paraName = getCmdOption("-wz");
  if (!paraName.empty()) {
    workgroup_size_z_ = (uint32_t)std::atoi(paraName.c_str());
  }
}
const std::string &
CommandLineParser::getCmdOption(const std::string &option) const {
  std::vector<std::string>::const_iterator itr;
  itr = std::find(this->tokens.begin(), this->tokens.end(), option);
  if (itr != this->tokens.end() && ++itr != this->tokens.end()) {
    return *itr;
  }
  static const std::string empty_string("");
  return empty_string;
}
bool CommandLineParser::cmdOptionExists(const std::string &option) const {
  return std::find(this->tokens.begin(), this->tokens.end(), option) !=
         this->tokens.end();
}
const uint32_t CommandLineParser::getWidth() { return width_; }
const uint32_t CommandLineParser::getHeight() { return height_; }
const uint32_t CommandLineParser::getWorkgroupSizeX() { return workgroup_size_x_; }
const uint32_t CommandLineParser::getWorkgroupSizeY() { return workgroup_size_y_; }
const uint32_t CommandLineParser::getWorkgroupSizeZ() { return workgroup_size_z_; }
