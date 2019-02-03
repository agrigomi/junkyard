#include "zone.h"

/* aligns size to next degree on 2 */
static unsigned int _align(unsigned int size) {
	unsigned int r = 0;

	if(size && size < ZONE_MIN_ALLOC)
		r = ZONE_MIN_ALLOC;
	else if(size > ZONE_PAGE_SIZE) {
		r = size / ZONE_PAGE_SIZE;
		r += (size % ZONE_PAGE_SIZE) ? 1 : 0;
		r *= ZONE_PAGE_SIZE;
	} else if(size && size < ZONE_PAGE_SIZE) {
		unsigned int m = 0;

		r = ZONE_PAGE_SIZE;
		while(r && !(r & size))
			r >>= 1;

		m = r >> 1;
		while(m && !(m & size))
			m >>= 1;

		if(m)
			r <<= 1;
	} else
		r = size;

	return r;
}

static void _clear(void *ptr, unsigned int size) {
	unsigned int n = size / sizeof(unsigned long long);
	unsigned long long *p = (unsigned long long *)ptr;
	unsigned int i = 0;

	for(; i < n; i++)
		*(p + i) = 0;
}

static unsigned long long _lock(_zone_context_t *p_zcxt, unsigned long long mutex_handle) {
	unsigned long long r = 0;

	if(p_zcxt->pf_mutex_lock)
		r = p_zcxt->pf_mutex_lock(mutex_handle, p_zcxt->user_data);

	return r;
}

static void _unlock(_zone_context_t *p_zcxt, unsigned long long mutex_handle) {
	if(p_zcxt->pf_mutex_unlock)
		p_zcxt->pf_mutex_unlock(mutex_handle, p_zcxt->user_data);
}

static unsigned int _bit_number(unsigned int x) {
	unsigned int r = 0;
	unsigned int mask = 1;

	while(!(mask & x)) {
		mask <<= 1;
		r++;
	}

	return r;
}

static _zone_page_t **_zone_page(_zone_context_t *p_zcxt, unsigned int size, unsigned int *aligned_size) {
	_zone_page_t **r = (_zone_page_t **)0;
	unsigned int zone_index = _bit_number((*aligned_size = _align(size)));

	if(zone_index && zone_index < ZONE_MAX_ZONES) /* because index 0 not allowed */
		r = p_zcxt->zones + zone_index;

	return r;
}

/* Initialize zone context and return 0 for success */
int zone_init(_zone_context_t *p_zcxt) {
	int r = -1;

	if(p_zcxt->pf_page_alloc && p_zcxt->pf_page_free && !p_zcxt->zones)
		p_zcxt->zones = (_zone_page_t **)p_zcxt->pf_page_alloc(1, p_zcxt->limit, p_zcxt->user_data);

	if(p_zcxt->zones) {
		_clear(p_zcxt->zones, ZONE_PAGE_SIZE);
		r = 0;
	}

	return r;
}

/* returns unit as result and bit number in 'bit' parameter
	'bit' parameter must contains a requested bit
*/
static unsigned long long *_bitmap_unit_bit(_zone_context_t *p_zcxt, _zone_entry_t *p_entry,
					unsigned int aligned_size,
					unsigned char *bit // [in / out]
					) {
	unsigned long long *r = (unsigned long long *)0;
	unsigned int max_bits = ZONE_PAGE_SIZE / aligned_size;

	if(*bit < max_bits) {
		unsigned char unit = *bit / ZONE_BITMAP_UNIT_BITS;
		*bit = *bit % ZONE_BITMAP_UNIT_BITS;
		r = &p_entry->bitmap[unit];
	}

	return r;
}

