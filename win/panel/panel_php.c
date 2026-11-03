#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "phpdetect.h"

int panel_php_list(const wchar_t *root, const wchar_t *ini, const wchar_t *current, wchar_t (*labels)[MAX_PATH + 96],
                   wchar_t (*dirs)[MAX_PATH], int max)
{
    wchar_t laragon[MAX_PATH], xampp[MAX_PATH], extra[MAX_PATH];
    int i, n = 0;
    if (!phpd_find_laragon(laragon))
        laragon[0] = 0;
    if (!phpd_find_xampp(xampp))
        xampp[0] = 0;
    phpd_detect(laragon, xampp, root);
    if (GetPrivateProfileStringW(L"php", L"extra", L"", extra, MAX_PATH, ini) && extra[0])
        phpd_scan(extra, L"*", L"Custom", root);
    if (current && current[0])
        phpd_add(current, L"Current", root);
    qsort(phpd_list, phpd_count, sizeof(phpd_t), phpd_cmp);
    for (i = 0; i < phpd_count && n < max; ++i, ++n)
    {
        swprintf(labels[n], MAX_PATH + 96, L"PHP %d.%d.%d %s%s (%s)", phpd_list[i].major, phpd_list[i].minor,
                 phpd_list[i].patch, phpd_list[i].ts ? L"TS" : L"NTS", phpd_list[i].lsapi ? L" LSAPI" : L"",
                 phpd_list[i].source);
        wcsncpy(dirs[n], phpd_list[i].dir, MAX_PATH - 1);
        dirs[n][MAX_PATH - 1] = 0;
    }
    return n;
}
