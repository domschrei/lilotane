/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 1 "src/parser/hddl.y" /* yacc.c:339  */

	#include <cstdio>
	#include <iostream>
	#include <vector>
	#include <cassert>
	#include <string.h>
	#include <algorithm>
	#include "parsetree.hpp"
	#include "domain.hpp"
	#include "cwa.hpp"
	
	using namespace std;
	
	// Declare stuff from Flex that Bison needs to know about:
	extern int yylex();
	extern int yyparse();
	extern FILE *yyin;
	char* current_parser_file_name;
	
	void yyerror(const char *s);

#line 88 "src/parser/hddl.cpp" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* In a future release of Bison, this section will be replaced
   by #include "hddl.hpp".  */
#ifndef YY_YY_SRC_PARSER_HDDL_HPP_INCLUDED
# define YY_YY_SRC_PARSER_HDDL_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    KEY_TYPES = 258,
    KEY_DEFINE = 259,
    KEY_DOMAIN = 260,
    KEY_PROBLEM = 261,
    KEY_REQUIREMENTS = 262,
    KEY_PREDICATES = 263,
    KEY_FUNCTIONS = 264,
    KEY_TASK = 265,
    KEY_CONSTANTS = 266,
    KEY_ACTION = 267,
    KEY_PARAMETERS = 268,
    KEY_PRECONDITION = 269,
    KEY_EFFECT = 270,
    KEY_METHOD = 271,
    KEY_GOAL = 272,
    KEY_INIT = 273,
    KEY_OBJECTS = 274,
    KEY_HTN = 275,
    KEY_TIHTN = 276,
    KEY_MIMIZE = 277,
    KEY_METRIC = 278,
    KEY_AND = 279,
    KEY_OR = 280,
    KEY_NOT = 281,
    KEY_IMPLY = 282,
    KEY_FORALL = 283,
    KEY_EXISTS = 284,
    KEY_WHEN = 285,
    KEY_INCREASE = 286,
    KEY_TYPEOF = 287,
    KEY_CAUSAL_LINKS = 288,
    KEY_CONSTRAINTS = 289,
    KEY_ORDER = 290,
    KEY_ORDER_TASKS = 291,
    KEY_TASKS = 292,
    NAME = 293,
    REQUIRE_NAME = 294,
    VAR_NAME = 295,
    FLOAT = 296,
    INT = 297
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 29 "src/parser/hddl.y" /* yacc.c:355  */

	bool bval;
	int ival;
	float fval;
	char *sval;
	std::vector<std::string>* vstring;
	var_declaration* vardecl;
	predicate_definition* preddecl;
	general_formula* formula;
	std::vector<predicate_definition*>* preddecllist;
	std::vector<general_formula*>* formulae;
	var_and_const* varandconst;
	sub_task* subtask;
	std::vector<sub_task*>* subtasks;
	std::pair<bool,std::vector<sub_task*>*>* osubtasks;
	parsed_task_network* tasknetwork;
	std::pair<string,string>* spair;
	std::vector<std::pair<string,string>*>* spairlist;

#line 191 "src/parser/hddl.cpp" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


extern YYSTYPE yylval;
extern YYLTYPE yylloc;
int yyparse (void);

#endif /* !YY_YY_SRC_PARSER_HDDL_HPP_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 222 "src/parser/hddl.cpp" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if 1

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
# define YYCOPY_NEEDED 1
#endif


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
             && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE) + sizeof (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  6
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   265

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  48
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  84
/* YYNRULES -- Number of rules.  */
#define YYNRULES  158
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  313

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   297

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      43,    44,     2,     2,     2,    46,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      47,    45,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   111,   111,   111,   114,   117,   118,   119,   120,   121,
     122,   123,   123,   125,   130,   131,   132,   133,   134,   135,
     136,   136,   138,   139,   140,   151,   161,   163,   165,   165,
     166,   166,   167,   188,   190,   191,   192,   196,   201,   202,
     202,   209,   210,   217,   230,   232,   232,   233,   242,   244,
     244,   245,   254,   256,   264,   264,   266,   266,   275,   275,
     277,   291,   291,   292,   292,   302,   334,   355,   356,   357,
     360,   360,   361,   361,   362,   362,   372,   373,   374,   375,
     376,   377,   378,   386,   387,   388,   389,   390,   391,   399,
     400,   401,   402,   403,   404,   405,   406,   413,   413,   413,
     414,   414,   415,   421,   422,   423,   424,   425,   426,   427,
     428,   429,   431,   432,   434,   435,   436,   437,   438,   439,
     440,   441,   443,   450,   451,   453,   453,   454,   465,   466,
     468,   469,   470,   471,   472,   473,   475,   476,   477,   478,
     481,   481,   482,   486,   487,   488,   489,   490,   491,   495,
     496,   497,   502,   503,   504,   508,   514,   515,   520
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "KEY_TYPES", "KEY_DEFINE", "KEY_DOMAIN",
  "KEY_PROBLEM", "KEY_REQUIREMENTS", "KEY_PREDICATES", "KEY_FUNCTIONS",
  "KEY_TASK", "KEY_CONSTANTS", "KEY_ACTION", "KEY_PARAMETERS",
  "KEY_PRECONDITION", "KEY_EFFECT", "KEY_METHOD", "KEY_GOAL", "KEY_INIT",
  "KEY_OBJECTS", "KEY_HTN", "KEY_TIHTN", "KEY_MIMIZE", "KEY_METRIC",
  "KEY_AND", "KEY_OR", "KEY_NOT", "KEY_IMPLY", "KEY_FORALL", "KEY_EXISTS",
  "KEY_WHEN", "KEY_INCREASE", "KEY_TYPEOF", "KEY_CAUSAL_LINKS",
  "KEY_CONSTRAINTS", "KEY_ORDER", "KEY_ORDER_TASKS", "KEY_TASKS", "NAME",
  "REQUIRE_NAME", "VAR_NAME", "FLOAT", "INT", "'('", "')'", "'='", "'-'",
  "'<'", "$accept", "document", "domain", "domain_defs", "problem",
  "problem_defs", "p_object_declaration", "p_init", "init_el", "p_goal",
  "htn_type", "parameters-option", "p_htn", "p_constraint", "p_metric",
  "metric_f_exp", "domain_symbol", "require_def", "require_defs",
  "type_def", "type_def_list", "const_def", "constant_declaration_list",
  "constant_declarations", "predicates_def", "atomic_predicate_def-list",
  "atomic_predicate_def", "functions_def",
  "typed_atomic_function_def-list", "typed_function_list_continuation",
  "atomic_function_def-list", "task_or_action", "task_def",
  "precondition_option", "effect_option", "method_def", "tasknetwork_def",
  "subtasks_option", "ordering_option", "constraints_option",
  "causal_links_option", "subtask_defs", "subtask_def-list", "subtask_def",
  "ordering_defs", "ordering_def-list", "ordering_def",
  "constraint_def-list", "constraint_def", "causallink_defs",
  "causallink_def-list", "causallink_def", "gd", "gd-list", "gd_empty",
  "gd_conjuction", "gd_disjuction", "gd_negation", "gd_implication",
  "gd_existential", "gd_universal", "gd_equality_constraint",
  "var_or_const-list", "var_or_const", "atomic_formula", "effect-list",
  "effect", "eff_empty", "eff_conjunction", "eff_universal",
  "eff_conditional", "literal", "neg_atomic_formula", "p_effect",
  "assign_op", "f_head", "f_exp", "NAME-list-non-empty", "NAME-list",
  "VAR_NAME-list-non-empty", "VAR_NAME-list", "typed_vars", "typed_var",
  "typed_var_list", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,    40,    41,    61,    45,    60
};
# endif

