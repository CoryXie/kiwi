/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		IPC system.
 *
 * This system implements a bi-directional communication mechanism for local
 * processes. An overview of the system is available in documentation/ipc.txt.
 * Some implementation notes are included below.
 *
 * Firstly, connections have a single lock protecting both ends of the
 * connection. This vastly simplifies locking, as with a lock for each end it
 * becomes easy to cause a deadlock when performing an operation that requires
 * locking of both endpoints.
 *
 * Secondly, neither endpoint is freed until both ends of the connection are
 * closed. This makes it simpler to handle one end of a connection being
 * closed.
 */

#include <cpu/intr.h>

#include <ipc/ipc.h>

#include <lib/avl_tree.h>
#include <lib/notifier.h>
#include <lib/refcount.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <proc/process.h>
#include <proc/sched.h>

#include <sync/mutex.h>
#include <sync/semaphore.h>

#include <assert.h>
#include <console.h>
#include <errors.h>
#include <init.h>
#include <object.h>
#include <kdbg.h>
#include <vmem.h>

#if CONFIG_IPC_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

struct ipc_connection;

/** IPC port structure. */
typedef struct ipc_port {
	object_t obj;			/**< Object header. */

	mutex_t lock;			/**< Lock to protect data in structure. */
	port_id_t id;			/**< ID of the port. */
	refcount_t count;		/**< Number of handles open to the port. */
	list_t connections;		/**< List of currently open connections. */
	list_t waiting;			/**< List of in-progress connection attempts. */
	semaphore_t conn_sem;		/**< Semaphore counting connection attempts. */
	notifier_t conn_notifier;	/**< Notifier for connection attempts. */
} ipc_port_t;

/** IPC endpoint structure. */
typedef struct ipc_endpoint {
	list_t messages;		/**< List of queued messages. */
	semaphore_t space_sem;		/**< Semaphore counting space in message queue. */
	semaphore_t data_sem;		/**< Semaphore counting messages in message queue. */
	notifier_t msg_notifier;	/**< Notifier for message arrival. */
	notifier_t hangup_notifier;	/**< Notifier for remote end being closed. */
	struct ipc_endpoint *remote;	/**< Other end of the connection. */
	struct ipc_connection *conn;	/**< Connection structure. */
} ipc_endpoint_t;

/** IPC connection structure. */
typedef struct ipc_connection {
	object_t obj;			/**< Object header. */
	list_t header;			/**< Link to port connection list. */
	mutex_t lock;			/**< Lock covering connection. */
	ipc_port_t *port;		/**< Port that the connection is on. */
	refcount_t count;		/**< Count of handles to either end of the connection. */
	ipc_endpoint_t endpoints[2];	/**< Endpoints for each end of the connection. */
	semaphore_t *sem;		/**< Pointer to semaphore used during connection setup. */
} ipc_connection_t;

/** In-kernel IPC message structure. */
typedef struct ipc_message {
	list_t header;			/**< Link to message queue. */
	uint32_t type;			/**< Type of message. */
	size_t size;			/**< Size of message data. */
	char data[];			/**< Message data. */
} ipc_message_t;

/** Definitions for endpoint IDs. */
#define SERVER_ENDPOINT		0	/**< Endpoint for the listener. */
#define CLIENT_ENDPOINT		1	/**< Endpoint for the opener. */

/** Cache for port/connection structures. */
static slab_cache_t *ipc_port_cache;
static slab_cache_t *ipc_connection_cache;

/** Arena for port ID allocations. */
static vmem_t *port_id_arena;

/** Tree of all open ports. */
static AVL_TREE_DECLARE(port_tree);
static MUTEX_DECLARE(port_tree_lock, 0);

/** Port object constructor.
 * @param obj		Object to construct.
 * @param data		Cache data (unused).
 * @param mmflag	Allocation flags.
 * @return		0 on success, negative error code on failure. */
static int ipc_port_ctor(void *obj, void *data, int mmflag) {
	ipc_port_t *port = obj;

	mutex_init(&port->lock, "ipc_port_lock", 0);
	list_init(&port->connections);
	list_init(&port->waiting);
	semaphore_init(&port->conn_sem, "ipc_listen_sem", 0);
	semaphore_init(&port->conn_sem, "ipc_conn_sem", 0);
	notifier_init(&port->conn_notifier, port);
	return 0;
}

