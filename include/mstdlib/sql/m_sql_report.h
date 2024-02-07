/* The MIT License (MIT)
 *
 * Copyright (c) 2017 Monetra Technologies, LLC.
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

#ifndef __M_SQL_REPORT_H__
#define __M_SQL_REPORT_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/sql/m_sql.h>
#include <mstdlib/sql/m_sql_stmt.h>
#include <mstdlib/formats/m_json.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_sql_report SQL Report Generation
 *  \ingroup m_sql
 *
 * SQL Report Generation
 *
 * @{
 */

typedef enum {
    M_SQL_REPORT_FLAG_NONE              = 0,      /*!< No special flags */
    M_SQL_REPORT_FLAG_ALWAYS_ENCAP      = 1 << 0, /*!< CSV: Always encapsulate the fields, even if there are no conflicting characters.
                                                   *   However, NULL fields will still never be encapsulated so NULL vs empty
                                                   *   strings can be determined by the output. Ignored for JSON. */
    M_SQL_REPORT_FLAG_OMIT_HEADERS      = 1 << 1, /*!< CSV: Do not output the first row as headers. Ignored for JSON. */
    M_SQL_REPORT_FLAG_PASSTHRU_UNLISTED = 1 << 2  /*!< By default, all columns in the output report must be specified.  This flag
                                                       will use the column name returned from the SQL server as the header and
                                                       pass the data thru with no manipulation.  Other columns may be overwritten
                                                       or added, or even suppressed.  When set, columns will be output in the order
                                                       returned from the SQL server, additional added columns will be appended to
                                                       the end. */
} M_sql_report_flags_t;


/*! Error conditions returned by #M_sql_report_fetch_cb_t */
typedef enum {
    M_SQL_REPORT_ERROR    = 0, /*!< Error, abort report generation */
    M_SQL_REPORT_SUCCESS  = 1, /*!< Success */
    M_SQL_REPORT_SKIP_ROW = 2  /*!< Do not output this row, but continue */
} M_sql_report_cberror_t;


struct M_sql_report;
/*! Object holding the definition for report processing */
typedef struct M_sql_report M_sql_report_t;

struct M_sql_report_state;
/*! Object holding state data for M_sql_report_process_partial() */
typedef struct M_sql_report_state M_sql_report_state_t;


/*! Create a report object for processing SQL query results into a delimited data form.
 *
 *  Report processing is often used to turn SQL query results into delimited data
 *  like CSV.
 *
 *  Each column to be output must be defined, or set #M_SQL_REPORT_FLAG_PASSTHRU_UNLISTED
 *  to pass through the data elements in their native form.
 *
 *  \param[in] flags  Bitmap of #M_sql_report_flags_t values to control behavior.
 *  \return Initialized report object
 */
M_API M_sql_report_t *M_sql_report_create(M_uint32 flags);


/*! Destroy the report object.
 *
 * \param[in] report Report object to be destroyed.
 */
M_API void M_sql_report_destroy(M_sql_report_t *report);

/*! Set desired CSV delimiters, encapsulation, and escaping sequences to be used for the
 *  output data.  Ignored for JSON.
 *
 *  If this function is not called, the defaults are used.
 *
 * \param[in] report            Initialized report object.
 * \param[in] field_delim       Delimiter to use between fields, default is a comma (,). NULL to not change.
 * \param[in] field_delim_size  Number of characters used in field delimiter.  Max size 8.
 * \param[in] row_delim         Delimiter to use between rows, default is a new line (\\r\\n - CRLF). NULL to not change.
 * \param[in] row_delim_size    Number of characters used in row delimiter.  Max size 8.
 * \param[in] field_encaps      Encapsulation character to use for field data that may
 *                              contain the field_delim or row_delim, default is a double quote. NULL to not change.
 * \param[in] field_encaps_size Number of characters used in field encapsulation. Max size 8.
 * \param[in] field_escape      Escape character to use if the field contains the encapsulation char,
 *                              default is the same as the encapsulation, a double quote ("), as this is
 *                              what is defined by RFC4180 (CSV)
 * \param[in] field_escape_size Number of characters used in field escaping. Max size 8.
 * \return M_TRUE on success, M_FALSE on usage error.
 */
M_API M_bool M_sql_report_set_delims(M_sql_report_t *report, const unsigned char *field_delim, size_t field_delim_size, const unsigned char *row_delim, size_t row_delim_size, const unsigned char *field_encaps, size_t field_encaps_size, const unsigned char *field_escape, size_t field_escape_size);


