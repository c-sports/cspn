// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Copyright (c) 2014-2019 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <miner.h>

#include <amount.h>
#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/consensus.h>
#include <consensus/tx_verify.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <hash.h>
#include <validation.h>
#include <net.h>
#include <policy/feerate.h>
#include <policy/policy.h>
#include <pow.h>
#include <primitives/transaction.h>
#include <sign.h>
#include <script/standard.h>
#include <timedata.h>
#include <util.h>
#include <utilmoneystr.h>
#include <warnings.h>
#include <masternode/masternode-payments.h>
#include <masternode/masternode-sync.h>
#include <validationinterface.h>
#include <wallet/wallet.h>

#include <evo/specialtx.h>
#include <evo/cbtx.h>
#include <evo/simplifiedmns.h>
#include <evo/deterministicmns.h>

#include <llmq/quorums_blockprocessor.h>
#include <llmq/quorums_chainlocks.h>

#include <algorithm>
#include <memory>
#include <queue>
#include <utility>

//////////////////////////////////////////////////////////////////////////////
//
// DashMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest fee rate of a transaction combined with all
// its ancestors.

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;

int64_t nLastCoinStakeSearchInterval = 0;

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());

    if (nOldTime < nNewTime)
        pblock->nTime = nNewTime;

    // Updating time can change work required on testnet:
    if (consensusParams.fPowAllowMinDifficultyBlocks)
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams);

    return nNewTime - nOldTime;
}

BlockAssembler::Options::Options() {
    blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    nBlockMaxSize = DEFAULT_BLOCK_MAX_SIZE;
}

BlockAssembler::BlockAssembler(const CChainParams& params, const Options& options) : chainparams(params)
{
    blockMinFeeRate = options.blockMinFeeRate;
    // Limit size to between 1K and MaxBlockSize()-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MaxBlockSize(fDIP0001ActiveAtTip) - 1000), (unsigned int)options.nBlockMaxSize));
}

static BlockAssembler::Options DefaultOptions(const CChainParams& params)
{
    // Block resource limits
    BlockAssembler::Options options;
    options.nBlockMaxSize = DEFAULT_BLOCK_MAX_SIZE;
    if (gArgs.IsArgSet("-blockmaxsize")) {
        options.nBlockMaxSize = gArgs.GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    }
    if (gArgs.IsArgSet("-blockmintxfee")) {
        CAmount n = 0;
        ParseMoney(gArgs.GetArg("-blockmintxfee", ""), n);
        options.blockMinFeeRate = CFeeRate(n);
    } else {
        options.blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    }
    return options;
}

BlockAssembler::BlockAssembler(const CChainParams& params) : BlockAssembler(params, DefaultOptions(params)) {}