/** Connection object constructor.
 * @param obj		Object to construct.
 * @param data		Cache data (unused).
 * @param mmflag	Allocation flags.
 * @return		0 on success, negative error code on failure. */
static int ipc_connection_ctor(void *obj, void *data, int mmflag) {
	ipc_connection_t *conn = obj;
	size_t i;

	list_init(&conn->header);
	mutex_init(&conn->lock, "ipc_connection_lock", 0);
	for(i = 0; i < 2; i++) {
		list_init(&conn->endpoints[i].messages);
		semaphore_init(&conn->endpoints[i].space_sem, "ipc_space_sem", IPC_QUEUE_MAX);
		semaphore_init(&conn->endpoints[i].data_sem, "ipc_data_sem", 0);
		notifier_init(&conn->endpoints[i].msg_notifier, &conn->endpoints[i]);
		notifier_init(&conn->endpoints[i].hangup_notifier, &conn->endpoints[i]);
	}
	return 0;
}

/** Closes a handle to an IPC port.
 * @param handle	Handle being closed. */
static void port_object_close(handle_t *handle) {
	ipc_port_t *port = (ipc_port_t *)handle->object;
	ipc_connection_t *conn;
	size_t i;

	if(refcount_dec(&port->count) == 0) {
		/* Take the port tree lock across the operation to prevent
		 * any threads from trying to open/connect to the port. */
		mutex_lock(&port_tree_lock);
		mutex_lock(&port->lock);

		/* Cancel all in-progress connection attempts. */
		LIST_FOREACH(&port->waiting, iter) {
			conn = list_entry(iter, ipc_connection_t, header);

			list_remove(&conn->header);
			conn->port = NULL;
			semaphore_up(conn->sem, 1);
		}

		/* Terminate all currently open connections. We do this by
		 * disconnecting both ends of the connection from each other. */
		LIST_FOREACH_SAFE(&port->connections, iter) {
			conn = list_entry(iter, ipc_connection_t, header);

			mutex_lock(&conn->lock);

			for(i = 0; i < 2; i++) {
				waitq_wake_all(&conn->endpoints[i].space_sem.queue);
				waitq_wake_all(&conn->endpoints[i].data_sem.queue);
				notifier_run(&conn->endpoints[i].hangup_notifier, NULL, false);
				conn->endpoints[i].remote = NULL;
			}

			list_remove(&conn->header);
			conn->port = NULL;

			mutex_unlock(&conn->lock);
		}

		avl_tree_remove(&port_tree, port->id);
		mutex_unlock(&port_tree_lock);
		mutex_unlock(&port->lock);

		dprintf("ipc: destroyed port %d (%p)\n", port->id, port);
		vmem_free(port_id_arena, (vmem_resource_t)port->id, 1);
		slab_cache_free(ipc_port_cache, port);
	}
}

/** Signal that a port event is being waited for.
 * @param wait		Wait information structure.
 * @return		0 on success, negative error code on failure. */
static int port_object_wait(object_wait_t *wait) {
	ipc_port_t *port = (ipc_port_t *)wait->handle->object;
	int ret = 0;

	mutex_lock(&port->lock);

	switch(wait->event) {
	case PORT_EVENT_CONNECTION:
		if(semaphore_count(&port->conn_sem)) {
			object_wait_callback(wait);
		} else {
			notifier_register(&port->conn_notifier, object_wait_notifier, wait);
		}
		break;
	default:
		ret = -ERR_PARAM_INVAL;
		break;
	}

	mutex_unlock(&port->lock);
	return ret;
}

/** Stop waiting for a port event.
 * @param wait		Wait information structure. */
static void port_object_unwait(object_wait_t *wait) {
	ipc_port_t *port = (ipc_port_t *)wait->handle->object;

	switch(wait->event) {
	case PORT_EVENT_CONNECTION:
		notifier_unregister(&port->conn_notifier, object_wait_notifier, wait);
		break;
	}
}

/** IPC port object type. */
static object_type_t port_object_type = {
	.id = OBJECT_TYPE_PORT,
	.close = port_object_close,
	.wait = port_object_wait,
	.unwait = port_object_unwait,
};

