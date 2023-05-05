/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Monetra Technologies, LLC.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __M_GETOPT_H__
#define __M_GETOPT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_getopt getopt
 *  \ingroup mstdlib_base
 *
 * Command line argument parsing.
 *
 * Handles parsing using a series of provided callbacks for each type. Auto
 * conversion of the argument for the option type will take place. Callbacks
 * are allowed to reject the option or argument. This will stop parsing.
 *
 * If auto conversion is unwanted use the string option type. String options
 * will alway shave their arguments passed unmodified.
 *
 * Options can be marked as having required or optional arguments.
 *
 * The boolean type is somewhat special. If marked as not having a required
 * argument it is treated as a flag. For example -b would call the boolean
 * callback with a value of M_TRUE. If a boolean is marked as val required
 * then a value is required and the result of conversion (using M_str_istrue)
 * will be passed to the callback.
 *
 * For options that should _not_ have an argument use the boolean type with
 * val not required.
 *
 * Option callbacks are only called when an option is specified. Meaning for
 * boolean options a value of M_FALSE will only be sent if the option was
 * explcitly used and set to false.
 *
 * Supports auto generation of help message.
 *
 * Valid characters for options (short/long) is all ASCII printable [!-~] except:
 * - space
 * - '-' (short or start/end long)
 * - '='
 * - '"'
 * - '''
 *
 * To stop option processing and treat all following values as nonoptions use --
 * as an option.
 *
 * An optional thunk can be passed in during parsing which will be passed to all
 * callbacks. This can be used to collect all options into an object instead of
 * stored in global variables.
 *
 * Option callbacks will receive the short and long options associated with the
 * option. If no short option was set the short_opt callback value will be 0.
 * If no long option was set the long_opt callback value will be NULL.
 *
 * Example:
 *
 * \code{.c}
 *     static M_bool nonopt_cb(size_t idx, const char *option, void *thunk)
 *     {
 *         M_printf("option='%s'\n", option);
 *     }
 *
 *     static M_bool int_cb(char short_opt, const char *long_opt, M_int64 *integer, void *thunk)
 *     {
 *         M_printf("short_opt='%d', long_opt='%s', integer='%lld'\n", short_opt, long_opt, integer!=NULL?*integer:-1);
 *     }
 *
 *     int main(int argc, char **argv) {
 *         M_getopt_t *g; 
 *         char       *help;
 *         const char *fail;
 *
 *         g = M_getopt_create(nonopt_cb);
 *         M_getopt_addinteger(g, 'i', "i1", M_TRUE, "DESCR 1", int_cb);
 *
 *         help = M_getopt_help(g);
 *         M_printf("help=\n%s\n", help);
 *         M_free(help);
 *
 *         if (M_getopt_parse(g, argv, argc, &fail, NULL) == M_GETOPT_ERROR_SUCCESS) {
 *             M_printf("Options parsed successfully\n");
 *         } else {
 *             M_printf("Options parse error: %s\n", fail);
 *         }
 *
 *         M_getopt_destroy(g);
 *         return 0;
 *     } 
 * \endcode
 *
 * @{
 */

struct M_getopt;
typedef struct M_getopt M_getopt_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Error codes. */
typedef enum {
	M_GETOPT_ERROR_SUCCESS = 0,
	M_GETOPT_ERROR_INVALIDOPT,
	M_GETOPT_ERROR_INVALIDDATATYPE,
	M_GETOPT_ERROR_INVALIDORDER,
	M_GETOPT_ERROR_MISSINGVALUE,
	M_GETOPT_ERROR_NONOPTION
} M_getopt_error_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 
/*! Callback for non-option parameters */
typedef M_bool (*M_getopt_nonopt_cb)(size_t idx, const char *option, void *thunk);


/*! Callback for integer data type */
typedef M_bool (*M_getopt_integer_cb)(char short_opt, const char *long_opt, M_int64 *integer, void *thunk);
 

/*! Callback for decimal data type */
typedef M_bool (*M_getopt_decimal_cb)(char short_opt, const char *long_opt, M_decimal_t *decimal, void *thunk);
 

/*! Callback for string data type */
typedef M_bool (*M_getopt_string_cb)(char short_opt, const char *long_opt, const char *string, void *thunk);
 

/*! Callback for boolean data type */
typedef M_bool (*M_getopt_boolean_cb)(char short_opt, const char *long_opt, M_bool boolean, void *thunk);
 
 
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a new getopt object.
 * 
 * \param cb Callback to be called with non-option parameters. NULL if non-option parameters are not allowed.
 *
 * \return Getopt object.
 */
M_API M_getopt_t *M_getopt_create(M_getopt_nonopt_cb cb);
 

/*! Destroy a getopt object
 *
 * \param[in] g Getopt object to destroy.
 */
M_API void M_getopt_destroy(M_getopt_t *g);
 

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Output help text for command line options.
 *
 * Components:
 *   \<val\>     value is required
 *   [val]       value is optional
 *   -s          short option
 *   --long      long option
 *   (type)      Type such as integer, decimal ... Type will not be printed for boolean options.
 *   Description Text description about the option
 *
 * Example:
 *  \code -s \<val\> (type) Description \endcode
 *  \code --long [val] (type) \endcode
 *  \code -s, --long [val] (type) Description \endcode
 *  \code -s Description \endcode
 *
 * \param[in] g Getopt object.
 */
M_API char *M_getopt_help(const M_getopt_t *g);
 

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Add an integer parameter.
 *
 * \param[in] g            Getopt object
 * \param[in] short_opt    Short option, must be alpha-numeric, case-sensitive. Pass 0 if not used
 * \param[in] long_opt     Long option name, must be alpha-numeric or hyphens, case-insensitive. Can not start or end
 *                         with hyphens. Pass NULL if not used.
 * \param[in] val_required Whether or not the option requires a value.
 * \param[in] description  Field description. Used with output putting help message.
 * \param[in] cb           Callback to call with value. NULL will be passed if no value provided
 *
 * \return M_TRUE on success, M_FALSE on failure.
 */
M_API M_bool M_getopt_addinteger(M_getopt_t *g, char short_opt, const char *long_opt, M_bool val_required, const char *description, M_getopt_integer_cb cb);
 

/*! Add a decimal parameter.
 *
 * \param[in] g            Getopt object
 * \param[in] short_opt    Short option, must be alpha-numeric, case-sensitive. Pass 0 if not used
 * \param[in] long_opt     Long option name, must be alpha-numeric or hyphens, case-insensitive. Can not start or end
 *                         with hyphens. Pass NULL if not used.
 * \param[in] val_required Whether or not the option requires a value.
 * \param[in] description  Field description. Used with output putting help message.
 * \param[in] cb           Callback to call with value. NULL will be passed if no value provided
 *
 * \return M_TRUE on success, M_FALSE on failure.
 */
M_API M_bool M_getopt_adddecimal(M_getopt_t *g, char short_opt, const char *long_opt, M_bool val_required, const char *description, M_getopt_decimal_cb cb);
 

/*! Add a string parameter.
 *
 * \param[in] g            Getopt object
 * \param[in] short_opt    Short option, must be alpha-numeric, case-sensitive. Pass 0 if not used
 * \param[in] long_opt     Long option name, must be alpha-numeric or hyphens, case-insensitive. Can not start or end
 *                         with hyphens. Pass NULL if not used.
 * \param[in] val_required Whether or not the option requires a value.
 * \param[in] description  Field description. Used with output putting help message.
 * \param[in] cb           Callback to call with value. NULL will be passed if no value provided
 *
 * \return M_TRUE on success, M_FALSE on failure.
 */
M_API M_bool M_getopt_addstring(M_getopt_t *g, char short_opt, const char *long_opt, M_bool val_required, const char *description, M_getopt_string_cb cb);
 

/*! Add a boolean parameter.
 *
 * \param[in] g            Getopt object
 * \param[in] short_opt    Short option, must be alpha-numeric, case-sensitive. Pass 0 if not used
 * \param[in] long_opt     Long option name, must be alpha-numeric or hyphens, case-insensitive. Can not start or end
 *                         with hyphens. Pass NULL if not used.
 * \param[in] val_required Whether or not the option requires a value. If M_FALSE this is treated as a flag and will be
 *                         treated as M_TRUE in the value of the callback. If M_FALSE a value cannot be provided.
 * \param[in] description  Field description. Used with output putting help message.
 * \param[in] cb           Callback to call with value. Value will be M_TRUE if no value provided. Considered a
 *                         flag enabling in this case.
 *
 * \return M_TRUE on success, M_FALSE on failure.
 */
M_API M_bool M_getopt_addboolean(M_getopt_t *g, char short_opt, const char *long_opt, M_bool val_required, const char *description, M_getopt_boolean_cb cb);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 
/*! Parse command line arguments.
 *
 * \param[in] g        Getopt object
 * \param[in] argv     Array of arguments. Will not be modified.
 * \param[in] argc     Number of arguments.
 * \param[in] opt_fail On failure will have the argument that caused the failure.
 * \param[in] thunk    Thunk that will be passed to callbacks.
 *
 * \return Result.
 */
M_API M_getopt_error_t M_getopt_parse(const M_getopt_t *g, const char * const *argv, int argc, const char **opt_fail, void *thunk);

/*! @} */

__END_DECLS

#endif /* __GETOPT_H__ */
