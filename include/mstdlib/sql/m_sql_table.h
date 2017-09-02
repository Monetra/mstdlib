/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
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

__END_DECLS

#endif /* __M_SQL_TABLE_H__ */
