# Check to see if birthtime is part of stat command.
#
# Once done, this will set the following variable(s):
#  HAVE_ST_BIRTHTIME

include(CheckCSourceCompiles)

check_c_source_compiles("
		#include <sys/stat.h>
		int main(int argc, char **argv) {
			struct stat buf;
			int ret;
			ret = stat(0, &buf);
			return (int)buf.st_birthtime;
		}
	"
	HAVE_ST_BIRTHTIME
)
