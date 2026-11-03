/*****************************************************************************
*    Open LiteSpeed is an open source HTTP server.                           *
*    Copyright (C) 2013 - 2022  LiteSpeed Technologies, Inc.                 *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation, either version 3 of the License, or       *
*    (at your option) any later version.                                     *
*                                                                            *
*    This program is distributed in the hope that it will be useful,         *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of          *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
*    GNU General Public License for more details.                            *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see http://www.gnu.org/licenses/.      *
*****************************************************************************/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include <main/lshttpdmain.h>
#include <http/useacme.h>
#include <http/iptoloc.h>

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <new>


//#define WANT_LSAN 1
#if !WANT_LSAN

int ls_lsan_off = 1;

extern "C" const char* __asan_default_options()
{ return "disable_coredump=0:unmap_shadow_on_exit=1:detect_leaks=0:abort_on_error=1"; }

#else

int ls_lsan_off = 0;

extern "C" const char* __asan_default_options()
{ return "disable_coredump=0:unmap_shadow_on_exit=1:abort_on_error=1"; }


extern "C" const char* __lsan_default_suppressions()
{
    return "leak:chunk_alloc";
};


#endif

extern "C" int __lsan_is_turned_off() { return ls_lsan_off; }


static LshttpdMain *s_pLshttpd = NULL;
#ifdef _WIN32
static int serverMain(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    ols_win_main_init(&argc, &argv);
    if (argc >= 3 && strcmp(argv[1], "-s") == 0)
        return ols_win_control(argv[2]);
    if (argc >= 2 && strcmp(argv[1], "--service") == 0)
        return ols_win_service_run();
    if (argc >= 3 && strcmp(argv[1], "--dev-sites") == 0)
    {
        const char *tld = "test";
        int withRoot = 0;
        for (int i = 3; i < argc; ++i)
        {
            if (strcmp(argv[i], "--root") == 0)
                withRoot = 1;
            else
                tld = argv[i];
        }
        return ols_dev_sites(argv[2], tld, withRoot);
    }
    if (argc >= 3 && strcmp(argv[1], "--new-site") == 0)
    {
        const char *tld = "test", *flavor = "blank";
        for (int i = 3; i < argc; ++i)
        {
            if (strcmp(argv[i], "--wordpress") == 0)
                flavor = "wordpress";
            else
                tld = argv[i];
        }
        return ols_new_site(argv[2], tld, flavor);
    }
    if (argc >= 3 && strcmp(argv[1], "--set-php") == 0)
        return ols_set_php(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "--lscgid") == 0)
        return ols_lscgid_main(argc, argv);
    if (argc >= 2 && strcmp(argv[1], "--import-apache") == 0)
        return ols_import_apache(argc - 2, argv + 2);
    if (argc >= 2 && strcmp(argv[1], "--import-nginx") == 0)
        return ols_import_nginx(argc - 2, argv + 2);
    if (argc >= 2 && strcmp(argv[1], "--acme-install") == 0)
        return ols_acme_main(argc - 2, argv + 2);
    if (argc >= 2 && strcmp(argv[1], "--modsec-install") == 0)
        return ols_modsec_main(argc - 2, argv + 2);
    if (argc >= 7 && strcmp(argv[1], "--acme-batch") == 0)
        return acmeBatchMain(argc, argv);
    if (argc >= 4 && strcmp(argv[1], "--test-ip2loc") == 0)
        return IpToLoc::testMain(argv[2], atoi(argv[3]));
    if (argc >= 2 && strcmp(argv[1], "--dev-hosts") == 0)
        return ols_dev_hosts(0);
    if (argc >= 2 && strcmp(argv[1], "--dev-hosts-clear") == 0)
        return ols_dev_hosts(1);
    if (argc >= 2 && strcmp(argv[1], "--install-service") == 0)
        return ols_win_service_install(1);
    if (argc >= 2 && strcmp(argv[1], "--uninstall-service") == 0)
        return ols_win_service_install(0);
    if (argc >= 4 && strcmp(argv[1], "--gen-cert") == 0)
        return ols_gen_cert(argv[2], argv[3], argc >= 5 ? argv[4] : "localhost");
    if (argc >= 4 && strcmp(argv[1], "--admin-password") == 0)
        return ols_set_admin_password(argv[2], argc >= 5 ? argv[4] : "admin", argv[3]);
    return serverMain(argc, argv);
}

static int serverMain(int argc, char *argv[])
{
    s_pLshttpd = new (std::nothrow) LshttpdMain();
    if (!s_pLshttpd)
    {
        perror("new LshttpdMain()");
        exit(1);
    }
    int ret = s_pLshttpd->main(argc, argv);
    delete s_pLshttpd;
    return ret;
}
#else
int main(int argc, char *argv[])
{
    s_pLshttpd = new (std::nothrow) LshttpdMain();
    if (!s_pLshttpd)
    {
        perror("new LshttpdMain()");
        exit(1);
    }
    int ret = s_pLshttpd->main(argc, argv);
    delete s_pLshttpd;
    exit(ret);
}
#endif



