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

#ifndef __M_SQL_TABLE_H__
#define __M_SQL_TABLE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/sql/m_sql.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_sql_schema SQL Table/Schema Management
 *  \ingroup m_sql
 *
 * SQL Table/Schema Management
 *
 * @{
 */


/*! Structure holding table definition */
struct M_sql_table;
/*! Type holding table definition */
typedef struct M_sql_table M_sql_table_t;


/*! Structure holding index definition */
struct M_sql_index;
/*! Type holding index definition */
typedef struct M_sql_index M_sql_index_t;


/*! Flags passed to M_sql_table_add_col() for a column */
typedef enum {
	M_SQL_TABLE_COL_FLAG_NONE    = 0,       /*!< Default, no special flags */
	M_SQL_TABLE_COL_FLAG_NOTNULL = 1 << 0,  /*!< Column is not allowed to be NULL */
} M_sql_table_col_flags_t;


/*! Index creation flags used by M_sql_table_add_index() */
typedef enum {
	M_SQL_INDEX_FLAG_NONE   = 0,     /*!< Default, no special flags */
	M_SQL_INDEX_FLAG_UNIQUE = 1 << 0 /*!< Index enforces a unique constraint */
} M_sql_table_index_flags_t;


/*! Check to see if a table exists by name.
 *
 * \param[in] pool Initialized #M_sql_connpool_t object
 * \param[in] name Table name to check for
 * \return M_TRUE if table exists, M_FALSE otherwise
 */
M_API M_bool M_sql_table_exists(M_sql_connpool_t *pool, const char *name);


/*! Create a table object which aids in creating a table definition, including
 *  indexes to be added to a database.
 *
 *  Table names must start with an alpha character or underscore, and can only
 *  contain alpha-numerics and underscores.
 *
 *  \warning Table names have a maximum length of 58 bytes, however if there are any indexes
 *  also created, then this maximum length cannot be used as the length of the table
 *  name and the length of the index name combined are limited to 58 bytes.  Some older
 *  databases (like Oracle before 12c R2 [March 2017]) were limited to much smaller sizes (30),
 *  it is therefore recommended to keep table names as short as possible, as a rule of thumb,
 *  15 or fewer characters should be safe.
 *
 *  \note All tables require primary keys (added via M_sql_table_add_pk_col()) and failure
 *  will occur if one tries to add a table without a primary key.
 *
 *  The table will not be created until M_sql_table_execute() is called.
 *
 *  \param[in] name  Table name to create
 *  \return Table object, or NULL on error.  Use M_sql_table_destroy() to free the object.
 */
M_API M_sql_table_t *M_sql_table_create(const char *name);


/*! Destroy a table object created with M_sql_table_create().
 *
 *  \param[in] table         Table object initialized by M_sql_table_create()
 */
M_API void M_sql_table_destroy(M_sql_table_t *table);


/*! Add a column to a table.
 *
 *  Column names have a maximum length of 63 characters and must start with
 *  an alpha character or underscore, and can only contain alpha-numerics and
 *  underscores.  However, some older databases might have shorter limits, such
 *  as versions of Oracle prior to 12c R2 (March 2017), were limited to 30 characters.
 *
 *  \param[in] table         Table object initialized by M_sql_table_create()
 *  \param[in] flags         Bitmap of #M_sql_table_col_flags_t flags
 *  \param[in] col_name      Column name to create.
 *  \param[in] datatype      Datatype of column
 *  \param[in] max_len       Maximum length of column (meant for text or binary columns).  Use 0 for
 *                           the maximum size supported by the database for the data type.  It is
 *                           strongly recommended to specify a reasonable maximum size as it may
 *                           have a significant impact on performance of some databases.  Typically
 *                           databases have maximum row sizes, and data over these limits will be
 *                           stored separately (meaning the sum of all columns also matters).
 *  \param[in] default_value Default value to assign to column.  There is little to no
 *                           validation performed on this value, use caution as it is
 *                           inserted directly into the create statement.  Strings must
 *                           be quoted with single quotes.
 *  \return M_TRUE on success, M_FALSE on error (most likely usage, bad name or type)
 */
M_API M_bool M_sql_table_add_col(M_sql_table_t *table, M_uint32 flags, const char *col_name, M_sql_data_type_t datatype, size_t max_len, const char *default_value);


/*! Add a column in the table to the primary key.
 *
 *  The order in which the columns are added to the primary key is how the
 *  primary key will be indexed/created.
 *
 *  The column name specified must exist in the table object.
 *
 *  \param[in] table    Table object initialized by M_sql_table_create()
 *  \param[in] col_name Column name to add to the primary key
 *  \return M_TRUE on success, M_FALSE on error (such as misuse)
 */
M_API M_bool M_sql_table_add_pk_col(M_sql_table_t *table, const char *col_name);


/*! Add an index to the table
 *
 *  \warning Index names have a maximum length of 58 bytes minus the table name
 *  length
 *
 * \param[in] table    Table object initialized by M_sql_table_create()
 * \param[in] flags    Bitmap of #M_sql_table_index_flags_t flags
 * \param[in] idx_name User-chosen index name.  This should be as short as reasonably possible.
 * \return Index object on success, NULL on failure (misuse)
 */
M_API M_sql_index_t *M_sql_table_add_index(M_sql_table_t *table, M_uint32 flags, const char *idx_name);


/*! Add a column to an index
 *
 *  The order in which the columns are added to the index is how the
 *  it will be indexed/created.
 *
 *  The referenced column name must exist in the table definition.
 *
 *  \param[in] idx       Index object initialized by M_sql_table_add_index()
 *  \param[in] col_name  Column name to add to index
 *  \return M_TRUE on success, M_FALSE on failure/misuse.
 */
M_API M_bool M_sql_index_add_col(M_sql_index_t *idx, const char *col_name);


/*! Simplified method to add an index to a table using a comma-delimited string of column names.
 *
 *  Identical to M_sql_table_add_index() followed by M_sql_index_add_col() for each
 *  column in the comma-separated string.
 *
 *  \param[in] table         Table object initialized by M_sql_table_create()
 *  \param[in] flags         Bitmap of #M_sql_table_index_flags_t flags
 *  \param[in] idx_name      User-chosen index name.  This should be as short as reasonably possible.
 *  \param[in] idx_cols_csv  Comma separated list of column names to add to the index.  The columns
 *                           must already exist in the table object.
 *  \return M_TRUE on success, M_FALSE on error/misuse
 */
M_API M_bool M_sql_table_add_index_str(M_sql_table_t *table, M_uint32 flags, const char *idx_name, const char *idx_cols_csv);


/*! Apply the table object definition to the database.
 *
 *  \note This does not destroy the table object. Use M_sql_table_destroy() for that.
 *
 *  \param[in]  pool       Initialized #M_sql_connpool_t object
 *  \param[in]  table      Table object initialized by M_sql_table_create() and populated with columns/indexes and primary keys.
 *  \param[out] error      User-supplied error buffer to output error message.
 *  \param[in]  error_size Size of user-supplied error buffer
 *  \return #M_SQL_ERROR_SUCCESS on success, or one of the #M_sql_error_t return values on failure.
 */
M_API M_sql_error_t M_sql_table_execute(M_sql_connpool_t *pool, M_sql_table_t *table, char *error, size_t error_size);

/*! @} */

/*! \addtogroup m_sql_tabledata SQL Table Data Management
 *  \ingroup m_sql
 *
 * SQL Table Data Management
 *
 * @{
 */


/*! Opaque structure holding field data. */
struct M_sql_tabledata_field;

/*! Opaque structure holding field data. Use corresponding setters/getters to manipulate. */
typedef struct M_sql_tabledata_field M_sql_tabledata_field_t;

/*! Opaque Data structure holding add/edit request transaction data. */
struct M_sql_tabledata_txn;

/*! Opaque Data structure holding add/edit request transaction data. Use the M_sql_tabledata_txn_*() functions to access/modify */
typedef struct M_sql_tabledata_txn M_sql_tabledata_txn_t;

/*! Flags for processing table data fields / columns */
typedef enum {
	M_SQL_TABLEDATA_FLAG_NONE         = 0,      /*!< No Flags */
	M_SQL_TABLEDATA_FLAG_VIRTUAL      = 1 << 0, /*!< Field is a virtual column, multiple serialized virtual columns can be stored in a single 'real' database column under 'table_column'. Any data type except binary may be used. */
	M_SQL_TABLEDATA_FLAG_EDITABLE     = 1 << 1, /*!< Field is allowed to be edited, not add-only */
	M_SQL_TABLEDATA_FLAG_NOTNULL      = 1 << 2, /*!< Field must be specified and is not allowed to be NULL */
	M_SQL_TABLEDATA_FLAG_ID           = 1 << 3, /*!< Field is an ID column (meaning it is used for lookups). Can be assigned on add,
	                                             *   but cannot be used with M_SQL_TABLEDATA_FLAG_EDITABLE or M_SQL_TABLEDATA_FLAG_VIRTUAL. */
	M_SQL_TABLEDATA_FLAG_ID_GENERATE  = 1 << 4, /*!< Auto-generate the ID on the user's behalf.  Must be an ID field. Only one allowed per field definition list. */
	M_SQL_TABLEDATA_FLAG_ID_REQUIRED  = 1 << 5, /*!< On edits, this ID must be specified.  On some DBs, you may not have any required IDs
	                                             *   as there may be multiple lookup indexes */
	M_SQL_TABLEDATA_FLAG_TIMESTAMP    = 1 << 6  /*!< Field is an auto-generated unix timestamp.  Must be INT64. Cannot be specified with ID. Field fetcher will never be called. If M_SQL_TABLEDATA_FLAG_EDITABLE is specified, will update on edit */
} M_sql_tabledata_flags_t;


/*! A callback to perform basic filtering and transformation of a user-input field to how it needs to be
 *  stored within the database.  This can also reject the data and return an error if it does
 *  not meet the requirements.
 *
 *  This callback is called only when new user data is provided.  For instance, on an edit operation,
 *  this callback will NOT be called if the user did not supply the field as the only data we have
 *  is the data from the database which is already sanitized.
 *
 *  \param[in]     txn        Transaction data.  May be used to retrieve other useful information that may be needed for proper validation.
 *  \param[in]     field_name Name of field
 *  \param[in,out] field      Current data in field.  This should be modified in-place if needed to be transformed.
 *  \param[out]    error      Buffer to hold error message
 *  \param[in]     error_len  Length of error buffer
 *  \return M_TRUE on success, M_FALSE on failure (set error buffer!)
 */
typedef M_bool (*M_sql_tabledata_filtertransform_cb)(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_field_t *field, char *error, size_t error_len);

/*! Implementation of M_sql_tabledata_filtertransform_cb to validate and transform a decimal value with 2 places to an integer with 2 implied decimal places */
M_API M_bool M_sql_tabledata_filter_int2dec_cb(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_field_t *field, char *error, size_t error_len);

/*! Implementation of M_sql_tabledata_filtertransform_cb to validate and transform a decimal value with 5 places to an integer with 5 implied decimal places */
M_API M_bool M_sql_tabledata_filter_int5dec_cb(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_field_t *field, char *error, size_t error_len);

/*! Implementation of M_sql_tabledata_filtertransform_cb to validate a string is alpha numeric */
M_API M_bool M_sql_tabledata_filter_alnum_cb(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_field_t *field, char *error, size_t error_len);

/*! Implementation of M_sql_tabledata_filtertransform_cb to validate a string is alpha numeric with possible spaces */
M_API M_bool M_sql_tabledata_filter_alnumsp_cb(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_field_t *field, char *error, size_t error_len);

/*! Implementation of M_sql_tabledata_filtertransform_cb to validate a string is alpha only */
M_API M_bool M_sql_tabledata_filter_alpha_cb(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_field_t *field, char *error, size_t error_len);

/*! Implementation of M_sql_tabledata_filtertransform_cb to validate a string is graph only */
M_API M_bool M_sql_tabledata_filter_graph_cb(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_field_t *field, char *error, size_t error_len);


/*! A callback to perform intensive validation of the data field, which may require performing additional SQL queries.
 *
 *  This callback is always called for the field, regardless of if it has changed (unless on edit and field is non-editable).
 *  Recommended to call M_sql_tabledata_txn_field_changed() if only need to perform operations when the field has changed.
 *
 *  The field data is not provided and must be fetched via M_sql_tabledata_txn_field_get() as it is not known which variant
 *  of the data may be needed. 
 *
 *  The field data may be manipulated if necessary.
 *
 *  \param[in]     sqltrans   SQL transaction to use for any external queries needed.
 *  \param[in]     txn        Transaction data.  May be used to retrieve other useful information that may be needed for proper validation.
 *  \param[in]     field_name Name of field
 *  \param[out]    error      Buffer to hold error message
 *  \param[in]     error_len  Length of error buffer
 *  \return M_SQL_ERROR_SUCCESS or M_SQL_ERROR_USER_SUCCESS if validationsucceeded, other error otherwise (possibly retryable).
 */
typedef M_sql_error_t (*M_sql_tabledata_validate_cb)(M_sql_trans_t *sqltrans, M_sql_tabledata_txn_t *txn, const char *field_name, char *error, size_t error_len);


/*! Structure to be used to define the various fields and columns stored in a table */
typedef struct {
	const char                          *table_column;    /*!< Database column name */
	const char                          *field_name;      /*!< Field name to fetch in order to retrieve column data. For virtual columns, this field name is also used as the tag name. If NULL or blank, means field not used. Reserved for external modification. */
	size_t                               max_column_len;  /*!< Maximum text or binary length of column allowed. For M_SQL_TABLEDATA_FLAG_ID_GENERATE fields, it is the desired number of digits to generate */
	M_sql_data_type_t                    type;            /*!< Column data type */
	M_sql_tabledata_flags_t              flags;           /*!< Flags controlling behavior */
	M_sql_tabledata_filtertransform_cb   filter_cb;       /*!< Callback to filter or transform input data. Called only on new user-specified params. */
	M_sql_tabledata_validate_cb          validate_cb;     /*!< Callback for in-depth validation of input data that may require external SQL queries. */
} M_sql_tabledata_t;


/* -------------------------------------------------------------------------- */


/*! Set the field pointer to a boolean value.
 *  Will override existing value and deallocate any prior memory consumed if necessary
 * \param[in,out] field  Field to set
 * \param[in]     val    Value to set. May be NULL if just error ch
 */
M_API void M_sql_tabledata_field_set_bool(M_sql_tabledata_field_t *field, M_bool val);

/*! Set the field pointer to a 16bit integer
 *  Will override existing value and deallocate any prior memory consumed if necessary
 * \param[in,out] field  Field to set
 * \param[in]     val    Value to set
 */
M_API void M_sql_tabledata_field_set_int16(M_sql_tabledata_field_t *field, M_int16 val);

/*! Set the field pointer to a 32bit integer
 *  Will override existing value and deallocate any prior memory consumed if necessary
 * \param[in,out] field  Field to set
 * \param[in]     val    Value to set
 */
M_API void M_sql_tabledata_field_set_int32(M_sql_tabledata_field_t *field, M_int32 val);

/*! Set the field pointer to a 64bit integer.
 *  Will override existing value and deallocate any prior memory consumed if necessary
 * \param[in,out] field  Field to set
 * \param[in]     val    Value to set
 */
M_API void M_sql_tabledata_field_set_int64(M_sql_tabledata_field_t *field, M_int64 val);

/*! Set the field pointer to a text value and will take ownership of pointer passed (will be freed automatically).
 *  Will override existing value and deallocate any prior memory consumed if necessary
 * \param[in,out] field  Field to set
 * \param[in]     val    Value to set
 */
M_API void M_sql_tabledata_field_set_text_own(M_sql_tabledata_field_t *field, char *val);

/*! Set the field pointer to a text value and will duplicate the pointer passed.
 *  Will override existing value and deallocate any prior memory consumed if necessary
 * \param[in,out] field  Field to set
 * \param[in]     val    Value to set
 */
M_API void M_sql_tabledata_field_set_text_dup(M_sql_tabledata_field_t *field, const char *val);

/*! Set the field pointer to a text value and will treat the pointer as const, it must be valid until field is deallocated.
 *  Will override existing value and deallocate any prior memory consumed if necessary
 * \param[in,out] field  Field to set
 * \param[in]     val    Value to set
 */
M_API void M_sql_tabledata_field_set_text_const(M_sql_tabledata_field_t *field, const char *val);

/*! Set the field pointer to a binary value and will duplicate the pointer passed.
 *  Will override existing value and deallocate any prior memory consumed if necessary
 * \param[in,out] field  Field to set
 * \param[in]     val    Value to set
 * \param[in]     len    Length of value
 */
M_API void M_sql_tabledata_field_set_binary_own(M_sql_tabledata_field_t *field, unsigned char *val, size_t len);

/*! Set the field pointer to a binary value and will duplicate the pointer passed.
 *  Will override existing value and deallocate any prior memory consumed if necessary
 * \param[in,out] field  Field to set
 * \param[in]     val    Value to set
 * \param[in]     len    Length of value
 */
M_API void M_sql_tabledata_field_set_binary_dup(M_sql_tabledata_field_t *field, const unsigned char *val, size_t len);

/*! Set the field pointer to a binary value and will treat the pointer as const, it must be valid until field is deallocated.
 *  Will override existing value and deallocate any prior memory consumed if necessary
 * \param[in,out] field  Field to set
 * \param[in]     val    Value to set
 * \param[in]     len    Length of value
 */
M_API void M_sql_tabledata_field_set_binary_const(M_sql_tabledata_field_t *field, const unsigned char *val, size_t len);

/*! Set the field to NULL.
 *  Will override existing value and deallocate any prior memory consumed if necessary
 * \param[in,out] field  Field to set
 */
M_API void M_sql_tabledata_field_set_null(M_sql_tabledata_field_t *field);

/*! Retrieve field data as a boolean.  If type conversion is necessary, it will be performed such that integer values
 *  are treated as true if non-zero and false if zero.  Text values must have a valid boolean string and evaluate as
 *  appropriate or return failure.  Any other conversion will return failure.
 *
 *  Once a field is fetched successfully as a bool, it is internally converted to a bool.
 *  \param[in,out] field  Field to retrieve data from
 *  \param[out]    val    Boolean output value. May be NULL if just error checking.
 *  \return M_FALSE if conversion was not possible, M_TRUE if successful.
 */
M_API M_bool M_sql_tabledata_field_get_bool(M_sql_tabledata_field_t *field, M_bool *val);

/*! Retrieve field data as a 16bit integer.  If type conversion is necessary, it will be performed such that integer values
 *  are truncated if possible, and boolean values are set to 1 or 0.  Text values will be passed through a string conversion
 *  if numeric. Any other conversion will return failure.
 *
 *  Once a field is fetched successfully as an int32, it is internally converted to an int32.
 *  \param[in,out] field  Field to retrieve data from
 *  \param[out]    val    Int32 output value. May be NULL if just error checking.
 *  \return M_FALSE if conversion was not possible, M_TRUE if successful.
 */
M_API M_bool M_sql_tabledata_field_get_int16(M_sql_tabledata_field_t *field, M_int16 *val);

/*! Retrieve field data as a 32bit integer.  If type conversion is necessary, it will be performed such that integer values
 *  are truncated if possible, and boolean values are set to 1 or 0.  Text values will be passed through a string conversion
 *  if numeric. Any other conversion will return failure.
 *
 *  Once a field is fetched successfully as an int32, it is internally converted to an int32.
 *  \param[in,out] field  Field to retrieve data from
 *  \param[out]    val    Int32 output value. May be NULL if just error checking.
 *  \return M_FALSE if conversion was not possible, M_TRUE if successful.
 */
M_API M_bool M_sql_tabledata_field_get_int32(M_sql_tabledata_field_t *field, M_int32 *val);

/*! Retrieve field data as a 64bit integer.  If type conversion is necessary, it will be performed such that integer values
 *  are expanded, and boolean values are set to 1 or 0.  Text values will be passed through a string conversion
 *  if numeric. Any other conversion will return failure.
 *
 *  Once a field is fetched successfully as an int64, it is internally converted to an int64
 *  \param[in,out] field  Field to retrieve data from
 *  \param[out]    val    Int64 output value. May be NULL if just error checking.
 *  \return M_FALSE if conversion was not possible, M_TRUE if successful.
 */
M_API M_bool M_sql_tabledata_field_get_int64(M_sql_tabledata_field_t *field, M_int64 *val);

/*! Retrieve field data as text.  If type conversion is necessary, it will be performed such that integer values
 *  are converted to base10 strings, and boolean values are converted into "yes" and "no".  ny other conversion will return failure.
 *
 *  Once a field is fetched successfully as text, it is internally converted to text
 *  \param[in,out] field  Field to retrieve data from
 *  \param[out]    val    Pointer to text value that is valid until another conversion occurs or is freed or out of scope.  May return NULL if value is NULL. Input may be NULL if just error checking.
 *  \return M_FALSE if conversion was not possible, M_TRUE if successful.
 */
M_API M_bool M_sql_tabledata_field_get_text(M_sql_tabledata_field_t *field, const char **val);

/*! Retrieve field data as binary.
 *
 * Binary fields are not eligible for conversion.
 *  \param[in,out] field  Field to retrieve data from
 *  \param[out]    val    Pointer to binary value until freed or out of scope.  May return NULL if value is NULL. Input may be NULL if just error checking.
 *  \param[out]    len    Length of value. May be NULL if just error checking, unless val is non-NULL.
 *  \return M_FALSE if conversion was not possible, M_TRUE if successful.
 */
M_API M_bool M_sql_tabledata_field_get_binary(M_sql_tabledata_field_t *field, const unsigned char **val, size_t *len);

/*! Determine if field is NULL or not.
 *
 * \param[in] field Field to determine if value is NULL.
 * \return M_TRUE if NULL, M_FALSE otherwise.
 */
M_API M_bool M_sql_tabledata_field_is_null(const M_sql_tabledata_field_t *field);

/*! Determine current field type.  May change if setter or another getter is called.
 *
 * \param[in] field Field to retrieve type of.
 * \return field type.
 */
M_API M_sql_data_type_t M_sql_tabledata_field_type(const M_sql_tabledata_field_t *field);


/* -------------------------------------------------------------------------- */

/*! Retrieve thunk parameter passed into transaction
 *
 *  \param[in] txn         Transaction provided to callback.
 *  \return thunk parameter or NULL if none
 */
M_API void *M_sql_tabledata_txn_get_thunk(M_sql_tabledata_txn_t *txn);

/*! Retrieve table name parameter passed into transaction
 *
 *  \param[in] txn         Transaction provided to callback.
 *  \return table name, or NULL on error
 */
M_API const char *M_sql_tabledata_txn_get_tablename(M_sql_tabledata_txn_t *txn);

/*! Retrieve generated id during add operation
 *
 *  \param[in] txn         Transaction provided to callback.
 *  \return generated id or 0 on error
 */
M_API M_int64 M_sql_tabledata_txn_get_generated_id(M_sql_tabledata_txn_t *txn);

/*! Retrieve if transaction is add (vs edit)
 *
 *  \param[in] txn         Transaction provided to callback.
 *  \return M_TRUE if is add, M_FALSE if is_edit (or misuse)
 */
M_API M_bool M_sql_tabledata_txn_is_add(M_sql_tabledata_txn_t *txn);


/*! When fetching a field from a transaction, the manner in which to fetch */
typedef enum {
	M_SQL_TABLEDATA_TXN_FIELD_MERGED         = 1, /*!< Grab the current specified value of the field, if not found, grab the prior value */
	M_SQL_TABLEDATA_TXN_FIELD_PRIOR          = 2, /*!< Grab the prior value of the field */
	M_SQL_TABLEDATA_TXN_FIELD_CURRENT        = 3, /*!< Grab the current specified value of the field.  May not exist on edit if value is unchanged. */
	M_SQL_TABLEDATA_TXN_FIELD_CURRENT_OR_NEW = 4, /*!< Grab the current specified value of the field.  If not found, create a new NULL field and return it for modification (field_name specified must be valid) */
} M_sql_tabledata_txn_field_select_t;


/*! Retrieve the field data associated with the field name in the current transaction.
 *
 * \param[in] txn         Transaction provided to callback.
 * \param[in] field_name  Name of field
 * \param[in] fselect     Manner in which to select the field
 * \return M_sql_tabledata_field_t on success, NULL on failure (not found, or invalid field name)
 */
M_API M_sql_tabledata_field_t *M_sql_tabledata_txn_field_get(M_sql_tabledata_txn_t *txn, const char *field_name, M_sql_tabledata_txn_field_select_t fselect);


/*! Retrieve if the field has changed.
 *
 * \param[in] txn        Transaction provided to callback.
 * \param[in] field_name Name of field
 * \return M_TRUE if the field is found and on an add, or has changed on an edit, M_FALSE otherwise
  */
M_API M_bool M_sql_tabledata_txn_field_changed(M_sql_tabledata_txn_t *txn, const char *field_name);


/*! Retrieve the field definition for a field name from the current transaction. 
 *
 * \param[in] txn        Transaction provided to callback.
 * \param[in] field_name Name of field
 * \return pointer to field definition, or NULL if not found
 */
M_API const M_sql_tabledata_t *M_sql_tabledata_txn_fetch_fielddef(M_sql_tabledata_txn_t *txn, const char *field_name);


/* -------------------------------------------------------------------------- */


/*! Callback for fetching a table field.
 *
 *  \param[out] out        Pointer to M_sql_tabledata_field_t to be filled in.  MUST allow NULL as it may be called during a 'test' operation.
 *  \param[in]  field_name Field name being fetched
 *  \param[in]  is_add     M_TRUE if this is called during an add operation, M_FALSE otherwise.
 *  \param[in]  thunk      Thunk parameter for custom state tracking passed to parent function.
 *  \return M_FALSE if field was not found, M_TRUE otherwise */
typedef M_bool (*M_sql_tabledata_fetch_cb)(M_sql_tabledata_field_t *out, const char *field_name, M_bool is_add, void *thunk);

/*! Callback that is called at completion of an add/edit.  Both the prior and new field data are available for the entire
 * table. It may be necessary to do cross-table modifications based on a change, so this facilitates that ability.  If
 * making linked changes, you must use the passed in sqltrans parameter to ensure it is treated as a single atomic operation.
 *
 * \param[in]     sqltrans  SQL Transaction for chaining atomic actions
 * \param[in]     txn       Tabledata transaction pointer to grab field data
 * \param[in,out] error     Buffer to hold error if any
 * \param[in]     error_len Size of error buffer
 * \return one fo the M_sql_error_t codes. Use M_SQL_ERROR_USER_SUCCESS and M_SQL_ERROR_USER_FAIL for non-SQL success/fail.
 */
typedef M_sql_error_t (*M_sql_tabledata_notify_cb)(M_sql_trans_t *sqltrans, M_sql_tabledata_txn_t *txn, char *error, size_t error_len);

/*! Add a row to a table based on the table definition.  If there are key conflicts, it will retry up to 10 times if an auto-generated ID column exists.
 *
 *  Use M_sql_tabledata_trans_add() if inside of a transaction.
 *
 * \param[in]     pool         Required if sqltrans not passed. The handle to the SQL pool in use.
 * \param[in]     table_name   Name of the table
 * \param[in]     fields       List of fields (columns) in the table.
 * \param[in]     num_fields   Number of fields in the list
 * \param[in]     fetch_cb     Callback to be called to fetch each field/column.
 * \param[in]     notify_cb    Optional. Callback to be called to be notified on successful completion.
 * \param[in]     thunk        Thunk parameter for custom state tracking, will be passed to fetch_cb.
 * \param[out]    generated_id If a column had specified M_SQL_TABLEDATA_FLAG_ID_GENERATE, then this will return that id
 * \param[in,out] error        Buffer to hold error if any
 * \param[in]     error_len    Size of error buffer
 * \return one of the M_sql_error_t codes. Will return M_SQL_ERROR_USER_FAILURE on invalid usage of this function
 */
M_API M_sql_error_t M_sql_tabledata_add(M_sql_connpool_t *pool, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, M_sql_tabledata_notify_cb notify_cb, void *thunk, M_int64 *generated_id, char *error, size_t error_len);


/*! Add a row to a table based on the table definition.  If there are key conflicts, it will retry up to 10 times if an auto-generated ID column exists.
 *
 * Use M_sql_tabledata_add() if not already in a transaction.
 *
 * \param[in]     sqltrans     Required if pool not passed.  If run within a transaction, this must be passed.
 * \param[in]     table_name   Name of the table
 * \param[in]     fields       List of fields (columns) in the table.
 * \param[in]     num_fields   Number of fields in the list
 * \param[in]     fetch_cb     Callback to be called to fetch each field/column.
 * \param[in]     notify_cb    Optional. Callback to be called to be notified on successful completion.
 * \param[in]     thunk        Thunk parameter for custom state tracking, will be passed to fetch_cb.
 * \param[out]    generated_id If a column had specified M_SQL_TABLEDATA_FLAG_ID_GENERATE, then this will return that id
 * \param[in,out] error        Buffer to hold error if any
 * \param[in]     error_len    Size of error buffer
 * \return one of the M_sql_error_t codes. Will return M_SQL_ERROR_USER_FAILURE on invalid usage of this function
 */
M_API M_sql_error_t M_sql_tabledata_trans_add(M_sql_trans_t *sqltrans, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb,  M_sql_tabledata_notify_cb notify_cb, void *thunk, M_int64 *generated_id, char *error, size_t error_len);


/*!  Edit an existing row in a table based on the field definitions.  Not all fields need to be available on edit, only
 *   fields that are able to be fetched will be modified.  It is valid to fetch a NULL value to explicitly set a column
 *   to NULL.  The ID(s) specified must match exactly one row or a failure will be emitted.
 *
 *   Use M_sql_tabledata_trans_edit() if already in a transaction.
 *
 * \param[in]     pool         Required if sqltrans not passed. The handle to the SQL pool in use.
 * \param[in]     table_name   Name of the table
 * \param[in]     fields       List of fields (columns) in the table.
 * \param[in]     num_fields   Number of fields in the list
 * \param[in]     fetch_cb     Callback to be called to fetch each field/column.
 * \param[in]     notify_cb    Optional. Callback to be called to be notified on successful completion. (Only if data changed)
 * \param[in]     thunk        Thunk parameter for custom state tracking, will be passed to fetch_cb.
 * \param[in,out] error        Buffer to hold error if any
 * \param[in]     error_len    Size of error buffer
 * \return one of the M_sql_error_t codes. Will return M_SQL_ERROR_USER_FAILURE on invalid usage of this function.
 *         Will return M_SQL_ERROR_USER_SUCCESS when no updates were performed (passed in data matches on file data).
 *         M_SQL_ERROR_SUCCESS means a single row was changed.
 */
M_API M_sql_error_t M_sql_tabledata_edit(M_sql_connpool_t *pool, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, M_sql_tabledata_notify_cb notify_cb, void *thunk, char *error, size_t error_len);

/*!  Edit an existing row in a table based on the field definitions.  Not all fields need to be available on edit, only
 *   fields that are able to be fetched will be modified.  It is valid to fetch a NULL value to explicitly set a column
 *   to NULL.  The ID(s) specified must match exactly one row or a failure will be emitted.
 *
 *   Use M_sql_tabledata_edit() if not already in a transaction.
 *
 * \param[in]     sqltrans     Required if pool not passed.  If run within a transaction, this must be passed.
 * \param[in]     table_name   Name of the table
 * \param[in]     fields       List of fields (columns) in the table.
 * \param[in]     num_fields   Number of fields in the list
 * \param[in]     fetch_cb     Callback to be called to fetch each field/column.
 * \param[in]     notify_cb    Optional. Callback to be called to be notified on successful completion. (Only if data changed)
 * \param[in]     thunk        Thunk parameter for custom state tracking, will be passed to fetch_cb.
 * \param[in,out] error        Buffer to hold error if any
 * \param[in]     error_len    Size of error buffer
 * \return one of the M_sql_error_t codes. Will return M_SQL_ERROR_USER_FAILURE on invalid usage of this function.
 *         Will return M_SQL_ERROR_USER_SUCCESS when no updates were performed (passed in data matches on file data).
 *         M_SQL_ERROR_SUCCESS means a single row was changed.
 */
M_API M_sql_error_t M_sql_tabledata_trans_edit(M_sql_trans_t *sqltrans, const char *table_name, const M_sql_tabledata_t *fields, size_t num_fields, M_sql_tabledata_fetch_cb fetch_cb, M_sql_tabledata_notify_cb notify_cb, void *thunk, char *error, size_t error_len);

/*! Convenience function to expand a list of tabledata fields base on an M_list_str_t list of
 *  virtual column names tied to a single table column that share the same attributes.  All
 *  virtual columns are always stored as text.
 *
 *  IMPORTANT: The passed in "table_column" and "field_names" MUST persist until the tabledata
 *             structure is no longer needed as they are used as const values internally.
 *
 * \param[in]     fields       Field list to expand.  Original input is NOT modified.
 * \param[in,out] num_fields   On input, the size of the fields table passed in. On return, the new size.
 * \param[in]     table_column Name of real table column virtual columns are tied to.  NOTE: Pointer must
 *                             persist until returned table is no longer needed.
 * \param[in]     field_names  List of virtual column names used to expand table.  NOTE: Pointer must
 *                             persist until returned table is no longer needed.
 * \param[in]     max_len      Maximum field value length for each field name.
 * \param[in]     flags        Shared field flags.  M_SQL_TABLEDATA_FLAG_VIRTUAL is automatically added if not specified.
 * \return Allocated tabledata structure.  Must be M_free()'d when no longer needed.
 */
M_API M_sql_tabledata_t *M_sql_tabledata_append_virtual_list(const M_sql_tabledata_t *fields, size_t *num_fields, const char *table_column, const M_list_str_t *field_names, size_t max_len, M_sql_tabledata_flags_t flags);


/*! Convenience function to try to auto-generate the table columns for table creation based on the same tabledata used
 *  to add/edit.
 *
 *  NOTE: This does NOT create the primary key or index, it is expected to be handled externally.
 *
 * \param[in,out] table      Initialized table to be populated with column data
 * \param[in]     fields     Field list to generate columns from
 * \param[in]     num_fields Number of fields in the field list
 *
 * \return M_TRUE on success, M_FALSE on error
 */
M_API M_bool M_sql_tabledata_to_table(M_sql_table_t *table, const M_sql_tabledata_t *fields, size_t num_fields);

/*! @} */


__END_DECLS

#endif /* __M_SQL_TABLE_H__ */