#define YYPACT_NINF -183

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-183)))

#define YYTABLE_NINF -154

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
       1,    51,    52,  -183,  -183,    18,  -183,    10,    32,    43,
    -183,    46,    50,  -183,    55,    -9,   114,   101,  -183,  -183,
    -183,  -183,  -183,  -183,  -183,  -183,    68,  -183,  -183,  -183,
    -183,  -183,  -183,  -183,    90,    95,    93,   118,   119,   125,
      33,    88,   120,    -1,   122,   154,   154,  -183,  -183,   130,
     123,  -183,  -183,   132,  -183,  -183,  -183,   133,  -183,  -183,
    -183,  -183,   126,   125,   131,   163,   161,    92,  -183,  -183,
    -183,   138,  -183,   134,   135,   164,     2,  -183,  -183,  -183,
    -183,  -183,  -183,  -183,  -183,  -183,   136,  -183,  -183,   137,
     144,     3,  -183,  -183,  -183,  -183,  -183,  -183,  -183,  -183,
    -183,  -183,   141,   142,   135,  -183,  -183,  -183,  -183,   165,
     135,   154,  -183,   139,   148,  -183,  -183,  -183,  -183,  -183,
     135,   135,   146,   147,  -183,  -183,    40,    38,  -183,  -183,
    -183,  -183,  -183,  -183,  -183,  -183,  -183,  -183,   149,    99,
     150,    62,   151,   108,   153,   152,    47,   105,   107,   155,
     135,  -183,  -183,    57,  -183,  -183,    40,  -183,   157,   158,
     135,  -183,  -183,    77,  -183,    48,  -183,  -183,  -183,  -183,
     159,   160,  -183,   162,   162,   166,   167,  -183,  -183,  -183,
     161,  -183,  -183,  -183,  -183,   168,   169,   170,  -183,   171,
     109,   173,   172,  -183,   141,  -183,   179,   -25,   175,   176,
    -183,    19,  -183,  -183,  -183,  -183,   178,   174,   164,  -183,
     135,   135,  -183,  -183,  -183,  -183,   180,   181,  -183,  -183,
    -183,   182,   -12,   156,  -183,  -183,   184,  -183,    27,  -183,
    -183,   185,   186,   108,   187,   188,   141,  -183,    78,  -183,
     189,   111,   191,    83,  -183,   183,  -183,    14,  -183,   192,
    -183,   190,  -183,  -183,   193,  -183,  -183,   198,  -183,  -183,
    -183,  -183,   113,   200,  -183,   196,   201,  -183,    40,    29,
    -183,  -183,  -183,  -183,    86,   202,  -183,  -183,   199,   115,
     -21,   177,   203,    40,  -183,   175,  -183,   204,  -183,  -183,
    -183,   201,    40,   206,  -183,   205,   117,   207,  -183,   208,
      40,  -183,  -183,   212,  -183,  -183,   209,   210,   211,  -183,
    -183,   213,  -183
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     2,     3,     0,     1,     0,     0,     0,
      37,     0,     0,    12,     0,     0,     0,     0,     4,     5,
       6,     7,     8,     9,    10,    11,     0,   151,    40,    50,
      57,    58,    46,    59,     0,     0,     0,     0,     0,    42,
       0,     0,     0,    55,   151,    31,    31,    21,    41,     0,
     150,    39,    38,     0,    48,    49,    52,     0,    56,    53,
      44,    45,     0,     0,     0,     0,    62,     0,   151,   158,
      57,     0,   158,     0,     0,    64,     0,    13,    15,    17,
      18,    16,    19,    20,    14,    43,   154,    54,    47,   154,
       0,     0,    61,   103,   107,   108,   105,   106,   109,   110,
     111,   104,     0,     0,     0,    26,    46,    28,    29,     0,
       0,    31,    51,     0,     0,   157,    30,   124,   113,   113,
       0,     0,     0,     0,   124,   114,     0,     0,   141,    63,
     130,   131,   132,   133,   134,   140,   135,    60,     0,     0,
     151,     0,     0,    69,     0,   152,     0,     0,     0,     0,
       0,   158,   158,     0,   125,   126,     0,   129,     0,     0,
       0,   144,   136,     0,    27,     0,    23,    24,    22,    35,
       0,     0,    33,     0,     0,     0,    71,   155,   122,   123,
      62,   115,   112,   116,   117,     0,   154,   154,   127,     0,
       0,     0,     0,   158,     0,   145,     0,     0,     0,     0,
      34,     0,    68,    77,    67,    32,     0,    73,    64,   118,
       0,     0,   121,   137,   128,   142,   154,     0,   124,   147,
     148,     0,     0,     0,    36,    80,   124,    76,     0,    70,
      84,     0,    75,    69,     0,     0,     0,   139,     0,   143,
       0,     0,     0,     0,    87,     0,    83,     0,    72,     0,
      66,     0,   120,   119,     0,   146,    25,     0,    78,    79,
     124,    81,     0,     0,    90,     0,     0,    91,     0,     0,
      74,    98,    65,   138,     0,     0,    85,    86,     0,     0,
       0,     0,     0,     0,   101,     0,    97,     0,    88,    92,
      89,     0,     0,     0,    95,     0,     0,     0,    82,     0,
       0,   156,    93,     0,    99,   100,     0,     0,     0,   102,
      96,     0,    94
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -183,  -183,  -183,  -183,  -183,  -183,  -183,  -183,  -183,  -183,
    -183,   -36,  -183,  -183,  -183,  -183,  -183,   129,  -183,  -183,
     194,  -183,    97,  -183,  -183,  -183,   215,  -183,   195,  -183,
    -183,  -183,  -183,    26,    34,  -183,   -26,  -183,  -183,  -183,
    -183,    35,  -183,   -19,  -183,  -183,   -16,  -183,   -28,  -183,
    -183,   -40,   -71,   140,  -183,  -183,  -183,  -183,  -183,  -183,
    -183,  -183,  -119,  -154,  -102,  -183,  -182,  -183,  -183,  -183,
    -183,  -138,  -183,  -183,  -183,    63,  -183,   -38,   -37,  -183,
    -183,  -183,   -30,   -68
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,     3,    15,     4,    67,    78,    79,   139,    80,
     111,    65,    81,    82,    83,   171,    11,    19,    40,    20,
      37,    21,    44,    61,    22,    41,    55,    23,    42,    59,
      43,    35,    24,    75,   103,    25,   175,   176,   207,   232,
     250,   202,   241,   203,   229,   262,   230,   279,   248,   270,
     296,   271,   182,   147,    93,    94,    95,    96,    97,    98,
      99,   100,   146,   156,   101,   190,   129,   130,   131,   132,
     133,   134,   135,   136,   163,   197,   221,    38,    39,   113,
     114,   115,   282,    86
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     128,   167,   189,    92,    89,   153,    62,    63,   214,    28,
      66,   291,   217,   195,   158,     8,     9,   219,   196,   104,
     105,   106,   107,   108,   292,   109,   124,   118,   119,   120,
     121,   122,   123,   138,    17,    18,   110,   128,   264,   142,
     265,   124,    53,   225,     1,    57,   266,   125,   126,   149,
     150,   244,     6,   284,   254,     5,   192,   226,   267,   268,
     223,     7,   157,   227,   158,   245,   159,   285,   160,   161,
      10,   246,    51,   286,   158,   143,   124,    52,   154,   185,
     155,    12,   162,   186,   187,   178,   124,   179,   128,   194,
      13,   180,   128,   198,    14,   178,   128,   179,    16,   238,
     169,   188,    62,    63,    27,   170,    36,   243,    28,    29,
      30,    31,    32,    33,   283,   195,   178,    34,   179,    26,
     196,   178,   255,   179,   178,   216,   179,   261,    45,   295,
     287,    53,    54,    46,   128,    76,    77,    47,   300,   234,
     235,   274,   165,   166,   173,   174,   308,   297,    91,   181,
      91,   183,   127,   213,   257,   258,   275,   276,   247,   289,
     303,   304,    48,    50,    56,    49,    60,    64,    68,  -149,
      69,    70,    71,    73,    72,    74,    88,    90,    91,   102,
     112,   116,   117,   128,   127,   144,   137,   141,   145,   151,
     152,   177,  -153,   164,   168,   172,    84,   199,   240,   184,
     191,   193,   206,   140,   200,   201,   208,   251,   231,   204,
     205,   124,   209,   210,   211,   212,   215,   218,   222,   249,
     224,   228,   259,   293,   236,   237,   239,   242,   247,   260,
     263,   252,   253,   256,   272,   269,   226,   273,   278,   280,
     245,   281,   233,   288,   301,   306,   277,   294,   298,   302,
     285,   290,   307,   309,   310,   311,   305,   312,    58,   148,
     220,   299,    85,     0,     0,    87
};