/** Closes a handle to a connection.
 * @param handle	Handle being closed. */
static void connection_object_close(handle_t *handle) {
	ipc_connection_t *conn = (ipc_connection_t *)handle->object;
	ipc_endpoint_t *endpoint = handle->data;
	ipc_message_t *message;
	int ret;

	assert(endpoint->conn == conn);

	mutex_lock(&conn->lock);

	/* If the remote is open, detach it from this end, and wake all threads
	 * waiting for space on this end or messages on the remote end. They
	 * will detect that we have set remote to NULL and return an error. */
	if(endpoint->remote) {
		waitq_wake_all(&endpoint->space_sem.queue);
		waitq_wake_all(&endpoint->remote->data_sem.queue);
		notifier_run(&endpoint->remote->hangup_notifier, NULL, false);
		endpoint->remote->remote = NULL;
		endpoint->remote = NULL;
	}

	/* Discard all currently queued messages. */
	LIST_FOREACH_SAFE(&endpoint->messages, iter) {
		message = list_entry(iter, ipc_message_t, header);

		/* We must change the semaphores even though the endpoint is
		 * being freed as they are initialised in the slab constructor
		 * rather than after being allocated. */
		ret = semaphore_down_etc(&endpoint->data_sem, 0, 0);
		assert(ret == 0);
		semaphore_up(&endpoint->space_sem, 1);

		list_remove(&message->header);
		kfree(message);
	}

	assert(semaphore_count(&endpoint->data_sem) == 0);
	assert(semaphore_count(&endpoint->space_sem) == IPC_QUEUE_MAX);
	assert(notifier_empty(&endpoint->msg_notifier));
	assert(notifier_empty(&endpoint->hangup_notifier));

	dprintf("ipc: destroyed endpoint %p (conn: %p, port: %d)\n", endpoint,
	        conn, (conn->port) ? conn->port->id : -1);
	mutex_unlock(&conn->lock);

	/* Free the connection if necessary. */
	if(refcount_dec(&conn->count) == 0) {
		/* This is a bit crap: take the port tree lock to ensure that
		 * the port isn't closed while detaching the connection from
		 * it. */
		mutex_lock(&port_tree_lock);
		if(conn->port) {
			mutex_lock(&conn->port->lock);
			list_remove(&conn->header);
			mutex_unlock(&conn->port->lock);
		}
		mutex_unlock(&port_tree_lock);

		dprintf("ipc: destroyed connection %p (port: %d)\n", conn,
		        (conn->port) ? conn->port->id : -1);
		object_destroy(&conn->obj);
		slab_cache_free(ipc_connection_cache, conn);
	}
}

/** Signal that a connection event is being waited for.
 * @param wait		Wait information structure.
 * @return		0 on success, negative error code on failure. */
static int connection_object_wait(object_wait_t *wait) {
	ipc_connection_t *conn = (ipc_connection_t *)wait->handle->object;
	ipc_endpoint_t *endpoint = wait->handle->data;
	int ret = 0;

	mutex_lock(&conn->lock);

	switch(wait->event) {
	case CONNECTION_EVENT_HANGUP:
		if(!endpoint->remote) {
			object_wait_callback(wait);
		} else {
			notifier_register(&endpoint->hangup_notifier, object_wait_notifier, wait);
		}
		break;
	case CONNECTION_EVENT_MESSAGE:
		if(semaphore_count(&endpoint->data_sem)) {
			object_wait_callback(wait);
		} else {
			notifier_register(&endpoint->msg_notifier, object_wait_notifier, wait);
		}
		break;

	default:
		ret = -ERR_PARAM_INVAL;
		break;
	}

	mutex_unlock(&conn->lock);
	return ret;
}

/** Stop waiting for a connection event.
 * @param wait		Wait information structure. */
static void connection_object_unwait(object_wait_t *wait) {
	ipc_endpoint_t *endpoint = wait->handle->data;

	switch(wait->event) {
	case CONNECTION_EVENT_HANGUP:
		notifier_unregister(&endpoint->hangup_notifier, object_wait_notifier, wait);
		break;
	case CONNECTION_EVENT_MESSAGE:
		notifier_unregister(&endpoint->msg_notifier, object_wait_notifier, wait);
		break;
	}
}

