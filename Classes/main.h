#pragma once

#include <string>

extern bool Exit;

void Parse_Commands(std::string &input);

void Send(unsigned int peerID, std::string message, int channel);
void Broadcast(std::string message, int channel);