/*! Prototype for fetch callback registered with M_sql_report_set_fetch_cb()
 *
 * \param[in] stmt Statement handle object after fetch
 * \param[in] arg  Argument passed to M_sql_report_process() or M_sql_report_process_partial().
 *
 * \return M_TRUE on success or M_FALSE on failure which will cause report processing to stop

 */
typedef M_bool (*M_sql_report_fetch_cb_t)(M_sql_stmt_t *stmt, void *arg);

/*! Register a callback to be automatically called any time M_sql_stmt_fetch() is
 *  called successfully from within M_sql_report_process() or M_sql_report_process_partial().
 *
 *  This may be used if some bulk operation needs to process the data just fetched
 *  prior to processing the individual rows that were fetched.
 *
 *  \param[in] report Initialized report object
 *  \param[in] fetch_cb Callback to run every time M_sql_stmt_fetch() is successfully called.
 *  \return M_TRUE on success, or M_FALSE on error
 */
M_API M_bool M_sql_report_set_fetch_cb(M_sql_report_t *report, M_sql_report_fetch_cb_t fetch_cb);

/*! Function callback prototype to use for cell formatting.
 *
 * This function signature is used to process every column in a report, the output buffer
 * is passed in by reference of a certain length.  If the buffer is insufficient for the
 * needs of the formatting function, the caller should allocate a new pointer instead
 * and set it to the new buffer location.  The report generation function will automatically
 * free the callback's buffer.
 *
 * \param[in]    stmt      Pointer to statement object being processed.
 * \param[in]    arg       Custom user-supplied argument for registered callbacks.
 * \param[in]    name      Assigned report name of column being processed (not necessarily the name of the SQL column returned)
 * \param[in]    row       Row of result set currently being processed.
 * \param[in]    col       Index of result set column being processed, or -1 if no specific column is referenced.
 * \param[out]   buf       Pre-allocated #M_buf_t buffer is provided to write column output. The provided buffer is empty,
 *                         it is not pre-filled with column data as 'col' may be -1 or otherwise not a 1:1 mapping between
 *                         input and output columns.  It is up to the person implementing the callback to use the normal
 *                         \link m_sql_stmt_result M_sql_stmt_result_*() \endlink functions to get the desired data.
 * \param[out]   is_null   Output parameter that if set to M_TRUE, will ignore any contents in buf.  It will also prevent
 *                         quoting of the output cell so the output differentiates a blank cell (quoted) vs a NULL cell
 *                         (unquoted).
 * \return \return #M_SQL_REPORT_SUCCESS on success or #M_SQL_REPORT_ERROR on failure which will cause report processing to stop
 *         or to just skip the row, return #M_SQL_REPORT_SKIP_ROW.
 */
typedef M_sql_report_cberror_t (*M_sql_report_cell_cb_t)(M_sql_stmt_t *stmt, void *arg, const char *name, size_t row, ssize_t col, M_buf_t *buf, M_bool *is_null);

/*! Callback template for column passthru.
 *
 *  Any data on file will be passed-thru as-is, except for Binary data which will be automatically
 *  base64 encoded as the report output mandates string data only.  If the cell is NULL, it will
 *  be output as blank.
 */
M_API M_sql_report_cberror_t M_sql_report_cell_cb_passthru(M_sql_stmt_t *stmt, void *arg, const char *name, size_t row, ssize_t col, M_buf_t *buf, M_bool *is_null);

/*! Callback template for outputting an integer column stored with a 2-digit implied decimal point as an actual
 *  decimal with 2 decimal places.  E.g.:
 *   -  1 -> 0.01
 *   -  100 -> 1.00
 *  If the cell is NULL, a blank column will be output instead of 0.00
 */
M_API M_sql_report_cberror_t M_sql_report_cell_cb_int2dec(M_sql_stmt_t *stmt, void *arg, const char *name, size_t row, ssize_t col, M_buf_t *buf, M_bool *is_null);

/*! Callback template for outputting an integer column stored with a 5-digit implied decimal point as an actual
 *  decimal with 5 decimal places.  E.g.:
 *   -  1 -> 0.00001
 *   -  100000 -> 1.00000
 *  If the cell is NULL, a blank column will be output instead of 0.00000
 */
M_API M_sql_report_cberror_t M_sql_report_cell_cb_int5dec(M_sql_stmt_t *stmt, void *arg, const char *name, size_t row, ssize_t col, M_buf_t *buf, M_bool *is_null);

