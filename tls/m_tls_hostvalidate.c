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

#include "m_config.h"
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_layer.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_tls.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include "base/m_defs_int.h"
#include "m_tls_hostvalidate.h"

/* Validate the certificate matches the hostname presented. 
 * Could be multiple common names as per:
 *    https://www.openssl.org/docs/manmaster/crypto/X509_NAME_get_entry.html
 * Some example name validation logic that doesn't realize this here:
 *    https://wiki.openssl.org/index.php/Hostname_validation
 * Needs to handle wildcard certificates, keeping in mind that a wildcard only
 * matches the one section, e.g.   *.google.com does not match
 * foo.bar.google.com
 */
#if OPENSSL_VERSION_NUMBER < 0x1010000fL || defined(LIBRESSL_VERSION_NUMBER)
#define ASN1_STRING_get0_data(x) ASN1_STRING_data(x)
#endif

static M_bool M_tls_verify_host_extract_base_domain(const char *hostname, char *basedomain, size_t basedomain_len)
{
	char       **strs;
	size_t       num  = 0;
	size_t       domain_sects;
	char        *out;

	/* Split at the periods */
	strs = M_str_explode_str('.', hostname, &num);

	/* If the last section is only 2 characters, then it is a 
	 * country domain and thus we need to include another domain
	 * section */
	if (strs && num && M_str_len(strs[num-1]) == 2) {
		domain_sects = 3; 
	} else {
		domain_sects = 2;
	}

	/* Either we need the whole thing, or there aren't
	 * enough desired sections, so just return the original
	 * input */
	if (domain_sects >= num) {
		M_str_explode_free(strs, num);
		M_str_cpy(basedomain, basedomain_len, hostname);
		return M_TRUE;
	}

	/* Implode the dots */
	out = M_str_implode('.', 0, 0, &(strs[num-domain_sects]), domain_sects, M_FALSE);
	M_str_explode_free(strs, num);

	if (out == NULL || *out == '\0') {
		M_str_cpy(basedomain, basedomain_len, hostname);
		return M_FALSE;
	}

	M_str_cpy(basedomain, basedomain_len, out);
	M_free(out);
	return M_TRUE;
}


static M_bool M_tls_verify_host_match_wildcard(const char *hostname, const char *name, M_bool allow_multilevel)
{
	char **hostname_parts     = NULL;
	size_t hostname_parts_num = 0;
	char **name_parts         = NULL;
	size_t name_parts_num     = 0;
	M_bool rv                 = M_FALSE;
	size_t i;

	if (M_str_isempty(hostname) || M_str_isempty(name))
		return M_FALSE;

	/* Not a wildcard if we don't start with a wildcard */
	if (!M_str_eq_max(name, "*.", 2))
		return M_FALSE;

	hostname_parts = M_str_explode_str('.', hostname, &hostname_parts_num);
	name_parts     = M_str_explode_str('.', name,     &name_parts_num);

	if (hostname_parts == NULL || name_parts == NULL || hostname_parts_num == 0 || name_parts_num == 0)
		goto fail;

	/* Must have equal parts if not allowing multilevel */
	if (!allow_multilevel && hostname_parts_num != name_parts_num)
		goto fail;

	/* Even if allowing multilevel, must have at least as many hostname parts */
	if (hostname_parts_num < name_parts_num)
		goto fail;

	/* Iterate in reverse over the parts to match against the hostname parts.  We aren't
	 * going to iterate over the first entry as we already know it is '*', so if we get
	 * to the end, we've matched all we need to match */
	for (i=name_parts_num-1; i>0; i--) {
		if (!M_str_caseeq(name_parts[i], hostname_parts[i]))
			goto fail;
	}

	/* If we got this far, everything matched */
	rv = M_TRUE;

fail:
	M_str_explode_free(hostname_parts, hostname_parts_num);
	M_str_explode_free(name_parts, name_parts_num);
	return rv;
}