static const yytype_int16 yycheck[] =
{
     102,   139,   156,    74,    72,   124,    44,    44,   190,     7,
      46,    32,   194,    38,    26,     5,     6,    42,    43,    17,
      18,    19,    20,    21,    45,    23,    38,    24,    25,    26,
      27,    28,    29,   104,    43,    44,    34,   139,    24,   110,
      26,    38,    43,    24,    43,    46,    32,    44,    45,   120,
     121,    24,     0,    24,   236,     4,   158,    38,    44,    45,
     198,    43,    24,    44,    26,    38,    28,    38,    30,    31,
      38,    44,    39,    44,    26,   111,    38,    44,    38,   150,
      40,    38,    44,   151,   152,    38,    38,    40,   190,   160,
      44,    44,   194,    45,    44,    38,   198,    40,    43,   218,
      38,    44,   140,   140,     3,    43,    38,   226,     7,     8,
       9,    10,    11,    12,   268,    38,    38,    16,    40,     5,
      43,    38,    44,    40,    38,   193,    40,    44,    38,   283,
      44,    43,    44,    38,   236,    43,    44,    44,   292,   210,
     211,   260,    43,    44,    36,    37,   300,   285,    43,    44,
      43,    44,    43,    44,    43,    44,    43,    44,    43,    44,
      43,    44,    44,    38,    44,    46,    44,    13,    38,    46,
      38,    38,    46,    10,    43,    14,    38,    43,    43,    15,
      44,    44,    38,   285,    43,    46,    44,    22,    40,    43,
      43,    38,    40,    44,    44,    44,    67,    38,    42,    44,
      43,    43,    35,   106,    44,    43,   180,   233,    34,   174,
      44,    38,    44,    44,    44,    44,    44,    38,    43,    33,
      44,    43,   241,    46,    44,    44,    44,    43,    43,    38,
      47,    44,    44,    44,    44,    43,    38,    44,    38,    43,
      38,    40,   208,    44,    38,    38,   262,    44,    44,    44,
      38,   279,    44,    44,    44,    44,   296,    44,    43,   119,
     197,   291,    68,    -1,    -1,    70
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    43,    49,    50,    52,     4,     0,    43,     5,     6,
      38,    64,    38,    44,    44,    51,    43,    43,    44,    65,
      67,    69,    72,    75,    80,    83,     5,     3,     7,     8,
       9,    10,    11,    12,    16,    79,    38,    68,   125,   126,
      66,    73,    76,    78,    70,    38,    38,    44,    44,    46,
      38,    39,    44,    43,    44,    74,    44,    46,    74,    77,
      44,    71,   125,   126,    13,    59,    59,    53,    38,    38,
      38,    46,    43,    10,    14,    81,    43,    44,    54,    55,
      57,    60,    61,    62,    65,    68,   131,    76,    38,   131,
      43,    43,   100,   102,   103,   104,   105,   106,   107,   108,
     109,   112,    15,    82,    17,    18,    19,    20,    21,    23,
      34,    58,    44,   127,   128,   129,    44,    38,    24,    25,
      26,    27,    28,    29,    38,    44,    45,    43,   112,   114,
     115,   116,   117,   118,   119,   120,   121,    44,   100,    56,
      70,    22,   100,    59,    46,    40,   110,   101,   101,   100,
     100,    43,    43,   110,    38,    40,   111,    24,    26,    28,
      30,    31,    44,   122,    44,    43,    44,   119,    44,    38,
      43,    63,    44,    36,    37,    84,    85,    38,    38,    40,
      44,    44,   100,    44,    44,   100,   131,   131,    44,   111,
     113,    43,   112,    43,   100,    38,    43,   123,    45,    38,
      44,    43,    89,    91,    89,    44,    35,    86,    81,    44,
      44,    44,    44,    44,   114,    44,   131,   114,    38,    42,
     123,   124,    43,   119,    44,    24,    38,    44,    43,    92,
      94,    34,    87,    82,   100,   100,    44,    44,   110,    44,
      42,    90,    43,   110,    24,    38,    44,    43,    96,    33,
      88,    84,    44,    44,   114,    44,    44,    43,    44,    91,
      38,    44,    93,    47,    24,    26,    32,    44,    45,    43,
      97,    99,    44,    44,   110,    43,    44,    94,    38,    95,
      43,    40,   130,   111,    24,    38,    44,    44,    44,    44,
      96,    32,    45,    46,    44,   111,    98,   119,    44,   130,
     111,    38,    44,    43,    44,    99,    38,    44,   111,    44,
      44,    44,    44
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    48,    49,    49,    50,    51,    51,    51,    51,    51,
      51,    51,    51,    52,    53,    53,    53,    53,    53,    53,
      53,    53,    54,    55,    56,    56,    56,    57,    58,    58,
      59,    59,    60,    61,    62,    63,    63,    64,    65,    66,
      66,    67,    68,    68,    69,    70,    70,    71,    72,    73,
      73,    74,    75,    76,    77,    77,    78,    78,    79,    79,
      80,    81,    81,    82,    82,    83,    84,    85,    85,    85,
      86,    86,    87,    87,    88,    88,    89,    89,    89,    90,
      90,    91,    91,    92,    92,    92,    93,    93,    94,    95,
      95,    96,    96,    96,    96,    96,    96,    97,    97,    97,
      98,    98,    99,   100,   100,   100,   100,   100,   100,   100,
     100,   100,   101,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   110,   110,   111,   111,   112,   113,   113,
     114,   114,   114,   114,   114,   114,   115,   116,   117,   118,
     119,   119,   120,   121,   122,   123,   123,   124,   124,   125,
     126,   126,   127,   128,   128,   129,   130,   131,   131
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     1,     8,     2,     2,     2,     2,     2,
       2,     2,     0,    12,     2,     2,     2,     2,     2,     2,
       2,     0,     4,     4,     2,     6,     0,     4,     1,     1,
       4,     0,     5,     4,     5,     1,     3,     1,     4,     2,
       0,     4,     1,     4,     4,     2,     0,     3,     4,     2,
       0,     4,     4,     2,     3,     0,     2,     0,     1,     1,
       7,     2,     0,     2,     0,    13,     4,     2,     2,     0,
       2,     0,     2,     0,     2,     0,     2,     1,     4,     2,
       0,     4,     7,     2,     1,     4,     2,     0,     5,     2,
       0,     2,     4,     5,     8,     4,     7,     2,     1,     4,
       2,     0,     5,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     0,     2,     4,     4,     4,     5,     7,
       7,     5,     2,     2,     0,     1,     1,     4,     2,     0,
       1,     1,     1,     1,     1,     1,     2,     4,     7,     5,
       1,     1,     4,     5,     1,     1,     4,     1,     1,     2,
       2,     0,     2,     2,     0,     3,     3,     2,     0
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      YY_LAC_DISCARD ("YYBACKUP");                              \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static unsigned
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  unsigned res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
 }

