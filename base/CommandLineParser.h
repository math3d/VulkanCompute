#ifndef COMMAND_LINE_PARSER_H_
#define COMMAND_LINE_PARSER_H_

#include <vector>
#include <string>

// https://stackoverflow.com/questions/865668/how-to-parse-command-line-arguments-in-c
class CommandLineParser {
public:
  CommandLineParser(int &argc, char **argv);
  const std::string &getCmdOption(const std::string &option) const;
  bool cmdOptionExists(const std::string &option) const;
  const uint32_t getWidth ();
  const uint32_t getHeight ();
private:
  uint32_t width_ = 4;
  uint32_t height_ = 8;
  std::vector<std::string> tokens;
};
#endif