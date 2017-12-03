#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_io.h>

int main(int argc, char **argv)
{
	M_io_hid_enum_t *he;
	size_t           len;
	size_t           i;

	he = M_io_hid_enum(0, NULL, 0, NULL);

	len = M_io_hid_enum_count(he);
	M_printf("Num devs = %zu\n", len);
	for (i=0; i<len; i++) {
		M_printf("Device:\n");
		M_printf("\tPath: %s\n", M_io_hid_enum_path(he, i));
		M_printf("\tManuf: %s\n", M_io_hid_enum_manufacturer(he, i));
		M_printf("\tProd: %s\n", M_io_hid_enum_product(he, i));
		M_printf("\tSerial: %s\n", M_io_hid_enum_serial(he, i));
		M_printf("\tVendid: %d\n", M_io_hid_enum_vendorid(he, i));
		M_printf("\tProdid: %d\n", M_io_hid_enum_productid(he, i));
	}

	M_io_hid_enum_destroy(he);

	return 0;
}
