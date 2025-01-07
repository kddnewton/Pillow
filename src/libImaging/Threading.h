typedef void (threading_destructor_t)(void *);

#ifdef _WIN32
#include <process.h>
#include <windows.h>

/* A key that can be used to retrieve the thread-local value. */
typedef DWORD threading_key_t;

static int
threading_ensure_fiber() {
    if (!GetCurrentFiber()) {
        if (!ConvertThreadToFiber(NULL)) {
            return GetLastError();
        }
    }
    return 0;
}

static void
threading_destructor_noop(void *value) {
    (void) value;
}

/* Initialize the threading key, with the given destructor callback. */
static int
threading_init(threading_key_t *key, threading_destructor_t *destructor) {
    if (!key) {
        return ERROR_INVALID_PARAMETER;
    }

    int error = wvls_ensure_fiber();
    if (error) {
        return error;
    }

    if (!destructor) {
        destructor = wvls_destructor_noop;
    }

    *key = FlsAlloc((PFLS_CALLBACK_FUNCTION) destructor);
    if (*key == FLS_OUT_OF_INDEXES) {
        return GetLastError();
    }

    return 0;
}

/* Retrieve the thread-local value for the given threading key. */
static void *
threading_get(threading_key_t key) {
    return (key && !threading_ensure_fiber()) ? FlsGetValue(key) : NULL;
}

/* Set the thread-local value for the given threading key. */
static int
threading_set(threading_key_t key, void *value) {
    if (!key) {
        return ERROR_INVALID_PARAMETER;
    }

    int error = threading_ensure_fiber();
    if (error) {
        return error;
    }

    /* If FlsGetValue returns NULL, it may be because the thread existed before
     * the DLL was loaded (e.g., main thread or threads created by other
     * libraries). In this case, we need to allocate memory for the TLS value
     * and set it.
     */
    LPVOID lpvData = FlsGetValue(key);

    if (!lpvData && GetLastError() != ERROR_SUCCESS) {
        lpvData = LocalAlloc(LPTR, 256);

        if (lpvData == NULL) {
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        if (!FlsSetValue(key, lpvData)) {
            LocalFree(lpvData);
            return GetLastError();
        }
    }

    if (!FlsSetValue(key, value)) {
        return GetLastError();
    }

    return 0;
}
#else
#include <pthread.h>

/* A key that can be used to retrieve the thread-local value. */
typedef pthread_key_t threading_key_t;

/* Initialize the threading key, with the given destructor callback. */
static int
threading_init(threading_key_t *key, threading_destructor_t *destructor) {
    return pthread_key_create(key, destructor);
}

/* Retrieve the thread-local value for the given threading key. */
static void *
threading_get(threading_key_t key) {
    return pthread_getspecific(key);
}

/* Set the thread-local value for the given threading key. */
static int
threading_set(threading_key_t key, void *value) {
    return pthread_setspecific(key, value);
}
#endif