/** IPC connection object type. */
static object_type_t connection_object_type = {
	.id = OBJECT_TYPE_CONNECTION,
	.close = connection_object_close,
	.wait = connection_object_wait,
	.unwait = connection_object_unwait,
};

/** Create a new IPC port.
 * @return		Handle to the port on success, negative error code on
 *			failure. */
handle_id_t sys_ipc_port_create(void) {
	ipc_port_t *port;
	handle_id_t ret;

	port = slab_cache_alloc(ipc_port_cache, MM_SLEEP);
	if(!(port->id = vmem_alloc(port_id_arena, 1, 0))) {
		slab_cache_free(ipc_port_cache, port);
		return -ERR_RESOURCE_UNAVAIL;
	}
	object_init(&port->obj, &port_object_type);
	refcount_set(&port->count, 1);

	if((ret = handle_create(&port->obj, NULL, curr_proc, 0, NULL)) < 0) {
		object_destroy(&port->obj);
		vmem_free(port_id_arena, port->id, 1);
		slab_cache_free(ipc_port_cache, port);
		return ret;
	}

	mutex_lock(&port_tree_lock);
	avl_tree_insert(&port_tree, port->id, port, NULL);
	dprintf("ipc: created port %d(%p) (process: %d)\n", port->id, port, curr_proc->id);
	mutex_unlock(&port_tree_lock);
	return ret;
}

/** Open a handle to an IPC port.
 * @param id		ID of the port to open.
 * @return		Handle to the port on success, negative error code on
 *			failure. */
handle_id_t sys_ipc_port_open(port_id_t id) {
	ipc_port_t *port;
	handle_t *handle;
	handle_id_t ret;

	mutex_lock(&port_tree_lock);

	if(!(port = avl_tree_lookup(&port_tree, id))) {
		mutex_unlock(&port_tree_lock);
		return -ERR_NOT_FOUND;
	}

	refcount_inc(&port->count);
	mutex_unlock(&port_tree_lock);

	handle_create(&port->obj, NULL, NULL, 0, &handle);
	ret = handle_attach(curr_proc, handle, 0);
	handle_release(handle);
	return ret;
}

/** Get the ID of a port.
 * @param handle	Handle to port to get ID of.
 * @return		ID of port on success, negative error code on failure. */
port_id_t sys_ipc_port_id(handle_id_t handle) {
	ipc_port_t *port;
	port_id_t ret;
	handle_t *obj;

	if((ret = handle_lookup(curr_proc, handle, OBJECT_TYPE_PORT, &obj)) != 0) {
		return ret;
	}

	port = (ipc_port_t *)obj->object;
	ret = port->id;
	handle_release(obj);
	return ret;
}

/** Wait for a connection attempt on a port.
 * @param handle	Handle to port to listen on.
 * @param timeout	Timeout in microseconds. If 0, the function will return
 *			immediately if nothing is currently attempting to
 *			connect to the port. If -1, the function will block
 *			indefinitely until a connection is made.
 * @return		Handle to the caller's end of the connection on
 *			success, negative error code on failure. */
handle_id_t sys_ipc_port_listen(handle_id_t handle, useconds_t timeout) {
	ipc_connection_t *conn = NULL;
	ipc_port_t *port;
	handle_id_t ret;
	handle_t *obj;

	if((ret = handle_lookup(curr_proc, handle, OBJECT_TYPE_PORT, &obj)) != 0) {
		return ret;
	}
	port = (ipc_port_t *)obj->object;

	/* Try to get a connection. FIXME: This does not handle timeout
	 * properly - implement SYNC_ABSOLUTE. */
	while(!conn) {
		if((ret = semaphore_down_etc(&port->conn_sem, timeout, SYNC_INTERRUPTIBLE)) != 0) {
			handle_release(obj);
			return ret;
		}

		mutex_lock(&port->lock);
		if(!list_empty(&port->waiting)) {
			conn = list_entry(port->waiting.next, ipc_connection_t, header);
			break;
		}
		mutex_unlock(&port->lock);
	}

	/* Create a handle to the endpoint. */
	refcount_inc(&conn->count);
	if((ret = handle_create(&conn->obj, &conn->endpoints[SERVER_ENDPOINT], curr_proc, 0, NULL)) < 0) {
		semaphore_up(&port->conn_sem, 1);
		mutex_unlock(&port->lock);
		handle_release(obj);
		return ret;
	}

	list_append(&port->connections, &conn->header);
	conn->port = port;

	/* Wake the thread that made the connection. */
	semaphore_up(conn->sem, 1);
	mutex_unlock(&port->lock);
	handle_release(obj);
	return ret;
}

