#include <string.h>
#include "startup.h"
#include "iMemory.h"
#include "iLog.h"
#include "iArgs.h"
#include "iTaskMaker.h"

#ifdef _CORE_
_EXPORT_ iRepository *_gpi_repo_ = 0;
_EXPORT_ iRepository *get_repository(void) {
#else
_LOCAL_ iRepository *_gpi_repo_ = 0;
_LOCAL_ iRepository *get_repository(void) {
#endif
	return _gpi_repo_;
}

#ifdef _CORE_
void _EXPORT_ register_object(iBase *pi_base) {
#else
void _LOCAL_ register_object(iBase *pi_base) {
#endif
	_base_entry_t e={pi_base, 0, 0};
	add_base_entry(&e);
}

#ifdef _CORE_
typedef struct {
	_cstr_t iname;
	_base_entry_t  *p_entry;
}_early_init_t;

_early_init_t ei[]= {
	{I_REPOSITORY,	0}, //0
	{I_HEAP,	0}, //1
	{I_ARGS,	0}, //2
	{I_LOG,		0}, //3
	{I_TASK_MAKER,	0}, //4
	{0,		0}
};

_err_t _EXPORT_ init(int argc, char *argv[]) {
#else
_err_t _EXPORT_ init(iRepository *pi_repo) {
	_gpi_repo_ = pi_repo;
#endif
	_err_t r = ERR_UNKNOWN;
	_u32 i = 0;
	_u32 count, limit;
	_base_entry_t *array = get_base_array(&count, &limit);

#ifdef _CORE_
	while(array && i < count) {
		_object_info_t oinfo;
		if(array[i].pi_base) {
			array[i].pi_base->object_info(&oinfo);
			for(_u32 j = 0; ei[j].iname; j++) {
				if(strcmp(oinfo.iname, ei[j].iname) == 0) {
					ei[j].p_entry = &array[i];
					break;
				}
			}
		}
		i++;
	}

	if(ei[0].p_entry && (_gpi_repo_ = (iRepository*)ei[0].p_entry->pi_base)) {
		// the repo is here !
		iHeap *pi_heap=0;
		if(ei[1].p_entry && (pi_heap = (iHeap*)ei[1].p_entry->pi_base)) {
			// the heap is here !
			if(!pi_heap->object_ctl(OCTL_INIT, _gpi_repo_))
				goto _init_done_;
			ei[1].p_entry->ref_cnt++;
			ei[1].p_entry->state |= ST_INITIALIZED;
			// init repository object
			if(!_gpi_repo_->object_ctl(OCTL_INIT, 0))
				goto _init_done_;
			ei[0].p_entry->ref_cnt++;
			ei[0].p_entry->state |= ST_INITIALIZED;
			// init args
			iArgs *pi_args = 0;
			if(ei[2].p_entry && (pi_args = (iArgs*)ei[2].p_entry->pi_base)) {
				// args is here
				if(pi_args->object_ctl(OCTL_INIT, _gpi_repo_)) {
					ei[2].p_entry->state |= ST_INITIALIZED;
					ei[2].p_entry->ref_cnt++;
					pi_args->init(argc, argv);
				}
			}
			// init log
			iLog *pi_log = 0;
			if(ei[3].p_entry && (pi_log = (iLog*)ei[3].p_entry->pi_base)) {
				// log is here
				if(pi_log->object_ctl(OCTL_INIT, _gpi_repo_)) {
					ei[3].p_entry->state |= ST_INITIALIZED;
					ei[3].p_entry->ref_cnt++;
				}
			}
			iTaskMaker *pi_tmaker = 0;
			if(ei[4].p_entry && (pi_tmaker = (iTaskMaker*)ei[4].p_entry->pi_base)) {
				if(pi_tmaker->object_ctl(OCTL_INIT, _gpi_repo_)) {
					ei[4].p_entry->state |= ST_INITIALIZED;
					ei[4].p_entry->ref_cnt++;
				}
			}
		}
	}
#endif
	if(_gpi_repo_) {
		// init everyone else
		iTaskMaker *pi_tmaker = (iTaskMaker *)_gpi_repo_->object_by_iname(I_TASK_MAKER, RF_ORIGINAL);
		i = 0;

		while(array && i < count) {
			_base_entry_t *pbe = &array[i];

			if(pbe->pi_base && !(pbe->state & ST_INITIALIZED)) {
				_object_info_t oi;

				pbe->pi_base->object_info(&oi);
				if(oi.flags & RF_ORIGINAL) {
					// init here ORIGINAL flagged objects only
					if(pbe->pi_base->object_ctl(OCTL_INIT, _gpi_repo_)) {
						pbe->state |= ST_INITIALIZED;
						if(oi.flags & RF_TASK) {
							// start task
							if(pi_tmaker)
								pi_tmaker->start(pbe->pi_base);
						}
					}
				}
			}
			i++;
		}

		_gpi_repo_->object_release(pi_tmaker);
		r = ERR_NONE;
	}
#ifdef _CORE_
_init_done_:
#endif
	return r;
}