void BlockAssembler::resetBlock()
{
    inBlock.clear();

    // Reserve space for coinbase tx
    nBlockSize = 1000;
    nBlockSigOps = 100;

    // These counters do not include coinbase tx
    nBlockTx = 0;
    nFees = 0;
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(const CScript& scriptPubKeyIn, std::shared_ptr<CWallet> pwallet, bool fProofOfStake, bool* pfPoSCancel)
{
    int64_t nTimeStart = GetTimeMicros();

    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    if(!pblocktemplate.get())
        return nullptr;
    pblock = &pblocktemplate->block; // pointer for convenience

    // Add dummy coinbase tx as first transaction
    pblock->vtx.emplace_back();
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end

    LOCK2(cs_main, mempool.cs);

    CBlockIndex* pindexPrev = chainActive.Tip();
    assert(pindexPrev != nullptr);
    nHeight = pindexPrev->nHeight + 1;

    pblock->nVersion = ComputeBlockVersion(pindexPrev, chainparams.GetConsensus(), chainparams.BIP9CheckMasternodesUpgraded());
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (chainparams.MineBlocksOnDemand())
        pblock->nVersion = gArgs.GetArg("-blockversion", pblock->nVersion);

    pblock->nTime = GetAdjustedTime();
    const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();

    nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                       ? nMedianTimePast
                       : pblock->GetBlockTime();

    if (nHeight > chainparams.GetConsensus().DIP0003HeightAndDIP0008Height) {
        for (auto& p : chainparams.GetConsensus().llmqs) {
            CTransactionRef qcTx;
            if (llmq::quorumBlockProcessor->GetMinableCommitmentTx(p.first, nHeight, qcTx)) {
                pblock->vtx.emplace_back(qcTx);
                pblocktemplate->vTxFees.emplace_back(0);
                pblocktemplate->vTxSigOps.emplace_back(0);
                nBlockSize += qcTx->GetTotalSize();
                ++nBlockTx;
            }
        }
    }

    //int nPackagesSelected = 0;
    //int nDescendantsUpdated = 0;
    //addPackageTxs(nPackagesSelected, nDescendantsUpdated);

    int64_t nTime1 = GetTimeMicros();

    nLastBlockTx = nBlockTx;
    nLastBlockSize = nBlockSize;
    LogPrintf("CreateNewBlock(): total size %u txs: %u fees: %ld sigops %d\n", nBlockSize, nBlockTx, nFees, nBlockSigOps);

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);
    // coinbaseTx.nVersion = fProofOfStake ? 2 : 1;
    coinbaseTx.nVersion = 2;
    coinbaseTx.nType = TRANSACTION_COINBASE;

    if (fProofOfStake)
        pblock->vtx.resize(pblock->vtx.size() + 1);

    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;

    {
        LOCK(mempool.cs);
        addPackageTxs(nPackagesSelected, nDescendantsUpdated);
    }

    coinbaseTx.vout[0].scriptPubKey = scriptPubKeyIn;

    if (fProofOfStake) // attemp to find a coinstake
    {
        assert(pwallet);
        *pfPoSCancel = true;
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus());
        CMutableTransaction txCoinStake;
        int64_t nSearchTime = pblock->nTime; // search to current time
        if (nSearchTime > nLastCoinStakeSearchTime)
        {
            uint32_t nTxNewTime = 0;
            if (pwallet->CreateCoinStake(pblock->nBits, nSearchTime-nLastCoinStakeSearchTime, txCoinStake, nTxNewTime, nFees)) // TODO: Fix Fees
            {
                if (nTxNewTime >= std::max(pindexPrev->GetMedianTimePast()+1, pindexPrev->GetBlockTime() - MAX_FUTURE_BLOCK_TIME))
                {   // make sure coinstake would meet timestamp protocol
                    // as it would be the same as the block timestamp
                    coinbaseTx.vout[0].SetEmpty();
                    pblock->nTime = nTxNewTime;
                    pblock->vtx[1] = MakeTransactionRef(txCoinStake);
                    *pfPoSCancel = false;
                }
            }
            nLastCoinStakeSearchInterval = nSearchTime - nLastCoinStakeSearchTime;
            nLastCoinStakeSearchTime = nSearchTime;
        }
        if (*pfPoSCancel)
            return nullptr; // there is no point to continue if we failed to create coinstake
    } else {
        coinbaseTx.vout[0].nValue = nFees + GetBlockSubsidy(nHeight, chainparams.GetConsensus());
    }

    coinbaseTx.vin[0].scriptSig = CScript() << OP_RETURN;

    CCbTx cbTx;
    cbTx.nVersion = (nHeight > chainparams.GetConsensus().DIP0003HeightAndDIP0008Height) ? 2 : 1;
    cbTx.nHeight = nHeight;

    CValidationState state;
    if (!CalcCbTxMerkleRootMNList(*pblock, pindexPrev, cbTx.merkleRootMNList, state)) {
        throw std::runtime_error(strprintf("%s: CalcCbTxMerkleRootMNList failed: %s", __func__, FormatStateMessage(state)));
    }
    if (!CalcCbTxMerkleRootQuorums(*pblock, pindexPrev, cbTx.merkleRootQuorums, state)) {
        throw std::runtime_error(strprintf("%s: CalcCbTxMerkleRootQuorums failed: %s", __func__, FormatStateMessage(state)));
    }

    SetTxPayload(coinbaseTx, cbTx);

    // Update coinbase transaction with additional info about masternode and governance payments,
    // get some info back to pass to getblocktemplate
    //FillBlockPayments(coinbaseTx, nHeight, blockReward, pblocktemplate->voutMasternodePayments, pblocktemplate->voutSuperblockPayments);

    pblock->vtx[0] = MakeTransactionRef(std::move(coinbaseTx));
    pblocktemplate->vTxFees[0] = -nFees;

    // Fill in header
    pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
    if (pblock->IsProofOfWork()) {
        UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
    }
    pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus());
    pblock->nNonce         = 0;
    pblocktemplate->nPrevBits = pindexPrev->nBits;
    pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(*pblock->vtx[0]);

    if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false)) {
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, FormatStateMessage(state)));
    }
    int64_t nTime2 = GetTimeMicros();

    LogPrint(BCLog::BENCHMARK, "CreateNewBlock() packages: %.2fms (%d packages, %d updated descendants), validity: %.2fms (total %.2fms)\n", 0.001 * (nTime1 - nTimeStart), nPackagesSelected, nDescendantsUpdated, 0.001 * (nTime2 - nTime1), 0.001 * (nTime2 - nTimeStart));

    return std::move(pblocktemplate);
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries& testSet)
{
    for (CTxMemPool::setEntries::iterator iit = testSet.begin(); iit != testSet.end(); ) {
        // Only test txs not already in the block
        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        }
        else {
            iit++;
        }
    }
}

