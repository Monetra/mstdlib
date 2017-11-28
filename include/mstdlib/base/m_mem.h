/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
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

#ifndef __M_MEM_H__
#define __M_MEM_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_fmt.h>
#include <mstdlib/base/m_str.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

M_BEGIN_IGNORE_REDECLARATIONS
#if M_BLACKLIST_FUNC == 1
#  define WARNING_MALLOC "Memory obtained by malloc can contain sensitive information, if not properly cleared when released, the memory could be obtained by another process or leaked by an incorrect program. use M_malloc/M_calloc,M_free/M_realloc instead."
#  ifdef malloc
#    undef malloc
#  else
     M_DEPRECATED_MSG(WARNING_MALLOC, void *malloc(size_t))
#  endif
#  ifdef free
#    undef free
#  else
     M_DEPRECATED_MSG(WARNING_MALLOC, void free(void *))
#  endif
#  ifdef calloc
#    undef calloc
#  else
     M_DEPRECATED_MSG(WARNING_MALLOC, void *calloc(size_t, size_t))
#  endif
#  ifdef realloc
#    undef realloc
#  else
     M_DEPRECATED_MSG(WARNING_MALLOC, void *realloc(void *, size_t))
#  endif
#  ifdef memcmp
#    undef memcmp
#  else
     M_DEPRECATED_MSG("memcmp is vulnerable to timing attacks. use M_mem_eq", int memcmp(const void *, const void *, size_t))
