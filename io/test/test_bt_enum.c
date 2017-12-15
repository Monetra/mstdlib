#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_io.h>

int main(int argc, char **argv)
{
	M_io_bluetooth_enum_t *be;
	size_t           len;
	size_t           i;

	be = M_io_bluetooth_enum();

	len = M_io_bluetooth_enum_count(be);
	M_printf("Num devs = %zu\n", len);
	for (i=0; i<len; i++) {
		M_printf("Device:\n");
		M_printf("\tName:      %s\n", M_io_bluetooth_enum_name(be, i));
		M_printf("\tMac:       %s\n", M_io_bluetooth_enum_mac(be, i));
		M_printf("\tConnected: %s\n", M_io_bluetooth_enum_connected(be, i)?"Yes":"No");
		M_printf("\tService:   %s\n", M_io_bluetooth_enum_service_name(be, i));
		M_printf("\tUUID:      %s\n", M_io_bluetooth_enum_service_uuid(be, i));
	}

	M_io_bluetooth_enum_destroy(be);

	return 0;
}
