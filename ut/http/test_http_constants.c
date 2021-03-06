/**
 * @copyright
 * Copyright (c) 2015-2017 Stanislav Ivochkin
 * Licensed under the MIT License (see LICENSE)
 */

#include <ut.h>
#include <dfk/http/constants.h>
#include <dfk/internal.h>

TEST(http, reason_phrase)
{
  /* For the most common error codes, check exact phrase */
  EXPECT(!strcmp(dfk_http_reason_phrase(DFK_HTTP_OK), "OK"));
  EXPECT(!strcmp(dfk_http_reason_phrase(DFK_HTTP_CREATED), "Created"));
  EXPECT(!strcmp(dfk_http_reason_phrase(DFK_HTTP_BAD_REQUEST), "Bad Request"));
  EXPECT(!strcmp(dfk_http_reason_phrase(DFK_HTTP_NOT_FOUND), "Not Found"));
  EXPECT(!strcmp(dfk_http_reason_phrase(DFK_HTTP_REQUEST_TIMEOUT),
      "Request Timeout"));
  EXPECT(!strcmp(dfk_http_reason_phrase(DFK_HTTP_INTERNAL_SERVER_ERROR),
      "Internal Server Error"));
  EXPECT(!strcmp(dfk_http_reason_phrase(DFK_HTTP_BAD_GATEWAY), "Bad Gateway"));
  EXPECT(!strcmp(dfk_http_reason_phrase(DFK_HTTP_GATEWAY_TIMEOUT),
      "Gateway Timeout"));

  /* For other codes, check presence of the phrase */
  dfk_http_status_e st[] = {
    DFK_HTTP_CONTINUE, DFK_HTTP_SWITCHING_PROTOCOLS, DFK_HTTP_PROCESSING,
    DFK_HTTP_OK, DFK_HTTP_CREATED, DFK_HTTP_ACCEPTED,
    DFK_HTTP_NON_AUTHORITATIVE_INFORMATION, DFK_HTTP_NO_CONTENT,
    DFK_HTTP_RESET_CONTENT, DFK_HTTP_PARTIAL_CONTENT, DFK_HTTP_MULTI_STATUS,
    DFK_HTTP_ALREADY_REPORTED, DFK_HTTP_IM_USED, DFK_HTTP_MULTIPLE_CHOICES,
    DFK_HTTP_MOVED_PERMANENTLY, DFK_HTTP_FOUND, DFK_HTTP_SEE_OTHER,
    DFK_HTTP_NOT_MODIFIED, DFK_HTTP_USE_PROXY, DFK_HTTP_SWITCH_PROXY,
    DFK_HTTP_TEMPORARY_REDIRECT, DFK_HTTP_PERMANENT_REDIRECT,
    DFK_HTTP_BAD_REQUEST, DFK_HTTP_UNAUTHORIZED, DFK_HTTP_PAYMENT_REQUIRED,
    DFK_HTTP_FORBIDDEN, DFK_HTTP_NOT_FOUND, DFK_HTTP_METHOD_NOT_ALLOWED,
    DFK_HTTP_NOT_ACCEPTABLE, DFK_HTTP_PROXY_AUTHENTICATION_REQUIRED,
    DFK_HTTP_REQUEST_TIMEOUT, DFK_HTTP_CONFLICT, DFK_HTTP_GONE,
    DFK_HTTP_LENGTH_REQUIRED, DFK_HTTP_PRECONDITION_FAILED,
    DFK_HTTP_PAYLOAD_TOO_LARGE, DFK_HTTP_URI_TOO_LONG,
    DFK_HTTP_UNSUPPORTED_MEDIA_TYPE, DFK_HTTP_RANGE_NOT_SATISFIABLE,
    DFK_HTTP_EXPECTATION_FAILED, DFK_HTTP_I_AM_A_TEAPOT,
    DFK_HTTP_MISDIRECTED_REQUEST, DFK_HTTP_UNPROCESSABLE_ENTITY,
    DFK_HTTP_LOCKED, DFK_HTTP_FAILED_DEPENDENCY, DFK_HTTP_UPGRADE_REQUIRED,
    DFK_HTTP_PRECONDITION_REQUIRED, DFK_HTTP_TOO_MANY_REQUESTS,
    DFK_HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE,
    DFK_HTTP_UNAVAILABLE_FOR_LEGAL_REASONS, DFK_HTTP_INTERNAL_SERVER_ERROR,
    DFK_HTTP_NOT_IMPLEMENTED, DFK_HTTP_BAD_GATEWAY,
    DFK_HTTP_SERVICE_UNAVAILABLE, DFK_HTTP_GATEWAY_TIMEOUT,
    DFK_HTTP_HTTP_VERSION_NOT_SUPPORTED, DFK_HTTP_VARIANT_ALSO_NEGOTIATES,
    DFK_HTTP_INSUFFICIENT_STORAGE, DFK_HTTP_LOOP_DETECTED,
    DFK_HTTP_NOT_EXTENDED, DFK_HTTP_NETWORK_AUTHENTICATION_REQUIRED
  };

  for (size_t i = 0; i < DFK_SIZE(st); ++i) {
    const char* rp = dfk_http_reason_phrase(st[i]);
    EXPECT(rp);
    EXPECT(rp[0] != '\0');
  }
}

TEST(http, reason_phrase_unknown)
{
  EXPECT(!strcmp(dfk_http_reason_phrase(0), "Unknown"));
}