static M_bool M_tls_verify_host_match(const char *hostname, const char *name, M_tls_verify_host_flags_t flags)
{
	if (M_str_isempty(hostname) || M_str_isempty(name))
		return M_FALSE;

	if (flags == M_TLS_VERIFY_HOST_FLAG_NONE)
		return M_TRUE;

	if (M_str_caseeq(hostname, name))
		return M_TRUE;

	if (flags & M_TLS_VERIFY_HOST_FLAG_ALLOW_WILDCARD) {
		if (M_tls_verify_host_match_wildcard(hostname, name, (flags & M_TLS_VERIFY_HOST_FLAG_MULTILEVEL_WILDCARD)?M_TRUE:M_FALSE))
			return M_TRUE;
	}

	if (flags & M_TLS_VERIFY_HOST_FLAG_FUZZY_BASE_DOMAIN) {
		char host_base[256];
		char name_base[256];
		/* When fuzzy is turned on, 'localhost' is always considered a match as we
		 * trust our local machine */
		if (M_str_caseeq(hostname, "localhost"))
			return M_TRUE;

		if (M_tls_verify_host_extract_base_domain(hostname, host_base, sizeof(host_base))) {
			if (M_tls_verify_host_extract_base_domain(name, name_base, sizeof(name_base))) {
				if (M_str_caseeq(host_base, name_base)) {
					 return M_TRUE;
				}
			}
		}
	}

	return M_FALSE;
}


static M_bool M_tls_verify_host_subjaltname(X509 *x509, const char *hostname, M_tls_verify_host_flags_t flags)
{
	GENERAL_NAMES          *SANs        = NULL;
	int                     count;
	int                     i;
	int                     idx         = -1;
	M_bool                  match_found = M_FALSE;

	while ((SANs = X509_get_ext_d2i(x509, NID_subject_alt_name, NULL, &idx)) != NULL && !match_found) {
		count = sk_GENERAL_NAME_num(SANs);
		for (i=0; i<count; i++) {
			const GENERAL_NAME *name    = sk_GENERAL_NAME_value(SANs, i);
			char                dnsname[256];
			char                ipaddr[64];

			if (name->type != GEN_DNS && name->type != GEN_IPADD)
				continue;

			if (name->type == GEN_DNS) {
				unsigned char *temp;
				int            dnsname_len;

				dnsname_len = ASN1_STRING_to_UTF8(&temp, name->d.dNSName);
				/* Check for malformed name (e.g. embedded NULL or binary data). Use M_str_len_max() so we don't possibly
				 * read beyond bounds of buffer */
				if (dnsname == NULL || dnsname_len <= 0 || M_str_len_max((const char *)temp, (size_t)dnsname_len) != (size_t)dnsname_len) {
					OPENSSL_free(temp);
					continue;
				}

				M_str_cpy(dnsname, sizeof(dnsname), (const char *)temp);

			} else if (name->type == GEN_IPADD) {
				const unsigned char *ip_bin     = ASN1_STRING_get0_data(name->d.iPAddress);
				size_t               ip_bin_len = (size_t)ASN1_STRING_length(name->d.iPAddress);

				if (!M_io_net_bin_to_ipaddr(ipaddr, sizeof(ipaddr), ip_bin, ip_bin_len))
					continue;

				M_str_cpy(dnsname, sizeof(dnsname), ipaddr);
			}

			if (M_tls_verify_host_match(hostname, dnsname, flags)) {
				match_found = M_TRUE;
				break;
			}
		}

		sk_GENERAL_NAME_pop_free(SANs, GENERAL_NAME_free);
	}
	return match_found;
}


static M_bool M_tls_verify_host_commonname(X509 *x509, const char *hostname, M_tls_verify_host_flags_t flags)
{
	int        idx     = -1;
	X509_NAME *subject = X509_get_subject_name(x509);

	if (subject == NULL)
		return M_FALSE;

	while ((idx = X509_NAME_get_index_by_NID(subject, NID_commonName, idx)) != -1) {
		X509_NAME_ENTRY *e    = X509_NAME_get_entry(subject, idx);
		ASN1_STRING     *asn1;
		unsigned char   *cn   = NULL;
		int              len;
		M_bool           rv;

		asn1 = X509_NAME_ENTRY_get_data(e);
		if (asn1 == NULL)
			continue;

		len  = ASN1_STRING_to_UTF8(&cn, asn1);
		if (len <= 0 || cn == NULL || M_str_len((const char *)cn) != (size_t)len)
			continue;

		rv = M_tls_verify_host_match(hostname, (const char *)cn, flags);
		OPENSSL_free(cn);
		if (rv)
			return M_TRUE;
	}

	return M_FALSE;
}


M_bool M_tls_verify_host(X509 *x509, const char *hostname, M_tls_verify_host_flags_t flags)
{
	if (x509 == NULL || M_str_isempty(hostname))
		return M_FALSE;

	if (flags & M_TLS_VERIFY_HOST_FLAG_VALIDATE_SAN && M_tls_verify_host_subjaltname(x509, hostname, flags)) {
		return M_TRUE;
	}

	if (flags & M_TLS_VERIFY_HOST_FLAG_VALIDATE_CN && M_tls_verify_host_commonname(x509, hostname, flags)) {
		return M_TRUE;
	}

	return M_FALSE;
}