#  endif
#endif
M_END_IGNORE_REDECLARATIONS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! \addtogroup m_mem Memory
 *  \ingroup mstdlib_base
 *
 * Memory manipulation.
 *
 * Hardening
 * =========
 *
 * To aid in hardening M_malloc wraps the system malloc and stores the length of allocated memory.
 * M_free uses this length and zeros the memory before calling the system free. The length is
 * prepended to the memory segment and the pointer to the memory after the size is returned.
 * The size is then read by M_free so the full allocated memory segment can be zeroed.
 *
 * Zeroing memory is performed to combat memory scan attacks. It's not possible to know what data in memory is
 * sensitive so all data is considered sensitive. Zeroing limits the amount of time data such as credit card
 * numbers or encryption keys are available. This way data is available only as long  as needed.
 *
 * M_malloc and M_free are not replacements for the system provided malloc and free functions. They work
 * on top of the system functions. This is so a hardened system malloc will not have it's functionality
 * disrupted. Further, a replacement malloc implementation (such as jemalloc) can still be used.
 *
 * Some system mallocs already zero memory but many do not. Mstdlib's M_malloc brings this to systems
 * that do not implement this security feature. This is a case where security trumps performance.
 *
 * Memory allocated using M_malloc must never be passed directly to the system free due to the length
 * offset prefix, the caller would not be passing the base of the block and therefore cause undefined
 * behavior (probably a segfault).
 *
 * Memory allocated by malloc (not M_malloc) should never be passed to M_free. This will result in
 * undefined behavior (probably a segfault) as well.
 *
 * All mstdlib functions that allocate memory use mstdlib's M_malloc. Thus any memory that needs
 * to be freed using free that is returned by an mstdlib function must be passed to M_free.
 *
 *
 * Hardening that doesn't work
 * ===========================
 *
 * There are a few "hardening" features that are available on Linux and other Unix platforms that were
 * evaluated. These were determined to not be usable.
 *
 * mlock
 * -----
 *
 * mlock prevents a memory segment from being written to on disk swap space.
 *
 * The issue with mlock is limits set by the OS. RLIMIT_MEMLOCK (ulimit -l) limits the amount of memory
 * that can be locked. munlock must be used (before or after, testing showed it didn't matter) to reduce
 * the locked memory amount. munmap should implicitly unlock the memory as well but in testing a simple
 * free did not cause the memory to be unlocked.
 *
 * munlock is not enough to avoid hitting the limit. In simple / small applications or test cases, it would
 * function fine. However, a larger application which uses more memory will fail. Once the lock limit
 * is reached an out of memory error will be returned.
 *
 * On Ubuntu 14.04.2 the default RLIMIT_MEMLOCK is 64K. On some versions of Debian is was found to be 32K.
 * This limit will quickly be reached by a non-trivial application.
 *
 * Configuring the system to have a larger limit or making the limit unlimited may not alleviate 
 * this issue. For example, FreeBSD allows mlock use to be restricted to the user-user only.
 *
 * Further, Requiring system configuration to use a general purpose library is unacceptable. Especially when the
 * configuration is non-obvious. Also if mlock is limited to super-user only then mstdlib would be unusable
 * as user level application.
 *
 * madvise with MADV_DONTDUMP
 * --------------------------
 *
 * This is used to prevent marked memory from being in a core dump.
 *
 * On Linux madvise requires the memory to be page-aligned. If the memory is not page-aligned madvise will return
 * a failure with errno EINVAL. Page-alignment can easily cause the application to run out of address space.
 * 
 * For example you could use an allocation like:
 *
 *     void *ptr;
 *     posix_memalign(&ptr, sysconf(_SC_PAGESIZE), size);
 *
 * Getting the page size on the command line (which is the size of _SC_PAGESIZE):
 *
 *     $ getconf PAGESIZE
 *     4096
 *
 * In this (and many) cases we have a 4096 byte boundary. Meaning the address of the allocated data must be the
 * address of a page boundary. There is 4K between each boundary. A large amount of data can be allocated there but
 * if a small amount of data is allocated then there is a large amount of unusable space due to the next allocation
 * needing to also be on a 4K boundary.
 *
 * Take the following allocations:
 *
 * 1. 8 bytes page-aligned.
 * 2. 4 bytes page-aligned.
 *
 * Assuming One and Two are allocated next to each other. One allocates 8 bytes. Two will be aligned to the
 * 4K boundary after One. A total of 8K of memory is reserved due to this. Only 12 bytes are actually needed but 8K
 * is reserved. Since memory is now aligned in 4K blocks the total available memory space is greatly reduced.
 * Not the amount of memory but the amount of allocations.
 *
 * On a 32bit system only ~2GB of memory is available to a process. With 4K page-alignment allocations the amount
 * usable memory is greatly reduced. This might be okay on a 64 bit system but will still be wasteful.
 *  
 * Also since Linux, since 3.18, has made madvise optional which severely limits its use.
 *
 * Conclusion
 * ----------
 *
 * Neither mlock nor madvise  can be used on every malloc. It may be okay to use this selectively but in a
 * general purpose library there is no way to truly know what is sensitive. For example M_list_str and
 * M_hash_dict duplicate the strings they are given. There is no way for them to know that a particular string needs
 * to be securely allocated.
 *
 * Additional External Security
 * ============================
 *
 * One option to add additional security is to create an encrypting wrapper around a list or hashtable:
 * - Use a different key for each hashtable.
 * - Limit the life of a hashtable in order to rotate the key often.
 * - Insert, wrapper will take the key and value. Encrypt them and store them in the hashtable.
 * - Get, wrapper will take the key, encrypt it. Use that to look up the value. Decrypt the value. Return the value.
 *
 * This option further limits the amount of time sensitive data is stored in the clear in memory because the
 * value in the hashtable is encrypted. The plain text data is only exposed as long as it is being actively used.
 * This will further protect against memory scrapers.
 *
 * It also, reduces the concern of swap and core dumps because the data is stored encrypted. Granted the key as
 * well as the encrypted value could be stored on disk. However, it will still be difficult to determine what data
 * is the key, and what set of data the key belongs to.  
 *
 * @{
 */

/*! Error callback for handling malloc failure.
 *
 * Can return M_TRUE to retry malloc.
 */
typedef M_bool (*M_malloc_error_cb)(void);

/*! Register a callback to be called when M_malloc()/M_realloc() returns a failure.
 *
 * Up to 12 callbacks can be registered. They will be called from newest to
 * oldest. If a callback returns M_TRUE callback processing will stop and
 * malloc will be retried. If malloc fails again the callbacks processing will
 * resume. Each callback will be run until either one returns success or all
 * have returned failure.
 *
 * Typically this will be used for external error reporting, or (more) graceful shutdown scenarios.
 *
 * \param[in] cb Callback to be called. This should not ever try to allocate memory as it will most likely fail.
 *
 * \return M_TRUE on success, M_FALSE on failure. The only failure reason currently is if the maximum
 *         number of registered callbacks has been reached.
 */
M_API M_bool M_malloc_register_errorcb(M_malloc_error_cb cb);


/*! Deregister an allocation error callback
 *
 * \param[in] cb The callback to remove.
 * 
 * \return M_TRUE if the callback was removed otherwise M_FALSE. M_FALSE means the callback is not
 *         currently registered.
 */
M_API M_bool M_malloc_deregister_errorcb(M_malloc_error_cb cb);


/*! Clears all user registered callbacks. The default abort callback is not cleared.
 */
M_API void M_malloc_clear_errorcb(void);


/*! Allocate size bytes and returns pointer to allocated memory.
 *
 *  Retains information about the size of the allocation and must be released using M_free().
 *
 *  On failure registered error callbacks will be called and malloc will be repleted if any
 *  error callback return M_TRUE indicating malloc should be retried. If no callbacks return 
 *  retry the application will abort. The callbacks will be run in reverse order they were registered.
 *
 * \param[in] size Number of bytes of memory to allocate.
 *
 * \return Pointer to the newly allocated memory or NULL if the requested memory is unavailable.
 *          Memory must be released using M_free().
 *
 * \see M_free
 */
M_API void *M_malloc(size_t size) M_ALLOC_SIZE(1) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Allocate size bytes and returns pointer to allocated memory and fills the memory with 0's.
 *
 *  Retains information about the size of the allocation and must be released using M_free().
 *
 * \param[in] size Number of bytes of memory to allocate.
 *
 * \return Pointer to the newly allocated memory or NULL if the requested memory is unavailable.
 *          Memory must be released using M_free().
 *
 * \see M_free
 */
M_API void *M_malloc_zero(size_t size) M_ALLOC_SIZE(1) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Release allocated memory.
 *
 * Like libc free, but works with memory allocated by M_malloc class of functions to free allocated memory.
 * Before being released, each byte of ptr is first set to zero.
 *
 * \param[in] ptr A pointer to a memory location to release returned by M_malloc like functions.
 *
 * \see M_malloc
 * \see M_malloc_zero
 * \see M_realloc
 * \see M_memdup
 * \see M_memdup_max
 */
M_API void M_free(void *ptr) M_FREE(1);


/*! Resize an allocated memory block.
 *
 * Like libc realloc, but works with memory allocated by M_malloc like functions.
 * If ptr is unable to be resized, before being released, each byte of ptr is first set to zero.
 *
 * \param[in] ptr  A pointer to a memory location to release/resize returned by M_malloc.
 * \param[in] size Number of bytes of memory to allocate.
 *
 * \return Pointer to the newly allocated memory or NULL if the requested
 *          memory is zero in size or unavailable. Memory must be released using M_free().
 *
 * \see M_free
 */
M_API void *M_realloc(void *ptr, size_t size) M_ALLOC_SIZE(2) M_WARN_UNUSED_RESULT;


/*! Resize an allocated memory block and fill any extended allocated memory with 0's.
 *
 * Like libc realloc, but works with memory allocated by M_malloc like functions.
 * If ptr is unable to be resized, before being released, each byte of ptr is first set to zero.
 *
 * \param[in] ptr  A pointer to a memory location to release/resize returned by M_malloc.
 * \param[in] size Number of bytes of memory to allocate.
 *
 * \return Pointer to the newly allocated memory or NULL if the requested
 *          memory is zero in size or unavailable. Memory must be released using M_free().
 *
 * \see M_free
 */
M_API void *M_realloc_zero(void *ptr, size_t size) M_ALLOC_SIZE(2) M_WARN_UNUSED_RESULT;


/*! Allocate and copy  size bytes from src to the newly allocated space.
 *
 *  src should be at least size memory area.
 *
 * \param[in] src  Memory area to copy.
 * \param[in] size Number of bytes of memory to allocate and copy from src.
 *
 * \return Pointer to the newly allocated memory or NULL if the requested
 *          memory is unavailable. Memory must be released with M_free().
 *
 * \see M_free
 */
M_API void *M_memdup(const void *src, size_t size) M_ALLOC_SIZE(2) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1) M_MALLOC;


/*! Allocate at minimum min_alloc_size bytes, but copy no more than size bytes from ptr to the newly allocated space.
 *
 *  If size is larger than min_alloc_size, then size bytes will be allocated. src should be at least size memory area
 *  or NULL is returned.
 *
 *  This function behaves like M_malloc(size) when called M_memdup_max(NULL,0,size).
 *
 * \param[in] src            Memory area to copy.
 * \param[in] size           Number of bytes of memory to allocate and copy from src.
 * \param[in] min_alloc_size The minimum size of the returned allocation.
 *
 * \return Pointer to the newly allocated memory or  NULL if the requested
 *          memory is unavailable or if src is NULL but has positive *
 *          size. Memory must be released with M_free().
 *
 * \see M_free
 */
M_API void *M_memdup_max(const void *src, size_t size, size_t min_alloc_size) M_ALLOC_SIZE(2) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1) M_MALLOC;


/* - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Set memory.
 *
 * \param[in,out] s The memory to set.
 * \param[in]     c The value to set.
 * \param[in]     n The length of the memory segement.
 *
 * \return A pointer to s.
 */
M_API void *M_mem_set(void *s, int c, size_t n);


/*! Copy memory area.
 *
 * This function behaves like memcpy, but handles NULL gracefully.
 *
 * \param[in,out] dst  Memory location to copy to.
 * \param[in]     src  Memory location to copy from.
 * \param[in]     size Number of bytes to copy.
 *
 * \return A pointer to dst.
 */
M_API void *M_mem_move(void *dst, const void *src, size_t size) M_WARN_NONNULL(1) M_WARN_NONNULL(2);


/*! Copy memory area.
 *
 * This function behaves like memcpy, but handles NULL gracefully.
 *
 * \param[in,out] dst  Memory location to copy to.
 * \param[in]     src  Memory location to copy from.
 * \param[in]     size Number of bytes to copy.
 *
 * \return A pointer to dst.
 */
M_API void *M_mem_copy(void *dst, const void *src, size_t size) M_WARN_NONNULL(1) M_WARN_NONNULL(2);


/*! Compare memory segments.
 *
 * This is done in a constant-time manner to prevent against timing related attacks.
 *
 * \param[in] m1   Memory address.
 * \param[in] m2   Memory address.
 * \param[in] size Length of memory to check.
 *
 * \return M_TRUE if equal, M_FALSE if not.
 */
