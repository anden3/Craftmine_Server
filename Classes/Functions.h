#pragma once

#include <string>
#include <vector>

std::vector<std::string> Split(const std::string &s, const char delim);

std::string Join(const std::vector<int> &list, const std::string joiner = "");
std::string Join(const std::vector<std::string> &list, const std::string joiner = " ");

std::string To_String(unsigned char* data);
std::string Get_Timestamp();
std::string Get_Name(void* data);
int Get_Seed();

std::string To_Hex(unsigned int value);