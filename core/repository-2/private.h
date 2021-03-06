#ifndef __REPOSITORY_PRIVATE_H__
#define __REPOSITORY_PRIVATE_H__

#include <mutex>
#include <vector>
#include <unordered_set>
#include "dtype.h"
#include "map_alg.h"
#include "ll_alg.h"
#include "sha1.h"
#include "iRepository.h"
#include "err.h"

typedef struct 	mutex 		_mutex_t;
typedef _u64			_mutex_handle_t;
typedef struct hash_map 	_map_t;
typedef struct linked_list	_list_t;
typedef struct extension 	_extension_t;

struct mutex {
private:
	std::mutex	m_mutex;
	_u32		m_lcount; // lock count
	_u32		m_hcount; // handle count

	_u64 get_handle(void);

public:
	mutex();
	~mutex();
	_mutex_handle_t lock(_mutex_handle_t h=0);
	_mutex_handle_t try_lock(_mutex_handle_t h);
	void unlock(_mutex_handle_t h);
};

// Zone allocator
bool zinit(void);
bool is_zinit(void);
void *zalloc(_u32 size);
void zfree(void *ptr, _u32 size);
void zdestroy(void);
bool zverify(void *ptr, _u32 size);

template <typename T>
class zAllocator {
public:
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef const void* const_void_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T value_type;

	zAllocator(){
		zinit();
	}
	~zAllocator(){}

	template <class U> struct rebind { typedef zAllocator<U> other; };
	template <class U> zAllocator(const zAllocator<U>&){}
	pointer address(reference x) const {return &x;}
	const_pointer address(const_reference x) const {
		return &x;
	}
	size_type max_size() const throw() {
		return size_t(-1) / sizeof(value_type);
	}
	pointer allocate(size_type n, const_void_pointer hint = 0) {
		return static_cast<pointer>(zalloc(n * sizeof(T)));
	}
	void deallocate(pointer p, size_type n) {
		zfree(p, n * sizeof(T));
	}
	void construct(pointer p, const T& val) {
		new (static_cast<void*>(p))T(val);
	}
	void construct(pointer p) {
		new (static_cast<void*>(p))T();
	}
	void destroy(pointer p) {
		p->~T();
	}
};

// Map
struct hash_map {
private:
	_map_context_t 	m_context;
	_mutex_t	m_mutex;
	SHA1Context	m_sha1;
public:
	hash_map();
	~hash_map();

	_mutex_handle_t lock(_mutex_handle_t hlock=0);
	void unlock(_mutex_handle_t hlock);

	void destroy(_mutex_handle_t hlock=0);

	void *add(void *key, _u32 sz_key, void *data, _u32 sz_data, _mutex_handle_t hlock=0);
	void *set(void *key, _u32 sz_key, void *data, _u32 sz_data, _mutex_handle_t hlock=0);
	void *get(void *key, _u32 sz_key, _u32 *sz_data, _mutex_handle_t hlock=0);
	void  del(void *key, _u32 sz_key, _mutex_handle_t hlock=0);
	void  clr(_mutex_handle_t hlock=0);
	void  enm(_s32 (*cb)(void *, _u32, void *), void *udata, _mutex_handle_t hlock=0);
};

struct linked_list {
private:
	_mutex_t	m_mutex;
	_ll_context_t	m_context;

public:
	linked_list();
	~linked_list();

	_mutex_handle_t lock(_mutex_handle_t hlock=0);
	void unlock(_mutex_handle_t hlock);

	void *add(void *ptr, _u32 size, _mutex_handle_t hlock=0);
	void del(_mutex_handle_t hlock=0);
	void col(_u8 col, _mutex_handle_t hlock=0);
	void clr(_mutex_handle_t hlock=0);
	_u32 cnt(_mutex_handle_t hlock=0);
	bool sel(void *rec, _mutex_handle_t hlock=0);
	bool mov(void *rec, _u8 col, _mutex_handle_t hlock=0);
	void *first(_u32 *p_size, _mutex_handle_t hlock=0);
	void *next(_u32 *p_size, _mutex_handle_t hlock=0);
	void *current(_u32 *p_size, _mutex_handle_t hlock=0);
	void destroy(void);
};

// Extensions
#define MAX_ALIAS_LEN	64
#define MAX_FILE_PATH	256

typedef void*	_dl_handle_t;
typedef _base_entry_t *_get_base_array_t(_u32 *count, _u32 *limit);
typedef _err_t _init_t(iRepository *);

struct extension {
private:
	_char_t m_alias[MAX_ALIAS_LEN];
	_char_t m_file[MAX_FILE_PATH];
	_dl_handle_t m_handle;
	_get_base_array_t *m_get_base_array;
	_init_t *m_init;
public:
	extension();

	_cstr_t alias(void) {
		return m_alias;
	}

	_cstr_t file(void) {
		return m_file;
	}

	_err_t load(_cstr_t file, _cstr_t alias);
	_err_t unload(void);
	_base_entry_t *array(_u32 *count, _u32 *limit);
	_err_t init(iRepository *pi_repo);
};