#  define YY_LOCATION_PRINT(File, Loc)          \
  yy_location_print_ (File, &(Loc))

# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, Location); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (yylocationp);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                       , &(yylsp[(yyi + 1) - (yynrhs)])                       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

/* Given a state stack such that *YYBOTTOM is its bottom, such that
   *YYTOP is either its top or is YYTOP_EMPTY to indicate an empty
   stack, and such that *YYCAPACITY is the maximum number of elements it
   can hold without a reallocation, make sure there is enough room to
   store YYADD more elements.  If not, allocate a new stack using
   YYSTACK_ALLOC, copy the existing elements, and adjust *YYBOTTOM,
   *YYTOP, and *YYCAPACITY to reflect the new capacity and memory
   location.  If *YYBOTTOM != YYBOTTOM_NO_FREE, then free the old stack
   using YYSTACK_FREE.  Return 0 if successful or if no reallocation is
   required.  Return 1 if memory is exhausted.  */
static int
yy_lac_stack_realloc (YYSIZE_T *yycapacity, YYSIZE_T yyadd,
#if YYDEBUG
                      char const *yydebug_prefix,
                      char const *yydebug_suffix,
#endif
                      yytype_int16 **yybottom,
                      yytype_int16 *yybottom_no_free,
                      yytype_int16 **yytop, yytype_int16 *yytop_empty)
{
  YYSIZE_T yysize_old =
    *yytop == yytop_empty ? 0 : *yytop - *yybottom + 1;
  YYSIZE_T yysize_new = yysize_old + yyadd;
  if (*yycapacity < yysize_new)
    {
      YYSIZE_T yyalloc = 2 * yysize_new;
      yytype_int16 *yybottom_new;
      /* Use YYMAXDEPTH for maximum stack size given that the stack
         should never need to grow larger than the main state stack
         needs to grow without LAC.  */
      if (YYMAXDEPTH < yysize_new)
        {
          YYDPRINTF ((stderr, "%smax size exceeded%s", yydebug_prefix,
                      yydebug_suffix));
          return 1;
        }
      if (YYMAXDEPTH < yyalloc)
        yyalloc = YYMAXDEPTH;
      yybottom_new =
        (yytype_int16*) YYSTACK_ALLOC (yyalloc * sizeof *yybottom_new);
      if (!yybottom_new)
        {
          YYDPRINTF ((stderr, "%srealloc failed%s", yydebug_prefix,
                      yydebug_suffix));
          return 1;
        }
      if (*yytop != yytop_empty)
        {
          YYCOPY (yybottom_new, *yybottom, yysize_old);
          *yytop = yybottom_new + (yysize_old - 1);
        }
      if (*yybottom != yybottom_no_free)
        YYSTACK_FREE (*yybottom);
      *yybottom = yybottom_new;
      *yycapacity = yyalloc;
    }
  return 0;
}

/* Establish the initial context for the current lookahead if no initial
   context is currently established.

   We define a context as a snapshot of the parser stacks.  We define
   the initial context for a lookahead as the context in which the
   parser initially examines that lookahead in order to select a
   syntactic action.  Thus, if the lookahead eventually proves
   syntactically unacceptable (possibly in a later context reached via a
   series of reductions), the initial context can be used to determine
   the exact set of tokens that would be syntactically acceptable in the
   lookahead's place.  Moreover, it is the context after which any
   further semantic actions would be erroneous because they would be
   determined by a syntactically unacceptable token.

   YY_LAC_ESTABLISH should be invoked when a reduction is about to be
   performed in an inconsistent state (which, for the purposes of LAC,
   includes consistent states that don't know they're consistent because
   their default reductions have been disabled).  Iff there is a
   lookahead token, it should also be invoked before reporting a syntax
   error.  This latter case is for the sake of the debugging output.

   For parse.lac=full, the implementation of YY_LAC_ESTABLISH is as
   follows.  If no initial context is currently established for the
   current lookahead, then check if that lookahead can eventually be
   shifted if syntactic actions continue from the current context.
   Report a syntax error if it cannot.  */
#define YY_LAC_ESTABLISH                                         \
do {                                                             \
  if (!yy_lac_established)                                       \
    {                                                            \
      YYDPRINTF ((stderr,                                        \
                  "LAC: initial context established for %s\n",   \
                  yytname[yytoken]));                            \
      yy_lac_established = 1;                                    \
      {                                                          \
        int yy_lac_status =                                      \
          yy_lac (yyesa, &yyes, &yyes_capacity, yyssp, yytoken); \
        if (yy_lac_status == 2)                                  \
          goto yyexhaustedlab;                                   \
        if (yy_lac_status == 1)                                  \
          goto yyerrlab;                                         \
      }                                                          \
    }                                                            \
} while (0)

/* Discard any previous initial lookahead context because of Event,
   which may be a lookahead change or an invalidation of the currently
   established initial context for the current lookahead.

   The most common example of a lookahead change is a shift.  An example
   of both cases is syntax error recovery.  That is, a syntax error
   occurs when the lookahead is syntactically erroneous for the
   currently established initial context, so error recovery manipulates
   the parser stacks to try to find a new initial context in which the
   current lookahead is syntactically acceptable.  If it fails to find
   such a context, it discards the lookahead.  */
