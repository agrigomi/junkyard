#include <string.h>
#include "iHttpHost.h"
#include "iRepository.h"
#include "iNet.h"
#include "iFS.h"
#include "iLog.h"
#include "iMemory.h"

#define DEFAULT_HTTP_PORT	8080
#define MAX_SERVER_NAME		32
#define MAX_DOC_ROOT		256

typedef struct { // server record
	_char_t		name[MAX_SERVER_NAME];
	_u32		port;
	_char_t		doc_root[MAX_DOC_ROOT];
	iHttpServer	*pi_http_server;
}_server_t;

class cHttpHost: public iHttpHost {
private:
	iNet		*mpi_net;
	iFileCache	*mpi_fcache;
	iLog		*mpi_log;
	iMap		*mpi_map;

	void release_object(iRepository *pi_repo, iBase **pp_obj) {
		if(*pp_obj) {
			pi_repo->object_release(*pp_obj);
			*pp_obj = NULL;
		}
	}
public:
	BASE(cHttpHost, "cHttpHost", RF_ORIGINAL, 1,0,0);

	bool object_ctl(_u32 cmd, void *arg, ...) {
		bool r = false;

		switch(cmd) {
			case OCTL_INIT: {
				iRepository *pi_repo = (iRepository *)arg;

				mpi_net = NULL;
				mpi_fcache = NULL;

				pi_repo->monitoring_add(NULL, I_NET, NULL, this);
				pi_repo->monitoring_add(NULL, I_FS, NULL, this);

				mpi_log = (iLog *)pi_repo->object_by_iname(I_LOG, RF_ORIGINAL);
				mpi_map = (iMap *)pi_repo->object_by_iname(I_MAP, RF_CLONE);
				if(mpi_log && mpi_map)
					r = true;
			} break;
			case OCTL_UNINIT: {
				iRepository *pi_repo = (iRepository *)arg;

				release_object(pi_repo, (iBase **)&mpi_net);
				release_object(pi_repo, (iBase **)&mpi_fcache);
				release_object(pi_repo, (iBase **)&mpi_map);
				release_object(pi_repo, (iBase **)&mpi_log);
				r = true;
			} break;
			case OCTL_NOTIFY: {
				_notification_t *pn = (_notification_t *)arg;
				_object_info_t oi;

				memset(&oi, 0, sizeof(_object_info_t));
				if(pn->object) {
					pn->object->object_info(&oi);

					if(pn->flags & NF_INIT) { // catch
						//...
					} else if(pn->flags & (NF_UNINIT | NF_REMOVE)) { // release
						//...
					}
				}
			} break;
		}

		return r;
	}

	bool create_http_server(_cstr_t name, _u32 port, _cstr_t doc_root) {
		bool r = false;

		if(mpi_net) {
			//...
		}

		return r;
	}
	//...
};

static cHttpHost _g_http_host_;
