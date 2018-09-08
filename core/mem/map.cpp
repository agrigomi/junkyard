#include "iRepository.h"
#include "iMemory.h"
#include "sha1.h"
#include "map_alg.h"

#define INITIAL_CAPACITY	256

static void *_alloc(_u32 size, void *udata);
static void _free(void *ptr, _u32 size, void *udata);
static void _hash(void *data, _u32 sz_data, _u8 *result, void *udata);

class cMap: public iMap {
private:
	iHeap *mpi_heap;
	iMutex *mpi_mutex;
	_map_context_t map_cxt;
	SHA1Context sha1_cxt;

	friend void _free(void *ptr, _u32 size, void *udata);
	friend void *_alloc(_u32 size, void *udata);
	friend void _hash(void *data, _u32 sz_data, _u8 *result, void *udata);
public:
	BASE(cMap, "cMap", RF_CLONE, 1,0,0);

	bool object_ctl(_u32 cmd, void *arg, ...) {
		bool r = false;

		switch(cmd) {
			case OCTL_INIT: {
				iRepository *pi_repo = (iRepository *)arg;

				mpi_mutex = (iMutex *)pi_repo->object_by_iname(I_MUTEX, RF_CLONE);
				mpi_heap = (iHeap *)pi_repo->object_by_iname(I_HEAP, RF_CLONE);

				map_cxt.records = map_cxt.collisions = 0;
				map_cxt.capacity = INITIAL_CAPACITY;
				map_cxt.pf_mem_alloc = _alloc;
				map_cxt.pf_mem_free = _free;
				map_cxt.pf_hash = _hash;
				map_cxt.pp_list = 0;
				map_cxt.udata = this;

				map_init(&map_cxt);
				r = true;
			} break;
			case OCTL_UNINIT: {
				iRepository *pi_repo = (iRepository *)arg;

				uninit();
				pi_repo->object_release(mpi_heap);
				pi_repo->object_release(mpi_mutex);
				r = true;
			} break;
		}

		return r;
	}

	void uninit(void) {
		map_destroy(&map_cxt);
	}

	HMUTEX lock(HMUTEX hlock=0) {
		HMUTEX r = 0;

		if(mpi_mutex)
			r = mpi_mutex->lock(hlock);

		return r;
	}

	void unlock(HMUTEX hlock) {
		if(mpi_mutex)
			mpi_mutex->unlock(hlock);
	}

	void *add(void *key, _u32 sz_key, void *data, _u32 sz_data, HMUTEX hlock=0) {
		void *r = 0;
		HMUTEX hm = lock(hlock);

		r = map_add(&map_cxt, key, sz_key, data, sz_data);
		unlock(hm);

		return r;
	}

	void del(void *key, _u32 sz_key, HMUTEX hlock=0) {
		HMUTEX hm = lock(hlock);

		map_del(&map_cxt, key, sz_key);
		unlock(hm);
	}

	_u32 cnt(void) {
		return map_cxt.records;
	}

	void *get(void *key, _u32 sz_key, _u32 *sz_data, HMUTEX hlock=0) {
		void *r = 0;
		HMUTEX hm = lock(hlock);

		r = map_get(&map_cxt, key, sz_key, sz_data);
		unlock(hm);

		return r;
	}

	void clr(HMUTEX hlock=0) {
		HMUTEX hm = lock(hlock);

		map_clr(&map_cxt);
		unlock(hm);
	}
};

static cMap _g_map_;

static void *_alloc(_u32 size, void *udata) {
	void *r = 0;
	cMap *pobj = (cMap *)udata;
	if(pobj)
		r = pobj->mpi_heap->alloc(size);
	return r;
}

static void _free(void *ptr, _u32 size, void *udata) {
	cMap *pobj = (cMap *)udata;
	if(pobj)
		pobj->mpi_heap->free(ptr, size);
}

static void _hash(void *data, _u32 sz_data, _u8 *result, void *udata) {
	cMap *pobj = (cMap *)udata;
	if(pobj) {
		SHA1Reset(&(_g_map_.sha1_cxt));
		SHA1Input(&(_g_map_.sha1_cxt), (_u8 *)data, sz_data);
		SHA1Result(&(_g_map_.sha1_cxt), result);
	}
}