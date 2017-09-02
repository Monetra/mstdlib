# - Check for getpwuid function.
# Once done this will set one of the following:
#  HAVE_GETPWUID_5 - Takes 5 arguments.
#  HAVE_GETPWUID_4 - Takes 4 arguments.
#  HAVE_GETPWUID_1 - Takes 1 argument. Not reentrant.
#  HAVE_GETPWUID_0 - Does not support getpwuid function.

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
			(void)getpwuid_r(getuid(), &pwd, pbuf, sizeof(pbuf), &pwd_result);
			return 0;
		}
	"
	HAVE_GETPWUID_5
)
# 4 args.
if (NOT HAVE_GETPWUID_5)
	check_c_source_compiles ("
			#include <pwd.h>
			#include <sys/types.h>
			#include <unistd.h>
			int main(int argc, char **argv) {
				struct passwd  pwd;
				char           pbuf[16384];
				(void)getpwuid_r(getuid(), &pwd, pbuf, sizeof(pbuf));
				return 0;
			}
		"
		HAVE_GETPWUID_4
	)
endif ()
# 1 args.
if (NOT HAVE_GETPWUID_5 AND NOT HAVE_GETPWUID_4)
	check_c_source_compiles ("
			#include <pwd.h>
			#include <sys/types.h>
			#include <unistd.h>
			int main(int argc, char **argv) {
				(void)getpwuid(getuid());
				return 0;
			}
		"
		HAVE_GETPWUID_1
	)
endif ()

if (HAVE_GETPWUID_5 OR HAVE_GETPWUID_4)
	set(HAVE_GETPWUID_R 1)
endif ()

if (NOT HAVE_GETPWUID_5 AND NOT HAVE_GETPWUID_4 AND NOT HAVE_GETPWUID_1)
	set(HAVE_GETPWUID_0 1)
endif ()