bool BlockAssembler::TestPackage(uint64_t packageSize, unsigned int packageSigOps) const
{
    if (nBlockSize + packageSize >= nBlockMaxSize)
        return false;
    if (nBlockSigOps + packageSigOps >= MaxBlockSigOps(fDIP0001ActiveAtTip))
        return false;
    return true;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
// - safe TXs in regard to ChainLocks
bool BlockAssembler::TestPackageTransactions(const CTxMemPool::setEntries& package)
{
    for (const CTxMemPool::txiter it : package) {
        if (!IsFinalTx(it->GetTx(), nHeight, nLockTimeCutoff))
            return false;
        if (!llmq::chainLocksHandler->IsTxSafeForMining(it->GetTx().GetHash())) {
            return false;
        }
    }
    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter)
{
    pblock->vtx.emplace_back(iter->GetSharedTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOps.push_back(iter->GetSigOpCount());
    nBlockSize += iter->GetTxSize();
    ++nBlockTx;
    nBlockSigOps += iter->GetSigOpCount();
    nFees += iter->GetFee();
    inBlock.insert(iter);

    bool fPrintPriority = gArgs.GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        LogPrintf("fee %s txid %s\n",
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }
}

int BlockAssembler::UpdatePackagesForAdded(const CTxMemPool::setEntries& alreadyAdded,
        indexed_modified_transaction_set &mapModifiedTx)
{
    int nDescendantsUpdated = 0;
    for (const CTxMemPool::txiter it : alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set
        for (CTxMemPool::txiter desc : descendants) {
            if (alreadyAdded.count(desc))
                continue;
            ++nDescendantsUpdated;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                modEntry.nSizeWithAncestors -= it->GetTxSize();
                modEntry.nModFeesWithAncestors -= it->GetModifiedFee();
                modEntry.nSigOpCountWithAncestors -= it->GetSigOpCount();
                mapModifiedTx.insert(modEntry);
            } else {
                mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
            }
        }
    }
    return nDescendantsUpdated;
}

// Skip entries in mapTx that are already in a block or are present
// in mapModifiedTx (which implies that the mapTx ancestor state is
// stale due to ancestor inclusion in the block)
// Also skip transactions that we've already failed to add. This can happen if
// we consider a transaction in mapModifiedTx and it fails: we can then
// potentially consider it again while walking mapTx.  It's currently
// guaranteed to fail again, but as a belt-and-suspenders check we put it in
// failedTx and avoid re-evaluation, since the re-evaluation would be using
// cached size/sigops/fee values that are not actually correct.
bool BlockAssembler::SkipMapTxEntry(CTxMemPool::txiter it, indexed_modified_transaction_set &mapModifiedTx, CTxMemPool::setEntries &failedTx)
{
    assert (it != mempool.mapTx.end());
    return mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it);
}