/*! Callback template for outputting an integer column stored with a 5-digit implied decimal point as an actual
 *  decimal with between 2 and 5 decimal places.  E.g.:
 *   -  1 -> 0.00001
 *   -  123000 -> 1.23
 *   -  111111 -> 1.11111
 *  If the cell is NULL, a blank column will be output instead of 0.00000
 */
M_API M_sql_report_cberror_t M_sql_report_cell_cb_int5min2dec(M_sql_stmt_t *stmt, void *arg, const char *name, size_t row, ssize_t col, M_buf_t *buf, M_bool *is_null);

/*! Callback template for outputting a boolean value column with yes or no.
 *
 *  If the cell is NULL, a blank column will be output instead of yes or no.
 */
M_API M_sql_report_cberror_t M_sql_report_cell_cb_boolyesno(M_sql_stmt_t *stmt, void *arg, const char *name, size_t row, ssize_t col, M_buf_t *buf, M_bool *is_null);

/*! Register column to be output in report.
 *
 *  If #M_SQL_REPORT_FLAG_PASSTHRU_UNLISTED was used to initialize the report object, then
 *  if the sql_col_name or sql_col_idx matches a column (rather than being NULL and -1, respectively),
 *  then instead of adding a column, it overwrites its behavior ... either output column name, or the
 *  default callback can be changed from the default of passthrough.
 *
 * \param[in] report       Initialized report object.
 * \param[in] name         Name of column (used for headers in report)
 * \param[in] cb           Callback to use for formatting the column.
 * \param[in] sql_col_name Optional, use NULL if not provieded. Retuned SQL column name returned with the
 *                         data from the SQL server.  This will be dereferenced and passed to the callback.
 * \param[in] sql_col_idx  Optional, use -1 if not provided.  Returned SQL column index returned with the
 *                         data from the SQL server.  This will be passed to the callback.
 * \return M_TRUE on success, M_FALSE on failure (misuse) */
M_API M_bool M_sql_report_add_column(M_sql_report_t *report, const char *name, M_sql_report_cell_cb_t cb, const char *sql_col_name, ssize_t sql_col_idx);

/*! Hide a column from a report if #M_SQL_REPORT_FLAG_PASSTHRU_UNLISTED was set.
 *
 * When #M_SQL_REPORT_FLAG_PASSTHRU_UNLISTED all columns in a report will be listed.  This can
 * be used to hide specific columns.
 *
 * \param[in] report       Initialized report object.
 * \param[in] sql_col_name Conditional.  Name of column to hide, or NULL.  Must be set if sql_col_idx is -1.
 * \param[in] sql_col_idx  Conditional. Index of column to hide, or -1. Must be set if sql_col_name is NULL.
 * \return M_TRUE on success, M_FALSE on misuse.
 */
M_API M_bool M_sql_report_hide_column(M_sql_report_t *report, const char *sql_col_name, ssize_t sql_col_idx);


struct M_sql_report_filter;
/*! Filter object created by M_sql_report_filter_create() */
typedef struct M_sql_report_filter M_sql_report_filter_t;

typedef enum {
    M_SQL_REPORT_FILTER_TYPE_OR  = 1, /*!< Rules for filter will be treated as OR */
    M_SQL_REPORT_FILTER_TYPE_AND = 2  /*!< Rules for filter will be treated as AND */
} M_sql_report_filter_type_t;

/*! Create filter object
 *
 *  \param[in] type  Type of filter to create
 *  \return filter object on success
 */
M_API M_sql_report_filter_t *M_sql_report_filter_create(M_sql_report_filter_type_t type);

/*! Destroy filter object
 *
 *  \note Do NOT call if passed to M_sql_report_add_filter() as it takes ownership
 *
 *  \param[in] filter Initialized filter object by M_sql_report_filter_create()
 */
M_API void M_sql_report_filter_destroy(M_sql_report_filter_t *filter);

typedef enum {
    M_SQL_REPORT_FILTER_RULE_MATCHES = 1,     /*!< Data matches */
    M_SQL_REPORT_FILTER_RULE_NOT_MATCHES,     /*!< Data does not match */
    M_SQL_REPORT_FILTER_RULE_CONTAINS,        /*!< Data contains (sub string) */
    M_SQL_REPORT_FILTER_RULE_NOT_CONTAINS,    /*!< Data does not contain (sub string) */
    M_SQL_REPORT_FILTER_RULE_BEGINS_WITH,     /*!< Data begins with */
    M_SQL_REPORT_FILTER_RULE_NOT_BEGINS_WITH, /*!< Data does not begin with */
    M_SQL_REPORT_FILTER_RULE_ENDS_WITH,       /*!< Data ends with */
    M_SQL_REPORT_FILTER_RULE_NOT_ENDS_WITH,   /*!< Data does not end with */
    M_SQL_REPORT_FILTER_RULE_EMPTY,           /*!< Data is empty */
    M_SQL_REPORT_FILTER_RULE_NOT_EMPTY        /*!< Data is not empty */
} M_sql_report_filter_rule_t;

