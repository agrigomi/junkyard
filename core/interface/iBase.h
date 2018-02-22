#ifndef __I_BASE_H__
#define __I_BASE_H__

#include "dtype.h"

#define I_BASE	"iBase"

#define INTERFACE(_class_, iname) \
	_class_() {} \
	virtual ~_class_() {} \
	virtual _cstr_t interface_name(void) 	{ return iname; }

typedef union {
	_u32	version;
	struct {
		_u32	revision:16;
		_u32	minor	:8;
		_u32	major	:8;
	};
}_version_t;

typedef _u8 	_rf_t;

// flags
#define RF_CLONE     (1<<0)
#define RF_ORIGINAL  (1<<1)
#define RF_TASK      (1<<2)


typedef struct {
	_cstr_t		iname;   // interface name
	_cstr_t		cname;   // class name
	_u32		size;    // object size
	_version_t	version; // object version
	_rf_t		flags;   // repository flags
}_object_info_t;

class iBase {
public:
	INTERFACE(iBase, I_BASE);
	virtual void object_info(_object_info_t *pi)=0;
	virtual bool object_ctl(_u32 cmd, void *arg)=0;
};

// notification flags
#define NF_INIT		(1<<0)
#define NF_START	(1<<1)
#define NF_STOP		(1<<2)
#define NF_UNINIT	(1<<3)
#define NF_REMOVE	(1<<4)

typedef struct {
	_u32	flags;
	iBase	*object;
}_notification_t;

// object controll
#define OCTL_INIT	10 // arg: iRepository*
#define OCTL_UNINIT	11 // arg: iRepository*
#define OCTL_START	12 // arg: void*
#define OCTL_STOP	13
#define OCTL_NOTIFY	14 // arg: _notification_t*

#define CONSTRUCTOR(_class_) \
	_class_()

#define INFO(_class_, name, f, a, i, r) \
	void object_info(_object_info_t *pi) { \
		pi->iname = interface_name(); \
		pi->cname = name; \
		pi->flags = f; \
		pi->size = sizeof(_class_); \
		pi->version.major = a; \
		pi->version.minor = i; \
		pi->version.revision = r; \
	}

#define DESTRUCTOR(_class_) \
	virtual ~_class_()

#define BASE(_class_, name, flags, a, i, r) \
	CONSTRUCTOR(_class_) { \
		register_object(dynamic_cast<iBase *>(this)); \
	} \
	DESTRUCTOR(_class_) {} \
	INFO(_class_, name, flags, a, i, r)


extern void register_object(iBase *);

#endif