M_API M_bool M_mem_eq(const void *m1, const void *m2, size_t size) M_WARN_NONNULL(1) M_WARN_NONNULL(2);


/*! A wrapper around memcmp that is NULL safe.
 *
 *  NOTE: this is not a constant-time comparison and thus should ONLY be used
 *        for sorting such as within qsort()!
 *
 * \param[in] m1    Memory address.
 * \param[in] size1 Size of m1.
 * \param[in] m2    Memory address.
 * \param[in] size2 Size of m2.
 *
 * \return an integer less than, equal to, or greater than zero if  m1 is
 *         less than, equal, or greater than m2 respectively
 */
M_API int M_mem_cmpsort(const void *m1, size_t size1, const void *m2, size_t size2) M_WARN_UNUSED_RESULT;


/* - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Find first occurrence of b in s.
 *
 * \param[in] s The memory area to search.
 * \param[in] b The byte to search the memory area for.
 * \param[in] n The size of the memory area to search.
 *
 * \return Pointer to the first occurence of b in s or NULL if not found or s is NULL or is 0.
 */
M_API void *M_mem_chr(const void *s, M_uint8 b, size_t n) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1);


/*! Determine if needle exists in haystack.
 *
 * \param[in] haystack     Memory to search in.
 * \param[in] haystack_len The size in bytes of haystack.
 * \param[in] needle       Memory to search for.
 * \param[in] needle_len   The size in bytes of needle.
 *
 * \return M_TRUE if needle exists in haystack or needle_len is 0, M_FALSE otherwise.
 */
M_API M_bool M_mem_contains(const void *haystack, size_t haystack_len, const void *needle, size_t needle_len) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1) M_WARN_NONNULL(3);