static void *_zone_entry_alloc(_zone_context_t *p_zcxt, _zone_entry_t *p_entry, unsigned int aligned_size, unsigned long long limit) {
	void *r = (void *)0;

	if(aligned_size < ZONE_PAGE_SIZE) {
		unsigned int unit = 0;
		unsigned int bit = 0, unit_bit = 0;
		unsigned int max_bits = ZONE_PAGE_SIZE / aligned_size;
		unsigned long long mask = ((unsigned long long)1 << (ZONE_BITMAP_UNIT_BITS -1));

		while(bit < max_bits) {
			if(unit_bit >= ZONE_BITMAP_UNIT_BITS) {
				unit_bit = 0;
				unit++;
			}

			if(!(p_entry->bitmap[unit] & (mask >> unit_bit)))
				break;

			bit++;
			unit_bit++;
		}

		if(bit < max_bits) {
			unsigned char *data = (unsigned char *)p_entry->data;

			if(!data) {
				data = (unsigned char *)p_zcxt->pf_page_alloc(1, limit, p_zcxt->user_data);
				p_entry->data = (unsigned long long)data;
			}

			if(data) {
				r = data + (bit * aligned_size);
				p_entry->bitmap[unit] |= (mask >> unit_bit);
				p_entry->objects++;
			}
		}
	} else {
		/* one object per entry */
		if(p_entry->objects == 0) {
			unsigned char *data = (unsigned char *)p_entry->data;

			if(!data) {
				data = (unsigned char *)p_zcxt->pf_page_alloc(aligned_size / ZONE_PAGE_SIZE, limit, p_zcxt->user_data);
				p_entry->data = (unsigned long long)data;
			}

			if(data) {
				r = data;
				p_entry->objects = 1;
				p_entry->data_size = aligned_size;
			}
		}
	}

	return r;
}

static void *_zone_page_alloc(_zone_context_t *p_zcxt, _zone_page_t *p_zone, unsigned int aligned_size, unsigned long long limit) {
	void *r = (void *)0;
	unsigned int max_objects = ZONE_MAX_ENTRIES;
	_zone_page_t *p_current_page = p_zone;
	unsigned long long mutex_handle = 0;

	if(aligned_size < ZONE_PAGE_SIZE)
		max_objects = (ZONE_MAX_ENTRIES * ZONE_PAGE_SIZE) / aligned_size;

	mutex_handle = _lock(p_zcxt, 0);
	while(p_current_page && p_current_page->header.objects == max_objects) {
		if(!(p_current_page->header.next)) {
			/* alloc new page header */
			_zone_page_t *p_new_page = (_zone_page_t *)p_zcxt->pf_page_alloc(1, limit, p_zcxt->user_data);

			if(p_new_page) {
				_clear(p_new_page, sizeof(_zone_page_t));
				p_new_page->header.object_size = aligned_size;
			}

			p_current_page->header.next = p_new_page;
		}

		/* swith to next page */
		p_current_page = p_current_page->header.next;
	}

	if(p_current_page) {
		unsigned int i = 0;
		unsigned int max_entry_objects = 1;

		if(aligned_size < ZONE_PAGE_SIZE)
			max_entry_objects = ZONE_PAGE_SIZE / aligned_size;

		while(i < ZONE_MAX_ENTRIES) {
			if(p_current_page->array[i].objects < max_entry_objects) {
				if((r = _zone_entry_alloc(p_zcxt, &p_current_page->array[i], aligned_size, limit)))
					p_current_page->header.objects++;
				break;
			}

			i++;
		}
	}
	_unlock(p_zcxt, mutex_handle);

	return r;
}

/* Allocates chunk of memory and returns pointer or NULL */
void *zone_alloc(_zone_context_t *p_zcxt, unsigned int size, unsigned long long limit) {
	void *r = (void *)0;
	unsigned int aligned_size = 0;
	_zone_page_t **pp_zone = _zone_page(p_zcxt, size, &aligned_size);

	if(pp_zone) {
		_zone_page_t *p_zone = *pp_zone;

		if(!p_zone) { // alloc zone page
			if((p_zone = (_zone_page_t *)p_zcxt->pf_page_alloc(1, limit, p_zcxt->user_data))) {
				unsigned long long mutex_handle = 0;

				_clear(p_zone, sizeof(_zone_page_t));

				mutex_handle = _lock(p_zcxt, 0);
				*pp_zone = p_zone;
				p_zone->header.object_size = aligned_size;
				_unlock(p_zcxt, mutex_handle);
			}
		}

		if(p_zone)
			r = _zone_page_alloc(p_zcxt, p_zone, aligned_size, limit);
	}

	return r;
}

/* Deallocate memory chunk */
void zone_free(_zone_context_t *p_zcxt, void *ptr, unsigned int size) {
	unsigned int aligned_size = 0;
	_zone_page_t **pp_zone = _zone_page(p_zcxt, size, &aligned_size);

	if(pp_zone) {
		_zone_page_t *p_zone = *pp_zone;
		//...
	}
}

/* Destroy zone context */
void zone_destroy(_zone_context_t *p_zcxt) {
	//...
}
