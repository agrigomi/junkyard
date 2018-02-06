#include <string.h>
#include "startup.h"
#include "iMemory.h"

// global pointer to repository
_LOCAL_ iRepository *_gpi_repo_ = 0;

static _base_vector_t _g_base_vector_;

#ifdef _CORE_
void _EXPORT_ register_object(iBase *pi_base) {
#else
void _LOCAL_ register_object(iBase *pi_base) {
#endif
	_base_entry_t be = {pi_base, 0, 0};
	_g_base_vector_.push_back(be);
}

typedef struct {
	_cstr_t iname;
	_base_entry_t  *p_entry;
}_early_init_t;

_err_t _EXPORT_ init(iRepository *pi_repo) {
	_err_t r = ERR_UNKNOWN;
	_gpi_repo_ = pi_repo;
#ifdef _CORE_
	_early_init_t ei[]= {
		{I_REPOSITORY,	0},
		{I_HEAP,	0},
		{0,		0}
	};

	_base_vector_t::iterator i = _g_base_vector_.begin();
	while(i != _g_base_vector_.end()) {
		_object_info_t oinfo;
		if(i->pi_base) {
			i->pi_base->object_info(&oinfo);
			for(_u32 j = 0; ei[j].iname; j++) {
				if(strcmp(oinfo.iname, ei[j].iname) == 0) {
					ei[j].p_entry = &(*i);
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
			ei[1].p_entry->state |= ST_INITIALIZED;
			// init repository object
			if(!_gpi_repo_->object_ctl(OCTL_INIT, 0))
				goto _init_done_;
			ei[0].p_entry->state |= ST_INITIALIZED;
		}
	}
#endif
	if(_gpi_repo_) {
		// init everyone else
		i = _g_base_vector_.begin();
		while(i != _g_base_vector_.end()) {
			_base_entry_t *pbe = &(*i);
			if(pbe->pi_base && !(pbe->state & ST_INITIALIZED)) {
				_object_info_t oi;
				pbe->pi_base->object_info(&oi);
				if(oi.flags & RF_ORIGINAL) {
					// init here ORIGINAL flagged objects only
					if(pbe->pi_base->object_ctl(OCTL_INIT, _gpi_repo_))
						pbe->state |= ST_INITIALIZED;
				}
			}
			i++;
		}
		r = ERR_NONE;
	}

_init_done_:
	return r;
}

_base_vector_t _EXPORT_ *get_base_vector(void) {
	return &_g_base_vector_;
}