#if YYDEBUG
# define YY_LAC_DISCARD(Event)                                           \
do {                                                                     \
  if (yy_lac_established)                                                \
    {                                                                    \
      if (yydebug)                                                       \
        YYFPRINTF (stderr, "LAC: initial context discarded due to "      \
                   Event "\n");                                          \
      yy_lac_established = 0;                                            \
    }                                                                    \
} while (0)
#else
# define YY_LAC_DISCARD(Event) yy_lac_established = 0
#endif

/* Given the stack whose top is *YYSSP, return 0 iff YYTOKEN can
   eventually (after perhaps some reductions) be shifted, return 1 if
   not, or return 2 if memory is exhausted.  As preconditions and
   postconditions: *YYES_CAPACITY is the allocated size of the array to
   which *YYES points, and either *YYES = YYESA or *YYES points to an
   array allocated with YYSTACK_ALLOC.  yy_lac may overwrite the
   contents of either array, alter *YYES and *YYES_CAPACITY, and free
   any old *YYES other than YYESA.  */
static int
yy_lac (yytype_int16 *yyesa, yytype_int16 **yyes,
        YYSIZE_T *yyes_capacity, yytype_int16 *yyssp, int yytoken)
{
  yytype_int16 *yyes_prev = yyssp;
  yytype_int16 *yyesp = yyes_prev;
  YYDPRINTF ((stderr, "LAC: checking lookahead %s:", yytname[yytoken]));
  if (yytoken == YYUNDEFTOK)
    {
      YYDPRINTF ((stderr, " Always Err\n"));
      return 1;
    }
  while (1)
    {
      int yyrule = yypact[*yyesp];
      if (yypact_value_is_default (yyrule)
          || (yyrule += yytoken) < 0 || YYLAST < yyrule
          || yycheck[yyrule] != yytoken)
        {
          yyrule = yydefact[*yyesp];
          if (yyrule == 0)
            {
              YYDPRINTF ((stderr, " Err\n"));
              return 1;
            }
        }
      else
        {
          yyrule = yytable[yyrule];
          if (yytable_value_is_error (yyrule))
            {
              YYDPRINTF ((stderr, " Err\n"));
              return 1;
            }
          if (0 < yyrule)
            {
              YYDPRINTF ((stderr, " S%d\n", yyrule));
              return 0;
            }
          yyrule = -yyrule;
        }
      {
        YYSIZE_T yylen = yyr2[yyrule];
        YYDPRINTF ((stderr, " R%d", yyrule - 1));
        if (yyesp != yyes_prev)
          {
            YYSIZE_T yysize = yyesp - *yyes + 1;
            if (yylen < yysize)
              {
                yyesp -= yylen;
                yylen = 0;
              }
            else
              {
                yylen -= yysize;
                yyesp = yyes_prev;
              }
          }
        if (yylen)
          yyesp = yyes_prev -= yylen;
      }
      {
        int yystate;
        {
          int yylhs = yyr1[yyrule] - YYNTOKENS;
          yystate = yypgoto[yylhs] + *yyesp;
          if (yystate < 0 || YYLAST < yystate
              || yycheck[yystate] != *yyesp)
            yystate = yydefgoto[yylhs];
          else
            yystate = yytable[yystate];
        }
        if (yyesp == yyes_prev)
          {
            yyesp = *yyes;
            *yyesp = yystate;
          }
        else
          {
            if (yy_lac_stack_realloc (yyes_capacity, 1,
#if YYDEBUG
                                      " (", ")",
#endif
                                      yyes, yyesa, &yyesp, yyes_prev))
              {
                YYDPRINTF ((stderr, "\n"));
                return 2;
              }
            *++yyesp = yystate;
          }
        YYDPRINTF ((stderr, " G%d", yystate));
      }
    }
}


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.  In order to see if a particular token T is a
   valid looakhead, invoke yy_lac (YYESA, YYES, YYES_CAPACITY, YYSSP, T).

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store or if
   yy_lac returned 2.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyesa, yytype_int16 **yyes,
                YYSIZE_T *yyes_capacity, yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
       In the first two cases, it might appear that the current syntax
       error should have been detected in the previous state when yy_lac
       was invoked.  However, at that time, there might have been a
       different syntax error that discarded a different initial context
       during error recovery, leaving behind the current lookahead.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      YYDPRINTF ((stderr, "Constructing syntax error message\n"));
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          int yyx;

          for (yyx = 0; yyx < YYNTOKENS; ++yyx)
            if (yyx != YYTERROR && yyx != YYUNDEFTOK)
              {
                {
                  int yy_lac_status = yy_lac (yyesa, yyes, yyes_capacity,
                                              yyssp, yyx);
                  if (yy_lac_status == 2)
                    return 2;
                  if (yy_lac_status == 1)
                    continue;
                }
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
# if YYDEBUG
      else if (yydebug)
        YYFPRINTF (stderr, "No expected tokens.\n");
# endif
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Location data for the lookahead symbol.  */
YYLTYPE yylloc
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.
       'yyls': related to locations.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    /* The location stack.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;

    /* The locations where the error started and ended.  */
    YYLTYPE yyerror_range[3];

    YYSIZE_T yystacksize;

    yytype_int16 yyesa[20];
    yytype_int16 *yyes;
    YYSIZE_T yyes_capacity;

  int yy_lac_established = 0;
  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yylsp = yyls = yylsa;
  yystacksize = YYINITDEPTH;

  yyes = yyesa;
  yyes_capacity = sizeof yyesa / sizeof *yyes;
  if (YYMAXDEPTH < yyes_capacity)
    yyes_capacity = YYMAXDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  yylsp[0] = yylloc;
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yyls1, yysize * sizeof (*yylsp),
                    &yystacksize);

        yyls = yyls1;
        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    {
      YY_LAC_ESTABLISH;
      goto yydefault;
    }
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      YY_LAC_ESTABLISH;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  YY_LAC_DISCARD ("shift");

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location.  */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  YY_REDUCE_PRINT (yyn);
  {
    int yychar_backup = yychar;
    switch (yyn)
      {
          case 24:
#line 140 "src/parser/hddl.y" /* yacc.c:1646  */
    {
		assert((yyvsp[0].formula)->type == ATOM);
		map<string,string> access;
		// for each constant a new sort with a uniq name has been created. We access it here and retrieve its only element, the constant in questions
		for(auto x : (yyvsp[0].formula)->arguments.newVar) access[x.first] = *sorts[x.second].begin();  
		ground_literal l;
		l.positive = true;
		l.predicate = (yyvsp[0].formula)->predicate;
		for(string v : (yyvsp[0].formula)->arguments.vars) l.args.push_back(access[v]);
		init.push_back(l);
	}
#line 1869 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 25:
#line 151 "src/parser/hddl.y" /* yacc.c:1646  */
    {
		assert((yyvsp[-2].formula)->type == ATOM);
		map<string,string> access;
		// for each constant a new sort with a uniq name has been created. We access it here and retrieve its only element, the constant in questions
		for(auto x : (yyvsp[-2].formula)->arguments.newVar) access[x.first] = *sorts[x.second].begin();
		ground_literal l;
		l.positive = true;
		l.predicate = (yyvsp[-2].formula)->predicate;
		for(string v : (yyvsp[-2].formula)->arguments.vars) l.args.push_back(access[v]);
		init_functions.push_back(std::make_pair(l,(yyvsp[-1].ival)));
	}
#line 1885 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 27:
#line 163 "src/parser/hddl.y" /* yacc.c:1646  */
    {goal_formula = (yyvsp[-1].formula);}
#line 1891 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 29:
#line 165 "src/parser/hddl.y" /* yacc.c:1646  */
    {assert(false); /*we don't support ti-htn yet*/}
#line 1897 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 30:
#line 166 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.vardecl) = (yyvsp[-1].vardecl);}
#line 1903 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 31:
#line 166 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.vardecl) = new var_declaration(); }
#line 1909 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 32:
#line 170 "src/parser/hddl.y" /* yacc.c:1646  */
    {
		parsed_method m;
		m.name = "__top_method";		
		string atName("__top"); // later for insertion into map
		m.vars = (yyvsp[-2].vardecl);
		m.prec = new general_formula(); m.prec->type = EMPTY;
		m.eff = new general_formula(); m.eff->type = EMPTY;
		m.tn = (yyvsp[-1].tasknetwork);
		parsed_methods[atName].push_back(m);

		parsed_task	top;
		top.name = "__top";
		top.arguments = new var_declaration();
		top.prec = new general_formula(); top.prec->type = EMPTY;
		top.eff = new general_formula(); top.eff->type = EMPTY;
		parsed_abstract.push_back(top);
}
#line 1931 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 35:
#line 191 "src/parser/hddl.y" /* yacc.c:1646  */
    { metric_target = (yyvsp[0].sval); }
#line 1937 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 36:
#line 192 "src/parser/hddl.y" /* yacc.c:1646  */
    { metric_target = (yyvsp[-1].sval); }
#line 1943 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 39:
#line 202 "src/parser/hddl.y" /* yacc.c:1646  */
    {string r((yyvsp[0].sval)); if (r == ":typeof-predicate") has_typeof_predicate = true; }
#line 1949 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 41:
#line 209 "src/parser/hddl.y" /* yacc.c:1646  */
    { /*reverse list after all types have been parsed*/ reverse(sort_definitions.begin(), sort_definitions.end()); }
#line 1955 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 42:
#line 210 "src/parser/hddl.y" /* yacc.c:1646  */
    {	sort_definition s; s.has_parent_sort = false; s.declared_sorts = *((yyvsp[0].vstring)); delete (yyvsp[0].vstring);
			  				if (s.declared_sorts.size()) {
								sort_definitions.push_back(s);
								// touch constant map to ensure a consistent access
								for (string & ss : s.declared_sorts) sorts[ss].size();
							}
				}
#line 1967 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 43:
#line 217 "src/parser/hddl.y" /* yacc.c:1646  */
    {
							sort_definition s; s.has_parent_sort = true; s.parent_sort = (yyvsp[-1].sval); free((yyvsp[-1].sval));
							s.declared_sorts = *((yyvsp[-3].vstring)); delete (yyvsp[-3].vstring);
			  				sort_definitions.push_back(s);
							// touch constant map to ensure a consistent access
							for (string & ss : s.declared_sorts) sorts[ss].size();
							sorts[s.parent_sort].size();
							}
#line 1980 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 47:
#line 233 "src/parser/hddl.y" /* yacc.c:1646  */
    {
						string type((yyvsp[0].sval));
						for(unsigned int i = 0; i < (yyvsp[-2].vstring)->size(); i++)
							sorts[type].insert((*((yyvsp[-2].vstring)))[i]);
}
#line 1990 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 49:
#line 244 "src/parser/hddl.y" /* yacc.c:1646  */
    {predicate_definitions.push_back(*((yyvsp[0].preddecl))); delete (yyvsp[0].preddecl);}
#line 1996 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 51:
#line 245 "src/parser/hddl.y" /* yacc.c:1646  */
    {
		(yyval.preddecl) = new predicate_definition();
		(yyval.preddecl)->name = (yyvsp[-2].sval);
		for (unsigned int i = 0; i < (yyvsp[-1].vardecl)->vars.size(); i++) (yyval.preddecl)->argument_sorts.push_back((yyvsp[-1].vardecl)->vars[i].second);
	}
#line 2006 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 53:
#line 256 "src/parser/hddl.y" /* yacc.c:1646  */
    {
	char * type_of_functions = (yyvsp[0].sval);
	for (predicate_definition* p : *(yyvsp[-1].preddecllist)){
		parsed_functions.push_back(std::make_pair(*p,type_of_functions));
		delete p;
	}
	delete (yyvsp[-1].preddecllist);
}
#line 2019 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 54:
#line 264 "src/parser/hddl.y" /* yacc.c:1646  */
    { (yyval.sval) = (yyvsp[-1].sval); }
#line 2025 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 55:
#line 264 "src/parser/hddl.y" /* yacc.c:1646  */
    { (yyval.sval) = strdup(numeric_funtion_type.c_str()); }
#line 2031 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 56:
#line 266 "src/parser/hddl.y" /* yacc.c:1646  */
    { (yyval.preddecllist)->push_back((yyvsp[0].preddecl)); }
#line 2037 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 57:
#line 266 "src/parser/hddl.y" /* yacc.c:1646  */
    { (yyval.preddecllist) = new std::vector<predicate_definition*>();}
#line 2043 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 58:
#line 275 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.bval)=true;}
#line 2049 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 59:
#line 275 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.bval)=false;}
#line 2055 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 60:
#line 280 "src/parser/hddl.y" /* yacc.c:1646  */
    {
				// found a new task, add it to list
				parsed_task t;
				t.name = (yyvsp[-4].sval);
				t.arguments = (yyvsp[-3].vardecl);
				t.prec = (yyvsp[-2].formula); 
				t.eff = (yyvsp[-1].formula);

				if ((yyvsp[-5].bval)) parsed_abstract.push_back(t); else parsed_primitive.push_back(t);
}
#line 2070 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 61:
#line 291 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2076 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 62:
#line 291 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type = EMPTY;}
#line 2082 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 63:
#line 292 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2088 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 64:
#line 292 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type = EMPTY;}
#line 2094 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 65:
#line 308 "src/parser/hddl.y" /* yacc.c:1646  */
    {
		parsed_method m;
		m.name = (yyvsp[-10].sval);		
		string atName((yyvsp[-6].sval)); // later for insertion into map
		m.atArguments = (yyvsp[-5].varandconst)->vars; 
		m.newVarForAT = (yyvsp[-5].varandconst)->newVar;
		m.vars = (yyvsp[-9].vardecl);
		m.prec = (yyvsp[-3].formula);
		m.eff = (yyvsp[-2].formula);
		m.tn = (yyvsp[-1].tasknetwork);

		parsed_methods[atName].push_back(m);
	}
#line 2112 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 66:
#line 337 "src/parser/hddl.y" /* yacc.c:1646  */
    {
	(yyval.tasknetwork) = new parsed_task_network();
	(yyval.tasknetwork)->tasks = *((yyvsp[-3].osubtasks)->second);
	(yyval.tasknetwork)->ordering = *((yyvsp[-2].spairlist));
	if ((yyvsp[-3].osubtasks)->first){
		if ((yyval.tasknetwork)->ordering.size()) assert(false); // given ordering but said that this is a total order
		for(unsigned int i = 1; i < (yyval.tasknetwork)->tasks.size(); i++){
			pair<string,string>* o = new pair<string,string>();
			o->first = (yyval.tasknetwork)->tasks[i-1]->id;
			o->second = (yyval.tasknetwork)->tasks[i]->id;
			(yyval.tasknetwork)->ordering.push_back(o);
		}
	}
	(yyval.tasknetwork)->constraint = (yyvsp[-1].formula);

	// TODO causal links?????
}
#line 2134 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 67:
#line 355 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.osubtasks) = new pair<bool,vector<sub_task*>*>(); (yyval.osubtasks)->first = false; (yyval.osubtasks)->second = (yyvsp[0].subtasks); }
#line 2140 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 68:
#line 356 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.osubtasks) = new pair<bool,vector<sub_task*>*>(); (yyval.osubtasks)->first = true; (yyval.osubtasks)->second = (yyvsp[0].subtasks); }
#line 2146 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 69:
#line 357 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.osubtasks) = new pair<bool,vector<sub_task*>*>();
					   (yyval.osubtasks)->first = true; (yyval.osubtasks)->second = new vector<sub_task*>();}
#line 2153 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 70:
#line 360 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.spairlist) = (yyvsp[0].spairlist);}
#line 2159 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 71:
#line 360 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.spairlist) = new vector<pair<string,string>*>();}
#line 2165 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 72:
#line 361 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2171 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 73:
#line 361 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type = EMPTY;}
#line 2177 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 76:
#line 372 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.subtasks) = new vector<sub_task*>();}
#line 2183 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 77:
#line 373 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.subtasks) = new vector<sub_task*>(); (yyval.subtasks)->push_back((yyvsp[0].subtask));}
#line 2189 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 78:
#line 374 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.subtasks) = (yyvsp[-1].subtasks);}
#line 2195 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 79:
#line 375 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.subtasks) = (yyvsp[-1].subtasks); (yyval.subtasks)->push_back((yyvsp[0].subtask));}
#line 2201 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 80:
#line 376 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.subtasks) = new vector<sub_task*>();}
#line 2207 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 81:
#line 377 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.subtask) = new sub_task(); (yyval.subtask)->id = "__t_id_" + to_string(task_id_counter); task_id_counter++; (yyval.subtask)->task = (yyvsp[-2].sval); (yyval.subtask)->arguments = (yyvsp[-1].varandconst); }
#line 2213 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 82:
#line 378 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.subtask) = new sub_task(); (yyval.subtask)->id = (yyvsp[-5].sval); (yyval.subtask)->task = (yyvsp[-3].sval); (yyval.subtask)->arguments = (yyvsp[-2].varandconst); }
#line 2219 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 83:
#line 386 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.spairlist) = new vector<pair<string,string>*>();}
#line 2225 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 84:
#line 387 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.spairlist) = new vector<pair<string,string>*>(); (yyval.spairlist)->push_back((yyvsp[0].spair));}
#line 2231 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 85:
#line 388 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.spairlist) = (yyvsp[-1].spairlist);}
#line 2237 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 86:
#line 389 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.spairlist) = (yyvsp[-1].spairlist); (yyval.spairlist)->push_back((yyvsp[0].spair));}
#line 2243 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 87:
#line 390 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.spairlist) = new vector<pair<string,string>*>();}
#line 2249 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 88:
#line 391 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.spair) = new pair<string,string>(); (yyval.spair)->first = (yyvsp[-3].sval); (yyval.spair)->second = (yyvsp[-1].sval);}
#line 2255 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 89:
#line 399 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formulae) = (yyvsp[-1].formulae); (yyval.formulae)->push_back((yyvsp[0].formula));}
#line 2261 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 90:
#line 400 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formulae) = new vector<general_formula*>();}
#line 2267 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 91:
#line 401 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type = EMPTY;}
#line 2273 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 92:
#line 402 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type=AND; (yyval.formula)->subformulae = *((yyvsp[-1].formulae));}
#line 2279 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 93:
#line 403 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type = EQUAL; (yyval.formula)->arg1 = (yyvsp[-2].sval); (yyval.formula)->arg2 = (yyvsp[-1].sval);}
#line 2285 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 94:
#line 404 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type = NOTEQUAL; (yyval.formula)->arg1 = (yyvsp[-3].sval); (yyval.formula)->arg2 = (yyvsp[-2].sval);}
#line 2291 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 95:
#line 405 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type = OFSORT; (yyval.formula)->arg1 = (yyvsp[-1].vardecl)->vars[0].first; (yyval.formula)->arg2 = (yyvsp[-1].vardecl)->vars[0].second; }
#line 2297 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 96:
#line 406 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type = NOTOFSORT; (yyval.formula)->arg1 = (yyvsp[-2].vardecl)->vars[0].first; (yyval.formula)->arg2 = (yyvsp[-2].vardecl)->vars[0].second; }
#line 2303 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 103:
#line 421 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2309 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 104:
#line 422 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2315 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 105:
#line 423 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2321 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 106:
#line 424 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2327 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 107:
#line 425 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2333 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 108:
#line 426 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2339 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 109:
#line 427 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2345 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 110:
#line 428 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2351 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 111:
#line 429 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2357 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 112:
#line 431 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formulae) = (yyvsp[-1].formulae); (yyval.formulae)->push_back((yyvsp[0].formula));}
#line 2363 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 113:
#line 432 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formulae) = new vector<general_formula*>();}
#line 2369 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 114:
#line 434 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type=EMPTY;}
#line 2375 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 115:
#line 435 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type=AND; (yyval.formula)->subformulae = *((yyvsp[-1].formulae));}
#line 2381 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 116:
#line 436 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type=OR; (yyval.formula)->subformulae = *((yyvsp[-1].formulae));}
#line 2387 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 117:
#line 437 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[-1].formula); (yyval.formula)->negate();}
#line 2393 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 118:
#line 438 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type=OR; (yyvsp[-2].formula)->negate(); (yyval.formula)->subformulae.push_back((yyvsp[-2].formula)); (yyval.formula)->subformulae.push_back((yyvsp[-1].formula));}
#line 2399 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 119:
#line 439 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type = EXISTS; (yyval.formula)->subformulae.push_back((yyvsp[-1].formula)); (yyval.formula)->qvariables = *((yyvsp[-3].vardecl));}
#line 2405 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 120:
#line 440 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type = FORALL; (yyval.formula)->subformulae.push_back((yyvsp[-1].formula)); (yyval.formula)->qvariables = *((yyvsp[-3].vardecl));}
#line 2411 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 121:
#line 441 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type = EQUAL; (yyval.formula)->arg1 = (yyvsp[-2].sval); (yyval.formula)->arg2 = (yyvsp[-1].sval);}
#line 2417 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 122:
#line 443 "src/parser/hddl.y" /* yacc.c:1646  */
    {
						(yyval.varandconst) = (yyvsp[-1].varandconst);
						string c((yyvsp[0].sval)); string s = "sort_for_" + c; string v = "?var_for_" + c;
						sorts[s].insert(c);
						(yyval.varandconst)->vars.push_back(v);
						(yyval.varandconst)->newVar.insert(make_pair(v,s));
					}
#line 2429 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 123:
#line 450 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.varandconst) = (yyvsp[-1].varandconst); string s((yyvsp[0].sval)); (yyval.varandconst)->vars.push_back(s);}
#line 2435 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 124:
#line 451 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.varandconst) = new var_and_const();}
#line 2441 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 125:
#line 453 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.sval)=(yyvsp[0].sval);}
#line 2447 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 126:
#line 453 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.sval)=(yyvsp[0].sval);}
#line 2453 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 127:
#line 454 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type=ATOM;
			   								   (yyval.formula)->predicate = (yyvsp[-2].sval); (yyval.formula)->arguments = *((yyvsp[-1].varandconst));
											  }
