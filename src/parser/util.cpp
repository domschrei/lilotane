#include "util.hpp"

bool no_colors_in_output = false;

string color(Color color, string text, Mode m, Color background){
	if (no_colors_in_output) return text;
	return string ()
		+ "\033[" +std::to_string (m)+ ";" + std::to_string (30 + color) + "m"
		+ ((background != COLOR_NONE)?("\033[" + std::to_string (40 + background) + "m"):"")
		+ text
		+ "\033[0;m"
  ;
}
