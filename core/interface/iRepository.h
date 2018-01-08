#ifndef __I_REPOSITORY_H__
#define __I_REPOSITORY_H__

#include "iBase.h"

#define I_REPOSITORY	"iRepository"
#define I_EXTENSION	"iExtension"

// ogject request flags
#define RQ_NAME		(1<<0)
#define RQ_INTERFACE	(1<<1)
#define RQ_VERSION	(1<<2)
#define RQ_CMP_OR	(1<<7) // comparsion type OR (AND is by default)

typedef struct {
	_u8 		flags;     // request flags
	_cstr_t		cnme;      // class name
	_cstr_t		iname;     // interface name
	_version_t	version;
}_object_request_t;

class iRepository: public iBase {
public:
	INTERFACE(iRepository, I_REPOSITORY);
	virtual iBase *object_request(_object_request_t *, _rf_t)=0;
	virtual void   object_release(iBase *)=0;
	virtual iBase *object_by_name(_cstr_t name, _rf_t)=0;
	virtual iBase *object_by_interface(_cstr_t iname, _rf_t)=0;
};

extern iRepository *_gpi_repo_;

class iExtension: public iBase {
public:
	INTERFACE(iExtension, I_EXTENSION);
	//...
};

#endif

