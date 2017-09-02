# - Check for getpwnam function.
# Once done this will set one of the following:
#  HAVE_GETPWNAM_5 - Takes 5 arguments.
#  HAVE_GETPWNAM_4 - Takes 4 arguments.
#  HAVE_GETPWNAM_1 - Takes 1 argument. Not reentrant.
#  HAVE_GETPWNAM_0 - Does not support getpwnam function.

include(CheckCSourceCompiles)

# 5 args.
check_c_source_compiles ("
		#include <pwd.h>
		#include <sys/types.h>
		#include <unistd.h>
		int main(int argc, char **argv) {
			struct passwd *pwd_result;
			struct passwd  pwd;
			char           pbuf[16384];
			(void)getpwnam_r(\"a\", &pwd, pbuf, sizeof(pbuf), &pwd_result);
			return 0;
		}
	"
	HAVE_GETPWNAM_5
)
# 4 args.
if (NOT HAVE_GETPWNAM_5)
	check_c_source_compiles ("
			#include <pwd.h>
			#include <sys/types.h>
			#include <unistd.h>
			int main(int argc, char **argv) {
				struct passwd  pwd;
				char           pbuf[16384];
				(void)getpwnam_r(\"a\", &pwd, pbuf, sizeof(pbuf));
				return 0;
			}
		"
		HAVE_GETPWNAM_4
	)
endif()
# 1 args.
if (NOT HAVE_GETPWNAM_5 AND NOT HAVE_GETPWNAM_4)
	check_c_source_compiles ("
			#include <pwd.h>
			#include <sys/types.h>
			#include <unistd.h>
			int main(int argc, char **argv) {
				(void)getpwnam(\"a\");
				return 0;
			}
		"
		HAVE_GETPWNAM_1
	)
endif ()

if (HAVE_GETPWNAM_5 OR HAVE_GETPWNAM_4)
	set(HAVE_GETPWNAM_R 1)
endif ()

if (NOT HAVE_GETPWNAM_5 AND NOT HAVE_GETPWNAM_4 AND NOT HAVE_GETPWNAM_1)
	set(HAVE_GETPWNAM_0 1)
endif ()
