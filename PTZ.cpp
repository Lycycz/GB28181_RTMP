#pragma once
#include "PTZ.h"

PTZ ptzconvert(std::string x) {
	x = x.c_str();
	return ptzconvert(x);
}

PTZ ptzconvert(const char* x) {
	if (strcmp(x, "zoomin") == 0) return PTZ::ZOOMIN;
	else if (strcmp(x, "zoomout") == 0) return PTZ::ZOOMOUT;
	else if (strcmp(x, "up") == 0) return PTZ::UP;
	else if (strcmp(x, "down") == 0) return PTZ::DOWN;
	else if (strcmp(x, "left") == 0) return PTZ::LEFT;
	else if (strcmp(x, "right") == 0) return PTZ::RIGHT;
}