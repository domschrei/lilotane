#include <string>

using namespace std;

enum Color{COLOR_GRAY,COLOR_RED,COLOR_GREEN,COLOR_YELLOW,COLOR_BLUE,COLOR_PURPLE,COLOR_CYAN,COLOR_WHITE,COLOR_NONE};
enum Mode{MODE_NORMAL,MODE_BOLD,MODE_CURSIVE,MODE_Y,MODE_UNDERLINE,MODE_BLINK};

string color (Color color, string text, Mode m = MODE_NORMAL, Color background = COLOR_NONE);

extern bool no_colors_in_output;
