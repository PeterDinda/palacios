#ifndef _config
#define _config
#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <sstream>

using namespace std;

#define MAX_CONFIG_LINE_SIZE 1024


struct eqstr {
  bool operator()(const string s1, const string s2) const {
    return strcmp(s1.c_str(), s2.c_str()) < 0;
  }
};

typedef map<const string, string, eqstr> config_t;


int read_config(string conf_file_name, config_t * config);



#endif
