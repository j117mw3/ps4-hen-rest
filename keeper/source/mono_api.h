#pragma once
typedef struct _MonoDomain MonoDomain;
typedef struct _MonoThread MonoThread;
typedef struct _MonoAssembly MonoAssembly;
typedef struct _MonoImage MonoImage;
typedef struct _MonoClass MonoClass;
typedef struct _MonoMethod MonoMethod;
extern MonoDomain *(*mono_get_root_domain)(void);
extern MonoThread *(*mono_thread_attach)(MonoDomain *);
extern void (*mono_thread_detach)(MonoThread *);
extern MonoAssembly *(*mono_domain_assembly_open)(MonoDomain *, const char *);
extern MonoImage *(*mono_assembly_get_image)(MonoAssembly *);
extern MonoClass *(*mono_class_from_name)(MonoImage *, const char *, const char *);
extern MonoMethod *(*mono_class_get_method_from_name)(MonoClass *, const char *, int);
extern void *(*mono_aot_get_method)(MonoDomain *, MonoMethod *);
int probe_resolve_mono(void);
