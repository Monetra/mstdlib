#include "m_io_mac_common.h"

M_io_error_t M_io_mac_ioreturn_to_err(IOReturn result)
{
    switch (result) {
        case kIOReturnSuccess:
            return M_IO_ERROR_SUCCESS;
        case kIOReturnNoMemory:
        case kIOReturnNoResources:
            return M_IO_ERROR_NOSYSRESOURCES;
        case kIOReturnNoDevice:
        case kIOReturnNotFound:
            return M_IO_ERROR_NOTFOUND;
        case kIOReturnNotPrivileged:
        case kIOReturnNotPermitted:
            return M_IO_ERROR_NOTPERM;
        case kIOReturnBadArgument:
            return M_IO_ERROR_INVALID;
        case kIOReturnLockedRead:
        case kIOReturnLockedWrite:
        case kIOReturnBusy:
            return M_IO_ERROR_WOULDBLOCK;
        case kIOReturnNotOpen:
            return M_IO_ERROR_NOTCONNECTED;
        case kIOReturnTimeout:
            return M_IO_ERROR_TIMEDOUT;
        case kIOReturnAborted:
            return M_IO_ERROR_CONNABORTED;
        default:
            return M_IO_ERROR_ERROR;
    }
    return M_IO_ERROR_ERROR;
}

const char *M_io_mac_ioreturn_errormsg(IOReturn result)
{
    switch (result) {
        case kIOReturnSuccess:
            return "OK";
        case kIOReturnError:
            return "general error";
        case kIOReturnNoMemory:
            return "can't allocate memory";
        case kIOReturnNoResources:
            return "resource shortage";
        case kIOReturnIPCError:
            return "error during IPC";
        case kIOReturnNoDevice:
            return "no such device";
        case kIOReturnNotPrivileged:
            return "privilege violation ";
        case kIOReturnBadArgument:
            return "invalid argument";
        case kIOReturnLockedRead:
            return "device read locked";
        case kIOReturnLockedWrite:
            return "device write locked";
        case kIOReturnExclusiveAccess:
            return "exclusive access and";
        case kIOReturnBadMessageID:
            return "sent/received messages";
        case kIOReturnUnsupported:
            return "unsupported function ";
        case kIOReturnVMError:
            return "misc. VM failure";
        case kIOReturnInternalError:
            return "internal error";
        case kIOReturnIOError:
            return "General I/O error";
        case kIOReturnCannotLock:
            return "can't acquire lock";
        case kIOReturnNotOpen:
            return "device not open ";
        case kIOReturnNotReadable:
            return "read not supported";
        case kIOReturnNotWritable:
            return "write not supported";
        case kIOReturnNotAligned:
            return "alignment error";
        case kIOReturnBadMedia:
            return "Media Error";
        case kIOReturnStillOpen:
            return "device(s) still open ";
        case kIOReturnRLDError:
            return "rld failure";
        case kIOReturnDMAError:
            return "DMA failure";
        case kIOReturnBusy:
            return "Device Busy";
        case kIOReturnTimeout:
            return "I/O Timeout";
        case kIOReturnOffline:
            return "device offline";
        case kIOReturnNotReady:
            return "not ready";
        case kIOReturnNotAttached:
            return "device not attached";
        case kIOReturnNoChannels:
            return "no DMA channels left";
        case kIOReturnNoSpace:
            return "no space for data";
        case kIOReturnPortExists:
            return "port already exists";
        case kIOReturnCannotWire:
            return "can't wire down ";
        case kIOReturnNoInterrupt:
            return "no interrupt attached";
        case kIOReturnNoFrames:
            return "no DMA frames enqueued";
        case kIOReturnMessageTooLarge:
            return "oversized msg received";
        case kIOReturnNotPermitted:
            return "not permitted";
        case kIOReturnNoPower:
            return "no power to device";
        case kIOReturnNoMedia:
            return "media not present";
        case kIOReturnUnformattedMedia:
            return "media not formatted";
        case kIOReturnUnsupportedMode:
            return "no such mode";
        case kIOReturnUnderrun:
            return "data underrun ";
        case kIOReturnOverrun:
            return "data overrun ";
        case kIOReturnDeviceError:
            return "the device is not working properly!";
        case kIOReturnNoCompletion:
            return "a completion routine is required";
        case kIOReturnAborted:
            return "operation aborted";
        case kIOReturnNoBandwidth:
            return "bus bandwidth would be exceeded";
        case kIOReturnNotResponding:
            return "device not responding";
        case kIOReturnIsoTooOld:
            return "isochronous I/O request for distant past!";
        case kIOReturnIsoTooNew:
            return "isochronous I/O request for distant future";
        case kIOReturnNotFound:
            return "data was not found";
        case kIOReturnInvalid:
            return "should never be seen ";
    }

    return "Error";
}

