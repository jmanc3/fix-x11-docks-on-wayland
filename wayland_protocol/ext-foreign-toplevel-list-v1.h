/* Generated by wayland-scanner 1.23.1 */

#ifndef EXT_FOREIGN_TOPLEVEL_LIST_V1_CLIENT_PROTOCOL_H
#define EXT_FOREIGN_TOPLEVEL_LIST_V1_CLIENT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * @page page_ext_foreign_toplevel_list_v1 The ext_foreign_toplevel_list_v1 protocol
 * list toplevels
 *
 * @section page_desc_ext_foreign_toplevel_list_v1 Description
 *
 * The purpose of this protocol is to provide protocol object handles for
 * toplevels, possibly originating from another client.
 *
 * This protocol is intentionally minimalistic and expects additional
 * functionality (e.g. creating a screencopy source from a toplevel handle,
 * getting information about the state of the toplevel) to be implemented
 * in extension protocols.
 *
 * The compositor may choose to restrict this protocol to a special client
 * launched by the compositor itself or expose it to all clients,
 * this is compositor policy.
 *
 * The key words "must", "must not", "required", "shall", "shall not",
 * "should", "should not", "recommended",  "may", and "optional" in this
 * document are to be interpreted as described in IETF RFC 2119.
 *
 * Warning! The protocol described in this file is currently in the testing
 * phase. Backward compatible changes may be added together with the
 * corresponding interface version bump. Backward incompatible changes can
 * only be done by creating a new major version of the extension.
 *
 * @section page_ifaces_ext_foreign_toplevel_list_v1 Interfaces
 * - @subpage page_iface_ext_foreign_toplevel_list_v1 - list toplevels
 * - @subpage page_iface_ext_foreign_toplevel_handle_v1 - a mapped toplevel
 * @section page_copyright_ext_foreign_toplevel_list_v1 Copyright
 * <pre>
 *
 * Copyright © 2018 Ilia Bozhinov
 * Copyright © 2020 Isaac Freund
 * Copyright © 2022 wb9688
 * Copyright © 2023 i509VCB
 *
 * Permission to use, copy, modify, distribute, and sell this
 * software and its documentation for any purpose is hereby granted
 * without fee, provided that the above copyright notice appear in
 * all copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of
 * the copyright holders not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 * </pre>
 */
struct ext_foreign_toplevel_handle_v1;
struct ext_foreign_toplevel_list_v1;

#ifndef EXT_FOREIGN_TOPLEVEL_LIST_V1_INTERFACE
#define EXT_FOREIGN_TOPLEVEL_LIST_V1_INTERFACE
/**
 * @page page_iface_ext_foreign_toplevel_list_v1 ext_foreign_toplevel_list_v1
 * @section page_iface_ext_foreign_toplevel_list_v1_desc Description
 *
 * A toplevel is defined as a surface with a role similar to xdg_toplevel.
 * XWayland surfaces may be treated like toplevels in this protocol.
 *
 * After a client binds the ext_foreign_toplevel_list_v1, each mapped
 * toplevel window will be sent using the ext_foreign_toplevel_list_v1.toplevel
 * event.
 *
 * Clients which only care about the current state can perform a roundtrip after
 * binding this global.
 *
 * For each instance of ext_foreign_toplevel_list_v1, the compositor must
 * create a new ext_foreign_toplevel_handle_v1 object for each mapped toplevel.
 *
 * If a compositor implementation sends the ext_foreign_toplevel_list_v1.finished
 * event after the global is bound, the compositor must not send any
 * ext_foreign_toplevel_list_v1.toplevel events.
 * @section page_iface_ext_foreign_toplevel_list_v1_api API
 * See @ref iface_ext_foreign_toplevel_list_v1.
 */
