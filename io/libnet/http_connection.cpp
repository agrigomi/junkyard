#include <string.h>
#include "private.h"
#include "time.h"
#include "url-codec.h"

// http connection status
enum httpc_state {
	HTTPC_RECEIVE_HEADER =	1,
	HTTPC_COMPLETE_HEADER,
	HTTPC_PARSE_HEADER,
	HTTPC_RECEIVE_CONTENT,
	HTTPC_SEND_HEADER,
	HTTPC_SEND_CONTENT,
	HTTPC_CLOSE
};

typedef struct {
	_u16 	rc;
	_cstr_t	text;
}_http_resp_text_t;

static _http_resp_text_t _g_http_resp_text[] = {
	{HTTPRC_CONTINUE,		"Continue"},
	{HTTPRC_SWITCHING_PROTOCOL,	"Switching Protocols"},
	{HTTPRC_OK,			"OK"},
	{HTTPRC_CREATED,		"Created"},
	{HTTPRC_ACCEPTED,		"Accepted"},
	{HTTPRC_NON_AUTH,		"Non-Authoritative Information"},
	{HTTPRC_NO_CONTENT,		"No Content"},
	{HTTPRC_RESET_CONTENT,		"Reset Content"},
	{HTTPRC_PART_CONTENT,		"Partial Content"},
	{HTTPRC_MULTICHOICES,		"Multiple Choices"},
	{HTTPRC_MOVED_PERMANENTLY,	"Moved Permanently"},
	{HTTPRC_FOUND,			"Found"},
	{HTTPRC_SEE_OTHER,		"See Other"},
	{HTTPRC_NOT_MODIFIED,		"Not Modified"},
	{HTTPRC_USE_PROXY,		"Use proxy"},
	{HTTPRC_TEMP_REDIRECT,		"Temporary redirect"},
	{HTTPRC_BAD_REQUEST,		"Bad Request"},
	{HTTPRC_UNAUTHORIZED,		"Unauthorized"},
	{HTTPRC_PAYMENT_REQUIRED,	"Payment Required"},
	{HTTPRC_FORBIDDEN,		"Forbidden"},
	{HTTPRC_NOT_FOUND,		"Not Found"},
	{HTTPRC_METHOD_NOT_ALLOWED,	"Method Not Allowed"},
	{HTTPRC_NOT_ACCEPTABLE,		"Not Acceptable"},
	{HTTPRC_PROXY_AUTH_REQUIRED,	"Proxy Authentication Required"},
	{HTTPRC_REQUEST_TIMEOUT,	"Request Time-out"},
	{HTTPRC_CONFLICT,		"Conflict"},
	{HTTPRC_GONE,			"Gone"},
	{HTTPRC_LENGTH_REQUIRED,	"Length Required"},
	{HTTPRC_PRECONDITION_FAILED,	"Precondition Failed"},
	{HTTPRC_REQ_ENTITY_TOO_LARGE,	"Request Entity Too Large"},
	{HTTPRC_REQ_URI_TOO_LARGE,	"Request-URI Too Large"},
	{HTTPRC_UNSUPPORTED_MEDIA_TYPE,	"Unsupported Media Type"},
	{HTTPRC_EXPECTATION_FAILED,	"Expectation Failed"},
	{HTTPRC_INTERNAL_SERVER_ERROR,	"Internal Server Error"},
	{HTTPRC_NOT_IMPLEMENTED,	"Not Implemented"},
	{HTTPRC_BAD_GATEWAY,		"Bad Gateway"},
	{HTTPRC_SERVICE_UNAVAILABLE,	"Service Unavailable"},
	{HTTPRC_GATEWAY_TIMEOUT,	"Gateway Time-out"},
	{HTTPRC_VERSION_NOT_SUPPORTED,	"HTTP Version not supported"},
	{0,				NULL}
};

typedef struct {
	_u8 	im;
	_cstr_t	sm;
}_http_method_map;