#line 2461 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 128:
#line 465 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formulae) = (yyvsp[-1].formulae); (yyval.formulae)->push_back((yyvsp[0].formula));}
#line 2467 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 129:
#line 466 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formulae) = new vector<general_formula*>();}
#line 2473 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 130:
#line 468 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2479 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 131:
#line 469 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2485 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 132:
#line 470 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2491 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 133:
#line 471 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2497 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 134:
#line 472 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2503 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 135:
#line 473 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2509 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 136:
#line 475 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type=EMPTY;}
#line 2515 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 137:
#line 476 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type=AND; (yyval.formula)->subformulae = *((yyvsp[-1].formulae));}
#line 2521 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 138:
#line 477 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type = FORALL; (yyval.formula)->subformulae.push_back((yyvsp[-1].formula)); (yyval.formula)->qvariables = *((yyvsp[-3].vardecl));}
#line 2527 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 139:
#line 478 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type=WHEN; (yyval.formula)->subformulae.push_back((yyvsp[-2].formula)); (yyval.formula)->subformulae.push_back((yyvsp[-1].formula));}
#line 2533 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 140:
#line 481 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2539 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 141:
#line 481 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[0].formula);}
#line 2545 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 142:
#line 482 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = (yyvsp[-1].formula); (yyval.formula)->negate();}
#line 2551 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 143:
#line 486 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.formula) = new general_formula(); (yyval.formula)->type=COST_CHANGE; (yyval.formula)->subformulae.push_back((yyvsp[-2].formula)); (yyval.formula)->subformulae.push_back((yyvsp[-1].formula)); }
#line 2557 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 145:
#line 488 "src/parser/hddl.y" /* yacc.c:1646  */
    { (yyval.formula) = new general_formula(); (yyval.formula)->type = COST; (yyval.formula)->predicate = (yyvsp[0].sval); }