/** Open an IPC connection to a port.
 * @param id		Port ID to connect to.
 * @return		Handle referring to caller's end of connection on
 *			success, negative error code on failure. */
handle_id_t sys_ipc_connection_open(port_id_t id) {
	semaphore_t sem = SEMAPHORE_INITIALISER(sem, "ipc_open_sem", 0);
	ipc_connection_t *conn;
	handle_id_t handle;
	ipc_port_t *port;
	int i, ret;

	mutex_lock(&port_tree_lock);

	if(!(port = avl_tree_lookup(&port_tree, id))) {
		mutex_unlock(&port_tree_lock);
		return -ERR_NOT_FOUND;
	}

	mutex_lock(&port->lock);
	mutex_unlock(&port_tree_lock);

	/* Create a connection structure. */
	conn = slab_cache_alloc(ipc_connection_cache, MM_SLEEP);
	object_init(&conn->obj, &connection_object_type);
	refcount_set(&conn->count, 1);
	for(i = 0; i < 2; i++) {
		conn->endpoints[i].conn = conn;
		conn->endpoints[i].remote = &conn->endpoints[(i + 1) % 2];
	}
	conn->port = port;
	conn->sem = &sem;

	/* Create a handle now, as we do not want to find that we cannot create
	 * the handle after the connection has been accepted. */
	if((handle = handle_create(&conn->obj, &conn->endpoints[CLIENT_ENDPOINT], curr_proc, 0, NULL)) < 0) {
		object_destroy(&conn->obj);
		slab_cache_free(ipc_connection_cache, conn);
		mutex_unlock(&port->lock);
		return handle;
	}

	/* Place the connection in the port's waiting list. */
	list_append(&port->waiting, &conn->header);
	semaphore_up(&port->conn_sem, 1);
	notifier_run(&port->conn_notifier, NULL, false);
	mutex_unlock(&port->lock);

	/* Wait for the connection to be accepted. */
	if((ret = semaphore_down_etc(&sem, -1, SYNC_INTERRUPTIBLE)) != 0) {
		/* Take the port tree lock to ensure that the port doesn't get
		 * freed. This is a bit naff, but oh well. */
		mutex_lock(&port_tree_lock);
		if(conn->port) {
			mutex_lock(&conn->port->lock);
			list_remove(&conn->header);
			mutex_unlock(&conn->port->lock);
			conn->port = NULL;
		}
		mutex_unlock(&port_tree_lock);
		handle_detach(curr_proc, handle);
		return ret;
	} else if(conn->port == NULL) {
		handle_detach(curr_proc, handle);
		return -ERR_NOT_FOUND;
	}

	return handle;
}

