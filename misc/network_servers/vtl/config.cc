#include "config.h"


int read_config(string conf_file_name, config_t * config) {
  fstream conf_file(conf_file_name.c_str(), ios::in);
  char line[MAX_CONFIG_LINE_SIZE];

  if (!conf_file.is_open()) {
    return -1;
  }

  while ((conf_file.getline(line, MAX_CONFIG_LINE_SIZE))) {
    string conf_line = line;
    string tag;
    string value;
    int offset, ltrim_index, rtrim_index;

    if (conf_line[0] == '#') {
      continue;
    }

    offset = conf_line.find(":", 0);
    tag = conf_line.substr(0,offset);

    // kill white space
    istringstream tag_stream(tag, istringstream::in);
    tag_stream >> tag;

    if (tag.empty()) {
      continue;
    }

    // basic whitespace trimming, we assume that the config handlers will deal with 
    // tokenizing and further formatting
    value = conf_line.substr(offset + 1, conf_line.length() - offset);
    ltrim_index = value.find_first_not_of(" \t");
    rtrim_index = value.find_last_not_of(" \t");
    
    if ((ltrim_index >= 0) && (rtrim_index >= 0)) {
	value = value.substr(ltrim_index, (rtrim_index + 1) - ltrim_index);
    }

    (*config)[tag] = value;
  }
  return 0;
}