#line 2563 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 146:
#line 489 "src/parser/hddl.y" /* yacc.c:1646  */
    { (yyval.formula) = new general_formula(); (yyval.formula)->type = COST; (yyval.formula)->predicate = (yyvsp[-2].sval); (yyval.formula)->arguments = *((yyvsp[-1].varandconst)); }
#line 2569 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 147:
#line 490 "src/parser/hddl.y" /* yacc.c:1646  */
    { (yyval.formula) = new general_formula(); (yyval.formula)->type = VALUE; (yyval.formula)->value = (yyvsp[0].ival); }
#line 2575 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 148:
#line 491 "src/parser/hddl.y" /* yacc.c:1646  */
    { (yyval.formula) = (yyvsp[0].formula); }
#line 2581 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 149:
#line 495 "src/parser/hddl.y" /* yacc.c:1646  */
    {string s((yyvsp[0].sval)); free((yyvsp[0].sval)); (yyval.vstring)->push_back(s);}
#line 2587 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 150:
#line 496 "src/parser/hddl.y" /* yacc.c:1646  */
    {string s((yyvsp[0].sval)); free((yyvsp[0].sval)); (yyval.vstring)->push_back(s);}
#line 2593 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 151:
#line 497 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.vstring) = new vector<string>();}
#line 2599 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 152:
#line 502 "src/parser/hddl.y" /* yacc.c:1646  */
    {string s((yyvsp[0].sval)); free((yyvsp[0].sval)); (yyval.vstring)->push_back(s);}