/**
 * @defgroup iface_ext_foreign_toplevel_list_v1 The ext_foreign_toplevel_list_v1 interface
 *
 * A toplevel is defined as a surface with a role similar to xdg_toplevel.
 * XWayland surfaces may be treated like toplevels in this protocol.
 *
 * After a client binds the ext_foreign_toplevel_list_v1, each mapped
 * toplevel window will be sent using the ext_foreign_toplevel_list_v1.toplevel
 * event.
 *
 * Clients which only care about the current state can perform a roundtrip after
 * binding this global.
 *
 * For each instance of ext_foreign_toplevel_list_v1, the compositor must
 * create a new ext_foreign_toplevel_handle_v1 object for each mapped toplevel.
 *
 * If a compositor implementation sends the ext_foreign_toplevel_list_v1.finished
 * event after the global is bound, the compositor must not send any
 * ext_foreign_toplevel_list_v1.toplevel events.
 */
extern const struct wl_interface ext_foreign_toplevel_list_v1_interface;
#endif
#ifndef EXT_FOREIGN_TOPLEVEL_HANDLE_V1_INTERFACE
#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_INTERFACE
/**
 * @page page_iface_ext_foreign_toplevel_handle_v1 ext_foreign_toplevel_handle_v1
 * @section page_iface_ext_foreign_toplevel_handle_v1_desc Description
 *
 * A ext_foreign_toplevel_handle_v1 object represents a mapped toplevel
 * window. A single app may have multiple mapped toplevels.
 * @section page_iface_ext_foreign_toplevel_handle_v1_api API
 * See @ref iface_ext_foreign_toplevel_handle_v1.
 */
/**
 * @defgroup iface_ext_foreign_toplevel_handle_v1 The ext_foreign_toplevel_handle_v1 interface
 *
 * A ext_foreign_toplevel_handle_v1 object represents a mapped toplevel
 * window. A single app may have multiple mapped toplevels.
 */
extern const struct wl_interface ext_foreign_toplevel_handle_v1_interface;
#endif

/**
 * @ingroup iface_ext_foreign_toplevel_list_v1
 * @struct ext_foreign_toplevel_list_v1_listener
 */
struct ext_foreign_toplevel_list_v1_listener {
	/**
	 * a toplevel has been created
	 *
	 * This event is emitted whenever a new toplevel window is
	 * created. It is emitted for all toplevels, regardless of the app
	 * that has created them.
	 *
	 * All initial properties of the toplevel (identifier, title,
	 * app_id) will be sent immediately after this event using the
	 * corresponding events for ext_foreign_toplevel_handle_v1. The
	 * compositor will use the ext_foreign_toplevel_handle_v1.done
	 * event to indicate when all data has been sent.
	 */
	void (*toplevel)(void *data,
			 struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1,
			 struct ext_foreign_toplevel_handle_v1 *toplevel);
	/**
	 * the compositor has finished with the toplevel manager
	 *
	 * This event indicates that the compositor is done sending
	 * events to this object. The client should should destroy the
	 * object. See ext_foreign_toplevel_list_v1.destroy for more
	 * information.
	 *
	 * The compositor must not send any more toplevel events after this
	 * event.
	 */
	void (*finished)(void *data,
			 struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1);
};

/**
 * @ingroup iface_ext_foreign_toplevel_list_v1
 */
static inline int
ext_foreign_toplevel_list_v1_add_listener(struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1,
					  const struct ext_foreign_toplevel_list_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) ext_foreign_toplevel_list_v1,
				     (void (**)(void)) listener, data);
}

#define EXT_FOREIGN_TOPLEVEL_LIST_V1_STOP 0
#define EXT_FOREIGN_TOPLEVEL_LIST_V1_DESTROY 1

/**
 * @ingroup iface_ext_foreign_toplevel_list_v1
 */
#define EXT_FOREIGN_TOPLEVEL_LIST_V1_TOPLEVEL_SINCE_VERSION 1
/**
 * @ingroup iface_ext_foreign_toplevel_list_v1
 */
#define EXT_FOREIGN_TOPLEVEL_LIST_V1_FINISHED_SINCE_VERSION 1

/**
 * @ingroup iface_ext_foreign_toplevel_list_v1
 */
#define EXT_FOREIGN_TOPLEVEL_LIST_V1_STOP_SINCE_VERSION 1
/**
 * @ingroup iface_ext_foreign_toplevel_list_v1
 */