void BlockAssembler::SortForBlock(const CTxMemPool::setEntries& package, CTxMemPool::txiter entry, std::vector<CTxMemPool::txiter>& sortedEntries)
{
    // Sort package by ancestor count
    // If a transaction A depends on transaction B, then A's ancestor count
    // must be greater than B's.  So this is sufficient to validly order the
    // transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(), CompareTxIterByAncestorCount());
}

// This transaction selection algorithm orders the mempool based
// on feerate of a transaction including all unconfirmed ancestors.
// Since we don't remove transactions from the mempool as we select them
// for block inclusion, we need an alternate method of updating the feerate
// of a transaction with its not-yet-selected ancestors as we go.
// This is accomplished by walking the in-mempool descendants of selected
// transactions and storing a temporary modified state in mapModifiedTxs.
// Each time through the loop, we compare the best transaction in
// mapModifiedTxs with the next transaction in the mempool to decide what
// transaction package to work on next.
void BlockAssembler::addPackageTxs(int &nPackagesSelected, int &nDescendantsUpdated)
{
    // mapModifiedTx will store sorted packages after they are modified
    // because some of their txs are already in the block
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work
    CTxMemPool::setEntries failedTx;

    // Start by adding all descendants of previously added txs to mapModifiedTx
    // and modifying them for their already included ancestors
    UpdatePackagesForAdded(inBlock, mapModifiedTx);

    CTxMemPool::indexed_transaction_set::index<ancestor_score>::type::iterator mi = mempool.mapTx.get<ancestor_score>().begin();
    CTxMemPool::txiter iter;

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    while (mi != mempool.mapTx.get<ancestor_score>().end() || !mapModifiedTx.empty())
    {
        // First try to find a new transaction in mapTx to evaluate.
        if (mi != mempool.mapTx.get<ancestor_score>().end() &&
                SkipMapTxEntry(mempool.mapTx.project<0>(mi), mapModifiedTx, failedTx)) {
            ++mi;
            continue;
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == mempool.mapTx.get<ancestor_score>().end()) {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx entry to the mapModifiedTx entry
            iter = mempool.mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() &&
                    CompareTxMemPoolEntryByAncestorFee()(*modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score
                // than the one from mapTx.
                // Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        unsigned int packageSigOps = iter->GetSigOpCountWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOps = modit->nSigOpCountWithAncestors;
        }

        if (packageFees < blockMinFeeRate.GetFee(packageSize)) {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOps)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx,
                // we must erase failed entries so that we can consider the
                // next best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }

            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES && nBlockSize > nBlockMaxSize - 1000) {
                // Give up if we're close to full and haven't succeeded in a while
                break;
            }
            continue;
        }

        CTxMemPool::setEntries ancestors;
        uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
        std::string dummy;
        mempool.CalculateMemPoolAncestors(*iter, ancestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy, false);

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        // Test if all tx's are Final and safe
        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Package can be added. Sort the entries in a valid order.
        std::vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, iter, sortedEntries);

        for (size_t i=0; i<sortedEntries.size(); ++i) {
            AddToBlock(sortedEntries[i]);
            // Erase from the modified set, if present
            mapModifiedTx.erase(sortedEntries[i]);
        }

        ++nPackagesSelected;

        // Update transactions that depend on each of these
        nDescendantsUpdated += UpdatePackagesForAdded(ancestors, mapModifiedTx);
    }
}

void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(*pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}


// proof-of-stake
static bool ProcessBlockFound(const CBlock* pblock)
{
    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("%s: generated block is stale", __func__);
    }

    // Process this block the same as if we had received it from another node
    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
    if (!ProcessNewBlock(Params(), shared_pblock, true, nullptr))
        return error("%s: block not accepted", __func__);

    return true;
}