#line 2605 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 153:
#line 503 "src/parser/hddl.y" /* yacc.c:1646  */
    {string s((yyvsp[0].sval)); free((yyvsp[0].sval)); (yyval.vstring)->push_back(s);}
#line 2611 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 154:
#line 504 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.vstring) = new vector<string>();}
#line 2617 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 155:
#line 508 "src/parser/hddl.y" /* yacc.c:1646  */
    {
		   	(yyval.vardecl) = new var_declaration;
			string t((yyvsp[0].sval));
			for (unsigned int i = 0; i < (yyvsp[-2].vstring)->size(); i++)
				(yyval.vardecl)->vars.push_back(make_pair((*((yyvsp[-2].vstring)))[i],t));
			}
#line 2628 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 156:
#line 514 "src/parser/hddl.y" /* yacc.c:1646  */
    { (yyval.vardecl) = new var_declaration; string v((yyvsp[-2].sval)); string t((yyvsp[0].sval)); (yyval.vardecl)->vars.push_back(make_pair(v,t));}
#line 2634 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 157:
#line 515 "src/parser/hddl.y" /* yacc.c:1646  */
    {
			   		(yyval.vardecl) = (yyvsp[-1].vardecl);
					for (unsigned int i = 0; i < (yyvsp[0].vardecl)->vars.size(); i++) (yyval.vardecl)->vars.push_back((yyvsp[0].vardecl)->vars[i]);
					delete (yyvsp[0].vardecl);
				}
#line 2644 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;

  case 158:
#line 520 "src/parser/hddl.y" /* yacc.c:1646  */
    {(yyval.vardecl) = new var_declaration;}
#line 2650 "src/parser/hddl.cpp" /* yacc.c:1646  */
    break;


#line 2654 "src/parser/hddl.cpp" /* yacc.c:1646  */
        default: break;
      }
    if (yychar_backup != yychar)
      YY_LAC_DISCARD ("yychar change");
  }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyesa, &yyes, &yyes_capacity, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        if (yychar != YYEMPTY)
          YY_LAC_ESTABLISH;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }

  yyerror_range[1] = yylloc;

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  yyerror_range[1] = yylsp[1-yylen];
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, yylsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  /* If the stack popping above didn't lose the initial context for the
     current lookahead token, the shift below will for sure.  */
  YY_LAC_DISCARD ("error recovery");

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the lookahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, yyerror_range, 2);
  *++yylsp = yyloc;

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if 1
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, yylsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  if (yyes != yyesa)
    YYSTACK_FREE (yyes);
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 522 "src/parser/hddl.y" /* yacc.c:1906  */

void run_parser_on_file(FILE* f, char* filename){
	current_parser_file_name = filename;
	yyin = f;
	yyparse();
}

void yyerror(const char *s) {
  cout << "\x1b[31mParse error\x1b[0m in file " << current_parser_file_name << " in line \x1b[1m" << yylloc.first_line << "\x1b[0m" << endl;
  if (strlen(s) >= 14 && (strncmp("syntax error, ",s,14) == 0)){
    s += 14;
  }
  cout << s << endl;
  // might as well halt now:
  exit(-1);
}
