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
#include <ls.h>

#include <string.h>

extern lsi_module_t modcompress;
extern lsi_module_t moddecompress;
extern lsi_module_t cache;
#ifdef _WIN32
extern "C" lsi_module_t mod_acme;
extern lsi_module_t uploadprogress;
extern lsi_module_t modreqparser;
extern lsi_module_t mod_lua;
extern lsi_module_t mod_security;
#endif

extern int addModgzipFilter(lsi_session_t *session, int isSend,
                            uint8_t compressLevel);
struct Prelinked_Module
{
    const char       *_pName;
    lsi_module_t     *_pModule;
};

Prelinked_Module g_prelinked[] =
{
    { "modcompress",    &modcompress   },
    { "moddecompress",  &moddecompress },
    { "cache",          &cache         }

};

#ifdef _WIN32
Prelinked_Module g_optional[] =
{
    { "mod_acme",       &mod_acme       },
    { "uploadprogress", &uploadprogress },
    { "modreqparser",   &modreqparser   },
    { "mod_lua",        &mod_lua        },
    { "mod_security",   &mod_security   }
};

lsi_module_t *getOptionalPrelinkedModule(const char *pName)
{
    for (unsigned int i = 0; i < sizeof(g_optional) / sizeof(Prelinked_Module); ++i)
        if (strcmp(g_optional[i]._pName, pName) == 0)
            return g_optional[i]._pModule;
    return NULL;
}
#endif

int getPrelinkedModuleCount()
{
    return sizeof(g_prelinked) / sizeof(Prelinked_Module);
}

lsi_module_t *getPrelinkedModuleByIndex(unsigned int index,
                                        const char **pName)
{
    if (index >= sizeof(g_prelinked) / sizeof(Prelinked_Module))
        return NULL;
    *pName = g_prelinked[index]._pName;
    return g_prelinked[index]._pModule;
}

// lsi_module_t * getPrelinkedModule( const char * pModule )
// {
//     for( int i=0; i < sizeof( g_prelinked ) / sizeof( Prelinked_Module );++i )
//     {
//         if ( strcmp( g_prelinked[i]._pName, pModule ) == 0 )
//             return g_prelinked[i]._pModule;
//     }
//     return NULL;
// }




