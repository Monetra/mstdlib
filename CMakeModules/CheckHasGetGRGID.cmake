# - Check for getgrgid function.
# Once done this will set one of the following:
#  HAVE_GETGRGID_5 - Takes 5 arguments.
#  HAVE_GETGRGID_4 - Takes 4 arguments.
#  HAVE_GETGRGID_1 - Takes 1 argument. Not reentrant.
#  HAVE_GETGRGID_0 - Does not support getgrgid function.

include(CheckCSourceCompiles)

# 5 args.
check_c_source_compiles ("
		#include <grp.h>
		#include <sys/types.h>
		#include <unistd.h>
		int main(int argc, char **argv) {
			struct group   grp;
			struct group  *grp_result;
			char           pg_buf[16384];
			(void)getgrgid_r(getgid(), &grp, pg_buf, sizeof(pg_buf), &grp_result);
			return 0;
		}
	"
	HAVE_GETGRGID_5
)
# 4 args.
if (NOT HAVE_GETGRGID_5)
	check_c_source_compiles ("
			#include <grp.h>
			#include <sys/types.h>
			#include <unistd.h>
			int main(int argc, char **argv) {
				struct group   grp;
				char           pg_buf[16384];
				(void)getgrgid_r(getgid(), &grp, pg_buf, sizeof(pg_buf));
				return 0;
			}
		"
		HAVE_GETGRGID_4
	)
endif()
# 1 args.
if (NOT HAVE_GETGRGID_5 AND NOT HAVE_GETGRGID_4)
	check_c_source_compiles ("
			#include <grp.h>
			#include <sys/types.h>
			#include <unistd.h>
			int main(int argc, char **argv) {
				(void)getgrgid(getgid());
				return 0;
			}
		"
		HAVE_GETGRGID_1
	)
endif ()

if (HAVE_GETGRGID_5 OR HAVE_GETGRGID_4)
	set(HAVE_GETGRGID_R 1)
endif ()

if (NOT HAVE_GETGRGID_5 AND NOT HAVE_GETGRGID_4 AND NOT HAVE_GETGRGID_1)
	set(HAVE_GETGRGID_0 1)
endif ()
