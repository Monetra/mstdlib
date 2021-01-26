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

#ifndef __M_CONF_H__
#define __M_CONF_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/mstdlib.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_conf CONF
 *  \ingroup m_formats
 *
 * Wrapper around mstdlib's INI module for parsing configuration files and saving
 * values. The file must be formatted as described in the INI module. This does
 * not cover other file formats, such as JSON or XML.
 *
 * This module is used for reading values from a configuration file directly into
 * the provided memory. If you want to hold the values in temporary memory for
 * manipulation and retrieval, you should use the Settings module.
 *
 * You begin by building out all the key registrations, which specify the key to
 * parse and where to store the value. There are multiple methods to handle the
 * various data types that can be set. If you need to set a non-standardized data
 * type like an enum or struct, you should use the custom registration method.
 *
 * When building a key registration, you can also specify validators (depending on
 * the data type) and a default value to use if a value isn't specified in the
 * config file.
 *
 * Every registration type has a corresponding conversion callback specific to it.
 * If a callback is set with the registration, then that callback must do all the
 * work of validating, converting, and storing the value.
 *
 * Once all the registrations are set, you send the call to run through them all
 * at once. We made the design decision to set everything up first and then parse
 * the values over parsing on the fly as keys are registered so that all errors
 * would be contained in one area. Instead of needing to do error checking for
 * every registration, you only have to check the outcome of M_conf_parse().
 *
 * Alternatively, you can access a key's value directly without setting up a
 * registration.
 *
 * To received debug and/or error messages, you can register a callback that will
 * be provided the message as well as the filename of the file currently being
 * processed. This is optional.
 *
 * Example:
 *
 * \code{.c}
 *     M_int8   num1;
 *     M_uint8  num2;
 *     M_uint32 sys_flags;
 *
 *     static void log_conf_debug(const char *path, const char *msg)
 *     {
 *         M_printf("[DEBUG] %s: %s\n", path, msg);
 *     }
 *     static void log_conf_error(const char *path, const char *msg)
 *     {
 *         M_printf("[ERROR] %s: %s\n", path, msg);
 *     }
 *
 *     static M_bool handle_num2(M_uint32 *mem, const char *value, M_uint32 default_val)
 *     {
 *         M_int64 num;
 *
 *         if (M_str_isempty(value)) {
 *            *mem = default_val;
 *            return M_TRUE;
 *         }
 *
 *         num = M_str_to_int64(value);
 *         if (num < 0) {
 *            *mem = 0;
 *         } else if (num > 64) {
 *            *mem = 64;
 *         } else {
 *            *mem = num;
 *         }
 *
 *         return M_TRUE;
 *     }
 *
 *     static M_bool handle_flags(const char *value)
 *     {
 *         char   **flags;
 *         size_t   num_flags;
 *         size_t   i;
 *
 *         sys_flags = 0;
 *
 *         flags = M_str_explode_str('|', value, &num_flags);
 *         if (flags == NULL)
 *            return M_FALSE;
 *
 *         for (i=0; i<num_flags; i++) {
 *            sys_flags |= parse_flag(flags[i]);
 *         }
 *         M_str_explode_free(flags, num_flags);
 *
 *         return M_TRUE;
 *     }
 *
 *     static M_bool validate_data(void *data)
 *     {
 *         char *name_buf = data;
 *
 *         if (M_str_len(name_buf) > 128) {
 *            return M_FALSE;
 *         }
 *
 *         if (num1 > num2) {
 *            return M_FALSE;
 *         }
 *
 *         return M_TRUE;
 *     }
 *
 *     M_bool parse_conf(const char *path)
 *     {
 *         M_conf_t *conf;
 *         char      name_buf[256];
 *         char     *desc;
 *         M_bool    active;
 *         M_bool    ret;
 *
 *         conf = M_conf_create(path, M_FALSE);
 *         if (conf == NULL) {
 *            M_printf("Error reading conf");
 *            return M_FALSE;
 *         }
 *
 *         M_conf_add_debug_logger(conf, log_conf_debug);
 *         M_conf_add_error_logger(conf, log_conf_error);
 *
 *         M_conf_register_buf(conf, "name", name_buf, sizeof(name_buf), "unknown", "^[a-zA-Z]$", NULL);
 *         M_conf_register_strdup(conf, "description", &desc, NULL, NULL, NULL);
 *         M_conf_register_int8(conf, "num1", &num1, 0, -10, 10, NULL);
 *         M_conf_register_uint32(conf, "num2", &num2, 16, 0, 0, handle_num2);
 *         M_conf_register_bool(conf, "active", &active, M_FALSE, NULL);
 *         M_conf_register_custom(conf, "flags", handle_flags);
 *
 *         M_conf_register_validator(conf, validate_data, name_buf);
 *
 *         ret = M_conf_parse(conf);
 *
 *         M_conf_destroy(conf);
 *
 *         if (ret) {
 *            M_printf("Successfully parsed conf");
 *         } else {
 *            M_printf("Error parsing conf");
 *         }
 *
 *         return ret;
 *     }
 * \endcode
 *
 * @{
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

struct M_conf_t;
typedef struct M_conf_t M_conf_t;

/*! Callback prototype for logging messages while parsing values.
 *
 * \param[in] path   Path of configuration file being read.
 * \param[in] msg    Message to log.
 */
typedef void (*M_conf_logger_t)(const char *path, const char *msg);

/*! Callback prototype for manual string-to-string conversions.
 *
 * \param[in] buf           Buffer to store the value.
 * \param[in] buf_len       Length of buffer.
 * \param[in] value         The value in the configuration file for the registered key. May be NULL.
 * \param[in] default_val   The default value as registered for the key.
 *
 * \return                  M_TRUE if the conversion was successful. Otherwise, M_FALSE.
 */
typedef M_bool (*M_conf_converter_buf_t)(char *buf, size_t buf_len, const char *value, const char *default_val);

/*! Callback prototype for manual string-to-string conversions.
 *
 * \param[in] mem           Memory address where the value should be stored. The caller is responsible for free'ing this.
 * \param[in] value         The value in the configuration file for the registered key. May be NULL.
 * \param[in] default_val   The default value as registered for the key.
 *
 * \return                  M_TRUE if the conversion was successful. Otherwise, M_FALSE.
 */
typedef M_bool (*M_conf_converter_strdup_t)(char **mem, const char *value, const char *default_val);

/*! Callback prototype for manual string-to-integer conversions for signed 8-bit integers.
 *
 * \param[in] mem           Memory address where the value should be stored.
 * \param[in] value         The value in the configuration file for the registered key. May be NULL.
 * \param[in] default_val   The default value as registered for the key.
 *
 * \return                  M_TRUE if the conversion was successful. Otherwise, M_FALSE.
 */
typedef M_bool (*M_conf_converter_int8_t)(M_int8 *mem, const char *value, M_int8 default_val);

/*! Callback prototype for manual string-to-integer conversions for signed 16-bit integers.
 *
 * \param[in] mem           Memory address where the value should be stored.
 * \param[in] value         The value in the configuration file for the registered key. May be NULL.
 * \param[in] default_val   The default value as registered for the key.
 *
 * \return                  M_TRUE if the conversion was successful. Otherwise, M_FALSE.
 */
typedef M_bool (*M_conf_converter_int16_t)(M_int16 *mem, const char *value, M_int16 default_val);

/*! Callback prototype for manual string-to-integer conversions for signed 32-bit integers.
 *
 * \param[in] mem           Memory address where the value should be stored.
 * \param[in] value         The value in the configuration file for the registered key. May be NULL.
 * \param[in] default_val   The default value as registered for the key.
 *
 * \return                  M_TRUE if the conversion was successful. Otherwise, M_FALSE.
 */
typedef M_bool (*M_conf_converter_int32_t)(M_int32 *mem, const char *value, M_int32 default_val);

/*! Callback prototype for manual string-to-integer conversions for signed 64-bit integers.
 *
 * \param[in] mem           Memory address where the value should be stored.
 * \param[in] value         The value in the configuration file for the registered key. May be NULL.
 * \param[in] default_val   The default value as registered for the key.
 *
 * \return                  M_TRUE if the conversion was successful. Otherwise, M_FALSE.
 */
typedef M_bool (*M_conf_converter_int64_t)(M_int64 *mem, const char *value, M_int64 default_val);

/*! Callback prototype for manual string-to-integer conversions for unsigned 8-bit integers.
 *
 * \param[in] mem           Memory address where the value should be stored.
 * \param[in] value         The value in the configuration file for the registered key. May be NULL.
 * \param[in] default_val   The default value as registered for the key.
 *
 * \return                  M_TRUE if the conversion was successful. Otherwise, M_FALSE.
 */
typedef M_bool (*M_conf_converter_uint8_t)(M_uint8 *mem, const char *value, M_uint8 default_val);

/*! Callback prototype for manual string-to-integer conversions for unsigned 16-bit integers.
 *
 * \param[in] mem           Memory address where the value should be stored.
 * \param[in] value         The value in the configuration file for the registered key. May be NULL.
 * \param[in] default_val   The default value as registered for the key.
 *
 * \return                  M_TRUE if the conversion was successful. Otherwise, M_FALSE.
 */
typedef M_bool (*M_conf_converter_uint16_t)(M_uint16 *mem, const char *value, M_uint16 default_val);

/*! Callback prototype for manual string-to-integer conversions for unsigned 32-bit integers.
 *
 * \param[in] mem           Memory address where the value should be stored.
 * \param[in] value         The value in the configuration file for the registered key. May be NULL.
 * \param[in] default_val   The default value as registered for the key.
 *
 * \return                  M_TRUE if the conversion was successful. Otherwise, M_FALSE.
 */
typedef M_bool (*M_conf_converter_uint32_t)(M_uint32 *mem, const char *value, M_uint32 default_val);

/*! Callback prototype for manual string-to-integer conversions for unsigned 64-bit integers.
 *
 * \param[in] mem           Memory address where the value should be stored.
 * \param[in] value         The value in the configuration file for the registered key. May be NULL.
 * \param[in] default_val   The default value as registered for the key.
 *
 * \return                  M_TRUE if the conversion was successful. Otherwise, M_FALSE.
 */
typedef M_bool (*M_conf_converter_uint64_t)(M_uint64 *mem, const char *value, M_uint64 default_val);

/*! Callback prototype for manual string-to-boolean conversions.
 *
 * \param[in] mem           Memory address where the value should be stored.
 * \param[in] value         The value in the configuration file for the registered key. May be NULL.
 * \param[in] default_val   The default value as registered for the key.
 *
 * \return                  M_TRUE if the conversion was successful. Otherwise, M_FALSE.
 */
typedef M_bool (*M_conf_converter_bool_t)(M_bool *mem, const char *value, M_bool default_val);

/*! Callback prototype for custom conversions. This is used for manually validating and setting a value with
 * M_conf_register_custom();
 *
 * \param[in] mem     Memory address where the value should be stored. May be NULL if address was not registered.
 * \param[in] value   The value in the configuration file for the registered key. May be NULL.
 *
 * \return            M_TRUE if the conversion was successful. Otherwise, M_FALSE.
 */
typedef M_bool (*M_conf_converter_custom_t)(void *mem, const char *value);

/*! Callback prototype for validating arbitrary data.
 *
 * \param[in] data   Arbitrary data, as registered with the callback. May be NULL.
 *
 * \return           M_TRUE if the validation was successful. Otherwise, M_FALSE.
 */
typedef M_bool (*M_conf_validator_t)(void *data);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a new M_conf_t object with the specified ini.
 *
 * \param[in] path             Path to the ini file.
 * \param[in] allow_multiple   M_TRUE to allow a single key to have multiple values. Otherwise, M_FALSE.
 *
 * \return                     A newly allocated M_conf_t object, or NULL on error.
 */
M_API M_conf_t *M_conf_create(const char *path, M_bool allow_multiple);


/*! Destroy an M_conf_t object and free the memory.
 *
 * Before destruction, all unused keys will be logged with the debug loggers (if any).
 *
 * \param[in] conf   The M_conf_t object to destroy.
 */
M_API void M_conf_destroy(M_conf_t *conf);


/*! Add a debug logger to this conf object that will be passed various debug messages while parsing the values.
 *
 * \param[in] conf     The M_conf_t object to attach this logger to.
 * \param[in] logger   The logging callback that will receive the error message.
 */
M_API M_bool M_conf_add_debug_logger(M_conf_t *conf, M_conf_logger_t err_logger);


/*! Add an error logger to this conf object that will be passed any errors that happen while parsing the values.
 *
 * \param[in] conf     The M_conf_t object to attach this logger to.
 * \param[in] logger   The logging callback that will receive the error message.
 */
M_API M_bool M_conf_add_error_logger(M_conf_t *conf, M_conf_logger_t err_logger);


/*! Go through the key registrations and set the values at the specified locations.
 *
 * \param[in] conf   The M_conf_t object to use.
 *
 * \return           M_TRUE if the registrations were processed successfully. Otherwise, M_FALSE. This fails if any of
 *                   the registrations fail, like if a value does not pass the regex check or is outside of the min/max
 *                   bounds.
 */
M_API M_bool M_conf_parse(M_conf_t *conf);


/*! Get a list of keys from the ini file that were not used.
 *
 * \param[in] conf   The M_conf_t object to use.
 *
 * \return           A list of keys.
 */
M_API M_list_str_t *M_conf_unused_keys(M_conf_t *conf);


/*! Get a list of the section in the ini file.
 *
 * \param[in] conf   The M_conf_t object to use.
 *
 * \return           A list of sections.
 */
M_API M_list_str_t *M_conf_get_sections(M_conf_t *conf);


/*! Get the value for the provided key in the ini file. If a single key is allowed to have multiple values, then this
 * returns the first value.
 *
 * \param[in] conf   The M_conf_t object to use.
 * \param[in] key    The key to look up.
 *
 * \return           The value for the key.
 */
M_API const char *M_conf_get_value(M_conf_t *conf, const char *key);


/*! Get all the values for the provided key in the ini file.
 *
 * \param[in] conf   The M_conf_t object to use.
 * \param[in] key    The key to look up.
 *
 * \return           A list of values for the key.
 */
M_API M_list_str_t *M_conf_get_values(M_conf_t *conf, const char *key);


/*! Register a key that will have its value stored in the provided char buf.
 *
 * \param[in]  conf          M_conf_t object to use.
 * \param[in]  key           Key to register.
 * \param[out] buf           Buffer where the value will be stored.
 * \param[in]  buf_len       Length of buffer.
 * \param[in]  default_val   Default value to store, if a value is not set in the ini file. Pass NULL for no default.
 * \param[in]  regex         Regular expression to check the value against. Matching is done in a case-insensitive
 *                           fashion. If the check fails, then M_conf_parse() will also fail. Pass NULL to skip check.
 * \param[in]  converter     Callback for manual conversion. The value will be pulled out of the ini and passed directly
 *                           to the callback, which must do all validation/conversion. The value passed to the callback
 *                           can be NULL. Pass NULL if not needed.
 *
 * \return                   M_TRUE if the registration was successful. Otherwise, M_FALSE. Currently, this returns
 *                           M_FALSE only if any of the arguments are invalid.
 */
M_API M_bool M_conf_register_buf(M_conf_t *conf, const char *key, char *buf, size_t buf_len, const char *default_val, const char *regex, M_conf_converter_buf_t converter);


/*! Register a key that will have its value stored at the provided address as an allocated string.
 *
 * \param[in]  conf          M_conf_t object to use.
 * \param[in]  key           Key to register.
 * \param[out] buf           Address where the value will be stored. The caller is responsible for free'ing this memory.
 * \param[in]  default_val   Default value to store, if a value is not set in the ini file. Pass NULL for no default.
 * \param[in]  regex         Regular expression to check the value against. Matching is done in a case-insensitive
 *                           fashion. If the check fails, then M_conf_parse() will also fail. Pass NULL to skip check.
 * \param[in]  converter     Callback for manual conversion. The value will be pulled out of the ini and passed directly
 *                           to the callback, which must do all validation/conversion. The value passed to the callback
 *                           can be NULL. Pass NULL if not needed.
 *
 * \return                   M_TRUE if the registration was successful. Otherwise, M_FALSE. Currently, this returns
 *                           M_FALSE only if any of the arguments are invalid.
 */
M_API M_bool M_conf_register_strdup(M_conf_t *conf, const char *key, char **address, const char *default_val, const char *regex, M_conf_converter_strdup_t converter);


/*! Register a key that will have its value stored at the provided address as a signed 8-bit integer.
 *
 * \param[in]  conf          M_conf_t object to use.
 * \param[in]  key           Key to register.
 * \param[out] mem           Memory where the value will be stored.
 * \param[in]  default_val   Default value to store, if a value is not set in the ini file.
 * \param[in]  min_val       Minimum allowed value. A value in the ini file less than this will cause M_conf_parse() to
 *                           fail.
 * \param[in]  max_val       Maximum allowed value. A value in the ini file greater than this will cause M_conf_parse()
 *                           to fail.
 * \param[in]  converter     Callback for manual conversion. The value will be pulled out of the ini and passed directly
 *                           to the callback, which must do all validation/conversion. The value passed to the callback
 *                           can be NULL. Pass NULL if not needed.
 *
 * \return                   M_TRUE if the registration was successful. Otherwise, M_FALSE. Currently, this returns
 *                           M_FALSE only if any of the arguments are invalid.
 */
M_API M_bool M_conf_register_int8(M_conf_t *conf, const char *key, M_int8 *mem, M_int8 default_val, M_int8 min_val, M_int8 max_val, M_conf_converter_int8_t converter);


/*! Register a key that will have its value stored at the provided address as a signed 16-bit integer.
 *
 * \param[in]  conf          M_conf_t object to use.
 * \param[in]  key           Key to register.
 * \param[out] mem           Memory where the value will be stored.
 * \param[in]  default_val   Default value to store, if a value is not set in the ini file.
 * \param[in]  min_val       Minimum allowed value. A value in the ini file less than this will cause M_conf_parse() to
 *                           fail.
 * \param[in]  max_val       Maximum allowed value. A value in the ini file greater than this will cause M_conf_parse()
 *                           to fail.
 * \param[in]  converter     Callback for manual conversion. The value will be pulled out of the ini and passed directly
 *                           to the callback, which must do all validation/conversion. The value passed to the callback
 *                           can be NULL. Pass NULL if not needed.
 *
 * \return                   M_TRUE if the registration was successful. Otherwise, M_FALSE. Currently, this returns
 *                           M_FALSE only if any of the arguments are invalid.
 */
M_API M_bool M_conf_register_int16(M_conf_t *conf, const char *key, M_int16 *mem, M_int16 default_val, M_int16 min_val, M_int16 max_val, M_conf_converter_int16_t converter);


/*! Register a key that will have its value stored at the provided address as a signed 32-bit integer.
 *
 * \param[in]  conf          M_conf_t object to use.
 * \param[in]  key           Key to register.
 * \param[out] mem           Memory where the value will be stored.
 * \param[in]  default_val   Default value to store, if a value is not set in the ini file.
 * \param[in]  min_val       Minimum allowed value. A value in the ini file less than this will cause M_conf_parse() to
 *                           fail.
 * \param[in]  max_val       Maximum allowed value. A value in the ini file greater than this will cause M_conf_parse()
 *                           to fail.
 * \param[in]  converter     Callback for manual conversion. The value will be pulled out of the ini and passed directly
 *                           to the callback, which must do all validation/conversion. The value passed to the callback
 *                           can be NULL. Pass NULL if not needed.
 *
 * \return                   M_TRUE if the registration was successful. Otherwise, M_FALSE. Currently, this returns
 *                           M_FALSE only if any of the arguments are invalid.
 */
M_API M_bool M_conf_register_int32(M_conf_t *conf, const char *key, M_int32 *mem, M_int32 default_val, M_int32 min_val, M_int32 max_val, M_conf_converter_int32_t converter);


/*! Register a key that will have its value stored at the provided address as a signed 64-bit integer.
 *
 * \param[in]  conf          M_conf_t object to use.
 * \param[in]  key           Key to register.
 * \param[out] mem           Memory where the value will be stored.
 * \param[in]  default_val   Default value to store, if a value is not set in the ini file.
 * \param[in]  min_val       Minimum allowed value. A value in the ini file less than this will cause M_conf_parse() to
 *                           fail.
 * \param[in]  max_val       Maximum allowed value. A value in the ini file greater than this will cause M_conf_parse()
 *                           to fail.
 * \param[in]  converter     Callback for manual conversion. The value will be pulled out of the ini and passed directly
 *                           to the callback, which must do all validation/conversion. The value passed to the callback
 *                           can be NULL. Pass NULL if not needed.
 *
 * \return                   M_TRUE if the registration was successful. Otherwise, M_FALSE. Currently, this returns
 *                           M_FALSE only if any of the arguments are invalid.
 */
M_API M_bool M_conf_register_int64(M_conf_t *conf, const char *key, M_int64 *mem, M_int64 default_val, M_int64 min_val, M_int64 max_val, M_conf_converter_int64_t converter);


/*! Register a key that will have its value stored at the provided address as an unsigned 8-bit integer.
 *
 * \param[in]  conf          M_conf_t object to use.
 * \param[in]  key           Key to register.
 * \param[out] mem           Memory where the value will be stored.
 * \param[in]  default_val   Default value to store, if a value is not set in the ini file.
 * \param[in]  min_val       Minimum allowed value. A value in the ini file less than this will cause M_conf_parse() to
 *                           fail.
 * \param[in]  max_val       Maximum allowed value. A value in the ini file greater than this will cause M_conf_parse()
 *                           to fail.
 * \param[in]  converter     Callback for manual conversion. The value will be pulled out of the ini and passed directly
 *                           to the callback, which must do all validation/conversion. The value passed to the callback
 *                           can be NULL. Pass NULL if not needed.
 *
 * \return                   M_TRUE if the registration was successful. Otherwise, M_FALSE. Currently, this returns
 *                           M_FALSE only if any of the arguments are invalid.
 */
M_API M_bool M_conf_register_uint8(M_conf_t *conf, const char *key, M_uint8 *mem, M_uint8 default_val, M_uint8 min_val, M_uint8 max_val, M_conf_converter_uint8_t converter);


/*! Register a key that will have its value stored at the provided address as an unsigned 16-bit integer.
 *
 * \param[in]  conf          M_conf_t object to use.
 * \param[in]  key           Key to register.
 * \param[out] mem           Memory where the value will be stored.
 * \param[in]  default_val   Default value to store, if a value is not set in the ini file.
 * \param[in]  min_val       Minimum allowed value. A value in the ini file less than this will cause M_conf_parse() to
 *                           fail.
 * \param[in]  max_val       Maximum allowed value. A value in the ini file greater than this will cause M_conf_parse()
 *                           to fail.
 * \param[in]  converter     Callback for manual conversion. The value will be pulled out of the ini and passed directly
 *                           to the callback, which must do all validation/conversion. The value passed to the callback
 *                           can be NULL. Pass NULL if not needed.
 *
 * \return                   M_TRUE if the registration was successful. Otherwise, M_FALSE. Currently, this returns
 *                           M_FALSE only if any of the arguments are invalid.
 */
M_API M_bool M_conf_register_uint16(M_conf_t *conf, const char *key, M_uint16 *mem, M_uint16 default_val, M_uint16 min_val, M_uint16 max_val, M_conf_converter_uint16_t converter);


/*! Register a key that will have its value stored at the provided address as an unsigned 32-bit integer.
 *
 * \param[in]  conf          M_conf_t object to use.
 * \param[in]  key           Key to register.
 * \param[out] mem           Memory where the value will be stored.
 * \param[in]  default_val   Default value to store, if a value is not set in the ini file.
 * \param[in]  min_val       Minimum allowed value. A value in the ini file less than this will cause M_conf_parse() to
 *                           fail.
 * \param[in]  max_val       Maximum allowed value. A value in the ini file greater than this will cause M_conf_parse()
 *                           to fail.
 * \param[in]  converter     Callback for manual conversion. The value will be pulled out of the ini and passed directly
 *                           to the callback, which must do all validation/conversion. The value passed to the callback
 *                           can be NULL. Pass NULL if not needed.
 *
 * \return                   M_TRUE if the registration was successful. Otherwise, M_FALSE. Currently, this returns
 *                           M_FALSE only if any of the arguments are invalid.
 */
M_API M_bool M_conf_register_uint32(M_conf_t *conf, const char *key, M_uint32 *mem, M_uint32 default_val, M_uint32 min_val, M_uint32 max_val, M_conf_converter_uint32_t converter);


/*! Register a key that will have its value stored at the provided address as an unsigned 64-bit integer.
 *
 * \param[in]  conf          M_conf_t object to use.
 * \param[in]  key           Key to register.
 * \param[out] mem           Memory where the value will be stored.
 * \param[in]  default_val   Default value to store, if a value is not set in the ini file.
 * \param[in]  min_val       Minimum allowed value. A value in the ini file less than this will cause M_conf_parse() to
 *                           fail.
 * \param[in]  max_val       Maximum allowed value. A value in the ini file greater than this will cause M_conf_parse()
 *                           to fail.
 * \param[in]  converter     Callback for manual conversion. The value will be pulled out of the ini and passed directly
 *                           to the callback, which must do all validation/conversion. The value passed to the callback
 *                           can be NULL. Pass NULL if not needed.
 *
 * \return                   M_TRUE if the registration was successful. Otherwise, M_FALSE. Currently, this returns
 *                           M_FALSE only if any of the arguments are invalid.
 */
M_API M_bool M_conf_register_uint64(M_conf_t *conf, const char *key, M_uint64 *mem, M_uint64 default_val, M_uint64 min_val, M_uint64 max_val, M_conf_converter_uint64_t converter);


/*! Register a key that will have its value parsed for boolean truthfulness and stored at the provided address.
 *
 * \param[in]  conf          M_conf_t object to use.
 * \param[in]  key           Key to register.
 * \param[out] mem           Memory where the value will be stored.
 * \param[in]  default_val   Default value to store, if a value is not set in the ini file.
 * \param[in]  min_val       Minimum allowed value. A value in the ini file less than this will cause M_conf_parse() to
 *                           fail.
 * \param[in]  max_val       Maximum allowed value. A value in the ini file greater than this will cause M_conf_parse()
 *                           to fail.
 * \param[in]  converter     Callback for manual conversion. The value will be pulled out of the ini and passed directly
 *                           to the callback, which must do all validation/conversion. The value passed to the callback
 *                           can be NULL. Pass NULL if not needed.
 *
 * \return                   M_TRUE if the registration was successful. Otherwise, M_FALSE. Currently, this returns
 *                           M_FALSE only if any of the arguments are invalid.
 */
M_API M_bool M_conf_register_bool(M_conf_t *conf, const char *key, M_bool *mem, M_bool default_val, M_conf_converter_bool_t converter);


/*! Register a key that will have its value manually validated and converted. This passes the value straight to the
 * converter callback and does not do any validation or conversion internally.
 *
 * \param[in]  conf        M_conf_t object to use.
 * \param[in]  key         Key to register.
 * \param[out] mem         Memory where the value will be stored. Pass NULL if not needed.
 * \param[in]  converter   Callback for manual conversion. The value will be pulled out of the ini and passed directly
 *                         to the callback, which must do all validation/conversion. The value passed to the callback
 *                         can be NULL. This must be used.
 *
 * \return                 M_TRUE if the registration was successful. Otherwise, M_FALSE. Currently, this returns
 *                         M_FALSE only if any of the arguments are invalid.
 */
M_API M_bool M_conf_register_custom(M_conf_t *conf, const char *key, void *mem, M_conf_converter_custom_t converter);


/*! Register a validation callback. All registered validators are called after M_conf_parse() successfully sets the
 * registered keys. This can be used, for example, if you want to validate that one key's value is greater than another
 * key's value, or if you want to print a debug statement for a certain key or keys. This can also be used to run a hook
 * after the registrations are set.
 *
 * \param[in] data   Reference for passing in data in the callback. May be NULL if not needed.
 *
 * \return           M_TRUE if the registration was successful. Otherwise, M_FALSE. Currently, this returns M_FALSE only
 *                   if the callback is invalid.
 */
M_API M_bool M_conf_register_validator(M_conf_t *conf, M_conf_validator_t validator, void *data);


/*! @} */

__END_DECLS

#endif /* __M_CONF_H__ */