/** Send a message on a connection.
 *
 * Queues a message at the other end of a connection. Messages are sent
 * asynchronously. This function can block if the recipient's message queue is
 * full.
 *
 * @param handle	Handle to connection.
 * @param type		Type of message.
 * @param buf		Message data buffer. Does not need to be specified if
 *			size is 0.
 * @param size		Size of data buffer (can be 0).
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_ipc_message_send(handle_id_t handle, uint32_t type, const void *buf, size_t size) {
	ipc_endpoint_t *endpoint = NULL;
	ipc_message_t *message;
	handle_t *obj = NULL;
	bool state;
	int ret;

	if((!buf && size) || size > IPC_MESSAGE_MAX) {
		return -ERR_PARAM_INVAL;
	}

	/* Allocate a message structure, and copy the data buffer into it. */
	message = kmalloc(sizeof(ipc_message_t) + size, MM_SLEEP);
	list_init(&message->header);
	message->type = type;
	message->size = size;
	if(size) {
		if((ret = memcpy_from_user(message->data, buf, size)) != 0) {
			goto fail;
		}
	}

	/* Look up the handle. */
	if((ret = handle_lookup(curr_proc, handle, OBJECT_TYPE_CONNECTION, &obj)) != 0) {
		goto fail;
	}
	endpoint = obj->data;
	mutex_lock(&endpoint->conn->lock);

	/* Wait for space in the remote message queue. The unlock/wait needs to
	 * be atomic in order to interact properly with connection_object_close().
	 * FIXME: Should integrate this in the semaphore API. */
	if(endpoint->remote) {
		state = waitq_sleep_prepare(&endpoint->remote->space_sem.queue);
		if(endpoint->remote->space_sem.count) {
			--endpoint->remote->space_sem.count;
			spinlock_unlock_ni(&endpoint->remote->space_sem.queue.lock);
			intr_restore(state);
		} else {
			mutex_unlock(&endpoint->conn->lock);
			ret = waitq_sleep_unsafe(&endpoint->remote->space_sem.queue, -1, SYNC_INTERRUPTIBLE, state);
			mutex_lock(&endpoint->conn->lock);
			if(ret != 0) {
				goto fail;
			}
		}
	}

	/* If remote is now NULL the remote process has hung up or the port
	 * has disappeared */
	if(!endpoint->remote) {
		ret = -ERR_DEST_UNREACHABLE;
		goto fail;
	}

	/* Queue the message. */
	list_append(&endpoint->remote->messages, &message->header);
	semaphore_up(&endpoint->remote->data_sem, 1);
	notifier_run(&endpoint->remote->msg_notifier, NULL, false);

	mutex_unlock(&endpoint->conn->lock);
	handle_release(obj);
	return 0;
fail:
	if(obj) {
		mutex_unlock(&endpoint->conn->lock);
		handle_release(obj);
	}
	kfree(message);
	return ret;
}

/** Send multiple messages on a connection.
 *
 * Queues multiple messages at the other end of a connection. Messages are sent
 * asynchronously. They are queued in the order that they are found in the
 * array. The operation is atomic: the destination will not receive any of the
 * messages until all have been successfully queued, and if a failure occurs,
 * it will receive none of the messages. This function can block if the
 * recipient's message queue is full.
 *
 * @param handle	Handle to connection.
 * @param vec		Array of structures describing messages to send.
 * @param count		Number of messages in array.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_ipc_message_sendv(handle_id_t handle, ipc_message_vector_t *vec, size_t count) {
	return -ERR_NOT_IMPLEMENTED;
}

/** Wait until a message arrives.
 * @param endpoint	Endpoint to wait on. Connection should be locked.
 * @param timeout	Timeout.
 * @param messagep	Where to store pointer to message structure.
 * @return		0 on success, negative error code on failure. */
static int wait_for_message(ipc_endpoint_t *endpoint, useconds_t timeout, ipc_message_t **messagep) {
	bool state;
	int ret;

	/* Check if anything can send us a message. */
	if(!endpoint->remote) {
		return -ERR_DEST_UNREACHABLE;
	}

	/* Wait for data in our message queue. The unlock/wait needs to be
	 * atomic in order to interact properly with connection_object_close().
	 * FIXME: Integrate this in semaphore API. */
	state = waitq_sleep_prepare(&endpoint->data_sem.queue);
	if(endpoint->data_sem.count) {
		--endpoint->data_sem.count;
		spinlock_unlock_ni(&endpoint->data_sem.queue.lock);
		intr_restore(state);
	} else {
		mutex_unlock(&endpoint->conn->lock);
		ret = waitq_sleep_unsafe(&endpoint->data_sem.queue, timeout, SYNC_INTERRUPTIBLE, state);
		mutex_lock(&endpoint->conn->lock);
		if(ret != 0) {
			return ret;
		}
	}

	/* Recheck that we have a remote end, as it may have hung up. If there
	 * is a message in this case we must re-up the semaphore. */
	if(!endpoint->remote) {
		if(!list_empty(&endpoint->messages)) {
			semaphore_up(&endpoint->data_sem, 1);
		}
		return -ERR_DEST_UNREACHABLE;
	}

	assert(!list_empty(&endpoint->messages));
	*messagep = list_entry(endpoint->messages.next, ipc_message_t, header);
	return 0;
}