static _http_method_map _g_method_map[] = {
	{HTTP_METHOD_GET,	"GET"},
	{HTTP_METHOD_HEAD,	"HEAD"},
	{HTTP_METHOD_POST,	"POST"},
	{HTTP_METHOD_PUT,	"PUT"},
	{HTTP_METHOD_DELETE,	"DELETE"},
	{HTTP_METHOD_CONNECT,	"CONNECT"},
	{HTTP_METHOD_OPTIONS,	"OPTIONS"},
	{HTTP_METHOD_TRACE,	"TRACE"},
	{0,			NULL}
};

#define VAR_REQ_METHOD		(_str_t)"req-Method"
#define VAR_REQ_URL		(_str_t)"req-URL"
#define VAR_REQ_PROTOCOL	(_str_t)"req-Protocol"

bool cHttpConnection::object_ctl(_u32 cmd, void *arg, ...) {
	bool r = false;

	switch(cmd) {
		case OCTL_INIT: {
			iRepository *pi_repo = (iRepository *)arg;

			mp_sio = NULL;
			mpi_bmap = NULL;
			m_state = 0;
			m_ibuffer = m_oheader = m_obuffer = 0;
			m_ibuffer_offset = m_oheader_offset = m_obuffer_offset = 0;
			m_response_code = 0;
			m_error_code = 0;
			m_res_content_len = 0;
			m_req_content_len = 0;
			m_req_content_rcv = 0;
			m_oheader_sent = 0;
			m_content_sent = 0;
			m_header_len = 0;
			memset(m_udata, 0, sizeof(m_udata));
			mpi_str = (iStr *)pi_repo->object_by_iname(I_STR, RF_ORIGINAL);
			mpi_map = (iMap *)pi_repo->object_by_iname(I_MAP, RF_CLONE);
			if(mpi_str && mpi_map)
				r = true;
		} break;
		case OCTL_UNINIT: {
			iRepository *pi_repo = (iRepository *)arg;

			close();
			pi_repo->object_release(mpi_str);
			pi_repo->object_release(mpi_map);
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

void cHttpConnection::close(void) {
	if(mp_sio) {
		mp_sio->_close();
		mp_sio = 0;
		if(m_ibuffer) {
			mpi_bmap->free(m_ibuffer);
			m_ibuffer = 0;
		}
		if(m_oheader) {
			mpi_bmap->free(m_oheader);
			m_oheader = 0;
		}
		if(m_obuffer) {
			mpi_bmap->free(m_obuffer);
			m_obuffer = 0;
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


_cstr_t cHttpConnection::get_rc_text(_u16 rc) {
	_cstr_t r = "...";
	_u32 n = 0;

	while(_g_http_resp_text[n].rc) {
		if(rc == _g_http_resp_text[n].rc) {
			r = _g_http_resp_text[n].text;
			break;
		}
		n++;
	}

	return r;
}

_u32 cHttpConnection::receive(void) {
	_u32 r = 0;

	if(alive()) {
		if(!m_ibuffer)
			m_ibuffer = mpi_bmap->alloc();

		if(m_ibuffer) {
			_u8 *ptr = (_u8 *)mpi_bmap->ptr(m_ibuffer);
			_u32 sz_buffer = mpi_bmap->size();

			if(ptr) {
				r = mp_sio->read(ptr + m_ibuffer_offset, sz_buffer - m_ibuffer_offset);
				m_ibuffer_offset += r;
			}
		}
	}

	return r;
}

bool cHttpConnection::complete_req_header(void) {
	bool r = false;

	if(m_ibuffer_offset) {
		_u32 sz = mpi_bmap->size();

		_str_t ptr = (_str_t)mpi_bmap->ptr(m_ibuffer);

		if(ptr) {
			_s32 hl = 0;

			// !!! dangerous !!!
			if((hl = mpi_str->nfind_string(ptr, sz, "\r\n\r\n")) != -1) {
				r = true;
				m_header_len = hl + 4;
			} else
				m_header_len = 0;
		}
	}

	return r;
}

bool cHttpConnection::add_req_variable(_str_t name, _str_t value, _u32 sz_value) {
	bool r = false;

	if(mpi_map->add(name, mpi_str->str_len(name),
			value,
			(sz_value) ? sz_value : mpi_str->str_len(value)))
		r = true;

	return r;
}

_u32 cHttpConnection::parse_url(_str_t url, _u32 sz_max) {
	_u32 r = 0;
	HBUFFER hburl = mpi_bmap->alloc();

	if(hburl) {
		_str_t decoded = (_str_t)mpi_bmap->ptr(hburl);
		_u32 sz_decoded = UrlDecode((_cstr_t)url, decoded, mpi_bmap->size());

		//...

		mpi_bmap->free(hburl);
	}

	return r;
}

_u32 cHttpConnection::parse_request_line(_str_t req, _u32 sz_max) {
	_u32 r = 0;
	_char_t c = 0;
	_char_t _c = 0;
	_str_t fld[4] = {req, 0, 0, 0};
	_u32 fld_sz[4] = {0, 0, 0, 0};

	for(_u32 i=0, n=0; r < sz_max && i < 3; r++) {
		c = req[r];

		switch(c) {
			case ' ':
				if(c != _c) {
					fld_sz[i] = n;
					n = 0;
				}
				break;
			case '\r':
				fld_sz[i] = n;
				n = 0;
				break;
			case '\n':
				if(_c == '\r')
					i = 3;
				break;
			default:
				if(_c == ' ') {
					i++;
					fld[i] = req + r;
					n = 1;
				} else
					n++;
				break;
		}

		_c = c;
	}

	if(fld[0] && fld_sz[0] && fld_sz[0] < sz_max)
		add_req_variable(VAR_REQ_METHOD, fld[0], fld_sz[0]);
	if(fld[1] && fld_sz[1] && fld_sz[1] < sz_max)
		parse_url(fld[1], fld_sz[1]);
	if(fld[2] && fld_sz[2] && fld_sz[2] < sz_max)
		add_req_variable(VAR_REQ_PROTOCOL, fld[2], fld_sz[2]);

	return r;
}

_u32 cHttpConnection::parse_var_line(_str_t var, _u32 sz_max) {
	_u32 r = 0;
	_str_t fld[3] = {var, 0, 0};
	_u32 fld_sz[3] = {0, 0, 0};
	_char_t c = 0;
	_char_t _c = 0;
	bool val = false;

	for(_u32 i=0, n=0; r < sz_max && i < 2; r++) {
		c = var[r];

		switch(c) {
			case ' ':
				if(_c != ':' && i)
					n++;
				break;
			case ':':
				if(!i) {
					fld_sz[i] = n;
					n = 0;
					i++;
				} else
					n++;
				break;
			case '\r':
				if(i)
					fld_sz[i] = n;
				break;
			case '\n':
				if(_c == '\r')
					i = 2;
				break;
			default:
				if(!val && i && _c == ' ') {
					fld[i] = var + r;
					val = true;
					n = 1;
				} else
					n++;
				break;
		}

		_c = c;
	}

	if(fld[0] && fld_sz[0] && fld[1] && fld_sz[1] &&
			fld_sz[0] < sz_max && fld_sz[1] < sz_max)
		mpi_map->add(fld[0], fld_sz[0], fld[1], fld_sz[1]);

	return r;
}

bool cHttpConnection::parse_req_header(void) {
	bool r = false;

	if(m_ibuffer && m_ibuffer_offset) {
		_str_t hdr = (_str_t)mpi_bmap->ptr(m_ibuffer);

		if(hdr) {
			_u32 offset = parse_request_line(hdr, m_header_len);

			while(offset && offset < m_header_len) {
				_u32 n = parse_var_line(hdr + offset, m_header_len - offset);
				offset = (n) ? (offset + n) : 0;
			}

			if(offset == m_header_len) {
				r = true;
				if(m_ibuffer_offset > m_header_len) {
					// have request data
					mpi_str->mem_cpy(hdr, hdr + m_header_len, m_ibuffer_offset - m_header_len);
					m_ibuffer_offset -= m_header_len;
				} else
					m_ibuffer_offset = 0;

				m_header_len = 0;

				_str_t cl = req_var("Content-Length");
				if(cl)
					m_req_content_len = atoi(cl);
				m_req_content_rcv = m_ibuffer_offset;
			}
		}
	}

	return r;
}

_u32 cHttpConnection::res_remainder(void) {
	return m_res_content_len - (m_content_sent < m_res_content_len) ? m_content_sent : m_res_content_len;
}

void cHttpConnection::clear_ibuffer(void) {
	if(m_ibuffer) {
		_u8 *ptr = (_u8 *)mpi_bmap->ptr(m_ibuffer);

		if(ptr) {
			_u32 sz = mpi_bmap->size();

			memset(ptr, 0, sz);
			m_ibuffer_offset = 0;
		}
	}
}

_u32 cHttpConnection::send_header(void) {
	_u32 r = 0;
	_char_t rs[128]="";

	if(m_response_code) {
		if(!m_oheader_sent) {
			// send first line
			_u32 n = snprintf(rs, sizeof(rs), "HTTP/1.1 %u %s\r\n",
					m_response_code, get_rc_text(m_response_code));

			mp_sio->write(rs, n);
		}

		if(m_oheader_offset) {
			// send header
			_u8 *ptr = (_u8 *)mpi_bmap->ptr(m_oheader);

			if(ptr) {
				r = mp_sio->write(ptr + m_oheader_sent,
						m_oheader_offset - m_oheader_sent);
				m_oheader_sent += r;
			}
		}

		if(m_oheader_sent == m_oheader_offset) {
			// send header end
			mp_sio->write("\r\n", 2);
			m_oheader_sent = r;
		}
	}

	return r;
}

_u32 cHttpConnection::receive_content(void) {
	_u32 r = receive();

	m_req_content_rcv += r;

	return r;
}

_u32 cHttpConnection::send_content(void) {
	_u32 r = 0;

	if(m_res_content_len && m_content_sent < m_res_content_len) {
		_u8 *ptr = (_u8 *)mpi_bmap->ptr(m_obuffer);
		if(ptr && m_obuffer_offset) {
			if((r = mp_sio->write(ptr, m_obuffer_offset))) {
				// folding of output buffer
				mpi_str->mem_cpy(ptr, ptr + r, m_obuffer_offset - r);
				m_obuffer_offset -= r;
				m_content_sent += r;
			}
		}
	}

	return r;
}

_u8 cHttpConnection::process(void) {
	_u8 r = 0;

	switch(m_state) {
		case 0:
			r = HTTP_ON_OPEN;
			m_state = HTTPC_RECEIVE_HEADER;
			break;
		case HTTPC_RECEIVE_HEADER:
			if(!receive()) {
				if(alive())
					m_state = HTTPC_COMPLETE_HEADER;
				else {
					m_error_code = HTTPRC_GONE;
					m_state = HTTPC_CLOSE;
					r = HTTP_ON_ERROR;
				}
			}
			break;
		case HTTPC_COMPLETE_HEADER:
			if(complete_req_header())
				m_state = HTTPC_PARSE_HEADER;
			else
				m_state = HTTPC_RECEIVE_HEADER;
			break;
		case HTTPC_PARSE_HEADER:
			if(parse_req_header()) {
				r = HTTP_ON_REQUEST;
				m_state = HTTPC_RECEIVE_CONTENT;
			} else {
				r = HTTP_ON_ERROR;
				m_error_code = HTTPRC_BAD_REQUEST;
				m_state = HTTPC_CLOSE;
			}
			break;
		case HTTPC_RECEIVE_CONTENT:
			clear_ibuffer();
			if(receive_content())
				r = HTTP_ON_REQUEST_DATA;
			else {
				if(alive()) {
					if(m_req_content_rcv >= m_req_content_len)
						m_state = HTTPC_SEND_HEADER;
				} else
					m_state = HTTPC_CLOSE;
			}
			break;
		case HTTPC_SEND_HEADER:
			clear_ibuffer();
			if(!receive_content()) {
				if(alive() && m_response_code) {
					send_header();
					if(m_oheader_sent == m_oheader_offset)
						m_state = HTTPC_SEND_CONTENT;
				} else
					m_state = HTTPC_CLOSE;
			} else
				r = HTTP_ON_REQUEST_DATA;
			break;
		case HTTPC_SEND_CONTENT:
			clear_ibuffer();
			if(receive_content())
				r = HTTP_ON_REQUEST_DATA;
			else {
				if(alive()) {
					send_content();
					if(m_content_sent < m_res_content_len)
						r = HTTP_ON_RESPONSE_DATA;
					else
						m_state = HTTPC_CLOSE;
				} else
					m_state = HTTPC_CLOSE;
			}
			break;
		case HTTPC_CLOSE:
			close();
			break;
	}

	return r;
}

_u32 cHttpConnection::res_write(_u8 *data, _u32 size) {
	_u32 r = 0;

	if(!m_obuffer)
		m_obuffer = mpi_bmap->alloc();

	if(m_obuffer) {
		_u8 *ptr = (_u8 *)mpi_bmap->ptr(m_obuffer);

		if(ptr) {
			_u32 bsz = mpi_bmap->size();
			_u32 brem = bsz - m_obuffer_offset;

			if((r = (size < brem ) ? size : brem)) {
				mpi_str->mem_cpy(ptr + m_obuffer_offset, data, r);
				m_obuffer_offset += r;
			}
		}
	}

	return r;
}

_u8 cHttpConnection::req_method(void) {
	_u8 r = 0;
	_str_t sm = req_var(VAR_REQ_METHOD);

	if(sm) {
		_u32 n = 0;

		while(_g_method_map[n].sm) {
			if(strcmp(sm, _g_method_map[n].sm) == 0) {
				r = _g_method_map[n].im;
				break;
			}

			n++;
		}
	}

	return r;
}

_str_t cHttpConnection::req_url(void) {
	return req_var(VAR_REQ_URL);
}

_str_t cHttpConnection::req_var(_cstr_t name) {
	_u32 sz = 0;
	_str_t vn = (_str_t)name;
	return (_str_t)mpi_map->get(vn, strlen(vn), &sz);
}

_u8 *cHttpConnection::req_data(_u32 *size) {
	_u8 *r = 0;

	if(m_ibuffer) {
		if((r = (_u8 *)mpi_bmap->ptr(m_ibuffer)))
			*size = m_ibuffer_offset;
	}

	return r;
}

bool cHttpConnection::res_var(_cstr_t name, _cstr_t value) {
	bool r = false;

	if(!m_oheader)
		m_oheader = mpi_bmap->alloc();
	if(m_oheader) {
		_char_t *ptr = (_char_t *)mpi_bmap->ptr(m_oheader);

		if(ptr) {
			_u32 sz = mpi_bmap->size();
			_u32 rem = sz - m_oheader_offset;

			if(rem) {
				_u32 n = snprintf(ptr + m_oheader_offset, rem, "%s: %s\r\n", name, value);

				m_oheader_offset += n;
				r = true;
			}
		}
	}

	return r;
}

bool cHttpConnection::res_content_len(_u32 content_len) {
	_char_t cl[32]="";

	sprintf(cl, "%u", content_len);
	m_res_content_len = content_len;
	return res_var("Content-Length", cl);
}

static cHttpConnection _g_httpc_;
