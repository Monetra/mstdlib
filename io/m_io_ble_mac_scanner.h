/* The MIT License (MIT)
 * 
 * Copyright (c) 2018 Main Street Softworks, Inc.
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

#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>

@interface ScanTrigger: NSObject

+ (id)scanTrigger:(M_event_trigger_t *)trigger timer:(NSTimer *)timer;
- (id)init:(M_event_trigger_t *)trigger timer:(NSTimer *)timer;

@property M_event_trigger_t *trigger;
@property NSTimer           *timer;

@end


@interface M_io_ble_mac_scanner: NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>

+ (id)m_io_ble_mac_scanner;
- (id)init;

- (void)setManager:(CBCentralManager *)manager;
- (void)startScan:(M_event_trigger_t *)trigger timeout_ms:(M_uint64)timeout_ms;
- (void)startScanBlind;
- (void)stopScanBlind;
- (void)scanTimeout:(NSTimer *)timer;
- (BOOL)connectToDevice:(CBPeripheral *)peripheral;
- (void)disconnectFromDevice:(CBPeripheral *)peripheral;
- (BOOL)writeDataToPeripherial:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *)characteristic data:(NSData *)data blind:(BOOL)blind;
- (BOOL)requestDataFromPeripherial:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *)characteristic;

@end
