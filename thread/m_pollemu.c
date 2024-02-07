#include "m_config.h"

#include <mstdlib/mstdlib_thread.h>
#include "m_thread_int.h"
#include "m_pollemu.h"

int M_pollemu(struct pollfd *fds, nfds_t nfds, int timeout)
{
#ifdef _WIN32
    SOCKET         maxfd = 0;
#else
    int            maxfd = 0;
#endif
    int            rv;
    fd_set         readfds;
    fd_set         writefds;
    fd_set         exceptfds;
    nfds_t         i;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    for (i=0; i<nfds; i++) {
        /* Clear output events */
        fds[i].revents = 0;

        if (fds[i].fd > maxfd)
            maxfd = fds[i].fd;

        if (fds[i].events & POLLIN)
            FD_SET(fds[i].fd, &readfds);
        if (fds[i].events & POLLOUT)
            FD_SET(fds[i].fd, &writefds);
        FD_SET(fds[i].fd, &exceptfds);
    }

    if (timeout >= 0) {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
    }

    rv = select((int)maxfd+1, &readfds, &writefds, &exceptfds, (timeout < 0)?NULL:&tv);

    /* Return value is the same as poll uses */
    if (rv <= 0)
        return rv;

    rv = 0;
    for (i=0; i<nfds; i++) {
        if (FD_ISSET(fds[i].fd, &readfds)) {
            fds[i].revents |= POLLIN;
        }
        if (FD_ISSET(fds[i].fd, &writefds)) {
            fds[i].revents |= POLLOUT;
        }
        if (FD_ISSET(fds[i].fd, &exceptfds)) {
            fds[i].revents |= POLLERR;
        }
        if (fds[i].revents)
            rv++;
    }

    return rv;
}