/*! Find first occurring bytes needle of length needle_len in haystack.
 *
 * \param[in] haystack     Memory to search in.
 * \param[in] haystack_len The size in bytes of haystack.
 * \param[in] needle       Memory to search for.
 * \param[in] needle_len   The size in bytes of needle.
 *
 * \return Pointer to first occurrence of needle in haystack or NULL if not found or haystack is NULL or haystack_len
 *          is 0.
 */
M_API void *M_mem_mem(const void *haystack, size_t haystack_len, const void *needle, size_t needle_len) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1) M_WARN_NONNULL(3);


/*! Find last occurring bytes needle of length needle_len in haystack.
 *
 * \param[in] haystack     Memory to search in.
 * \param[in] haystack_len The size in bytes of haystack.
 * \param[in] needle       Memory to search for.
 * \param[in] needle_len   The size in bytes of needle.
 *
 * \return Pointer to last occurrence of needle in haystack or NULL if not found or haystack is NULL or haystack_len
 *          is 0.
 */
M_API void *M_mem_rmem(const void *haystack, size_t haystack_len, const void *needle, size_t needle_len) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1) M_WARN_NONNULL(3);


/*! Find first occurring string needle in haystack.
 *
 * \param[in] haystack     Memory to search in.
 * \param[in] haystack_len The size in bytes of haystack.
 * \param[in] needle       Memory to search for.
 *
 * \return Pointer to first occurrence of needle in haystack or NULL if not found or haystack is NULL.
 */
M_API void *M_mem_str(const void *haystack, size_t haystack_len, const char *needle) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1);
#define M_mem_str(haystack,haystack_len,needle) \
        M_mem_mem(haystack,haystack_len,needle,M_str_len(needle) )


/*! Find index of first occurring bytes needle of length needle_len in haystack.
 *
 * \param[in]  haystack     Memory to search in.
 * \param[in]  haystack_len The size in bytes of haystack.
 * \param[in]  needle       Memory to search for.
 * \param[in]  needle_len   The size in bytes of needle.
 * \param[out] idx          The index of first occurrence of needle in haystack. Optional, pass NULL if not needed.
 *
 * \return M_TRUE if found, M_FALSE otherwise.
 */
M_API M_bool M_mem_mempos(const void *haystack, size_t haystack_len, const void *needle, size_t needle_len, size_t *idx) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1) M_WARN_NONNULL(3);
#define M_mem_strpos(haystack,haystack_len,needle,idx) \
        M_mem_mempos(haystack,haystack_len,needle,M_str_len(needle) ,idx)


/*! Count the number of occurrences of byte b in memory area s of length s_len
 *
 * \param[in] s     Pointer to the memory area to search.
 * \param[in] s_len The size of the memory area s.
 * \param[in] b     The byte value to count occurrences of.
 */
M_API size_t M_mem_count(const void *s, size_t s_len, M_uint8 b) M_WARN_UNUSED_RESULT M_WARN_NONNULL(1);


/*! Calculate an LRC.
 *
 * \param[in] s     Pointer to the memory area to search.
 * \param[in] s_len The size of the memory area s.
 *
 * \return LRC
 */
M_API unsigned char M_mem_calc_lrc(const void *s, size_t s_len);


/*! Calculate a CRC (CRC-8/CCITT).
 *
 * This is an 8-bit cyclic redundancy check (CRC), using the CCITT standard
 * polynomial: <tt>x^8 + x^2 + x + 1</tt>. It's calculated using an initial
 * value of zero.
 *
 * Implementation is based on public-domain code that can be found here: https://www.3dbrew.org/wiki/CRC-8-CCITT
 *
 * \param[in] s     Pointer to data to perform check on.
 * \param[in] s_len Size of memory area s.
 * \return          CRC value.
 */
M_API M_uint8 M_mem_calc_crc8_ccitt(const void *s, size_t s_len);


/*! Swap byes between positions.
 *
 * \param[in,out] s     Buffer with data to swap.
 * \param[in]     s_len size of s.
 * \param[in]     idx1  Index to swap.
 * \param[in]     idx2  Index to swap.
 *
 * \return M_TRUE on success. Otherwise M_FALSE.
 */
M_API M_bool M_mem_swap_bytes(M_uint8 *s, size_t s_len, size_t idx1, size_t idx2);

/*! @} */

__END_DECLS

#endif /* __M_MEM_H__ */
