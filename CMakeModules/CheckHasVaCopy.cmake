# Check to see what type of va_copy is possible.
# 
# Once done, this will set the following variable(s):
#  HAVE_VA_COPY
#  HAVE_VA___COPY
#  VA_LIST_IS_ARRAY_TYPE
#  VA_LIST_IS_POINTER_TYPE

include(CheckCSourceCompiles)

check_c_source_compiles("
		#include <stdarg.h>
		static void test(int a, ...) {
			va_list aq;
			va_list ap;

			va_start(ap, a);
			va_copy(aq, ap);
			va_end(ap);
		}
		int main(int argc, char **argv) {
			test(1);
			return 0;
		}
	"
	HAVE_VA_COPY
)

check_c_source_compiles("
		#include <stdarg.h>
		static void test(int a, ...) {
			va_list aq;
			va_list ap;

			va_start(ap, a);
			__va_copy(aq, ap);
			va_end(ap);
		}
		int main(int argc, char **argv) {
			test(1);
			return 0;
		}
	"
	HAVE_VA___COPY
)

check_c_source_compiles("
		#include <stdarg.h>
		static void test(int a, ...) {
			va_list aq;
			va_list ap;

			va_start(ap, a);
			aq = ap;
			va_end(ap);
		}
		int main(int argc, char **argv) {
			test(1);
			return 0;
		}
	"
	VA_LIST_IS_ARRAY_TYPE
)

if (NOT VA_LIST_IS_ARRAY_TYPE)
	check_c_source_compiles("
			#include <stdarg.h>
			static void test(int a, ...) {
				va_list aq;
				va_list ap;

				va_start(ap, a);
				*aq = *ap;
				va_end(ap);
			}
			int main(int argc, char **argv) {
				test(1);
				return 0;
			}
		"
		VA_LIST_IS_POINTER_TYPE
	)
endif ()
