#ifdef PHOSPHOR_HAS_LIBCURL

#include "phosphor/certs.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>

/* ---- response buffer ---- */

typedef struct {
    char   *data;
    size_t  len;
    size_t  cap;
} ph_curl_buf_t;

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    ph_curl_buf_t *buf = userdata;
    size_t total = size * nmemb;

    if (buf->len + total >= buf->cap) {
        size_t new_cap = (buf->cap + total) * 2;
        char *new_data = ph_realloc(buf->data, new_cap);
        if (!new_data) return 0;
        buf->data = new_data;
        buf->cap = new_cap;
    }
    memcpy(buf->data + buf->len, ptr, total);
    buf->len += total;
    buf->data[buf->len] = '\0';
    return total;
}

/* ---- header capture ---- */

typedef struct {
    const char *name;
    char       *value;
} ph_header_capture_t;

static size_t header_cb(char *buffer, size_t size, size_t nitems,
                         void *userdata) {
    ph_header_capture_t *cap = userdata;
    size_t total = size * nitems;

    size_t name_len = strlen(cap->name);
    if (total > name_len + 1 &&
        strncasecmp(buffer, cap->name, name_len) == 0 &&
        buffer[name_len] == ':') {
        /* skip ": " */
        const char *val = buffer + name_len + 1;
        while (*val == ' ') val++;

        /* trim trailing \r\n */
        size_t vlen = total - (size_t)(val - buffer);
        while (vlen > 0 && (val[vlen - 1] == '\r' || val[vlen - 1] == '\n'))
            vlen--;

        ph_free(cap->value);
        cap->value = ph_alloc(vlen + 1);
        if (cap->value) {
            memcpy(cap->value, val, vlen);
            cap->value[vlen] = '\0';
        }
    }
    return total;
}

/* ---- multi-header capture for POST responses ---- */

typedef struct {
    ph_header_capture_t nonce;
    ph_header_capture_t location;
} ph_post_headers_t;

static size_t post_header_cb(char *buffer, size_t size, size_t nitems,
                               void *userdata) {
    ph_post_headers_t *h = userdata;
    size_t total = size * nitems;
    header_cb(buffer, size, nitems, &h->nonce);
    header_cb(buffer, size, nitems, &h->location);
    return total;
}

/* ---- public API ---- */

ph_result_t ph_acme_http_get(const char *url,
                               char **out_body, size_t *out_len,
                               ph_error_t **err) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "curl_easy_init failed");
        return PH_ERR;
    }

    ph_curl_buf_t buf = {.data = ph_alloc(4096), .len = 0, .cap = 4096};
    if (!buf.data) { curl_easy_cleanup(curl); return PH_ERR; }
    buf.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "phosphor-acme/1.0");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "curl GET %s: %s", url, curl_easy_strerror(res));
        ph_free(buf.data);
        curl_easy_cleanup(curl);
        return PH_ERR;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code < 200 || http_code >= 300) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "HTTP GET %s returned %ld", url, http_code);
        ph_free(buf.data);
        return PH_ERR;
    }

    *out_body = buf.data;
    if (out_len) *out_len = buf.len;
    return PH_OK;
}

ph_result_t ph_acme_http_head(const char *url,
                                const char *header_name,
                                char **out_header_value,
                                ph_error_t **err) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "curl_easy_init failed");
        return PH_ERR;
    }

    ph_header_capture_t cap = {.name = header_name, .value = NULL};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &cap);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "phosphor-acme/1.0");

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "curl HEAD %s: %s", url, curl_easy_strerror(res));
        ph_free(cap.value);
        return PH_ERR;
    }

    if (!cap.value) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "header '%s' not found in HEAD %s", header_name, url);
        return PH_ERR;
    }

    *out_header_value = cap.value;
    return PH_OK;
}

ph_result_t ph_acme_http_post(const char *url,
                                const char *jws_body,
                                char **out_body, size_t *out_len,
                                char **out_nonce,
                                char **out_location,
                                int *out_http_status,
                                ph_error_t **err) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "curl_easy_init failed");
        return PH_ERR;
    }

    ph_curl_buf_t buf = {.data = ph_alloc(4096), .len = 0, .cap = 4096};
    if (!buf.data) { curl_easy_cleanup(curl); return PH_ERR; }
    buf.data[0] = '\0';

    ph_post_headers_t hdrs = {
        .nonce    = {.name = "Replay-Nonce", .value = NULL},
        .location = {.name = "Location",     .value = NULL},
    };

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers,
        "Content-Type: application/jose+json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jws_body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, post_header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &hdrs);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "phosphor-acme/1.0");

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "curl POST %s: %s", url, curl_easy_strerror(res));
        ph_free(buf.data);
        ph_free(hdrs.nonce.value);
        ph_free(hdrs.location.value);
        return PH_ERR;
    }

    if (out_http_status) *out_http_status = (int)http_code;
    if (out_nonce) *out_nonce = hdrs.nonce.value;
    else ph_free(hdrs.nonce.value);
    if (out_location) *out_location = hdrs.location.value;
    else ph_free(hdrs.location.value);
    *out_body = buf.data;
    if (out_len) *out_len = buf.len;
    return PH_OK;
}

#endif /* PHOSPHOR_HAS_LIBCURL */