/*! Add filter rule
 *
 *  \note when using OR type filters, you can specify the same column more than once
 *
 * \param[in] filter           Initialized filter object from M_sql_report_filter_create()
 * \param[in] column           Name of column in report
 * \param[in] rule             Type of rule
 * \param[in] case_insensitive For rules with data, whether the data should match case insensitive or not
 * \param[in] data             Data for matching
 * \return M_TRUE on success, M_FALSE on failure
 *
 */
M_API M_bool M_sql_report_filter_add_rule(M_sql_report_filter_t *filter, const char *column, M_sql_report_filter_rule_t rule, M_bool case_insensitive, const char *data);


/*! Attach a filter to a report.  Only a single filter can be added to a report.
 *
 * \note in the future filters will be allowed to add subfilters to do complex logic, but that is not yet supported
 *
 * This function will take ownership of the filter object
 * \param[in] report   Initialized report object
 * \param[in] filter   Initialized filter object with at least one rule
 * \return M_TRUE on success, M_FALSE on failure (such as bad arguments)
 */
M_API M_bool M_sql_report_add_filter(M_sql_report_t *report, M_sql_report_filter_t *filter);


/*! Process the results from the SQL statement based on the report template configured.
 *
 *  This function will call the registered report output generation functions to output
 *  each desired column of the report.  If row fetching is used due to M_sql_stmt_set_max_fetch_rows(),
 *  this will automatically call M_sql_stmt_fetch() until all rows are returned.
 *
 *  No state is tracked in the report handle, it may be reused, and used concurrently if
 *  the implementor decides to cache the handle.
 *
 * \param[in]  report     Initialized report object.
 * \param[in]  stmt       Executed statement handle.
 * \param[in]  arg        Custom user-supplied argument to be passed through to registered callbacks for column formatting.
 * \param[out] out        Formatted report data is returned in this variable and must be free'd by the caller.
 * \param[out] out_len    Optional. Returned length of output data. Should be equivalent to M_str_len(out), but
 *                        on huge reports may be more efficient to already know the size.
 * \param[out] error      Buffer to hold error message.
 * \param[in]  error_size Size of error buffer passed in.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t error conditions. If an internal error is
 *         generated, the text from the error can be found in the statement handle's error buffer via
 *         M_sql_stmt_get_error_string().
 */
M_API M_sql_error_t M_sql_report_process(const M_sql_report_t *report, M_sql_stmt_t *stmt, void *arg, char **out, size_t *out_len, char *error, size_t error_size);


/*! Process the results from the SQL statement based on the report template configured and
 *  append to the provided JSON array.
 *
 *  This function will call the registered report output generation functions to output
 *  each desired column of the report.  If row fetching is used due to M_sql_stmt_set_max_fetch_rows(),
 *  this will automatically call M_sql_stmt_fetch() until all rows are returned.
 *
 *  No state is tracked in the report handle, it may be reused, and used concurrently if
 *  the implementor decides to cache the handle.
 *
 * \param[in]     report     Initialized report object.
 * \param[in]     stmt       Executed statement handle.
 * \param[in]     arg        Custom user-supplied argument to be passed through to registered callbacks for column formatting.
 * \param[in,out] json       Passed in initialized json array node that each row will be appended to.
 * \param[out]    error      Buffer to hold error message.
 * \param[in]     error_size Size of error buffer passed in.
 * \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t error conditions. If an internal error is
 *         generated, the text from the error can be found in the statement handle's error buffer via
 *         M_sql_stmt_get_error_string().
 */
M_API M_sql_error_t M_sql_report_process_json(const M_sql_report_t *report, M_sql_stmt_t *stmt, void *arg, M_json_node_t *json, char *error, size_t error_size);


