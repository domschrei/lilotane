#ifndef __VERIFY
#define __VERIFY

#include "parsetree.hpp"

bool verify_plan(istream & plan, bool use_order_information, bool lenient_mode, int verbose_output);

#endif
