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

#ifndef __M_XML_H__
#define __M_XML_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_fs.h>
#include <mstdlib/base/m_hash_dict.h>
#include <mstdlib/base/m_list_str.h>
#include <mstdlib/base/m_buf.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_xml XML
 *  \ingroup m_formats
 *
 * DOM based XML data processing.
 *
 * This is a simple API for reading, creating, manipulating, searching, and writing
 * XML data.
 *
 * It is possible to construct an invalid XML document. It is also possible that
 * an invalid XML document created with M_xml will not be readable by M_xml.
 * Specifically, when dealing with attribute and text encoding options for M_xml_read
 * and M_xml_write. Also, attribute keys and tag names are not validated to ensure
 * they are valid XML names. This must happen outside of M_xml.
 *
 * M_xml is not susceptible to common XML entity expansion attaches such as
 * billion laughs, quadratic blowup, and external entity expansion. Only basic
 * XML entities are expanded and only one level is expanded. Further external
 * entity references are not downloaded. Neither are external DTDs. This will
 * mitigate against network based attacks relying on retrieval.
 *
 * Example:
 *
 * \code{.c}
 *     M_xml_node_t *xml;
 *     const char   *data = "<r><tag1>abc</tag1><tag2>123</tag2><tag3>xyz</tag3></r>";
 *     char         *out;
 *
 *     xml = M_xml_read(data, M_str_len(data), M_XML_READER_NONE, NULL, NULL, NULL);
 *     if (xml == NULL) {
 *         M_printf("xml parse error\n");
 *         return M_FALSE;
 *     }
 *
 *     M_xml_create_element_with_text("AbCd", "blah blah blah", 0, xml);
 *
 *     out = M_xml_write(xml, M_XML_WRITER_LOWER_TAGS|M_XML_WRITER_LOWER_ATTRS|M_XML_WRITER_PRETTYPRINT_SPACE, NULL);
 *     M_printf("out=\n%s\n", out);
 *     M_free(out);
 * \endcode
 *
 * @{
 */

 struct M_xml_node;
 typedef struct M_xml_node M_xml_node_t;


/*! Node types in our XML tree structure */
typedef enum {
	M_XML_NODE_TYPE_UNKNOWN                = 0, /*!< An invalid node type. */
	M_XML_NODE_TYPE_DOC                    = 1, /*!< The base of every XML tree and provides access to the
	                                                 documents data.
	                                                 Supports: Nodes.
	                                                 Does not support: Name, Attributes, Text, Tag data. */
	M_XML_NODE_TYPE_ELEMENT                = 2, /*!< Element (tag).
	                                                 E.g: \code{xml}<name key="val" />, <name>...</name>\endcode.
	                                                 Supports: Attributes, Nodes, Name.
	                                                 Does not support: Text, Tag data. */
	M_XML_NODE_TYPE_TEXT                   = 3, /*!< Text.
	                                                 E.g: abc.
	                                                 Supports: Text.
	                                                 Does not support: Nodes, Name, Attributes, Tag data. */
	M_XML_NODE_TYPE_PROCESSING_INSTRUCTION = 4, /*!< Conveys information.
	                                                 E.g: \code{xml}<?xml version="1.0" encoding="UTF-8" ?>\endcode
	                                                 Supports: Name, Attributes, Tag data.
	                                                 Does not support: Nodes, Text. */
	M_XML_NODE_TYPE_DECLARATION            = 5, /*!< HTML/DTD declaration.
	                                                 E.g: \code{xml}<!DOCTYPE html>, <!ELEMENT element-name category>, <!ATTLIST payment type CDATA "check">\endcode
	                                                 Supports: Name, Tag data.
	                                                 Does not support: Nodes, Attributes, text. */
	M_XML_NODE_TYPE_COMMENT                = 6  /*!< Comment.
	                                                 E.g: \code{xml}<!-- comment -->\endcode
	                                                 Supports: Tag data.
	                                                 Does not support: Nodes, Name, Attributes, Text. */
} M_xml_node_type_t;


/*! Flags to control the behavior of the XML writer. */
typedef enum {
	M_XML_READER_NONE              = 0,      /*!< Normal operation. */
	M_XML_READER_IGNORE_COMMENTS   = 1 << 0, /*!< Don't include comments as part of the output during parse. */
	M_XML_READER_TAG_CASECMP       = 1 << 1, /*!< Compare open and close tags case insensitive. */
	M_XML_READER_DONT_DECODE_TEXT  = 1 << 2, /*!< By default text data will be xml entity decoded.
	                                              This prevents the decode from taking place. It should be paired
	                                              with the equivalent don't encode option when writing. */
	M_XML_READER_DONT_DECODE_ATTRS = 1 << 3  /*!< By default attribute values will be attribute entity decoded.
	                                              This prevents the decode from taking place. It should be paired
	                                              with the equivalent don't encode option when writing. */
} M_xml_reader_flags_t;


/*! Flags to control the behavior of the XML reader. */
typedef enum {
	M_XML_WRITER_NONE              = 0,      /*!< No indent. All data on a single line. */
	M_XML_WRITER_IGNORE_COMMENTS   = 1 << 0, /*!< Comments are not included. */
	M_XML_WRITER_LOWER_TAGS        = 1 << 1, /*!< All tags are written lower case. */
	M_XML_WRITER_LOWER_ATTRS       = 1 << 2, /*!< All attribute keys are written lower case. */
	M_XML_WRITER_DONT_ENCODE_TEXT  = 1 << 3, /*!< By default text data will be xml entity encoded.
	                                              This prevents the encode from taking place. It should be paired
	                                              with the equivalent don't decode option when reading. */
	M_XML_WRITER_DONT_ENCODE_ATTRS = 1 << 4, /*!< By default attribute values will be attribute entity encoded.
	                                              This prevents the encode from taking place. It should be paired
	                                              with the equivalent don't decode option when reading. */
	M_XML_WRITER_PRETTYPRINT_SPACE = 1 << 5, /*!< 2 space indent. */
	M_XML_WRITER_PRETTYPRINT_TAB   = 1 << 6, /*!< Tab indent. */
	M_XML_WRITER_SELFCLOSE_SPACE   = 1 << 7  /*!< Add a space before the closing slash for self closing tags. */
} M_xml_writer_flags_t;


/*! Error codes. */
typedef enum {
	M_XML_ERROR_SUCCESS = 0,                        /*!< success */
	M_XML_ERROR_GENERIC,                            /*!< generic error */
	M_XML_ERROR_MISUSE,                             /*!< API misuse */
	M_XML_ERROR_ATTR_EXISTS,                        /*!< the attribute already exists on the node */
	M_XML_ERROR_NO_ELEMENTS,                        /*!< unexpected end of XML, no elements in data */
	M_XML_ERROR_INVALID_START_TAG,                  /*!< invalid tag start character */
	M_XML_ERROR_INVALID_CHAR_IN_START_TAG,          /*!< invalid character '<' found in tag */
	M_XML_ERROR_EMPTY_START_TAG,                    /*!< only whitespace after tag start */
	M_XML_ERROR_MISSING_DECLARATION_NAME,           /*!< missing name after ! */
	M_XML_ERROR_INELIGIBLE_FOR_CLOSE,               /*!< cannot close element of this type */
	M_XML_ERROR_UNEXPECTED_CLOSE,                   /*!< cannot close element with the given tag */
	M_XML_ERROR_MISSING_CLOSE_TAG,                  /*!< missing closing element statement(s) */
	M_XML_ERROR_MISSING_PROCESSING_INSTRUCTION_END, /*!< missing processing instruction close */
	M_XML_ERROR_EXPECTED_END                        /*!< expected end but more data found */
} M_xml_error_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create an XML document.
 *
 * \return An XML node on success. NULL on failure.
 *
 * \see M_xml_node_destroy
 */
M_API M_xml_node_t *M_xml_create_doc(void) M_MALLOC;


/*! Create an XML element node.
 *
 * \param[in]     name   The tag name for the element.
 * \param[in,out] parent The parent this node should be inserted into. Optional, pass NULL
 *                       if the node should be created without a parent.
 *
 * \return An XML node on success. NULL on failure.
 *
 * \see M_xml_node_destroy
 */
M_API M_xml_node_t *M_xml_create_element(const char *name, M_xml_node_t *parent) M_MALLOC;


/*! Create an XML element with text node.
 *
 * \param[in]     name    The tag name for the element.
 * \param[in]     text    The text for the element.
 * \param[in]     max_len The maximum length the text is allowed to be when xml encoded. If the encoded length
 *                        is greater an error will result. Use 0 to specify that the text length should not be
 *                        checked and any length is allowed.
 * \param[in,out] parent  The parent this node should be inserted into. Optional, pass NULL
 *                        if the node should be created without a parent.
 *
 * \return An XML node on success. NULL on failure.
 *
 * \see M_xml_node_destroy
 */
M_API M_xml_node_t *M_xml_create_element_with_text(const char *name, const char *text, size_t max_len, M_xml_node_t *parent) M_MALLOC;


/*! Create an XML text node.
 *
 * \param[in]     text    The text.
 * \param[in]     max_len The maximum length the text is allowed to be when xml encoded. If the encoded length
 *                        is greater an error will result. Use 0 to specify that the text length should not be
 *                        checked and any length is allowed.
 * \param[in,out] parent  The parent this node should be inserted into. Optional, pass NULL
 *                        if the node should be created without a parent.
 *
 * \return An XML node on success. NULL on failure.
 *
 * \see M_xml_node_destroy
 */
M_API M_xml_node_t *M_xml_create_text(const char *text, size_t max_len, M_xml_node_t *parent) M_MALLOC;


/*! Create an XML declaration node.
 *
 * E.g:
 *   - <?xml version="1.0" encoding="UTF-8" ?>
 *
 * \param[in]     encoding The encoding.
 * \param[in,out] parent   The parent this node should be inserted into. Optional, pass NULL
 *                         if the node should be created without a parent.
 *
 * \return An XML node on success. NULL on failure.
 *
 * \see M_xml_node_destroy
 */
M_API M_xml_node_t *M_xml_create_xml_declaration(const char *encoding, M_xml_node_t *parent) M_MALLOC;


/*! Create a declaration node.
 *
 * E.g:
 *   - \code{xml}<!DOCTYPE html>\endcode
 *   - \code{xml}<!ELEMENT element-name category>\endcode
 *   - \code{xml}<!ATTLIST payment type CDATA "check">\endcode
 *
 * \param[in]     name   The tag name for the declaration.
 * \param[in,out] parent The parent this node should be inserted into. Optional, pass NULL
 *                       if the node should be created without a parent.
 *
 * \return An XML node on success. NULL on failure.
 *
 * \see M_xml_node_destroy
 */
M_API M_xml_node_t *M_xml_create_declaration(const char *name, M_xml_node_t *parent) M_MALLOC;


/*! Create a declaration node.
 *
 * E.g:
 *   - \code{xml}<!DOCTYPE html>\endcode
 *   - \code{xml}<!ELEMENT element-name category>\endcode
 *   - \code{xml}<!ATTLIST payment type CDATA "check">\endcode
 *
 * \param[in]     name   The tag name for the declaration.
 * \param[in]     data   The tag data.
 * \param[in,out] parent The parent this node should be inserted into. Optional, pass NULL
 *                       if the node should be created without a parent.
 *
 * \return An XML node on success. NULL on failure.
 *
 * \see M_xml_node_destroy
 */
M_API M_xml_node_t *M_xml_create_declaration_with_tag_data(const char *name, const char *data, M_xml_node_t *parent) M_MALLOC;


/*! Create an XML processing instruction node.
 *
 * \param[in]     name   The instruction name for the node.
 * \param[in,out] parent The parent this node should be inserted into. Optional, pass NULL
 *                       if the node should be created without a parent.
 *
 * \return An XML node on success. NULL on failure.
 *
 * \see M_xml_node_destroy
 */
M_API M_xml_node_t *M_xml_create_processing_instruction(const char *name, M_xml_node_t *parent) M_MALLOC;


/*! Create an XML comment node.
 *
 * \param[in]     comment The comment.
 * \param[in,out] parent  The parent this node should be inserted into. Optional, pass NULL
 *                        if the node should be created without a parent.
 *
 * \return An XML node on success. NULL on failure.
 *
 * \see M_xml_node_destroy
 */
M_API M_xml_node_t *M_xml_create_comment(const char *comment, M_xml_node_t *parent) M_MALLOC;


/*! Destroy an XML node.
 *
 * Destroying a node will destroy every node under it and remove it from it's parent node if it is a child.
 *
 * \param[in] node The node to destroy.
 */
M_API void M_xml_node_destroy(M_xml_node_t *node) M_FREE(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Parse a string into an XML object.
 *
 * \param[in]  data          The data to parse.
 * \param[in]  data_len      The length of the data to parse.
 * \param[in]  flags         M_xml_reader_flags_t flags to control the behavior of the reader.
 * \param[out] processed_len Length of data processed. Useful if you could have multiple XML documents
 *                           in a stream. Optional pass NULL if not needed.
 * \param[out] error         Error code if creation failed. Optional, Pass NULL if not needed.
 * \param[out] error_line    The line the error occurred. Optional, pass NULL if not needed.
 * \param[out] error_pos     The column the error occurred if error_line is not NULL, otherwise the position
 *                           in the stream the error occurred. Optional, pass NULL if not needed.
 * \return The XML doc node of the parsed data, or NULL on error.
 */
M_API M_xml_node_t *M_xml_read(const char *data, size_t data_len, M_uint32 flags, size_t *processed_len, M_xml_error_t *error, size_t *error_line, size_t *error_pos) M_MALLOC;


/*! Parse a file into an XML object.
 *
 * \param[in]  path       The file to read.
 * \param[in]  flags      M_xml_reader_flags_t flags to control the behavior of the reader.
 * \param[in]  max_read   The maximum amount of data to read from the file. If the data in the file is
 *                        larger than max_read an error will most likely result. Optional pass 0 to read all data.
 * \param[out] error      Error code if creation failed. Optional, Pass NULL if not needed.
 * \param[out] error_line The line the error occurred. Optional, pass NULL if not needed.
 * \param[out] error_pos  The column the error occurred if error_line is not NULL, otherwise the position
 *                        in the stream the error occurred. Optional, pass NULL if not needed.
 *
 * \return The XML doc node of the parsed data, or NULL on error.
 */
M_API M_xml_node_t *M_xml_read_file(const char *path, M_uint32 flags, size_t max_read, M_xml_error_t *error, size_t *error_line, size_t *error_pos) M_MALLOC;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Write XML to a string.
 *
 * This writes nodes to a string. The string may not be directly usable by M_xml_read.
 * E.g. If you are only writing a string node.
 *
 * \param[in]  node  The node to write. This will write the node and any nodes under it. 
 * \param[in]  flags M_xml_writer_flags_t flags to control writing.
 * \param[out] len   The length of the string that was returned. Optional, pass NULL if not needed.
 *
 * \return A string with data or NULL on error.
 */
M_API char *M_xml_write(const M_xml_node_t *node, M_uint32 flags, size_t *len) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Write XML to a buffer.
 *
 * This writes nodes out to a buffer. The resulting string may not be directly usable by M_xml_read -
 * for example, you can write out a string node or other component of an XML document, without writing
 * the whole thing.
 *
 * \param[out] buf   buffer to write XML to
 * \param[in]  node  the node to write (children of this node are also included in output)
 * \param[in]  flags OR'd combo of M_xml_write_flags_t values (to control writing)
 * \return           M_TRUE if successful, M_FALSE otherwise.
 */
M_API M_bool M_xml_write_buf(M_buf_t *buf, const M_xml_node_t *node, M_uint32 flags);


/*! Write XML to a file.
 *
 * This writes nodes to a string. The string may not be directly usable by M_xml_read_file.
 * E.g. If you are only writing a string node.
 *
 * \param[in] node  The node to write. This will write the node and any nodes under it. 
 * \param[in] path  The filename and path to write the data to.
 * \param[in] flags M_xml_writer_flags_t flags to control writing.
 *
 * \return Result.
 */
M_API M_fs_error_t M_xml_write_file(const M_xml_node_t *node, const char *path, M_uint32 flags);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Convert an XML error code to a string.
 *
 * \param[in] err error code
 * \return        name of error code (not a description, just the enum name, like M_XML_ERROR_SUCCESS)
 */
M_API const char *M_xml_errcode_to_str(M_xml_error_t err);

/*! Get the type of node.
 *
 * \param[in] node The node.
 *
 * \return The type.
 */
M_API M_xml_node_type_t M_xml_node_type(const M_xml_node_t *node);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Using XPath expressions, scan for matches.
 *
 * Note that full XPath support does not yet exist. Also only element nodes are currently returned unless text() is
 * used to return text nodes.
 *
 * This will return an array of nodes within the tree. The nodes in the return are not copies, they are
 * references. Changing a node in the xpath return will modify the node in the tree.
 *
 * Supports:
 * Syntax             | Meaning
 * -------------------|---------
 * tag                | Selects children of the current element node with the given tag name.
 * *:tag              | Selects children of the current element node with the given tag name ignoring any namespace on the tag. Also matches tags without a namespace.
 * *                  | Selects all children of the current element node.
 * /                  | Selects children one level below the current element node.
 * //                 | Selects children on all levels bellow the current element node.
 * .                  | Selects the current element node.
 * ..                 | Selects the parent of the current element node.
 * text()             | Selects all text nodes.
 * [\@attrib]         | Selects elements which have an attribute attrib.
 * [\@attrib=val]     | Selects elements which have an sttribute attrib with a value of val.
 * [\@attrib="val"]   | Selects elements which have an sttribute attrib with a value of val.
 * [\@attrib='val']   | Selects elements which have an sttribute attrib with a value of val.
 * [\@*]              | Selects elements which have an (any) attribute set.
 * [idx]              | Select an element at a given position.
 * [position() ? idx] | Select an element at a given position.
 *
 * More information about [idx]:
 *
 * Index matching can either be an integer (starting from 1) or the expression "last()". "last()" can
 * be followed by a negative integer in order to count backwards from the last elements. For example: "last()-1"
 * is the second to last element. The idx is not the index within the parent the node but the index of the
 * matching nodes. idx must be preceded by either a tag or text().
 *
 * More information about [position() ? idx]:
 *
 * Position matching can use the equality modifiers '=', '<=', '>=', '<', '>'. These will select one or
 * more nodes that match the given criteria. "last()" can be used as the index.
 *
 * E.g: "a/b[2]" for `<a><b/><c/><b/></a>` will select the second "b" after "c".
 *
 * \param[in]  node        The node.
 * \param[in]  search      search expression
 * \param[in]  flags       M_xml_reader_flags_t flags to control the behavior of the search.
 *                         valid flags are:
 *                         - M_XML_READER_NONE
 *                         - M_XML_READER_TAG_CASECMP
 * \param[out] num_matches Number of matches found
 *
 * \return array of M_xml_node_t pointers on success (must free array, but not internal pointers), NULL on failure
 */ 
M_API M_xml_node_t **M_xml_xpath(M_xml_node_t *node, const char *search, M_uint32 flags, size_t *num_matches) M_MALLOC;


/*! Using XPath expressions, scan for matches and return the first text value.
 *
 * This will only return the first text node. Meaning if multiple text nodes are inserted in a row only the
 * text from the first will be returned. If the XML tree was created using M_xml_read or M_xml_read_file then
 * the fist text node will contain all of the text.
 *
 * \see M_xml_xpath for information about supported XPath features.
 *
 * \param[in]  node        The node.
 * \param[in]  search      search expression
 *
 * \return Text on success otherwise  NULL.
 */ 
M_API const char *M_xml_xpath_text_first(M_xml_node_t *node, const char *search);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the parent node of a given node.
 *
 * \param[in] node The node.
 *
 * \return The parent node or NULL if there is no parent.
 */
M_API M_xml_node_t *M_xml_node_parent(const M_xml_node_t *node);


/*! Take a node from its parent but does not destroy the node.
 *
 * This allows a node to be moved between different parents.
 *
 * \param[in,out] node The node.
 */
M_API void M_xml_take_from_parent(M_xml_node_t *node);


/*! Insert a node into a doc or element node.
 *
 * The parent node will take ownership of the child node.
 *
 * Only parentless nodes can be insert into other nodes. You must use M_xml_take_from_parent first if you
 * are moving nodes from one parent to another.
 *
 * \param[in,out] parent The parent node.
 * \param[in]     child  The child node.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_xml_node_insert_node(M_xml_node_t *parent, M_xml_node_t *child);


/*! Insert a node into a doc or element node at a given position.
 *
 * The parent node will take ownership of the child node.
 *
 * \param[in,out] parent The parent node.
 * \param[in]     child  The child node.
 * \param[in]     idx    The location where the node should be inserted.
 *
 * \return M_TRUE on success otherwise M_FALSE.
 */
M_API M_bool M_xml_node_insert_node_at(M_xml_node_t *parent, M_xml_node_t *child, size_t idx);


/*! Get the number of child nodes for a doc or element node.
 *
 * \param[in] node The node.
 *
 * \return The number of children.
 *
 * \see M_xml_node_child
 */
M_API size_t M_xml_node_num_children(const M_xml_node_t *node);


/*! Get the child node at the given position for a doc or element node.
 *
 * \param[in] node The node.
 * \param[in] idx  The position for the child to retrieve.
 *
 * \return The child node or NULL if a child does not exist at the given position.
 *
 * \see M_xml_node_num_children
 */
M_API M_xml_node_t *M_xml_node_child(const M_xml_node_t *node, size_t idx);


/*! Get the sibling for the node.
 *
 * The sibling is the node at the same level as the current node which is either
 * before or after the node.
 *
 * \param[in] node  The node.
 * \param[in] after M_TRUE to return the node after. M_FALSE to return the one before.
 *
 * \return The sibling node. NULL if there is no sibling in the given direction.
 */
M_API M_xml_node_t *M_xml_node_sibling(const M_xml_node_t *node, M_bool after);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Set the tag name for an element or processing instruction node.
 *
 * The name is the tag name. E.g. "<name>".
 * This will change the name of the node.
 *
 * \param[in,out] node   The node.
 * \param[in]     name   The name.
 *
 * \return M_TRUE on sucess otherwise M_FALSE.
 */
M_API M_bool M_xml_node_set_name(M_xml_node_t *node, const char *name);


/*! Get the tag name for node an element or processing instruction node.
 *
 * \param[in] node The node.
 *
 * \return The name.
 */
M_API const char *M_xml_node_name(const M_xml_node_t *node);


/*! Set the text for a text node.
 *
 * \param[in,out] node    The node.
 * \param[in]     text    The text.
 * \param[in]     max_len The maximum length the text is allowed to be when xml encoded. If the encoded length
 *                        is greater an error will result. Use 0 to specify that the text length should not be
 *                        checked and any length is allowed.
 *
 * \return M_TRUE on success, otherwise M_FALSE.
 */
M_API M_bool M_xml_node_set_text(M_xml_node_t *node, const char *text, size_t max_len);


/*! Get the text for a text node.
 *
 * \param[in] node The node
 *
 * \return The node's text.
 */
M_API const char *M_xml_node_text(const M_xml_node_t *node);


/*! Set the tag data for a node.
 *
 * \param[in,out] node  The node.
 * \param[in]     data  The tag data.
 *
 * \return M_TRUE on success, otherwise M_FALSE.
 */
M_API M_bool M_xml_node_set_tag_data(M_xml_node_t *node, const char *data);


/*! Get the tag data for a node.
 *
 * \param[in] node The node
 *
 * \return The node's tag data.
 */
M_API const char *M_xml_node_tag_data(const M_xml_node_t *node);


/*! Insert an attribute into an element or processing instruction node.
 *
 * \param[in,out] node      The node.
 * \param[in]     key       The attribute key.
 * \param[in]     val       The attribute value.
 * \param[in]     max_len   The maximum length the text is allowed to be when xml encoded. If the encoded length
 *                          is greater an error will result. Use 0 to specify that the text length should not be
 *                          checked and any length is allowed.
 * \param[in]     overwrite Action to take when the given key exists. If M_TRUE the value will be overwritten when
 *                          with the given value. If M_FALSE the attribute will not be written. This will be treated
 *                          as an error condition.
 *
 * \return M_TRUE on success, otherwise M_FALSE.
 */
M_API M_bool M_xml_node_insert_attribute(M_xml_node_t *node, const char *key, const char *val, size_t max_len, M_bool overwrite);


/*! Remove an attribute from an element or processing instruction node.
 *
 * \param[in,out] node      The node.
 * \param[in]     key       The attribute key.
 *
 * \return M_TRUE on success, otherwise M_FALSE.
 */
M_API M_bool M_xml_node_remove_attribute(M_xml_node_t *node, const char *key);


/*! Get a list of all attribute keys set for an element or processing instruction node.
 *
 * \param[in] node The node.
 *
 * \return A list of keys.
 */
M_API M_list_str_t *M_xml_node_attribute_keys(const M_xml_node_t *node) M_MALLOC;


/*! Get a dictionary of all attribute set for an element or processing instruction node.
 *
 * \param[in] node The node.
 *
 * \return A dictionary with attributes.
 */
M_API const M_hash_dict_t *M_xml_node_attributes(const M_xml_node_t *node);


/*! Get the value of a given attribute for an element or processing instruction node.
 *
 * \param[in] node The node.
 * \param[in] key  The attribute key.
 *
 * \return The value of the attribute. NULL if the key does not exist.
 */
M_API const char *M_xml_node_attribute(const M_xml_node_t *node, const char *key);

/*@}*/

__END_DECLS

#endif /* __M_XML_H__ */
