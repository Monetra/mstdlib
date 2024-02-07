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

#ifndef __M_MTZFILE_H__
#define __M_MTZFILE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_time.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_mtzfile Mstdlib TZ File
 *  \ingroup m_formats
 *
 * The Mstdlib TZ format is a simple way to describe timezone information. It is intended
 * for use on systems that do not provide timezone information. Such as embedded systems.
 * A time.zone file is provided in the formats/time directory in the source package
 * which provides timezone descriptions for US, Canada, and Mexico.
 *
 * The format is an ini file which can be parsed with \link m_ini \endlink. However, functions
 * are provided here which will parse and load the format into a M_time_tzs_t object.
 *
 * Format description:
 *
 * Section define zones. Aliases are alternate names that identify the zone.
 * The section must have an offset and abbreviation. The offset is assumed negative unless
 * the value start explicitly with '+'. DST offsets and abbreviations are used when a DST
 * rule is in effect. A DST rule is a Posix DST rule with the first part replaced with a year
 * (the year the rule applies) followed by a ';'. For DST rules only the M day format is supported.
 *
 * Example:
 *
 * \code{.ini}
 *     [EST5]
 *     offset=5
 *     abbr=EST
 *
 *     [EST5EDT]
 *     alias=America/New_York
 *     alias=America/Detroit
 *     alias=America/Indiana/Indianapolis
 *     offset=5
 *     offset_dst=4
 *     abbr=EST
 *     abbr_dst=EDT
 *     dst=2007;M3.2.0/02:00:00,M11.1.0/02:00:00
 *     dst=1987;M4.1.0/02:00:00,M10.-1.0/02:00:00
 * \endcode
 *
 * @{
 */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Add Mstdlib TZ from a file.
 *
 *
 * \param[in,out] tzs      The tz db.
 * \param[in]     path     The file and path to load.
 * \param[out]    err_line The line number if failure is due to ini parse failure.
 * \param[out]    err_sect The section that failed to parse if a parse failure not due to an ini parse failure.
 * \param[out]    err_data The data that caused the failure.
 *
 * \return Success on success. Otherwise error condition.
 */
M_API M_time_result_t M_mtzfile_tzs_add_file(M_time_tzs_t *tzs, const char *path, size_t *err_line, char **err_sect, char **err_data);


/*! Add Mstdlib TZ from a string.
 *
 * \param[in,out] tzs      The tz db.
 * \param[in]     data     The data to load.
 * \param[out]    err_line The line number if failure is due to ini parse failure.
 * \param[out]    err_sect The section that failed to parse if a parse failure not due to an ini parse failure.
 * \param[out]    err_data The data that caused the failure.
 *
 * \return Success on success. Otherwise error condition.
 *
 * \see M_mtzfile_tzs_add_file for details of the format.
 */
M_API M_time_result_t M_mtzfile_tzs_add_str(M_time_tzs_t *tzs, const char *data, size_t *err_line, char **err_sect, char **err_data);

/*! @} */

__END_DECLS

#endif /* __M_TZFILE_H__ */
