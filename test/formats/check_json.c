#include "m_config.h"
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, srand, rand */
#include <check.h>

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_formats.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern Suite *M_json_suite(void);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static struct {
	const char *data;
	const char *out;
	M_uint32    writer_flags;
} check_json_valid_data[] = {
	/* Values in an object. */
	{ "{}",                  "{}",                            M_JSON_WRITER_NONE                                                   },
	{ "{}",                  "{\n}",                          M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "{}",                  "{\n}",                          M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "{}",                  "{\r\n}",                        M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "{  }",                "{}",                            M_JSON_WRITER_NONE                                                   },
	{ "{  }",                "{\n}",                          M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "{  }",                "{\n}",                          M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "{  }",                "{\r\n}",                        M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "{ \n }",              "{}",                            M_JSON_WRITER_NONE                                                   },
	{ "{ \n }",              "{\n}",                          M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "{ \n }",              "{\n}",                          M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "{ \n }",              "{\r\n}",                        M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "{ \"a\":1 }",         "{\"a\":1}",                     M_JSON_WRITER_NONE                                                   },
	{ "{ \"a\":1 }",         "{\n\t\"a\" : 1\n}",             M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "{ \"a\":1 }",         "{\n  \"a\" : 1\n}",             M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "{ \"a\":1 }",         "{\r\n  \"a\" : 1\r\n}",         M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "{ \"a\":0.55 }",      "{\"a\":0.55}",                  M_JSON_WRITER_NONE                                                   },
	{ "{ \"a\":0.55 }",      "{\n\t\"a\" : 0.55\n}",          M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "{ \"a\":0.55 }",      "{\n  \"a\" : 0.55\n}",          M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "{ \"a\":0.55 }",      "{\r\n  \"a\" : 0.55\r\n}",      M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "{ \"a\":0.5500 }",    "{\"a\":0.55}",                  M_JSON_WRITER_NONE                                                   },
	{ "{ \"a\":\"1\" }",     "{\"a\":\"1\"}",                 M_JSON_WRITER_NONE                                                   },
	{ "{ \"a\":\"1\" }",     "{\n\t\"a\" : \"1\"\n}",         M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "{ \"a\":\"1\" }",     "{\n  \"a\" : \"1\"\n}",         M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "{ \"a\":\"1\" }",     "{\r\n  \"a\" : \"1\"\r\n}",     M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "{ \"a\":\"1\\n2\" }", "{\"a\":\"1\\n2\"}",             M_JSON_WRITER_NONE                                                   },
	{ "{ \"a\":\"1\\n2\" }", "{\n\t\"a\" : \"1\\n2\"\n}",     M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "{ \"a\":\"1\\n2\" }", "{\n  \"a\" : \"1\\n2\"\n}",     M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "{ \"a\":\"1\\n2\" }", "{\r\n  \"a\" : \"1\\n2\"\r\n}", M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "{ \"a\":true }",      "{\"a\":true}",                  M_JSON_WRITER_NONE                                                   },
	{ "{ \"a\":true }",      "{\n\t\"a\" : true\n}",          M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "{ \"a\":true }",      "{\n  \"a\" : true\n}",          M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "{ \"a\":true }",      "{\r\n  \"a\" : true\r\n}",      M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "{ \"b\":false}",      "{\"b\":false}",                 M_JSON_WRITER_NONE                                                   },
	{ "{ \"b\":false}",      "{\n\t\"b\" : false\n}",         M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "{ \"b\":false}",      "{\n  \"b\" : false\n}",         M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "{ \"b\":false}",      "{\r\n  \"b\" : false\r\n}",     M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "{ \"c\":null}",       "{\"c\":null}",                  M_JSON_WRITER_NONE                                                   },
	{ "{ \"c\":null}",       "{\n\t\"c\" : null\n}",          M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "{ \"c\":null}",       "{\n  \"c\" : null\n}",          M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "{ \"c\":null}",       "{\r\n  \"c\" : null\r\n}",      M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	/* Values in an array. */
	{ "[]",                  "[]",                            M_JSON_WRITER_NONE                                                   },
	{ "[]",                  "[\n]",                          M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[]",                  "[\n]",                          M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[]",                  "[\r\n]",                        M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[  ]",                "[]",                            M_JSON_WRITER_NONE                                                   },
	{ "[  ]",                "[\n]",                          M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[  ]",                "[\n]",                          M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[  ]",                "[\r\n]",                        M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[ \n ]",              "[]",                            M_JSON_WRITER_NONE                                                   },
	{ "[ \n ]",              "[\n]",                          M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ \n ]",              "[\n]",                          M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ \n ]",              "[\r\n]",                        M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[ 1 ]",               "[1]",                           M_JSON_WRITER_NONE                                                   },
	{ "[ 1 ]",               "[\n\t1\n]",                     M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ 1 ]",               "[\n  1\n]",                     M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ 1 ]",               "[\r\n  1\r\n]",                 M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[ 1, 2]",             "[1,2]",                         M_JSON_WRITER_NONE                                                   },
	{ "[ 1, 2]",             "[\n\t1,\n\t2\n]",               M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ 1, 2]",             "[\n  1,\n  2\n]",               M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ 1, 2]",             "[\r\n  1,\r\n  2\r\n]",         M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[ 1, 2 ]",            "[1,2]",                         M_JSON_WRITER_NONE                                                   },
	{ "[ 1, 2 ]",            "[\n\t1,\n\t2\n]",               M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ 1, 2 ]",            "[\n  1,\n  2\n]",               M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ 1, 2 ]",            "[\r\n  1,\r\n  2\r\n]",         M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[ \"ab\\\"cd\" ]",    "[\"ab\\\"cd\"]",                M_JSON_WRITER_NONE                                                   },
	{ "[ \"ab\\\"cd\" ]",    "[\n\t\"ab\\\"cd\"\n]",          M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ \"ab\\\"cd\" ]",    "[\n  \"ab\\\"cd\"\n]",          M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ \"ab\\\"cd\" ]",    "[\r\n  \"ab\\\"cd\"\r\n]",      M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[0.55, 5.01]",        "[0.55,5.01]",                   M_JSON_WRITER_NONE                                                   },
	{ "[0.55, 5.01]",        "[\n\t0.55,\n\t5.01\n]",         M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[0.55, 5.01]",        "[\n  0.55,\n  5.01\n]",         M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[0.55, 5.01]",        "[\r\n  0.55,\r\n  5.01\r\n]",   M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[0.550, 5.0100000]",  "[0.55,5.01]",                   M_JSON_WRITER_NONE                                                   },
	{ "[ 1, \"abc\",2 ]",    "[1,\"abc\",2]",                 M_JSON_WRITER_NONE                                                   },
	{ "[ 1, \"abc\",2 ]",    "[\n\t1,\n\t\"abc\",\n\t2\n]",   M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ 1, \"abc\",2 ]",    "[\n  1,\n  \"abc\",\n  2\n]",   M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ 1, \"abc\",2 ]",    "[\r\n  1,\r\n  \"abc\",\r\n  2\r\n]",
	    M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND                                                       },
	{ "[ true ]",            "[true]",                        M_JSON_WRITER_NONE                                                   },
	{ "[ true ]",            "[\n\ttrue\n]",                  M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ true ]",            "[\n  true\n]",                  M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ true ]",            "[\r\n  true\r\n]",              M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[ false,true ]",      "[false,true]",                  M_JSON_WRITER_NONE                                                   },
	{ "[ false,true ]",      "[\n\tfalse,\n\ttrue\n]",        M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ false,true ]",      "[\n  false,\n  true\n]",        M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ false,true ]",      "[\r\n  false,\r\n  true\r\n]",  M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[ null]",             "[null]",                        M_JSON_WRITER_NONE                                                   },
	{ "[ null]",             "[\n\tnull\n]",                  M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ null]",             "[\n  null\n]",                  M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ null]",             "[\r\n  null\r\n]",              M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[ false,null  ,  true,  null, false, true, null, 1, 12, \"abc\\nalpah\"]",
	    "[false,null,true,null,false,true,null,1,12,\"abc\\nalpah\"]",
	    M_JSON_WRITER_NONE                                                                                                         },
	{ "[ false,null  ,  true,  null, false, true, null, 1, 12, \"abc\\nalpah\"]",
	    "[\n\tfalse,\n\tnull,\n\ttrue,\n\tnull,\n\tfalse,\n\ttrue,\n\tnull,\n\t1,\n\t12,\n\t\"abc\\nalpah\"\n]",
	    M_JSON_WRITER_PRETTYPRINT_TAB                                                                                              },
	{ "[ false,null  ,  true,  null, false, true, null, 1, 12, \"abc\\nalpah\"]",
	    "[\n  false,\n  null,\n  true,\n  null,\n  false,\n  true,\n  null,\n  1,\n  12,\n  \"abc\\nalpah\"\n]",
	    M_JSON_WRITER_PRETTYPRINT_SPACE                                                                                            },
	{ "[ false,null  ,  true,  null, false, true, null, 1, 12, \"abc\\nalpah\"]",
	    "[\r\n  false,\r\n  null,\r\n  true,\r\n  null,\r\n  false,\r\n  true,\r\n  null," \
	    "\r\n  1,\r\n  12,\r\n  \"abc\\nalpah\"\r\n]",
	    M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND                                                       },
	/* Nested */
	{ "{ \"b\" : [1, 2]}",   "{\"b\":[1,2]}",                 M_JSON_WRITER_NONE                                                   },
	{ "{ \"b\" : [1, 2]}",   "{\n\t\"b\" : [\n\t\t1,\n\t\t2\n\t]\n}",
	    M_JSON_WRITER_PRETTYPRINT_TAB                                                                                              },
	{ "{ \"b\" : [1, 2]}",   "{\n  \"b\" : [\n    1,\n    2\n  ]\n}",
	    M_JSON_WRITER_PRETTYPRINT_SPACE                                                                                            },
	{ "{ \"b\" : [1, 2]}",   "{\r\n  \"b\" : [\r\n    1,\r\n    2\r\n  ]\r\n}",
	    M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND                                                       },
	{ "{ \"b\"   :[1,2 ]}",  "{\"b\":[1,2]}",
	    M_JSON_WRITER_NONE                                                                                                         },
	{ "{ \"b\"   :[1,2 ]}",  "{\n\t\"b\" : [\n\t\t1,\n\t\t2\n\t]\n}",
	    M_JSON_WRITER_PRETTYPRINT_TAB                                                                                              },
	{ "{ \"b\"   :[1,2 ]}",  "{\n  \"b\" : [\n    1,\n    2\n  ]\n}",
	    M_JSON_WRITER_PRETTYPRINT_SPACE                                                                                            },
	{ "{ \"b\"   :[1,2 ]}",  "{\r\n  \"b\" : [\r\n    1,\r\n    2\r\n  ]\r\n}",
	    M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND                                                       },
	{ "{ \"a\" :\n[1, \"abc\",2 ]\n}",
	    "{\"a\":[1,\"abc\",2]}",                              M_JSON_WRITER_NONE                                                   },
	{ "{ \"a\" :\n[1, \"abc\",2 ]\n}",
	    "{\n\t\"a\" : [\n\t\t1,\n\t\t\"abc\",\n\t\t2\n\t]\n}", M_JSON_WRITER_PRETTYPRINT_TAB                                       },
	{ "{ \"a\" :\n[1, \"abc\",2 ]\n}",
	    "{\n  \"a\" : [\n    1,\n    \"abc\",\n    2\n  ]\n}", M_JSON_WRITER_PRETTYPRINT_SPACE                                     },
	{ "{ \"a\" :\n[1, \"abc\",2 ]\n}",
	    "{\r\n  \"a\" : [\r\n    1,\r\n    \"abc\",\r\n    2\r\n  ]\r\n}",
	    M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND                                                       },
	{ "[ { \"a\" : 1 }, { \"b\":[ 1, \"x\", \"y\", 2 ] }, [ false, null, true], { \"d\": false } ]",
	    "[{\"a\":1},{\"b\":[1,\"x\",\"y\",2]},[false,null,true],{\"d\":false}]",
	    M_JSON_WRITER_NONE                                                                                                         },
	{ "[ { \"a\" : 1 }, { \"b\":[ 1, \"x\", \"y\", 2 ] }, [ false, null, true], { \"d\": false } ]",
	    "[\n\t{\n\t\t\"a\" : 1\n\t},\n\t{\n\t\t\"b\" : [\n\t\t\t1,\n\t\t\t\"x\",\n\t\t\t\"y\"," \
	    "\n\t\t\t2\n\t\t]\n\t},\n\t[\n\t\tfalse,\n\t\tnull,\n\t\ttrue\n\t],\n\t{\n\t\t\"d\" : false\n\t}\n]",
	    M_JSON_WRITER_PRETTYPRINT_TAB },
	{ "[ { \"a\" : 1 }, { \"b\":[ 1, \"x\", \"y\", 2 ] }, [ false, null, true], { \"d\": false } ]",
	    "[\n  {\n    \"a\" : 1\n  },\n  {\n    \"b\" : [\n      1,\n      \"x\",\n      \"y\"," \
	    "\n      2\n    ]\n  },\n  [\n    false,\n    null,\n    true\n  ],\n  {\n    \"d\" : false\n  }\n]",
	    M_JSON_WRITER_PRETTYPRINT_SPACE                                                                                            },
	{ "[ { \"a\" : 1 }, { \"b\":[ 1, \"x\", \"y\", 2 ] }, [ false, null, true], { \"d\": false } ]",
	    "[\r\n  {\r\n    \"a\" : 1\r\n  },\r\n  {\r\n    \"b\" : [\r\n      1,\r\n      \"x\"," \
	    "\r\n      \"y\",\r\n      2\r\n    ]\r\n  },\r\n  [\r\n    false,\r\n    null,\r\n    true\r\n  ]," \
	    "\r\n  {\r\n    \"d\" : false\r\n  }\r\n]",
	    M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND                                                       },
	/* Object with multiple keys. */
	{ "{ \"a\":1, \"b\":2,\"c\" : 3}", NULL,                  M_JSON_WRITER_NONE                                                   },
	/* Comments. */
	{ "{ \"a\": /*1*/ 2 }", "{\"a\":2}",                      M_JSON_WRITER_NONE                                                   },
	{ "{ \"a\": /*1*/ 2 }", "{\n\t\"a\" : 2\n}",              M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "{ \"a\": /*1*/ 2 }", "{\n  \"a\" : 2\n}",              M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "{ \"a\": /*1*/ 2 }", "{\r\n  \"a\" : 2\r\n}",          M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[ /*1*/ 2 ]",        "[2]",                            M_JSON_WRITER_NONE                                                   },
	{ "[ /*1*/ 2 ]",        "[\n\t2\n]",                      M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ /*1*/ 2 ]",        "[\n  2\n]",                      M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ /*1*/ 2 ]",        "[\r\n  2\r\n]",                  M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[ //1\n2]",          "[2]",                            M_JSON_WRITER_NONE                                                   },
	{ "[ //1\n2]",          "[\n\t2\n]",                      M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ //1\n2]",          "[\n  2\n]",                      M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ //1\n2]",          "[\r\n  2\r\n]",                  M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[ 2 ] // abc ",      "[2]",                            M_JSON_WRITER_NONE                                                   },
	{ "[ 2 ] // abc ",      "[\n\t2\n]",                      M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ 2 ] // abc ",      "[\n  2\n]",                      M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ 2 ] // abc ",      "[\r\n  2\r\n]",                  M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[ 2 ] /* abc */",    "[2]",                            M_JSON_WRITER_NONE                                                   },
	{ "[ 2 ] /* abc */",    "[\n\t2\n]",                      M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ 2 ] /* abc */",    "[\n  2\n]",                      M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ 2 ] /* abc */",    "[\r\n  2\r\n]",                  M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	/* \u escapes. */
	{ "[ \"\\uABCD\" ]",        "[\"\\uABCD\"]",                  M_JSON_WRITER_NONE                                                   },
	{ "[ \"\\uABCD\" ]",        "[\n\t\"\\uABCD\"\n]",            M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ \"\\uAbcd\" ]",        "[\n  \"\\uABCD\"\n]",            M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ \"\\uaBCD\" ]",        "[\r\n  \"\\uABCD\"\r\n]",        M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[ \"\\uAbcD\\uA23D\" ]", "[\"\\uABCD\\uA23D\"]",           M_JSON_WRITER_NONE                                                   },
	{ "[ \"\\uaBCD\\uA23d\" ]", "[\n\t\"\\uABCD\\uA23D\"\n]",     M_JSON_WRITER_PRETTYPRINT_TAB                                        },
	{ "[ \"\\uAbCD\\uA23D\" ]", "[\n  \"\\uABCD\\uA23D\"\n]",     M_JSON_WRITER_PRETTYPRINT_SPACE                                      },
	{ "[ \"\\uABcD\\ua23d\" ]", "[\r\n  \"\\uABCD\\uA23D\"\r\n]", M_JSON_WRITER_PRETTYPRINT_SPACE|M_JSON_WRITER_PRETTYPRINT_WINLINEEND },
	{ "[ \"ꯍ\" ]",              "[\"\\uABCD\"]",                  M_JSON_WRITER_NONE                                                   },
	{ "[ \"ꈽ\" ]",             "[\"\\uA23D\"]",                  M_JSON_WRITER_NONE                                                   },
	{ "[ \"ꈽ\" ]",             "[\"ꈽ\"]",                       M_JSON_WRITER_DONT_ENCODE_UNICODE                                    },
	{ NULL, NULL, M_JSON_WRITER_NONE    }
};

START_TEST(check_json_valid)
{
	M_json_node_t  *json;
	char           *out;
	M_json_error_t  error;
	size_t          i;

	for (i=0; check_json_valid_data[i].data!=NULL; i++) {
		json = M_json_read(check_json_valid_data[i].data, M_str_len(check_json_valid_data[i].data), M_JSON_READER_NONE, NULL, &error, NULL, NULL);

		ck_assert_msg(json != NULL, "JSON (%zu) '%s' could not be parsed: %d", i, check_json_valid_data[i].data, error);

		if (check_json_valid_data[i].out != NULL) {
			out = M_json_write(json, check_json_valid_data[i].writer_flags, NULL);
			ck_assert_msg(M_str_eq(out, check_json_valid_data[i].out), "Output not as expected (%zu):\ngot='%s'\nexpected='%s'", i, out, check_json_valid_data[i].out);
			M_free(out);
		}

		M_json_node_destroy(json);
	}
}
END_TEST

static struct {
	const char *data;
	size_t      error_line;
	size_t      error_pos;
} check_json_invalid_data[] = {
	{ "",                            1, 1  },
	{ "/",                           1, 1  },
	{ "1",                           1, 1  },
	{ "1 /",                         1, 1  },
	{ "q",                           1, 1  },
	{ "{",                           1, 2  },
	{ "{ 1",                         1, 3  },
	{ "[",                           1, 2  },
	{ "[ 1",                         1, 4  },
	{ "[1.",                         1, 2  },
	{ "\"a\"",                       1, 1  },
	{ "[ \"a\nb\" ]",                1, 5  },
	{ "{ \"a\": 1\n\n\n 2: 3 }",     4, 2  },
	{ "{ \"a\" 1 }",                 1, 7  },
	{ "{ \"a\":  }",                 1, 9  },
	{ "{ \"a\":",                    1, 7  },
	{ "{ \"a\"}",                    1, 6  },
	{ "{ \"a\": 1, }",               1, 11 },
	{ "{ \"a\": 1,",                 1, 10 },
	{ "{ \"a\": 1, a",               1, 11 },
	{ "{ \"a\": 1, a }",             1, 11 },
	{ "{ \"a\": 1, {",               1, 11 },
	{ "{ \"a\": 1, a {",             1, 11 },
	{ "[ 1, ]",                      1, 6  },
	{ "[ 1,",                        1, 5  },
	{ "[ 1, a",                      1, 6  },
	{ "[ 1, a ]",                    1, 6  },
	{ "[ 1, a [",                    1, 6  },
	{ "[ 1, [",                      1, 7  },
	{ "[ \\a ]",                     1, 3  },
	{ "[ t ]",                       1, 3  },
	{ "[ truq ]",                    1, 3  },
	{ "[ trueq ]",                   1, 7  },
	{ "[ f]",                        1, 3  },
	{ "[ fales]",                    1, 3  },
	{ "[ falseq]",                   1, 8  },
	{ "[ n]",                        1, 3  },
	{ "[ nul]",                      1, 3  },
	{ "[ nullq]",                    1, 7  },
	{ "[ 99999999999999999999999 ]", 1, 3  },
	{ "[ 9.999999999999999999999 ]", 1, 3  },
	{ "[ /* ]",                      1, 3  },
	{ "[ /*/ 1 ]",                   1, 3  },
	{ "[ // ]",                      1, 7  },
	{ "[ 1 ] 123",                   1, 7  },
	{ "[ 1 ] [2]",                   1, 7  },
	{ "[ \"\\uAB\" ]",               1, 4  },
	{ "[ \"\\uABRE\" ]",             1, 4  },
	{ "[ / ]",                       1, 3  },
	{ NULL,                          0, 0  }
};

START_TEST(check_json_invalid)
{
	M_json_node_t  *json;
	M_json_error_t  error;
	size_t          error_line;
	size_t          error_pos;
	size_t          i;

	for (i=0; check_json_invalid_data[i].data!=NULL; i++) {
		json = M_json_read(check_json_invalid_data[i].data, M_str_len(check_json_invalid_data[i].data), M_JSON_READER_NONE, NULL, &error, &error_line, &error_pos);
		ck_assert_msg(json == NULL, "Invalid JSON was parsed (%zu): %s", i, check_json_invalid_data[i].data);
		if (check_json_invalid_data[i].error_line != 0 && check_json_invalid_data[i].error_pos != 0)
			ck_assert_msg(check_json_invalid_data[i].error_line == error_line && check_json_invalid_data[i].error_pos == error_pos, "Parse error (%zu) '%s' was not found at expected location. Found. %zu:%zu. Expected: %zu:%zu", i, check_json_invalid_data[i].data, error_line, error_pos, check_json_invalid_data[i].error_line, check_json_invalid_data[i].error_pos);
		M_json_node_destroy(json);
	}
}
END_TEST

static struct {
	const char *data;
	M_uint32    reader_flags;
	M_bool      will_error;
	const char *out;
	size_t      error_line;
	size_t      error_pos;
} check_json_reader_flags_data[] = {
	/* Comments. */
	{ "{ \"a\": /*1*/ 2 }",          M_JSON_READER_DISALLOW_COMMENTS,        M_TRUE,  NULL,      1, 8 },
	{ "[ /*1*/ 2 ]",                 M_JSON_READER_DISALLOW_COMMENTS,        M_TRUE,  NULL,      1, 3 },
	{ "[ //1\n2]",                   M_JSON_READER_DISALLOW_COMMENTS,        M_TRUE,  NULL,      1, 3 },
	{ "[ 2 ] // abc ",               M_JSON_READER_DISALLOW_COMMENTS,        M_TRUE,  NULL,      1, 7 },
	{ "[ \n2 ] /* abc */",           M_JSON_READER_DISALLOW_COMMENTS,        M_TRUE,  NULL,      2, 5 },
	/* Decimal truncation. */
	{ "[ 9.999999999999999999999 ]", M_JSON_READER_ALLOW_DECIMAL_TRUNCATION, M_FALSE, "[9.99999999999999999]", 0, 0 },
	{ "[ \"\\uABr\" ]",              M_JSON_READER_REPLACE_BAD_CHARS,        M_FALSE, "[\"?r\"]", 0, 0 },
	{ "[ \"\\uDCBA\" ]",             M_JSON_READER_REPLACE_BAD_CHARS,        M_FALSE, "[\"?\"]", 0, 0 },
	{ "[ \"\\uABCD\" ]",             M_JSON_READER_DONT_DECODE_UNICODE,      M_FALSE, "[\"\\\\uABCD\"]", 0, 0 },
	{ NULL, 0, M_FALSE, NULL, 0, 0 }
};

START_TEST(check_json_reader_flags)
{
	M_json_node_t  *json;
	char           *out;
	M_json_error_t  error;
	size_t          error_line;
	size_t          error_pos;
	size_t          i;

	for (i=0; check_json_reader_flags_data[i].data!=NULL; i++) {
		json = M_json_read(check_json_reader_flags_data[i].data, M_str_len(check_json_reader_flags_data[i].data), check_json_reader_flags_data[i].reader_flags, NULL, &error, &error_line, &error_pos);
		if (check_json_reader_flags_data[i].will_error == M_TRUE) {
			ck_assert_msg(json == NULL, "Invalid JSON was parsed (%zu): %s", i, check_json_reader_flags_data[i].data);
			if (check_json_reader_flags_data[i].error_line != 0 && check_json_reader_flags_data[i].error_pos != 0)
				ck_assert_msg(check_json_reader_flags_data[i].error_line == error_line && check_json_reader_flags_data[i].error_pos == error_pos, "Parse error (%zu) '%s' was not found at expected location. Found. %zu:%zu. Expected: %zu:%zu", i, check_json_reader_flags_data[i].data, error_line, error_pos, check_json_reader_flags_data[i].error_line, check_json_reader_flags_data[i].error_pos);
		} else {
			ck_assert_msg(json != NULL, "JSON (%zu) '%s' could not be parsed: %d", i, check_json_reader_flags_data[i].data, error);
			if (check_json_reader_flags_data[i].out != NULL) {
				out = M_json_write(json, M_JSON_WRITER_NONE, NULL);
				ck_assert_msg(M_str_eq(out, check_json_reader_flags_data[i].out), "Output not as expected (%zu):\ngot='%s'\nexpected='%s'", i, out, check_json_reader_flags_data[i].out);
				M_free(out);
			}
		}
		M_json_node_destroy(json);
	}
}
END_TEST

#define JSONPATH_BOOKS "{"                       \
"  \"store\": {"                                 \
"    \"book\": ["                                \
"      {"                                        \
"        \"category\": \"reference\","           \
"        \"author\": \"Nigel Rees\","            \
"        \"title\": \"Sayings of the Century\"," \
"        \"price\": 8.95"                        \
"      },"                                       \
"      {"                                        \
"        \"category\": \"fiction\","             \
"        \"author\": \"Evelyn Waugh\","          \
"        \"title\": \"Sword of Honour\","        \
"        \"price\": 12.99"                       \
"      },"                                       \
"      {"                                        \
"        \"category\": \"fiction\","             \
"        \"author\": \"Herman Melville\","       \
"        \"title\": \"Moby Dick\","              \
"        \"isbn\": \"0-553-21311-3\","           \
"        \"price\": 8.99"                        \
"      },"                                       \
"      {"                                        \
"        \"category\": \"fiction\","             \
"        \"author\": \"J. R. R. Tolkien\","      \
"        \"title\": \"The Lord of the Rings\","  \
"        \"isbn\": \"0-395-19395-8\","           \
"        \"price\": 22.99"                       \
"      }"                                        \
"    ],"                                         \
"    \"bicycle\": {"                             \
"      \"color\": \"red\","                      \
"      \"price\": 19.95"                         \
"    }"                                          \
"  }"                                            \
"}"

static struct {
	const char    *search;
	size_t         num_matches;
	M_json_type_t  type;
} check_json_jsonpath_book_data[] = {
	{ "$.store.book[*].author",      4, M_JSON_TYPE_STRING  },
	{ "$.store.book[1].author",      1, M_JSON_TYPE_STRING  },
	{ "$.store.book[0,2,3].author",  3, M_JSON_TYPE_STRING  },
	{ "$.store.book[1:3].author",    2, M_JSON_TYPE_STRING  },
	{ "$.store.book[1:3:4].author",  1, M_JSON_TYPE_STRING  },
	{ "$.store.book[0::2].author",   2, M_JSON_TYPE_STRING  },
	{ "$..author",                   4, M_JSON_TYPE_STRING  },
	{ "$.store..price",              5, M_JSON_TYPE_DECIMAL },
	{ "$.store.*",                   2, M_JSON_TYPE_UNKNOWN }, /* Matches multiple types. */
	{ "$..*",                       23, M_JSON_TYPE_UNKNOWN }, /* Matches multiple types. */
	{ "$.store.book",                1, M_JSON_TYPE_ARRAY   },
	{ "$..book",                     1, M_JSON_TYPE_ARRAY   },
	{ "$..book[2]",                  1, M_JSON_TYPE_OBJECT  },
	{ NULL,                          0, M_JSON_TYPE_UNKNOWN }
};

START_TEST(check_json_jsonpath_book)
{
	M_json_node_t   *json;
	M_json_node_t  **results;
	M_json_type_t    type;
	M_json_error_t   error;
	size_t           error_line;
	size_t           error_pos;
	size_t           num_matches;
	size_t           i;
	size_t           j;

	json = M_json_read(JSONPATH_BOOKS, M_str_len(JSONPATH_BOOKS), M_JSON_READER_NONE, NULL, &error, &error_line, &error_pos);
	ck_assert_msg(json != NULL, "JSONPath books string could not be parsed: %d, %zu:%zu", error, error_line, error_pos);

	for (i=0; check_json_jsonpath_book_data[i].search!=NULL; i++) {
		results = M_json_jsonpath(json, check_json_jsonpath_book_data[i].search, &num_matches);
		ck_assert_msg(results != NULL, "No matches found (%zu): '%s'", i, check_json_jsonpath_book_data[i].search);
		ck_assert_msg(num_matches == check_json_jsonpath_book_data[i].num_matches, "Unexpected matches found (%zu): '%s'. Got %zu, expected %zu matches", i, check_json_jsonpath_book_data[i].search, num_matches, check_json_jsonpath_book_data[i].num_matches);

		/* Silence clang warning using the if != NULL thoug the test won't get this far due to the ck_assert_msg for
 		 * NULL above. */
		if (results != NULL && check_json_jsonpath_book_data[i].type != M_JSON_TYPE_UNKNOWN) {
			for (j=0; j<num_matches; j++) {
				type = M_json_node_type(results[j]);
				ck_assert_msg(type == check_json_jsonpath_book_data[i].type, "Unexpected type (%zu): got %d, expected %d", i, type, check_json_jsonpath_book_data[i].type);
			}
		}

		M_free(results);
	}

	M_json_node_destroy(json);
}
END_TEST

#define JSONPATH_STR "{"     \
"  \"a\": \"res1\","         \
"  \"b\": ["                 \
"    \"a\","                 \
"    \"b\","                 \
"    {"                      \
"      \"b1\": 2,"           \
"      \"b2\": \"res.b.b2\"" \
"    }"                      \
"  ],"                       \
"  \"c\": ["                 \
"    1,"                     \
"    2,"                     \
"    3"                      \
"  ],"                       \
"  \"d\": ["                 \
"    {"                      \
"      \"z\": {"             \
"        \"r\": \"nest ed\"" \
"      }"                    \
"    }"                      \
"  ]"                        \
"}"

static struct {
	const char    *search;
	const char    *str;
} check_json_jsonpath_str_data[] = {
	{ "$.a",             "res1"     },
	{ "$..a",            "res1"     },
	{ "$.b[2].b2",       "res.b.b2" },
	{ "$.b[-1].b2",      "res.b.b2" },
	{ "$.b[-1:].b2",     "res.b.b2" },
	{ "$.b[14:0:-1].b2", "res.b.b2" },
	{ "$.b[0:14].b2",    "res.b.b2" },
	{ "$..b2",           "res.b.b2" },
	{ "$.d[0].z.r",      "nest ed"  },
	{ "$.d..z.r",        "nest ed"  },
	{ "$..z.r",          "nest ed"  },
	{ "$..r",            "nest ed"  },
	/* Bad searches. */
	{ "$.b.b2",          NULL       },
	{ "$.d.z.r",         NULL       },
	{ "$.q",             NULL       },
	{ "$.q[0]",          NULL       },
	{ "$.a[0]",          NULL       },
	{ "$.a.z[0]",        NULL       },
	{ "$.cake",          NULL       },
	/* Bad slices. */
	{ "$.b[2:2].b2",     NULL       },
	{ "$.b[14:0].b2",    NULL       },
	{ "$.b[0:4:-1].b2",  NULL       },
	{ NULL,              NULL       }
};

START_TEST(check_json_jsonpath_str)
{
	M_json_node_t  *json;
	M_json_node_t **results;
	const char     *str;
	M_json_type_t   type;
	M_json_error_t  error;
	size_t          error_line;
	size_t          error_pos;
	size_t          num_matches;
	size_t          i;

	json = M_json_read(JSONPATH_STR, M_str_len(JSONPATH_STR), M_JSON_READER_NONE, NULL, &error, &error_line, &error_pos);
	ck_assert_msg(json != NULL, "JSONPath string could not be parsed: %d, %zu:%zu", error, error_line, error_pos);

	for (i=0; check_json_jsonpath_str_data[i].search!=NULL; i++) {
		results = M_json_jsonpath(json, check_json_jsonpath_str_data[i].search, &num_matches);
		if (check_json_jsonpath_str_data[i].str == NULL) {
			ck_assert_msg(results == NULL, "Matches found (%zu) when there shouldn't be: '%s'", i, check_json_jsonpath_str_data[i].search);
			continue;
		}
		ck_assert_msg(results != NULL, "No matches found (%zu): '%s'", i, check_json_jsonpath_str_data[i].search);
		ck_assert_msg(num_matches == 1, "Unexpected matches found (%zu): '%s': got %zu matches", i, check_json_jsonpath_str_data[i].search, num_matches);

		/* Silence clang warning using the if != NULL thoug the test won't get this far due to the ck_assert_msg for
 		 * NULL above. */
		if (results != NULL) {
			type = M_json_node_type(results[0]);
			ck_assert_msg(type == M_JSON_TYPE_STRING, "(%zu) Search '%s' did not return string match", i, check_json_jsonpath_str_data[i].search);
			if (type == M_JSON_TYPE_STRING) {
				str  = M_json_get_string(results[0]);
				ck_assert_msg(M_str_eq(check_json_jsonpath_str_data[i].str, str), "(%zu) Search '%s': did not find expected node. Got '%s', expected '%s'", i, check_json_jsonpath_str_data[i].search, str, check_json_jsonpath_str_data[i].str);
			}
		}

		M_free(results);
	}

	M_json_node_destroy(json);
}
END_TEST

#define JSONPATH_ARRAY "[ [ 1,2 ], 88, [ 23 ], [ 94, 95 ] ]"
START_TEST(check_json_jsonpath_array)
{
	M_json_node_t   *json;
	M_json_node_t  **results;
	M_int64          val;
	M_json_error_t   error;
	size_t           num_matches;

	json = M_json_read(JSONPATH_ARRAY, M_str_len(JSONPATH_ARRAY), M_JSON_READER_NONE, NULL, &error, NULL, NULL);
	ck_assert_msg(json != NULL, "String could not be parsed: %d", error);

	results = M_json_jsonpath(json, "$[1]", &num_matches);
	ck_assert_msg(results != NULL && num_matches == 1, "Did not find expected match: results=%s, num_matches=%zu", results==NULL?"NULL":"NOT NULL", num_matches);
	if (results != NULL) {
		val = M_json_get_int(results[0]);
		ck_assert_msg(val == 88, "node value %lld != 88", val);
		M_free(results);
	}

	results = M_json_jsonpath(json, "$.[1]", &num_matches);
	ck_assert_msg(results != NULL && num_matches == 3, "Did not find expected match: results=%s, num_matches=%zu", results==NULL?"NULL":"NOT NULL", num_matches);
	if (results != NULL) {
		val = M_json_get_int(results[0]);
		ck_assert_msg(val == 88, "node value %lld != 88", val);
		val = M_json_get_int(results[1]);
		ck_assert_msg(val == 2, "node value %lld != 2", val);
		val = M_json_get_int(results[2]);
		ck_assert_msg(val == 95, "node value %lld != 95", val);
		M_free(results);
	}

	results = M_json_jsonpath(json, "$..[0]", &num_matches);
	ck_assert_msg(results != NULL && num_matches == 4, "Did not find expected match: results=%s, num_matches=%zu", results==NULL?"NULL":"NOT NULL", num_matches);
	if (results != NULL) {
		ck_assert_msg(M_json_node_type(results[0]) == M_JSON_TYPE_ARRAY, "node is not array");
		val = M_json_get_int(results[1]);
		ck_assert_msg(val == 1, "node value %lld != 1", val);
		val = M_json_get_int(results[2]);
		ck_assert_msg(val == 23, "node value %lld != 23", val);
		val = M_json_get_int(results[3]);
		ck_assert_msg(val == 94, "node value %lld != 94", val);
		M_free(results);
	}

	M_json_node_destroy(json);
}
END_TEST

START_TEST(check_json_values)
{
	M_json_node_t *json;
	M_decimal_t    dec;
	char           temp[256];
	char          *out;

	json = M_json_node_create(M_JSON_TYPE_STRING);

	/* Set, change and serialize values */
	ck_assert_msg(M_json_set_string(json, "AbC"), "Could not set initial string value");
	ck_assert_msg(M_str_eq(M_json_get_string(json), "AbC"), "String value does not match expected");
	ck_assert_msg(M_json_get_value(json, temp, sizeof(temp)), "String value could not be serialized");
	ck_assert_msg(M_str_eq(temp, "AbC"), "String value not serialized as expected");
	out = M_json_get_value_dup(json);
	ck_assert_msg(out != NULL, "Duped string value could not be serialized");
	ck_assert_msg(M_str_eq(out, "AbC"), "Duped string value not serialized as expected");
	M_free(out);

	ck_assert_msg(M_json_set_int(json, 21), "Could not change value to int");
	ck_assert_msg(M_json_get_int(json) ==  21, "Int value does not match expected");
	ck_assert_msg(M_json_get_value(json, temp, sizeof(temp)), "Int value could not be serialized");
	ck_assert_msg(M_str_eq(temp, "21"), "Int value not serialized as expected");
	out = M_json_get_value_dup(json);
	ck_assert_msg(out != NULL, "Duped int value could not be serialized");
	ck_assert_msg(M_str_eq(out, "21"), "Duped int value not serialized as expected");
	M_free(out);

	ck_assert_msg(M_json_set_bool(json, M_TRUE), "Could not change value to bool (true)");
	ck_assert_msg(M_json_get_bool(json), "bool (true) value does not match expected");
	ck_assert_msg(M_json_get_value(json, temp, sizeof(temp)), "Bool (true) value could not be serialized");
	ck_assert_msg(M_str_eq(temp, "true"), "Bool (true) value not serialized as expected");
	out = M_json_get_value_dup(json);
	ck_assert_msg(out != NULL, "Duped bool (true) value could not be serialized");
	ck_assert_msg(M_str_eq(out, "true"), "Duped bool (true) value not serialized as expected");
	M_free(out);

	ck_assert_msg(M_json_set_bool(json, M_FALSE), "Could not change value to bool (false)");
	ck_assert_msg(!M_json_get_bool(json), "bool (false) value does not match expected");
	ck_assert_msg(M_json_get_value(json, temp, sizeof(temp)), "Bool (false) value could not be serialized");
	ck_assert_msg(M_str_eq(temp, "false"), "Bool (false) value not serialized as expected");
	out = M_json_get_value_dup(json);
	ck_assert_msg(out != NULL, "Duped bool (false) value could not be serialized");
	ck_assert_msg(M_str_eq(out, "false"), "Duped bool (false) value not serialized as expected");
	M_free(out);

	ck_assert_msg(M_json_set_null(json), "Could not change value to null");
	ck_assert_msg(M_json_node_type(json) == M_JSON_TYPE_NULL, "json is not null type");
	ck_assert_msg(M_json_get_value(json, temp, sizeof(temp)), "Null value could not be serialized");
	ck_assert_msg(M_str_eq(temp, "null"), "Null value not serialized as expected");
	out = M_json_get_value_dup(json);
	ck_assert_msg(out != NULL, "Duped null value could not be serialized");
	ck_assert_msg(M_str_eq(out, "null"), "Duped null value not serialized as expected");
	M_free(out);

	M_decimal_create(&dec);
	M_decimal_from_int(&dec, 9123, 2);
	ck_assert_msg(M_json_set_decimal(json, &dec), "Could not change value to decimal");
	ck_assert_msg(M_decimal_cmp(&dec, M_json_get_decimal(json)) == 0, "decimal value does not match expected");
	ck_assert_msg(M_json_get_value(json, temp, sizeof(temp)), "Decimal value could not be serialized");
	ck_assert_msg(M_str_eq(temp, "91.23"), "Decimal value not serialized as expected");
	out = M_json_get_value_dup(json);
	ck_assert_msg(out != NULL, "Duped decimal value could not be serialized");
	ck_assert_msg(M_str_eq(out, "91.23"), "Duped decimal value not serialized as expected");
	M_free(out);

	M_json_node_destroy(json);
}
END_TEST

#define JSON_PARENT_OBJECT             "{ \"zz\":[1, 2 ], \"zy\": \"a\" }"
#define JSON_PARENT_OBJECT_OUT_STRING  "{\"zz\":[1,2,\"a\"]}"
#define JSON_PARENT_OBJECT_OUT_REMOVED "{}"
START_TEST(check_json_parent_object)
{
	M_json_node_t   *json;
	M_json_node_t  **results;
	M_json_node_t   *zy_node = NULL;
	M_json_node_t   *zz_node = NULL;
	char            *out;
	M_json_error_t   error;
	size_t           num_matches;

	json = M_json_read(JSON_PARENT_OBJECT, M_str_len(JSON_PARENT_OBJECT), M_JSON_READER_NONE, NULL, &error, NULL, NULL);
	ck_assert_msg(json != NULL, "String could not be parsed: %d", error);

	results = M_json_jsonpath(json, "$..zy", &num_matches);
	ck_assert_msg(results != NULL && num_matches == 1, "Did not find expected match for str node: results=%s, num_matches=%zu", results==NULL?"NULL":"NOT NULL", num_matches);
	if (results != NULL)
		zy_node = results[0];
	M_free(results);

	results = M_json_jsonpath(json, "$..zz", &num_matches);
	ck_assert_msg(results != NULL && num_matches == 1, "Did not find expected match for array node: results=%s, num_matches=%zu", results==NULL?"NULL":"NOT NULL", num_matches);
	if (results != NULL)
		zz_node = results[0];
	M_free(results);

	ck_assert_msg(!M_json_array_insert(zz_node, zy_node), "Cross reference happened. Shouldn't be able to insert node with parent into another node without first taking it");
	M_json_take_from_parent(zy_node);
	ck_assert_msg(M_json_array_insert(zz_node, zy_node), "Insert into array failed");

	out = M_json_write(json, M_JSON_WRITER_NONE, NULL);
	ck_assert_msg(M_str_eq(out, JSON_PARENT_OBJECT_OUT_STRING), "Output not as expected:\ngot='%s'\nexpected='%s'", out, JSON_PARENT_OBJECT_OUT_STRING);
	M_free(out);
	
	M_json_take_from_parent(zz_node);
	out = M_json_write(json, M_JSON_WRITER_NONE, NULL);
	ck_assert_msg(M_str_eq(out, JSON_PARENT_OBJECT_OUT_REMOVED), "Output not as expected:\ngot='%s'\nexpected='%s'", out, JSON_PARENT_OBJECT_OUT_REMOVED);
	M_free(out);

	M_json_object_insert(json, "zz", zz_node);
	out = M_json_write(json, M_JSON_WRITER_NONE, NULL);
	ck_assert_msg(M_str_eq(out, JSON_PARENT_OBJECT_OUT_STRING), "Output not as expected:\ngot='%s'\nexpected='%s'", out, JSON_PARENT_OBJECT_OUT_STRING);
	M_free(out);

	M_json_node_destroy(zz_node);
	out = M_json_write(json, M_JSON_WRITER_NONE, NULL);
	ck_assert_msg(M_str_eq(out, JSON_PARENT_OBJECT_OUT_REMOVED), "Output not as expected:\ngot='%s'\nexpected='%s'", out, JSON_PARENT_OBJECT_OUT_REMOVED);
	M_free(out);

	M_json_node_destroy(json);
}
END_TEST

#define JSON_PARENT_ARRAY             "[ [1, 2 ], \"a\", true ]"
#define JSON_PARENT_ARRAY_OUT_STRING  "[[\"a\",1,2],true]"
#define JSON_PARENT_ARRAY_OUT_DECIMAL "[[1,2,1.5],true]"
#define JSON_PARENT_ARRAY_OUT_REMOVED "[[1,2],true]"
START_TEST(check_json_parent_array)
{
	M_json_node_t   *json;
	M_json_node_t  **results;
	M_json_node_t   *str_node;
	M_json_node_t   *array_node;
	M_json_node_t   *dec_node;
	M_decimal_t      dec;
	M_json_error_t   error;
	char            *out;
	size_t           num_matches;

	json = M_json_read(JSON_PARENT_ARRAY, M_str_len(JSON_PARENT_ARRAY), M_JSON_READER_NONE, NULL, &error, NULL, NULL);
	ck_assert_msg(json != NULL, "String could not be parsed: %d", error);

	results = M_json_jsonpath(json, "$[1]", &num_matches);
	ck_assert_msg(results != NULL && num_matches == 1, "Did not find expected match for str node: results=%s, num_matches=%zu", results==NULL?"NULL":"NOT NULL", num_matches);

	if (results != NULL) {
		str_node = results[0];
		M_free(results);
		M_json_take_from_parent(str_node);

		out = M_json_write(json, M_JSON_WRITER_NONE, NULL);
		ck_assert_msg(M_str_eq(out, JSON_PARENT_ARRAY_OUT_REMOVED), "Take output not as expected:\ngot='%s'\nexpected='%s'", out, JSON_PARENT_ARRAY_OUT_REMOVED);
		M_free(out);

		results = M_json_jsonpath(json, "$[0]", &num_matches);
		ck_assert_msg(results != NULL && num_matches == 1, "Did not find expected match for array node: results=%s, num_matches=%zu", results==NULL?"NULL":"NOT NULL", num_matches);
		if (results != NULL) {
			array_node = results[0];
			M_free(results);
			M_json_array_insert_at(array_node, str_node, 0);
			
			out = M_json_write(json, M_JSON_WRITER_NONE, NULL);
			ck_assert_msg(M_str_eq(out, JSON_PARENT_ARRAY_OUT_STRING), "Insert str node output not as expected:\ngot='%s'\nexpected='%s'", out, JSON_PARENT_ARRAY_OUT_STRING);
			M_free(out);
		}

		M_json_node_destroy(str_node);
		out = M_json_write(json, M_JSON_WRITER_NONE, NULL);
		ck_assert_msg(M_str_eq(out, JSON_PARENT_ARRAY_OUT_REMOVED), "Destroy str node output not as expected:\ngot='%s'\nexpected='%s'", out, JSON_PARENT_ARRAY_OUT_REMOVED);
		M_free(out);
	}

	results = M_json_jsonpath(json, "$[0]", &num_matches);
	ck_assert_msg(results != NULL && num_matches == 1, "Did not find expected match for array node (2nd search): results=%s, num_matches=%zu", results==NULL?"NULL":"NOT NULL", num_matches);
	if (results != NULL) {
		array_node = results[0];
		M_free(results);
		M_decimal_create(&dec);
		M_decimal_from_int(&dec, 15, 1);
		dec_node = M_json_node_create(M_JSON_TYPE_DECIMAL);
		M_json_set_decimal(dec_node, &dec);
		M_json_array_insert(array_node, dec_node);
		
		out = M_json_write(json, M_JSON_WRITER_NONE, NULL);
		ck_assert_msg(M_str_eq(out, JSON_PARENT_ARRAY_OUT_DECIMAL), "Insert of dec node output not as expected:\ngot='%s'\nexpected='%s'", out, JSON_PARENT_ARRAY_OUT_DECIMAL);
		M_free(out);

		M_json_node_destroy(dec_node);
		out = M_json_write(json, M_JSON_WRITER_NONE, NULL);
		ck_assert_msg(M_str_eq(out, JSON_PARENT_ARRAY_OUT_REMOVED), "Destroy dec node output not as expected:\ngot='%s'\nexpected='%s'", out, JSON_PARENT_ARRAY_OUT_REMOVED);
		M_free(out);
	}

	M_json_node_destroy(json);
}
END_TEST

#define JSON_OBJECT_UNIQUE_KEYS "{\"a\":1,\"a\":2,\"b\":3}"
START_TEST(check_json_object_unique_keys)
{
	M_json_node_t  *json;
	M_json_error_t  error;

	json = M_json_read(JSON_OBJECT_UNIQUE_KEYS, M_str_len(JSON_OBJECT_UNIQUE_KEYS), M_JSON_READER_OBJECT_UNIQUE_KEYS, NULL, &error, NULL, NULL);
	ck_assert_msg(json == NULL, "String was parsed");
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

Suite *M_json_suite(void)
{
	Suite *suite;
	TCase *tc_json_valid;
	TCase *tc_json_invalid;
	TCase *tc_json_reader_flags;
	TCase *tc_json_jsonpath_book;
	TCase *tc_json_jsonpath_str;
	TCase *tc_json_jsonpath_array;
	TCase *tc_json_values;
	TCase *tc_json_parent_object;
	TCase *tc_json_parent_array;
	TCase *tc_json_object_unique_keys;

	suite = suite_create("json");

	tc_json_valid = tcase_create("check_json_valid");
	tcase_add_test(tc_json_valid, check_json_valid);
	suite_add_tcase(suite, tc_json_valid);

	tc_json_invalid = tcase_create("check_json_invalid");
	tcase_add_test(tc_json_invalid, check_json_invalid);
	suite_add_tcase(suite, tc_json_invalid);

	tc_json_reader_flags = tcase_create("check_json_reader_flags");
	tcase_add_test(tc_json_reader_flags, check_json_reader_flags);
	suite_add_tcase(suite, tc_json_reader_flags);

	tc_json_jsonpath_book = tcase_create("check_json_jsonpath_book");
	tcase_add_test(tc_json_jsonpath_book, check_json_jsonpath_book);
	suite_add_tcase(suite, tc_json_jsonpath_book);

	tc_json_jsonpath_str = tcase_create("check_json_jsonpath_str");
	tcase_add_test(tc_json_jsonpath_str, check_json_jsonpath_str);
	suite_add_tcase(suite, tc_json_jsonpath_str);

	tc_json_jsonpath_array = tcase_create("check_json_jsonpath_array");
	tcase_add_test(tc_json_jsonpath_array, check_json_jsonpath_array);
	suite_add_tcase(suite, tc_json_jsonpath_array);

	tc_json_values = tcase_create("check_json_values");
	tcase_add_test(tc_json_values, check_json_values);
	suite_add_tcase(suite, tc_json_values);

	tc_json_parent_object = tcase_create("check_json_parent_object");
	tcase_add_test(tc_json_parent_object, check_json_parent_object);
	suite_add_tcase(suite, tc_json_parent_object);

	tc_json_parent_array = tcase_create("check_json_parent_array");
	tcase_add_test(tc_json_parent_array, check_json_parent_array);
	suite_add_tcase(suite, tc_json_parent_array);

	tc_json_object_unique_keys = tcase_create("check_json_object_unique_keys");
	tcase_add_test(tc_json_object_unique_keys, check_json_object_unique_keys);
	suite_add_tcase(suite, tc_json_object_unique_keys);

	return suite;
}

int main(void)
{
	int nf;
	SRunner *sr = srunner_create(M_json_suite());
	srunner_set_log(sr, "check_json.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
