#pragma once

typedef enum _CGSS_STREAM_SEEK_ORIGIN {
    CGSS_ORIGIN_BEGIN = 0,
    CGSS_ORIGIN_CURRENT = 1,
    CGSS_ORIGIN_END = 2,
    _CGSS_ORIGIN_FORCE_DWORD = 0x7fffffff,
} CGSS_STREAM_SEEK_ORIGIN;
