# - Check for readdir function.
# Once done this will set one of the following:
#  HAVE_READDIR_3 - Takes 5 arguments.
#  HAVE_READDIR_2 - Takes 4 arguments.
#  HAVE_READDIR_1 - Takes 1 argument. Not reentrant.
#  HAVE_READDIR_0 - Does not support readdir function.

include(CheckCSourceCompiles)

# 3 args.
check_c_source_compiles ("
		#include <dirent.h>
		#include <stddef.h>
		#include <stdlib.h>
		#include <string.h>
		#include <sys/types.h>
		int main(int argc, char **argv) {
			DIR           *dir;
			struct dirent *entry;
			struct dirent *result;
			size_t         len;
			len   = offsetof(struct dirent, d_name) + 16385;
			entry = malloc(len);
			memset(entry, 0, len);
			dir = opendir(\".\");
			(void)readdir_r(dir, entry, &result); 
			free(entry);
			closedir(dir);
			return 0;
		}
	"
	HAVE_READDIR_3
)
# 2 args.
if (NOT HAVE_READDIR_3)
	check_c_source_compiles ("
			#include <dirent.h>
			#include <stddef.h>
			#include <stdlib.h>
			#include <string.h>
			#include <sys/types.h>
			int main(int argc, char **argv) {
				DIR           *dir;
				struct dirent *entry;
				size_t         len;
				len   = offsetof(struct dirent, d_name) + 16385;
				entry = malloc(len);
				memset(entry, 0, len);
				dir = opendir(\".\");
				(void)readdir_r(dir, entry); 
				free(entry);
				closedir(dir);
				return 0;
			}
		"
		HAVE_READDIR_2
	)
endif ()
# 1 args.
if (NOT HAVE_READDIR_3 AND NOT HAVE_READDIR_2)
	check_c_source_compiles ("
			#include <dirent.h>
			#include <sys/types.h>
			int main(int argc, char **argv) {
				DIR           *dir;
				dir = opendir(\".\");
				(void)readdir(dir);
				closedir(dir);
				return 0;
			}
		"
		HAVE_READDIR_1
	)
endif ()

if (HAVE_READDIR_3 OR HAVE_READDIR_2)
	set(HAVE_READDIR_R 1)
endif ()

if (NOT HAVE_READDIR_3 AND NOT HAVE_READDIR_2 AND NOT HAVE_READDIR_1)
	set(HAVE_READDIR_0 1)
endif ()