#define EXT_FOREIGN_TOPLEVEL_LIST_V1_DESTROY_SINCE_VERSION 1

/** @ingroup iface_ext_foreign_toplevel_list_v1 */
static inline void
ext_foreign_toplevel_list_v1_set_user_data(struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) ext_foreign_toplevel_list_v1, user_data);
}

/** @ingroup iface_ext_foreign_toplevel_list_v1 */
static inline void *
ext_foreign_toplevel_list_v1_get_user_data(struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) ext_foreign_toplevel_list_v1);
}

static inline uint32_t
ext_foreign_toplevel_list_v1_get_version(struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) ext_foreign_toplevel_list_v1);
}

/**
 * @ingroup iface_ext_foreign_toplevel_list_v1
 *
 * This request indicates that the client no longer wishes to receive
 * events for new toplevels.
 *
 * The Wayland protocol is asynchronous, meaning the compositor may send
 * further toplevel events until the stop request is processed.
 * The client should wait for a ext_foreign_toplevel_list_v1.finished
 * event before destroying this object.
 */
static inline void
ext_foreign_toplevel_list_v1_stop(struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1)
{
	wl_proxy_marshal_flags((struct wl_proxy *) ext_foreign_toplevel_list_v1,
			 EXT_FOREIGN_TOPLEVEL_LIST_V1_STOP, NULL, wl_proxy_get_version((struct wl_proxy *) ext_foreign_toplevel_list_v1), 0);
}

/**
 * @ingroup iface_ext_foreign_toplevel_list_v1
 *
 * This request should be called either when the client will no longer
 * use the ext_foreign_toplevel_list_v1 or after the finished event
 * has been received to allow destruction of the object.
 *
 * If a client wishes to destroy this object it should send a
 * ext_foreign_toplevel_list_v1.stop request and wait for a ext_foreign_toplevel_list_v1.finished
 * event, then destroy the handles and then this object.
 */
static inline void
ext_foreign_toplevel_list_v1_destroy(struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1)
{
	wl_proxy_marshal_flags((struct wl_proxy *) ext_foreign_toplevel_list_v1,
			 EXT_FOREIGN_TOPLEVEL_LIST_V1_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) ext_foreign_toplevel_list_v1), WL_MARSHAL_FLAG_DESTROY);
}

/**
 * @ingroup iface_ext_foreign_toplevel_handle_v1
 * @struct ext_foreign_toplevel_handle_v1_listener
 */
struct ext_foreign_toplevel_handle_v1_listener {
	/**
	 * the toplevel has been closed
	 *
	 * The server will emit no further events on the
	 * ext_foreign_toplevel_handle_v1 after this event. Any requests
	 * received aside from the destroy request must be ignored. Upon
	 * receiving this event, the client should destroy the handle.
	 *
	 * Other protocols which extend the ext_foreign_toplevel_handle_v1
	 * interface must also ignore requests other than destructors.
	 */
	void (*closed)(void *data,
		       struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1);
	/**
	 * all information about the toplevel has been sent
	 *
	 * This event is sent after all changes in the toplevel state
	 * have been sent.
	 *
	 * This allows changes to the ext_foreign_toplevel_handle_v1
	 * properties to be atomically applied. Other protocols which
	 * extend the ext_foreign_toplevel_handle_v1 interface may use this
	 * event to also atomically apply any pending state.
	 *
	 * This event must not be sent after the
	 * ext_foreign_toplevel_handle_v1.closed event.
	 */
	void (*done)(void *data,
		     struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1);
	/**
	 * title change
	 *
	 * The title of the toplevel has changed.
	 *
	 * The configured state must not be applied immediately. See
	 * ext_foreign_toplevel_handle_v1.done for details.
	 */
	void (*title)(void *data,
		      struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
		      const char *title);
	/**
	 * app_id change
	 *
	 * The app id of the toplevel has changed.
	 *
	 * The configured state must not be applied immediately. See
	 * ext_foreign_toplevel_handle_v1.done for details.
	 */
	void (*app_id)(void *data,
		       struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
		       const char *app_id);
	/**
	 * a stable identifier for a toplevel
	 *
	 * This identifier is used to check if two or more toplevel
	 * handles belong to the same toplevel.
	 *
	 * The identifier is useful for command line tools or privileged
	 * clients which may need to reference an exact toplevel across
	 * processes or instances of the ext_foreign_toplevel_list_v1
	 * global.
	 *
	 * The compositor must only send this event when the handle is
	 * created.
	 *
	 * The identifier must be unique per toplevel and it's handles. Two
	 * different toplevels must not have the same identifier. The
	 * identifier is only valid as long as the toplevel is mapped. If
	 * the toplevel is unmapped the identifier must not be reused. An
	 * identifier must not be reused by the compositor to ensure there
	 * are no races when sharing identifiers between processes.
	 *
	 * An identifier is a string that contains up to 32 printable ASCII
	 * bytes. An identifier must not be an empty string. It is
	 * recommended that a compositor includes an opaque generation
	 * value in identifiers. How the generation value is used when
	 * generating the identifier is implementation dependent.
	 */
	void (*identifier)(void *data,
			   struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
			   const char *identifier);
};

