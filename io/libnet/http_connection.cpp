#include "private.h"

bool cHttpConnection::object_ctl(_u32 cmd, void *arg, ...) {
	bool r = false;

	switch(cmd) {
		case OCTL_INIT: {
			iRepository *pi_repo = (iRepository *)arg;

			mp_sio = NULL;
			mpi_bmap = 0;
			m_state = 0;
			m_req_buffer = 0;
			m_req_len = 0;
			m_res_buffer = 0;
			m_res_len = 0;
			if((mpi_str = (iStr *)pi_repo->object_by_iname(I_STR, RF_ORIGINAL))) {
				r = true;
			}
		} break;
		case OCTL_UNINIT: {
			iRepository *pi_repo = (iRepository *)arg;

			_close();
			pi_repo->object_release(mpi_str);
			r = true;
		} break;
	}

	return r;
}

bool cHttpConnection::_init(cSocketIO *p_sio, iBufferMap *pi_bmap) {
	bool r = false;

	if(!mp_sio && p_sio && (r = p_sio->alive())) {
		mp_sio = p_sio;
		mpi_bmap = pi_bmap;
		// use non blocking mode
		mp_sio->blocking(false);
	}

	return r;
}

void cHttpConnection::_close(void) {
	if(mp_sio) {
		mp_sio->_close();
		mp_sio = 0;
		if(m_req_buffer) {
			mpi_bmap->free(m_req_buffer);
			m_req_buffer = 0;
		}
		if(m_res_buffer) {
			mpi_bmap->free(m_res_buffer);
			m_res_buffer = 0;
		}
	}
}

bool cHttpConnection::alive(void) {
	bool r = false;

	if(mp_sio)
		r = mp_sio->alive();

	return r;
}

_u32 cHttpConnection::peer_ip(void) {
	_u32 r = 0;

	if(mp_sio)
		r = mp_sio->peer_ip();

	return r;
}

bool cHttpConnection::peer_ip(_str_t strip, _u32 len) {
	bool r = false;

	if(mp_sio)
		r = mp_sio->peer_ip(strip, len);

	return r;
}

static cHttpConnection _g_httpc_;
