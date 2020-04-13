// Copyright (c) 2018 The Dash Core developers
// Copyright (c) 2019 The BitTestForLaterSoICanUseReplaceGreen Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CSPN_SPECIAL_UTIL
#define CSPN_SPECIAL_UTIL

#include <primitives/transaction.h>
#include <coins.h>

bool GetUTXOCoin(const COutPoint& outpoint, Coin& coin);
int GetUTXOHeight(const COutPoint& outpoint);
int GetUTXOConfirmations(const COutPoint& outpoint);

#endif // CSPN_SPECIAL_UTIL
