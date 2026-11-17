#include "mime.h"
#include <string.h>
#include <strings.h>

const char *mime_type(const char *path)
{
    static const struct { const char *extension; const char *type; } types[] = {
        {".html", "text/html"}, {".css", "text/css"},
        {".js", "application/javascript"}, {".png", "image/png"},
        {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}
    };
    const char *extension = strrchr(path, '.');
    if (extension)
        for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i)
            if (strcasecmp(extension, types[i].extension) == 0)
                return types[i].type;
    return "application/octet-stream";
}
