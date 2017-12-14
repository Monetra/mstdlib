/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __M_IO_BLUETOOTH_MAC_RFCOMM_H__
#define __M_IO_BLUETOOTH_MAC_RFCOMM_H__

#import <Foundation/Foundation.h>

@interface M_io_bluetooth_mac_rfcomm : NSObject <IOBluetoothRFCOMMChannelDelegate>

/* Initializer */
+ (id)m_io_bluetooth_mac_rfcomm:(NSString *)mac uuid:(NSString *)uuid handle:(M_io_handle_t *)handle;

/* Stardard init/dealloc functions */
- (id)init:(NSString *)mac uuid:(NSString *)uuid handle:(M_io_handle_t *)handle;
- (void)dealloc;

/* Start connecting */
- (BOOL)connect;

/* Initiate a close */
- (void)close;

- (void)write_data_buffered;

@end

#endif
