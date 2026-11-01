#include "compat_internal.h"
#include <openssl/ssl.h>

int SSL_add_dir_cert_subjects_to_stack(STACK_OF(X509_NAME) *stack, const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    char path[PATH_MAX];
    int ok = 1;
    if (!d)
        return 0;
    while ((e = readdir(d)) != NULL)
    {
        if (e->d_type != DT_REG)
            continue;
        if (snprintf(path, sizeof(path), "%s/%s", dir, e->d_name) >= (int)sizeof(path))
        {
            ok = 0;
            break;
        }
        if (!SSL_add_file_cert_subjects_to_stack(stack, path))
        {
            ok = 0;
            break;
        }
    }
    closedir(d);
    return ok;
}
