// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "custom/custom.h"

using namespace std;

int custom_version_info(int version)
{
    return 0;
}

void custom_set_runtime_defaults(int exe_type)
{
    
}


bool custom_accept_transacton(const CTransaction& tx, 
                              const CCoinsViewCache &inputs,
                              int offset,
                              bool accept,
                              string& reason,
                              uint32_t *replay)
{
    return true;
}