void PoSMiner(std::shared_ptr<CWallet> pwallet)
{
    LogPrintf("%s: started for proof-of-stake\n", __func__);

    unsigned int nExtraNonce = 0;

    std::shared_ptr<CReserveScript> coinbase_script;
    pwallet->GetScriptForMining(coinbase_script);

    // Compute timeout for pos as sqrt(numUTXO)
    unsigned int pos_timio;
    {
        std::vector<COutput> vCoins;
        pwallet->AvailableCoins(vCoins, false);
        pos_timio = gArgs.GetArg("-staketimio", 500) + 30 * sqrt(vCoins.size());
        LogPrintf("set proof-of-stake timeout: %ums for %u UTXOs\n", pos_timio, vCoins.size());
    }

    std::string strMintMessage = _("Info: Minting suspended due to locked wallet.");
    std::string strMintSyncMessage = _("Info: Minting suspended while synchronizing wallet.");
    std::string strMintBlockMessage = _("Info: Minting suspended due to block creation failure.");
    std::string strMintEmpty = _("");
    int64_t nSleepTime = (Params().GetConsensus().nPosTargetSpacing / 2) * 1000;

    try {
        // Throw an error if no script was provided.  This can happen
        // due to some internal error but also if the keypool is empty.
        // In the latter case, already the pointer is NULL.
        if (!coinbase_script || coinbase_script->reserveScript.empty())
            throw std::runtime_error("No coinbase script available (mining requires a wallet)");

        while (true) {
            while (pwallet->IsLocked()) {
                SetMiscWarning(strMintMessage);
                MilliSleep(nSleepTime);
            }

            if (Params().MiningRequiresPeers()) {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                while(g_connman == nullptr || g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0 || IsInitialBlockDownload())
                    MilliSleep(nSleepTime);
            }

            // Check if we've reached the PoS start block.
            if (chainActive.Tip()->nHeight < Params().GetConsensus().nLastPoWBlock)
            {
                nLastCoinStakeSearchInterval = 0;
                MilliSleep(nSleepTime);
                continue;
            }

            while (GuessVerificationProgress(Params().TxData(), chainActive.Tip()) < 0.996)
            {
                LogPrintf("%s: minter thread sleeps while sync at %f\n", __func__, GuessVerificationProgress(Params().TxData(), chainActive.Tip()));
                SetMiscWarning(strMintSyncMessage);
                MilliSleep(nSleepTime);
            }

            SetMiscWarning(strMintEmpty);
            uiInterface.NotifyAlertChanged();

            //
            // Create new block
            //
            CBlockIndex* pindexPrev = chainActive.Tip();
            bool fPoSCancel = false;
            std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(Params()).CreateNewBlock(coinbase_script->reserveScript, pwallet, true, &fPoSCancel));
            if (!pblocktemplate.get())
            {
                if (fPoSCancel == true)
                {
                    MilliSleep(pos_timio);
                    continue;
                }
                SetMiscWarning(strMintBlockMessage);
                LogPrintf("%s: keypool ran out, please call keypoolrefill before restarting the mining thread\n", __func__);
                return;
            }
            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

            // if proof-of-stake block is found then sign block
            if (pblock->IsProofOfStake())
            {
                pblock->nFlags = CBlockIndex::BLOCK_PROOF_OF_STAKE;
                CBlockSigner signer(*pblock, pwallet);
                if (!signer.SignBlock())
                {
                    LogPrintf("%s: failed to sign PoS block", __func__);
                    continue;
                }

                LogPrintf("%s: proof-of-stake block=%s generated=%s\n",
                          __func__,
                          pblock->ToString(),
                          FormatMoney(pblock->vtx[0]->vout[0].nValue)
                );
                ProcessBlockFound(pblock);

                // Rest for ~3 minutes after successful block to preserve close quick
                MilliSleep(60 * 1000 + GetRand(4 * 60 * 1000));
            }
            MilliSleep(pos_timio);

            continue;
        }
    }
    catch (boost::thread_interrupted)
    {
        LogPrintf("%s: terminated\n", __func__);
        return;
        // throw;
    }
    catch (const std::runtime_error &e)
    {
        LogPrintf("%s: runtime error: %s\n", __func__, e.what());
        return;
    }
}