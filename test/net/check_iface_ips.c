#include "m_config.h"
#include <check.h>
#include <stdlib.h>

#include <mstdlib/mstdlib_net.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


START_TEST(check_iface_ips)
{
	M_net_iface_ips_t *ips = M_net_iface_ips(M_NET_IFACE_IPS_FLAG_OFFLINE|M_NET_IFACE_IPS_FLAG_LOOPBACK);
	size_t             i;

	ck_assert(ips != NULL);
	ck_assert(M_net_iface_ips_count(ips) != 0);

	M_printf("%zu entries\n", M_net_iface_ips_count(ips));
	for (i=0; i<M_net_iface_ips_count(ips); i++) {
		char *flags;
		ck_assert(M_net_iface_ips_get_name(ips, i) != NULL);
		M_printf("%zu: name=%s, ipaddr=", i, M_net_iface_ips_get_name(ips, i));
		if (M_net_iface_ips_get_addr(ips, i) == NULL) {
			M_printf("None");
		} else {
			M_printf("%s/%d", M_net_iface_ips_get_addr(ips, i), M_net_iface_ips_get_netmask(ips, i));
		}
		flags = M_net_iface_ips_flags_to_str(M_net_iface_ips_get_flags(ips, i));
		M_printf(", flags=%s\n", flags);
		M_free(flags);
	}
	M_net_iface_ips_free(ips);
}
END_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static Suite *iface_ips_suite(void)
{
	Suite *suite = suite_create("iface_ips");
	TCase *tc;

	tc = tcase_create("iface_ips");
	tcase_add_test(tc, check_iface_ips);
	suite_add_tcase(suite, tc);

	return suite;
}

int main(int argc, char **argv)
{
	SRunner *sr;
	int nf;

	(void)argc;
	(void)argv;

	sr = srunner_create(iface_ips_suite());
	if (getenv("CK_LOG_FILE_NAME")==NULL) srunner_set_log(sr, "check_iface_ips.log");

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