/*! Process a chunk of report data rather than the whole report.
 *
 *  This function is useful if it is necessary to send a report in pieces either to a file
 *  or via a network connection, especially if the report may become extremely large and
 *  exceed the memory capabilities of the machine.
 *
 *  This function will be called in a loop until the return value is NOT #M_SQL_ERROR_SUCCESS_ROW,
 *  it will fill in the user-supplied #M_buf_t with the data.  It is up to the user to clear
 *  the data from this buffer if the same buffer handle is passed in, or create a new handle,
 *  otherwise data will be appended.
 *
 *  \warning The caller MUST call this repeatedly until a return value other than #M_SQL_ERROR_SUCCESS_ROW is
 *           returned or otherwise risk memory leaks, or possibly holding a lock on an SQL connection.
 *
 *  \param[in]     report     Initialized report object.
 *  \param[in]     stmt       Executed statement handle.
 *  \param[in]     max_rows   Maximum number of rows to output per pass.  Or 0 to output all.  Typically it makes
 *                            more sense to just call M_sql_report_process() if you want to use 0 for this value.
 *  \param[in]     arg        Custom user-supplied argument to be passed through to registered callbacks for column formatting.
 *  \param[in,out] buf        User-supplied buffer to append report data to.
 *  \param[in,out] state      Pointer to an #M_sql_report_state_t * object, initialized to NULL on first pass.  When there
 *                            are more rows available, pass the same returned pointer back in.  When the report generation
 *                            is complete (last pass), this pointer will be automatically cleaned up.
 *  \param[out]    error      Buffer to hold error message.
 *  \param[in]     error_size Size of error buffer passed in.
 *  \return #M_SQL_ERROR_SUCCESS on successful completion of the report, or #M_SQL_ERROR_SUCCESS_ROW if this function
 *          must be called again to get the remaining report data.  On failure, one of the #M_sql_error_t errors may
 *          be returned
 */
M_API M_sql_error_t M_sql_report_process_partial(const M_sql_report_t *report, M_sql_stmt_t *stmt, size_t max_rows, void *arg, M_buf_t *buf, M_sql_report_state_t **state, char *error, size_t error_size);

/*! If for some reason a report must be aborted when using partial processing, this will clear up the memory associated
 *  with the state handle
 *
 *  \param[in] state #M_sql_report_state_t * object populated from M_sql_report_process_partial() or M_sql_report_process_partial_json()
 */
M_API void M_sql_report_state_cancel(M_sql_report_state_t *state);

/*! Process a chunk of report data rather than the whole report and append to the provided JSON array.
 *
 *  This function is useful if it is necessary to send a report in pieces either to a file
 *  or via a network connection, especially if the report may become extremely large and
 *  exceed the memory capabilities of the machine.
 *
 *  This function will be called in a loop until the return value is NOT #M_SQL_ERROR_SUCCESS_ROW,
 *  it will fill in the user-supplied #M_buf_t with the data.  It is up to the user to clear
 *  the data from this buffer if the same buffer handle is passed in, or create a new handle,
 *  otherwise data will be appended.
 *
 *  \warning The caller MUST call this repeatedly until a return value other than #M_SQL_ERROR_SUCCESS_ROW is
 *           returned or otherwise risk memory leaks, or possibly holding a lock on an SQL connection.
 *
 *  \param[in]     report     Initialized report object.
 *  \param[in]     stmt       Executed statement handle.
 *  \param[in]     max_rows   Maximum number of rows to output per pass.  Or 0 to output all.  Typically it makes
 *                            more sense to just call M_sql_report_process() if you want to use 0 for this value.
 *  \param[in]     arg        Custom user-supplied argument to be passed through to registered callbacks for column formatting.
 * \param[in,out]  json       Passed in initialized json array node that each row will be appended to.
 *  \param[in,out] state      Pointer to an #M_sql_report_state_t * object, initialized to NULL on first pass.  When there
 *                            are more rows available, pass the same returned pointer back in.  When the report generation
 *                            is complete (last pass), this pointer will be automatically cleaned up.
 *  \param[out]    error      Buffer to hold error message.
 *  \param[in]     error_size Size of error buffer passed in.
 *  \return #M_SQL_ERROR_SUCCESS on successful completion of the report, or #M_SQL_ERROR_SUCCESS_ROW if this function
 *          must be called again to get the remaining report data.  On failure, one of the #M_sql_error_t errors may
 *          be returned
 */
M_API M_sql_error_t M_sql_report_process_partial_json(const M_sql_report_t *report, M_sql_stmt_t *stmt, size_t max_rows, void *arg, M_json_node_t *json, M_sql_report_state_t **state, char *error, size_t error_size);


/*! @} */

__END_DECLS

#endif /* __M_SQL_REPORT_H__ */