/** Get details of the next message on a connection.
 *
 * Waits until a message arrives on a connection, and then returns the type and
 * size of the message, leaving the message on the queue.
 *
 * @param handle	Handle to connection.
 * @param timeout	Timeout in microseconds. If 0, the function will return
 *			immediately if no messages are queued to the caller.
 *			If -1, the function will block indefinitely until a
 *			message is received.
 * @param typep		Where to store message type ID.
 * @param sizep		Where to store message data size.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_ipc_message_peek(handle_id_t handle, useconds_t timeout, uint32_t *typep, size_t *sizep) {
	ipc_endpoint_t *endpoint;
	ipc_message_t *message;
	handle_t *obj;
	int ret;

	/* Look up the handle. */
	if((ret = handle_lookup(curr_proc, handle, OBJECT_TYPE_CONNECTION, &obj)) != 0) {
		return ret;
	}
	endpoint = obj->data;
	mutex_lock(&endpoint->conn->lock);

	/* Wait for a message. */
	if((ret = wait_for_message(endpoint, timeout, &message)) != 0) {
		mutex_unlock(&endpoint->conn->lock);
		handle_release(obj);
		return ret;
	}

	if(typep && (ret = memcpy_to_user(typep, &message->type, sizeof(uint32_t))) != 0) {
		goto out;
	}
	if(sizep && (ret = memcpy_to_user(sizep, &message->size, sizeof(size_t))) != 0) {
		goto out;
	}
out:
	semaphore_up(&endpoint->data_sem, 1);
	mutex_unlock(&endpoint->conn->lock);
	handle_release(obj);
	return ret;
}

/** Receive a message from a connection.
 *
 * Waits until a message arrives on a connection and then copies it's data into
 * the supplied buffers.
 *
 * Note that if the message being received is larger than the provided buffer,
 * the extra data will be discarded. This behaviour can be exploited to discard
 * an unwanted message, by giving a zero size.

 * @param handle	Handle to connection.
 * @param timeout	Timeout in microseconds. If 0, the function will return
 *			immediately if no messages are queued to the caller.
 *			If -1, the function will block indefinitely until a
 *			message is received.
 * @param typep		Where to store message type ID (can be NULL).
 * @param buf		Buffer to copy message data to (can be NULL if size is
 *			specified as 0).
 * @param size		Size of supplied buffer.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_ipc_message_receive(handle_id_t handle, useconds_t timeout, uint32_t *typep,
                            void *buf, size_t size) {
	ipc_message_t *message = NULL;
	ipc_endpoint_t *endpoint;
	handle_t *obj;
	int ret;

	if(size > 0 && !buf) {
		return -ERR_PARAM_INVAL;
	}

	/* Look up the handle. */
	if((ret = handle_lookup(curr_proc, handle, OBJECT_TYPE_CONNECTION, &obj)) != 0) {
		return ret;
	}
	endpoint = obj->data;
	mutex_lock(&endpoint->conn->lock);

	/* Wait for a message. */
	if((ret = wait_for_message(endpoint, timeout, &message)) != 0) {
		goto fail;
	}

	if(typep && (ret = memcpy_to_user(typep, &message->type, sizeof(uint32_t))) != 0) {
		goto fail;
	}
	if(size > 0) {
		if((ret = memcpy_to_user(buf, message->data, message->size)) != 0) {
			goto fail;
		}
	}

	/* Remove the message from the queue. */
	list_remove(&message->header);
	kfree(message);
	semaphore_up(&endpoint->space_sem, 1);

	mutex_unlock(&endpoint->conn->lock);
	handle_release(obj);
	return 0;
fail:
	if(message) {
		semaphore_up(&endpoint->data_sem, 1);
	}
	mutex_unlock(&endpoint->conn->lock);
	handle_release(obj);
	return ret;
}