_mutex_handle_t lock_extensions(void);
void unlock_extensions(_mutex_handle_t hlock);
_err_t load_extension(_cstr_t file, _cstr_t alias, _extension_t **pp_ext=NULL, _mutex_handle_t hlock=0);
_err_t unload_extension(_cstr_t alias, _mutex_handle_t hlock=0);
void unload_extensions(_mutex_handle_t hlock=0);
_extension_t *find_extension(_cstr_t alias, _mutex_handle_t hlock=0);
_err_t init_extension(_cstr_t alias, iRepository *pi_repo, _mutex_handle_t hlock=0);
_cstr_t error_text(void);
_base_entry_t *extension_array(_cstr_t alias, _u32 *count, _u32 *limit, _mutex_handle_t hlock=0);
void enum_extensions(_s32 (*enum_cb)(_extension_t *, void *), void *udata, _mutex_handle_t hlock=0);
void destroy_extensions_storage(_mutex_handle_t hlock=0);


// base array
#define MAX_INAME	64
#define MAX_CNAME	64

typedef struct {
	_char_t	iname[MAX_INAME];
	_char_t cname[MAX_CNAME];
	iBase 	*pi_base;
}_base_key_t;

void add_base_array(_base_entry_t *pb_entry, _u32 count);
void remove_base_array(_base_entry_t *pb_entry, _u32 count);
_base_entry_t *find_object(_base_key_t *p_key);
_base_entry_t *find_object_by_iname(_cstr_t iname);
_base_entry_t *find_object_by_cname(_cstr_t cname);
_base_entry_t *find_object_by_pointer(iBase *pi_base);
void destroy_base_array_storage(void);

#define ENUM_CONTINUE	MAP_ENUM_CONTINUE
#define ENUM_BREAK	MAP_ENUM_BREAK
#define ENUM_DELETE	MAP_ENUM_DELETE
#define ENUM_CURRENT	10

typedef _s32 _enum_cb_t(iBase *pi_base, void *udata);
typedef std::unordered_set<iBase *, std::hash<iBase *>, std::equal_to<iBase *>, zAllocator<iBase *>> _set_pi_object_t;

// object users
void users_add_object(_base_entry_t *p_bentry);
void users_remove_object(_base_entry_t *p_bentry);
void users_add_object_user(_base_entry_t *p_bentry, iBase *pi_user);
void users_remove_object_user(_base_entry_t *p_bentry, iBase *pi_user);
_set_pi_object_t *get_object_users(_base_entry_t *p_bentry);
void users_enum(_base_entry_t *p_bentry, _enum_cb_t *pcb, void *udata);
void destroy_users_storage(void);

// Monitoring
typedef void _monitoring_enum_cb_t(_cstr_t iname, _cstr_t cname, void *udata);

void add_monitoring(_cstr_t iname, _cstr_t cname, iBase *pi_handler);
void remove_monitoring(iBase *pi_handler);
void enum_monitoring(iBase *pi_handler, _monitoring_enum_cb_t *pcb, void *udata);
void destroy_monitoring_storage(void);

// Dynamic Context Storage (DCS)
_mutex_handle_t dcs_lock(_mutex_handle_t hlock=0);
void dcs_unlock(_mutex_handle_t hlock);
iBase *dcs_create_context(_base_entry_t *p_bentry, _rf_t flags, _mutex_handle_t hlock=0);
bool dcs_remove_context(iBase *pi_base, _mutex_handle_t hlock=0);
_cstat_t dcs_get_context_state(iBase *pi_base);
void dcs_set_context_state(iBase *pi_base, _cstat_t state);
void dcs_destroy_storage(void);

// Link map
#define PLMR_READY		(1<<0)
#define PLMR_KEEP_PENDING	(1<<1)
#define PLMR_FAILED		(1<<2)
#define PLMR_UNINIT		(1<<3)

typedef iBase *_cb_object_request_t(const _link_info_t *p_link_info, void *udata);
typedef iBase *_cb_create_object_t(_base_entry_t *p_bentry, _rf_t flags, void *udata);
typedef void _cb_release_object_t(iBase *pi_base, void *udata);
typedef void _cb_remove_user_t(const _link_info_t *p_link_info, iBase *pi_user, void *udata);

void lm_clean(iBase *pi_base);
_u32 lm_init(iBase *pi_base, _cb_object_request_t *pcb, void *udata);
_u32 lm_post_init(iBase *pi_base, _cb_object_request_t *pcb, void *udata);
_u32 lm_post_init(iBase *pi_base, _base_entry_t *p_bentry, _cb_create_object_t *pcb, void *udata);
_u32 lm_pre_uninit(iBase *pi_base,
			_cb_release_object_t *pcb_release,
			_cb_remove_user_t *pcb_remove_user,
			void *udata);
_u32 lm_uninit(iBase *pi_base, _cb_release_object_t *pcb, void *udata);
_u32 lm_remove(iBase *pi_base, _base_entry_t *p_bentry, _cb_release_object_t *pcb, void *udata);

#endif

