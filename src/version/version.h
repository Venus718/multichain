// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAINVERSION_H
#define	MULTICHAINVERSION_H

#define MULTICHAIN_VERSION_MAJOR     1
#define MULTICHAIN_VERSION_MINOR     0
#define MULTICHAIN_VERSION_REVISION  0
#define MULTICHAIN_VERSION_STAGE     2
#define MULTICHAIN_VERSION_BUILD     2

#define MULTICHAIN_PROTOCOL_VERSION 10008
#define MULTICHAIN_PROTOCOL_SUPPORTED "10004 - 10008, 20001"


#ifndef STRINGIZE
#define STRINGIZE(X) DO_STRINGIZE(X)
#endif

#ifndef DO_STRINGIZE
#define DO_STRINGIZE(X) #X
#endif

#define MULTICHAIN_BUILD_DESC_WITH_SUFFIX(maj, min, rev, build, suffix) \
    DO_STRINGIZE(maj) "." DO_STRINGIZE(min) "." DO_STRINGIZE(rev) "." DO_STRINGIZE(build) "-" DO_STRINGIZE(suffix)

#define MULTICHAIN_BUILD_DESC_FROM_UNKNOWN(maj, min, rev, build) \
    DO_STRINGIZE(maj) "." DO_STRINGIZE(min) "." DO_STRINGIZE(rev) "." DO_STRINGIZE(build)


#define MULTICHAIN_BUILD_DESC "1.0 beta 2"
#define MULTICHAIN_BUILD_DESC_NUMERIC 10000202


#ifndef MULTICHAIN_BUILD_DESC
#ifdef BUILD_SUFFIX
#define MULTICHAIN_BUILD_DESC MULTICHAIN_BUILD_DESC_WITH_SUFFIX(MULTICHAIN_VERSION_MAJOR, MULTICHAIN_VERSION_MINOR, MULTICHAIN_VERSION_REVISION, MULTICHAIN_VERSION_BUILD, BUILD_SUFFIX)
#else
#define MULTICHAIN_BUILD_DESC MULTICHAIN_BUILD_DESC_FROM_UNKNOWN(MULTICHAIN_VERSION_MAJOR, MULTICHAIN_VERSION_MINOR, MULTICHAIN_VERSION_REVISION, MULTICHAIN_VERSION_BUILD)
#endif
#endif

#define MULTICHAIN_FULL_DESC(build, protocol) \
    "build " build " protocol " DO_STRINGIZE(protocol)


#ifndef MULTICHAIN_FULL_VERSION
#define MULTICHAIN_FULL_VERSION MULTICHAIN_FULL_DESC(MULTICHAIN_BUILD_DESC, MULTICHAIN_PROTOCOL_VERSION)
#endif


#define MULTICHAIN_VERSION_CODE_PROTOCOL_THIS              0
#define MULTICHAIN_VERSION_CODE_PROTOCOL_MIN               1
#define MULTICHAIN_VERSION_CODE_PROTOCOL_MIN_DOWNGRADE     2
#define MULTICHAIN_VERSION_CODE_PROTOCOL_MIN_NO_DOWNGRADE  3
#define MULTICHAIN_VERSION_CODE_BUILD                     16
#define MULTICHAIN_VERSION_CODE_MIN_VALID              10000

#endif	/* MULTICHAINVERSION_H */

