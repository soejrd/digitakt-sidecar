/* Read-only inspection of a connected Elektron Digitakt in Overbridge mode.
 *
 * It reads IORegistry properties and asks the IOKit USB device interface for
 * the full configuration descriptor. It does not open or claim an interface,
 * select an alternate setting, or start any endpoint. No serial is printed.
 *
 * Build: cc -O2 -framework IOKit -framework CoreFoundation \
 *          tools/dt_usb_inspect.c -o /tmp/dt_usb_inspect
 * Run:   /tmp/dt_usb_inspect
 */
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ELEKTRON_VID 0x1935
#define DIGITAKT_OB_PID 0x0B2C

static void print_prop(CFDictionaryRef d, const char *key) {
    CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
    CFTypeRef v = CFDictionaryGetValue(d, k);
    printf("  %s: ", key);
    if (!v) printf("-\n");
    else if (CFGetTypeID(v) == CFNumberGetTypeID()) {
        long long n = 0; CFNumberGetValue((CFNumberRef)v, kCFNumberLongLongType, &n);
        printf("%lld\n", n);
    } else if (CFGetTypeID(v) == CFStringGetTypeID()) {
        char buf[256]; if (!CFStringGetCString((CFStringRef)v, buf, sizeof buf, kCFStringEncodingUTF8)) strcpy(buf, "?");
        printf("%s\n", buf);
    } else { CFStringRef s = CFCopyDescription(v); char buf[256]; if (!CFStringGetCString(s, buf, sizeof buf, kCFStringEncodingUTF8)) strcpy(buf, "?"); printf("%s\n", buf); CFRelease(s); }
    CFRelease(k);
}

static io_service_t find_device(void) {
    CFMutableDictionaryRef matching = IOServiceMatching("IOUSBHostDevice");
    if (!matching) return 0;
    io_iterator_t it = 0;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &it) != KERN_SUCCESS) return 0;
    io_service_t found = 0, s;
    while ((s = IOIteratorNext(it))) {
        CFTypeRef v = IORegistryEntryCreateCFProperty(s, CFSTR("idVendor"), NULL, 0);
        CFTypeRef p = IORegistryEntryCreateCFProperty(s, CFSTR("idProduct"), NULL, 0);
        long long vid = 0, pid = 0;
        if (v) { CFNumberGetValue(v, kCFNumberLongLongType, &vid); CFRelease(v); }
        if (p) { CFNumberGetValue(p, kCFNumberLongLongType, &pid); CFRelease(p); }
        if (vid == ELEKTRON_VID && pid == DIGITAKT_OB_PID && !found) found = s;
        else IOObjectRelease(s);
    }
    IOObjectRelease(it);
    return found;
}

static void print_registry(io_service_t service) {
    CFMutableDictionaryRef props = NULL;
    if (IORegistryEntryCreateCFProperties(service, &props, NULL, 0) != KERN_SUCCESS || !props) return;
    const char *keys[] = {"USB Product Name", "USB Vendor Name", "idVendor", "idProduct",
        "bcdUSB", "bcdDevice", "bDeviceClass", "bDeviceSubClass", "bDeviceProtocol",
        "bNumConfigurations", "kUSBCurrentConfiguration", "Device Speed", "USBSpeed",
        "bMaxPacketSize0"};
    printf("device registry:\n");
    for (size_t i = 0; i < sizeof keys / sizeof *keys; ++i) print_prop(props, keys[i]);
    CFRelease(props);

    io_iterator_t child = 0;
    if (IORegistryEntryGetChildIterator(service, kIOServicePlane, &child) == KERN_SUCCESS) {
        io_service_t iface;
        while ((iface = IOIteratorNext(child))) {
            CFMutableDictionaryRef ip = NULL;
            if (IORegistryEntryCreateCFProperties(iface, &ip, NULL, 0) == KERN_SUCCESS && ip) {
                if (CFDictionaryGetValue(ip, CFSTR("bInterfaceNumber"))) {
                    printf("interface: "); print_prop(ip, "bInterfaceNumber");
                    printf("  alt: "); print_prop(ip, "bAlternateSetting");
                    printf("  class: "); print_prop(ip, "bInterfaceClass");
                    printf("  subclass: "); print_prop(ip, "bInterfaceSubClass");
                    printf("  protocol: "); print_prop(ip, "bInterfaceProtocol");
                    printf("  endpoints: "); print_prop(ip, "bNumEndpoints");
                }
                CFRelease(ip);
            }
            IOObjectRelease(iface);
        }
        IOObjectRelease(child);
    }
}

static void hexdump(const unsigned char *b, size_t n) {
    for (size_t i = 0; i < n; i += 16) {
        printf("  %04zx: ", i);
        for (size_t j = i; j < n && j < i + 16; ++j) printf("%02x ", b[j]);
        printf("\n");
    }
}

static void print_descriptor(io_service_t service) {
    IOCFPlugInInterface **plugin = NULL;
    SInt32 score = 0;
    IOReturn kr = IOCreatePlugInInterfaceForService(service, kIOUSBDeviceUserClientTypeID,
                                                    kIOCFPlugInInterfaceID, &plugin, &score);
    if (kr != kIOReturnSuccess || !plugin) { printf("descriptor: plugin failed 0x%x\n", kr); return; }
    IOUSBDeviceInterface **dev = NULL;
    HRESULT hr = (*plugin)->QueryInterface(plugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
                                           (LPVOID *)&dev);
    if (hr || !dev) { printf("descriptor: QueryInterface failed 0x%x\n", hr); (*plugin)->Release(plugin); return; }
    IOUSBConfigurationDescriptorPtr desc = NULL;
    kr = (*dev)->GetConfigurationDescriptorPtr(dev, 0, &desc);
    if (kr != kIOReturnSuccess || !desc) {
        printf("descriptor: GetConfigurationDescriptorPtr failed 0x%x\n", kr);
    } else {
        unsigned total = ((const IOUSBConfigurationDescriptor *)desc)->wTotalLength;
        if (total >= 9 && total < 65536) {
            printf("raw configuration descriptor (%u bytes):\n", total);
            hexdump((const unsigned char *)desc, total);
        } else printf("descriptor: implausible wTotalLength %u\n", total);
    }
    (*dev)->Release(dev);
    (*plugin)->Release(plugin);
}

int main(void) {
    io_service_t service = find_device();
    if (!service) { fprintf(stderr, "No Elektron Digitakt Overbridge device (1935:0b2c) visible.\n"); return 2; }
    print_registry(service);
    print_descriptor(service);
    IOObjectRelease(service);
    return 0;
}
