#ifndef PARSE_H_
#define PARSE_H_

#include <iostream>
#include <vector>
#include <sstream>
#include <string>

using namespace std;

int separate_by_semicolon(char *cmdline, vector<string> *result);
int separate_by_vertical_bar(string command, vector<string> *result);
vector<vector<string>> parse_segments(vector<string> *segments);

#endif