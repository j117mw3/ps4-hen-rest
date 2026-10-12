#include "native_method.h"
#include "probe_common.h"
#include <string.h>
#include <orbis/libkernel.h>

/* The Mono-resolved method and crash module map on firmware 13.520.001
 * identify this RVA in app.exe.sprx. Never load a module or enter Mono here:
 * ShellUI can still be initializing its AOT runtime during restoration.
 * Require the exact image fingerprint and instruction digest before patching.
 * These are identifiers/checksums, not redistributed system code. */
enum { METHOD_RVA = 0xbc27f0, CHECK_BYTES = 256, MAX_HANDLE = 65536 };
static const unsigned char image_fingerprint[20] = {
    0x7c, 0x2e, 0xbe, 0x83, 0xad, 0xf8, 0x5b, 0xa6, 0x15, 0x80,
    0xc4, 0xf3, 0x9d, 0x13, 0xcc, 0xda, 0, 0, 0, 0
};

void *probe_find_native_power_method(void)
{
    /* Module-list APIs omit some system dependencies. Read metadata for the
     * bounded handle range directly; invalid handles simply fail the query.
     * No sys_dynlib_load_prx fallback is permitted. */
    for (unsigned handle = 0; handle < MAX_HANDLE; ++handle) {
        OrbisKernelModuleInfo info;
        memset(&info, 0, sizeof(info));
        info.size = sizeof(info);
        if (sceKernelGetModuleInfo(handle, &info) != 0 ||
            memcmp(info.fingerprint, image_fingerprint, sizeof(image_fingerprint)) != 0 ||
            info.segmentCount == 0 || info.segmentCount > 4) continue;
        for (unsigned i = 0; i < info.segmentCount; ++i) {
            OrbisKernelModuleSegmentInfo segment = info.segmentInfo[i];
            if ((segment.prot & 5) != 5 || !segment.address ||
                segment.size < METHOD_RVA + CHECK_BYTES) continue;
            const unsigned char *method = (const unsigned char *) segment.address + METHOD_RVA;
            uint64_t digest = UINT64_C(14695981039346656037);
            for (unsigned j = 0; j < CHECK_BYTES; ++j)
                digest = (digest ^ method[j]) * UINT64_C(1099511628211);
            probe_log("SHELL", "NATIVE_LOOKUP handle=%u name=%.256s text=%p target=%p digest=%llx",
                      handle, info.name, segment.address, (const void *) method,
                      (unsigned long long) digest);
            if (digest != UINT64_C(0xd4fbb82532360dff)) {
                probe_log("SHELL", "ABORT reason=native_instruction_digest");
                return NULL;
            }
            return (void *) method;
        }
    }
    return NULL;
}
