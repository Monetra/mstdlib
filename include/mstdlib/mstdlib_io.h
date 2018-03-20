#ifndef __MSTDLIB_IO_H__
#define __MSTDLIB_IO_H__

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>

/*! \defgroup m_eventio Event Based I/O Subsystem
 *
 * \code{.c}
 * #include <mstdlib/mstdlib_io.h>
 * \endcode
 * 
 * The I/O system is designed to be a series of stackable
 * layers. For example: Client - Trace <-> TLS <-> Net - Server.
 *
 * Data flown through each layer and each one process the data
 * accordingly. In the above example the trace has the ability to
 * print the data before it's encrypted and put on the wire and after
 * data has been received and decrypted.
 *
 * Layers for the most part can be any order. Some layers are required to be at
 * a certain level. For example, Base layers like Net should always be the
 * bottom most layer. However, you could have Client - TLS <-> Trace <-> Net -
 * Server.  Unlike above the Trace layer would have access to the data after
 * it's encrypted. Or before it's decrypted. Suffice it to say, order can make
 * a huge difference in how data is processed.
 *
 * At a high level the use of an I/O connection should have minimal if no
 * dependency on knowledge of lower layers. Meaning, if you're working with a
 * device that supports multiple connectivity methods there should be no
 * code changes needed for different Base I/O connections. That said some
 * layers, such as Net, expose additional information that could be useful.
 * That said, change from Bluetooth to HID should involve nothing more than
 * changing the create function that sets up the Base layer.
 *
 * The Event subsystem and I/O objects can be used independently of each
 * other. The Event subsystem can be used for general event processing like
 * you'd see in macOS or iOS. The I/O system can be used with a set of
 * synchronous functions negating the need for and Event loop.
 */

/*! \defgroup m_eventio_base Base I/O subsystems
 *  \ingroup m_eventio
 * 
 *  Some I/O layers are considered base layers and typically do not
 *  have any layers underneath them. These are subsystems that handle
 *  communication with an endpoint. While higher layers typically deal
 *  with manipulation of the data going into and out of the base end
 *  point layer. For example, Net is a base layer which handles sending
 *  and receiving data over a network socket. TLS is a layer above that
 *  manipulates (encrypts and decrypts) the data as it goes into and
 *  comes out of the Net layer.
 *
 */

/*! \defgroup m_eventio_base_addon Add-on IO layers
 *  \ingroup m_eventio
 * 
 *  Common layers that stack on base IO objects
 *
 */

/*! \defgroup m_eventio_semipublic Semi-Public interfaces
 *  \ingroup m_eventio
 * 
 *  Interfaces that can be used when extending the I/O subsystem, requires
 *  manual inclusion of additional headers that are not in the normal
 *  public API.
 * 
 *  These APIs are more likely to change from release to release and should
 *  not be considered as stable as the public interfaces.
 *
 */

#include <mstdlib/io/m_io.h>
#include <mstdlib/io/m_dns.h>
#include <mstdlib/io/m_io_net.h>
#include <mstdlib/io/m_io_pipe.h>
#include <mstdlib/io/m_event.h>
#include <mstdlib/io/m_io_ble.h>
#include <mstdlib/io/m_io_block.h>
#include <mstdlib/io/m_io_bluetooth.h>
#include <mstdlib/io/m_io_bwshaping.h>
#include <mstdlib/io/m_io_loopback.h>
#include <mstdlib/io/m_io_serial.h>
#include <mstdlib/io/m_io_hid.h>
#include <mstdlib/io/m_io_mfi.h>
#include <mstdlib/io/m_io_trace.h>

#endif /* __MSTDLIB_IO_H__ */
