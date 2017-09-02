# - Check for max_align_t
# Once done this will set one of the following:
#  HAVE_MAX_ALIGN_T

include(CheckCSourceCompiles)

check_c_source_compiles ("
		#include <stddef.h>
		#include <stdalign.h>
		int main(int argc, char **argv) {
			(void)alignof(max_align_t);
			return 0;
		}
	"
	HAVE_MAX_ALIGN_T
)