/**
 * @ingroup iface_ext_foreign_toplevel_handle_v1
 */
static inline int
ext_foreign_toplevel_handle_v1_add_listener(struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
					    const struct ext_foreign_toplevel_handle_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) ext_foreign_toplevel_handle_v1,
				     (void (**)(void)) listener, data);
}

#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_DESTROY 0

/**
 * @ingroup iface_ext_foreign_toplevel_handle_v1
 */
#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_CLOSED_SINCE_VERSION 1
/**
 * @ingroup iface_ext_foreign_toplevel_handle_v1
 */
#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_DONE_SINCE_VERSION 1
/**
 * @ingroup iface_ext_foreign_toplevel_handle_v1
 */
#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_TITLE_SINCE_VERSION 1
/**
 * @ingroup iface_ext_foreign_toplevel_handle_v1
 */
#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_APP_ID_SINCE_VERSION 1
/**
 * @ingroup iface_ext_foreign_toplevel_handle_v1
 */
#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_IDENTIFIER_SINCE_VERSION 1

/**
 * @ingroup iface_ext_foreign_toplevel_handle_v1
 */
#define EXT_FOREIGN_TOPLEVEL_HANDLE_V1_DESTROY_SINCE_VERSION 1

/** @ingroup iface_ext_foreign_toplevel_handle_v1 */
static inline void
ext_foreign_toplevel_handle_v1_set_user_data(struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) ext_foreign_toplevel_handle_v1, user_data);
}

/** @ingroup iface_ext_foreign_toplevel_handle_v1 */
static inline void *
ext_foreign_toplevel_handle_v1_get_user_data(struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) ext_foreign_toplevel_handle_v1);
}

static inline uint32_t
ext_foreign_toplevel_handle_v1_get_version(struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) ext_foreign_toplevel_handle_v1);
}

/**
 * @ingroup iface_ext_foreign_toplevel_handle_v1
 *
 * This request should be used when the client will no longer use the handle
 * or after the closed event has been received to allow destruction of the
 * object.
 *
 * When a handle is destroyed, a new handle may not be created by the server
 * until the toplevel is unmapped and then remapped. Destroying a toplevel handle
 * is not recommended unless the client is cleaning up child objects
 * before destroying the ext_foreign_toplevel_list_v1 object, the toplevel
 * was closed or the toplevel handle will not be used in the future.
 *
 * Other protocols which extend the ext_foreign_toplevel_handle_v1
 * interface should require destructors for extension interfaces be
 * called before allowing the toplevel handle to be destroyed.
 */
static inline void
ext_foreign_toplevel_handle_v1_destroy(struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1)
{
	wl_proxy_marshal_flags((struct wl_proxy *) ext_foreign_toplevel_handle_v1,
			 EXT_FOREIGN_TOPLEVEL_HANDLE_V1_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) ext_foreign_toplevel_handle_v1), WL_MARSHAL_FLAG_DESTROY);
}

#ifdef  __cplusplus
}
#endif

#endif
