# - Check for getgrnam function.
# Once done this will set one of the following:
#  HAVE_GETGRNAM_5 - Takes 5 arguments.
#  HAVE_GETGRNAM_4 - Takes 4 arguments.
#  HAVE_GETGRNAM_1 - Takes 1 argument. Not reentrant.
#  HAVE_GETGRNAM_0 - Does not support getgrnam function.

include(CheckCSourceCompiles)

# 5 args.
check_c_source_compiles ("
		#include <grp.h>
		#include <sys/types.h>
		int main(int argc, char **argv) {
			struct group   grp;
			struct group  *grp_result;
			char           pg_buf[16384];
			(void)getgrnam_r(\"a\", &grp, pg_buf, sizeof(pg_buf), &grp_result);
			return 0;
		}
	"
	HAVE_GETGRNAM_5
)
# 4 args.
if (NOT HAVE_GETGRNAM_5)
	check_c_source_compiles ("
			#include <grp.h>
			#include <sys/types.h>
			int main(int argc, char **argv) {
				struct group   grp;
				char           pg_buf[16384];
				(void)getgrnam_r(\"a\", &grp, pg_buf, sizeof(pg_buf));
				return 0;
			}
		"
		HAVE_GETGRNAM_4
	)
endif ()
# 1 args.
if (NOT HAVE_GETGRNAM_5 AND NOT HAVE_GETGRNAM_4)
	check_c_source_compiles ("
			#include <grp.h>
			#include <sys/types.h>
			int main(int argc, char **argv) {
				(void)getgrnam(\"a\");
				return 0;
			}
		"
		HAVE_GETGRNAM_1
	)
endif ()

if (HAVE_GETGRNAM_5 OR HAVE_GETGRNAM_4)
	set(HAVE_GETGRNAM_R 1)
endif ()

if (NOT HAVE_GETGRNAM_5 AND NOT HAVE_GETGRNAM_4 AND NOT HAVE_GETGRNAM_1)
	set(HAVE_GETGRNAM_0 1)
endif ()