/** Print information about IPC ports.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG status code. */
int kdbg_cmd_port(int argc, char **argv) {
	ipc_connection_t *conn;
	ipc_port_t *port;
	unative_t val;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [<port ID>]\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints either a list of all IPC ports or information about a certain port.\n");
		return KDBG_OK;
	} else if(argc == 1) {
		kprintf(LOG_NONE, "ID    Count  Waiting\n");
		kprintf(LOG_NONE, "==    =====  =======\n");

		AVL_TREE_FOREACH(&port_tree, iter) {
			port = avl_tree_entry(iter, ipc_port_t);

			kprintf(LOG_NONE, "%-5" PRIu32 " %-6d %u\n", port->id,
			        refcount_get(&port->count),
			        semaphore_count(&port->conn_sem));
		}

		return KDBG_OK;
	} else if(argc == 2) {
		if(kdbg_parse_expression(argv[1], &val, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		} else if(!(port = avl_tree_lookup(&port_tree, val))) {
			kprintf(LOG_NONE, "Invalid port ID.\n");
			return KDBG_FAIL;
		}

		kprintf(LOG_NONE, "Port %p(%d)\n", port, port->id);
		kprintf(LOG_NONE, "=================================================\n");

		kprintf(LOG_NONE, "Locked:  %d (%" PRId32 ")\n", atomic_get(&port->lock.locked),
		        (port->lock.holder) ? port->lock.holder->id : -1);
		kprintf(LOG_NONE, "Count:   %d\n", refcount_get(&port->count));
		kprintf(LOG_NONE, "Waiting (%u):\n", semaphore_count(&port->conn_sem));
		LIST_FOREACH(&port->waiting, iter) {
			conn = list_entry(iter, ipc_connection_t, header);
			kprintf(LOG_NONE, " %p: endpoint[0] = %p endpoint[1] = %p\n",
			        conn, &conn->endpoints[0], &conn->endpoints[1]);
		}
		kprintf(LOG_NONE, "Connections:\n");
		LIST_FOREACH(&port->connections, iter) {
			conn = list_entry(iter, ipc_connection_t, header);
			kprintf(LOG_NONE, " %p: endpoint[0] = %p endpoint[1] = %p\n",
			        conn, &conn->endpoints[0], &conn->endpoints[1]);
		}
		return KDBG_OK;
	} else {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}
}

/** Print information about an IPC endpoint.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG status code. */
int kdbg_cmd_endpoint(int argc, char **argv) {
	ipc_endpoint_t *endpoint;
	ipc_message_t *message;
	unative_t val;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s <addr>\n\n", argv[0]);

		kprintf(LOG_NONE, "Shows information about an IPC endpoint. The address can be obtained by\n");
		kprintf(LOG_NONE, "looking at the data field of an IPC handle.\n");
		return KDBG_OK;
	} else if(argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	if(kdbg_parse_expression(argv[1], &val, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	}
	endpoint = (ipc_endpoint_t *)((ptr_t)val);

	kprintf(LOG_NONE, "Endpoint %p\n", endpoint);
	kprintf(LOG_NONE, "=================================================\n");

	kprintf(LOG_NONE, "Locked: %d (%p) (%" PRId32 ")\n", atomic_get(&endpoint->conn->lock.locked),
	        (endpoint->conn->lock.holder) ? endpoint->conn->lock.holder->id : -1);
	kprintf(LOG_NONE, "Space:  %u\n", semaphore_count(&endpoint->space_sem));
	kprintf(LOG_NONE, "Data:   %u\n", semaphore_count(&endpoint->data_sem));
	kprintf(LOG_NONE, "Remote: %p\n\n", endpoint->remote);

	kprintf(LOG_NONE, "Messages:\n");
	LIST_FOREACH(&endpoint->messages, iter) {
		message = list_entry(iter, ipc_message_t, header);

		kprintf(LOG_NONE, " %p: type %" PRIu32 ", size: %zu, buffer: %p\n",
		        message, message->type, message->size, message->data);
	}

	return KDBG_OK;
}

/** Initialise the IPC slab caches. */
static void __init_text ipc_init(void) {
	port_id_arena = vmem_create("port_id_arena", 1, 65535, 1, NULL, NULL, NULL, 0, 0, MM_FATAL);
	ipc_port_cache = slab_cache_create("ipc_port_cache", sizeof(ipc_port_t),
	                                   0, ipc_port_ctor, NULL, NULL, NULL,
	                                   SLAB_DEFAULT_PRIORITY, NULL, 0, MM_FATAL);
	ipc_connection_cache = slab_cache_create("ipc_connection_cache", sizeof(ipc_connection_t),
	                                         0, ipc_connection_ctor, NULL, NULL, NULL,
	                                         SLAB_DEFAULT_PRIORITY, NULL, 0, MM_FATAL);
}
INITCALL(ipc_init);
