#pragma once
#include <string>

// In-game reference pages. Topics are numbered from 1.
namespace Help {
int topicCount();
const char *topicTitle(int topic);
std::string topicText(int topic);
} // namespace Help
