#include "mono_api.h"
#include "probe_common.h"
#include <string.h>
#include <orbis/libkernel.h>
#include <Syscall.h>

MonoDomain *(*mono_get_root_domain)(void);
MonoThread *(*mono_thread_attach)(MonoDomain *);
void (*mono_thread_detach)(MonoThread *);
MonoAssembly *(*mono_domain_assembly_open)(MonoDomain *, const char *);
MonoImage *(*mono_assembly_get_image)(MonoAssembly *);
MonoClass *(*mono_class_from_name)(MonoImage *, const char *, const char *);
MonoMethod *(*mono_class_get_method_from_name)(MonoClass *, const char *, int);
void *(*mono_aot_get_method)(MonoDomain *, MonoMethod *);

int probe_resolve_mono(void)
{
    OrbisKernelModule handles[256];
    size_t available = 0;
    int result = sceKernelGetModuleList(handles, sizeof(handles), &available);
    probe_log("SHELL", "MODULE_LIST status=%d count=%zu", result, available);
    if (result != 0) available = 0;
    if (available > sizeof(handles) / sizeof(handles[0]))
        available = sizeof(handles) / sizeof(handles[0]);
    int handle = -1;
    for (size_t i = 0; i < available; ++i) {
        OrbisKernelModuleInfo info;
        memset(&info, 0, sizeof(info));
        info.size = sizeof(info);
        result = sceKernelGetModuleInfo(handles[i], &info);
        probe_log("SHELL", "MODULE handle=%u status=%d name=%.256s", handles[i], result, info.name);
        if (result == 0 && strstr(info.name, "monosgen")) {
            handle = handles[i];
            probe_log("SHELL", "MONO_MODULE handle=%d name=%.256s", handle, info.name);
            break;
        }
    }
    if (handle < 0) {
        /* System modules may be absent from the public module enumeration.
         * Syscall 594 also returns the handle for an existing named module;
         * this does not invoke its module_start or managed methods. */
        int candidate = -1;
        result = sys_dynlib_load_prx("libmonosgen-2.0.sprx", &candidate);
        probe_log("SHELL", "MONO_DIRECT name=libmonosgen-2.0.sprx status=%d handle=%d", result, candidate);
        if (result == 0 && candidate >= 0) handle = candidate;
    }
    if (handle < 0) {
        probe_log("SHELL", "ABORT reason=mono_module_not_found");
        return 0;
    }
    int ok = 1;
#define RESOLVE(name) do { \
    void *address = NULL; \
    int status = sceKernelDlsym(handle, #name, &address); \
    _Static_assert(sizeof(name) == sizeof(address), "Pointer size mismatch"); \
    memcpy(&name, &address, sizeof(name)); \
    probe_log("SHELL", "MONO_SYMBOL name=%s status=%d address=%p", #name, status, address); \
    if (status != 0 || !address) ok = 0; \
} while (0)
    RESOLVE(mono_get_root_domain);
    RESOLVE(mono_thread_attach);
    RESOLVE(mono_thread_detach);
    RESOLVE(mono_domain_assembly_open);
    RESOLVE(mono_assembly_get_image);
    RESOLVE(mono_class_from_name);
    RESOLVE(mono_class_get_method_from_name);
    RESOLVE(mono_aot_get_method);
#undef RESOLVE
    return ok;
}
