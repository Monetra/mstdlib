// Build:
// clang -g -fobjc-arc -framework CoreFoundation test_ble_enum.c -I ../../include/ -L ../../build/lib/ -l mstdlib_io -l mstdlib_thread -l mstdlib
//
// Run:
// DYLD_LIBRARY_PATH="../../build/lib/" ./a.out

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>
#include <mstdlib/io/m_io_ble.h>

#include <CoreFoundation/CoreFoundation.h>

M_event_t    *el;
CFRunLoopRef  mrl = NULL;

static void scan_done_cb(M_event_t *event, M_event_type_t type, M_io_t *io, void *cb_arg)
{
    M_io_ble_enum_t *btenum;
    M_list_str_t    *service_uuids;
    size_t           len;
    size_t           len2;
    size_t           i;
    size_t           j;

    (void)event;
    (void)type;
    (void)io;
    (void)cb_arg;

    btenum = M_io_ble_enum();

    len = M_io_ble_enum_count(btenum);
    M_printf("Num devs = %zu\n", len);
    for (i=0; i<len; i++) {
        M_printf("Device:\n");
        M_printf("\tName: %s\n", M_io_ble_enum_name(btenum, i));
        M_printf("\tIdentifier: %s\n", M_io_ble_enum_identifier(btenum, i));
        M_printf("\tLast Seen: %llu\n", M_io_ble_enum_last_seen(btenum, i));
        M_printf("\tSerivces:\n");
        service_uuids = M_io_ble_enum_service_uuids(btenum, i);
        len2 = M_list_str_len(service_uuids);
        for (j=0; j<len2; j++) {
            M_printf("\t\t: %s\n", M_list_str_at(service_uuids, j));
        }
        M_list_str_destroy(service_uuids);
    }

    M_io_ble_enum_destroy(btenum);

    if (mrl != NULL)
        CFRunLoopStop(mrl);
}

static void *run_el(void *arg)
{
    (void)arg;
    M_event_loop(el, M_TIMEOUT_INF);
    return NULL;
}

int main(int argc, char **argv)
{
    M_threadid_t     el_thread;
    M_thread_attr_t *tattr;

    el = M_event_create(M_EVENT_FLAG_NONE);

    tattr = M_thread_attr_create();
    M_thread_attr_set_create_joinable(tattr, M_TRUE);
    el_thread = M_thread_create(tattr, run_el, NULL);
    M_thread_attr_destroy(tattr);

    M_io_ble_scan(el, scan_done_cb, NULL, 30000);

    mrl = CFRunLoopGetCurrent();
    CFRunLoopRun();

    // 5 sec timeout.
    M_event_done_with_disconnect(el, 0, 5*1000);
    M_thread_join(el_thread, NULL);

    return 0;
}
