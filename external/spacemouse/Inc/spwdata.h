/*----------------------------------------------------------------------
 *  spwdata.h -- datatypes
 *
 *
 *  This contains the only acceptable type definitions for 3Dconnexion
 *  products. Needs more work.
 *
 *----------------------------------------------------------------------
 *
 * Copyright notice:
 * Copyright (c) 1996-2015 3Dconnexion. All rights reserved.
 *
 * This file and source code are an integral part of the "3Dconnexion
 * Software Developer Kit", including all accompanying documentation,
 * and is protected by intellectual property laws. All use of the
 * 3Dconnexion Software Developer Kit is subject to the License
 * Agreement found in the "LicenseAgreementSDK.txt" file.
 * All rights not expressly granted by 3Dconnexion are reserved.
 *
 *----------------------------------------------------------------------
 *
 * 10/27/15 Added 64bit definitions for non ms-compilers
 */

#ifndef SPWDATA_H
#define SPWDATA_H

static char spwdataCvsId[] = "(C) 1996-2015 3Dconnexion: $Id: spwdata.h 11976 2015-10-27 10:51:11Z mbonk $";

#if defined (_MSC_VER)
typedef __int64            SPWint64;
#else
typedef int64_t            SPWint64;
#endif
typedef long               SPWint32;
typedef short              SPWint16;
typedef char               SPWint8;
typedef long               SPWbool;
#if defined (_MSC_VER)
typedef unsigned __int64   SPWuint64;
#else
typedef uint64_t           SPWuint64;
#endif
typedef unsigned long      SPWuint32;
typedef unsigned short     SPWuint16;
typedef unsigned char      SPWuint8;
typedef float              SPWfloat32;
typedef double             SPWfloat64;

#endif /* SPWDATA_H */
