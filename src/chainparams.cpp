// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2019 The BitTestForLaterSoICanUseReplaceGreen Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <chainparams.h>
#include <chainparamsseeds.h>
#include <consensus/merkle.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <versionbitsinfo.h>

#include <assert.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.nType = 0;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime = nTime;
    genesis.nBits = nBits;
    genesis.nNonce = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=18d67153a6109201bd1fa74d9ff94785d31a83cd0d0cda00af5d8ea79beca1bd, ver=0x00000001, hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, hashMerkleRoot=07cbcacfc822fba6bbeb05312258fa43b96a68fc310af8dfcec604591763f7cf, nTime=1565017975, nBits=1e0ffff0, nNonce=21212214, vtx=1)
 *  CTransaction(hash=07cbcacfc8, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *    CTxIn(COutPoint(0000000000, 4294967295), coinbase 04ffff001d01044c554576656e205769746820456e6572677920537572706c75732c2043616e61646120556e61626c6520746f204d65657420456c6563747269636974792044656d616e6473206f6620426974636f696e204d696e657273)
 *    CScriptWitness()
 *    CTxOut(nValue=0.00000000, scriptPubKey=4104e5a8143f86ad8ac63791fbbdb8)
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "06112020 Craig Wright Apparently Just Admitted to Hacking Mt. Gox";
    const CScript genesisOutputScript = CScript() << ParseHex("04e5a8143f86ad8ac63791fbbdb8e0b9111da88c8c693a2222c2c13c063ea790f7960b8025a9047a7bc671d5cfe707a2dd2e13b86182e1064a0eea7bf863636363") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}


// this one is for testing only
static Consensus::LLMQParams llmq5_60 = {
    .type = Consensus::LLMQ_5_60,
    .name = "llmq_5_60",
    .size = 5,
    .minSize = 3,
    .threshold = 3,

    .dkgInterval = 24, // one DKG per hour
    .dkgPhaseBlocks = 2,
    .dkgMiningWindowStart = 10, // dkgPhaseBlocks * 5 = after finalization
    .dkgMiningWindowEnd = 18,
    .dkgBadVotesThreshold = 8,

    .signingActiveQuorumCount = 2, // just a few ones to allow easier testing

    .keepOldConnections = 3,
};

static Consensus::LLMQParams llmq50_60 = {
    .type = Consensus::LLMQ_50_60,
    .name = "llmq_50_60",
    .size = 50,
    .minSize = 40,
    .threshold = 30,

    .dkgInterval = 24, // one DKG per hour
    .dkgPhaseBlocks = 2,
    .dkgMiningWindowStart = 10, // dkgPhaseBlocks * 5 = after finalization
    .dkgMiningWindowEnd = 18,
    .dkgBadVotesThreshold = 40,

    .signingActiveQuorumCount = 24, // a full day worth of LLMQs

    .keepOldConnections = 25,
};

static Consensus::LLMQParams llmq400_60 = {
    .type = Consensus::LLMQ_400_60,
    .name = "llmq_400_60",
    .size = 400,
    .minSize = 300,
    .threshold = 240,

    .dkgInterval = 24 * 12, // one DKG every 12 hours
    .dkgPhaseBlocks = 4,
    .dkgMiningWindowStart = 20, // dkgPhaseBlocks * 5 = after finalization
    .dkgMiningWindowEnd = 28,
    .dkgBadVotesThreshold = 300,

    .signingActiveQuorumCount = 4, // two days worth of LLMQs

    .keepOldConnections = 5,
};

// Used for deployment and min-proto-version signalling, so it needs a higher threshold
static Consensus::LLMQParams llmq400_85 = {
    .type = Consensus::LLMQ_400_85,
    .name = "llmq_400_85",
    .size = 400,
    .minSize = 350,
    .threshold = 340,

    .dkgInterval = 24 * 24, // one DKG every 24 hours
    .dkgPhaseBlocks = 4,
    .dkgMiningWindowStart = 20, // dkgPhaseBlocks * 5 = after finalization
    .dkgMiningWindowEnd = 48,   // give it a larger mining window to make sure it is mined
    .dkgBadVotesThreshold = 300,

    .signingActiveQuorumCount = 4, // two days worth of LLMQs

    .keepOldConnections = 5,
};

/**
 * Main network
 */
class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 525600;
        consensus.BIP16Exception = uint256();
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~arith_uint256(0) >> 20;
        consensus.nPowTargetTimespan = 1 * 24 * 60 * 60;                                                   // 1 day
        consensus.nPowTargetSpacing = 1 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nPosTargetSpacing = 60; // PoS: 1 minutes
        consensus.nPosTargetTimespan = 60 * 40; // change back to 40 instead of 5 later
        consensus.nStakeMinAge = 60 * 60; // 1 hours /// change back to 1 hour later
        consensus.nStakeMaxAge = 60 * 60 * 24; // 24 hours
        consensus.nModifierInterval = 60;      // Modifier interval: time to elapse before new modifier is computed (60 seconds)
        consensus.nLastPoWBlock = 1500; // change back to 1500 later
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016;       // nPowTargetTimespan / nPowTargetSpacing
        consensus.nMasternodeMinimumConfirmations = 16;

        // Stake constants
        consensus.nStakeEnforcement = 7001;
        consensus.nMinStakeAmount = 150 * COIN;
        consensus.nMinStakeHistory = 60;

        // Governance
        consensus.nSuperblockCycle = 20571; // ~(60*24*30)/2.1, actual number of blocks per month is 262800 / 12 = 21900
        consensus.nGovernanceMinQuorum = 10;
        consensus.nGovernanceFilterElements = 20000;
        consensus.nBudgetPaymentsStartBlock = 10000;
        consensus.nBudgetPaymentsCycleBlocks = 20571; // ~(60*24*30)/2.1, actual number of blocks per month is 262800 / 12 = 21900
        consensus.nBudgetPaymentsWindowBlocks = 100;
        consensus.nSuperblockStartBlock = 12000; // NOTE: Should satisfy nSuperblockStartBlock > nBudgetPaymentsStartBlock

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999;   // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1462060800; // May 1st, 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1493596800;   // May 1st, 2017

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");
        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        // InstantSend
        consensus.nInstantSendConfirmationsRequired = 6;
        consensus.nInstantSendKeepLock = 24;

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xd4;
        pchMessageStart[1] = 0xf4;
        pchMessageStart[2] = 0xa6;
        pchMessageStart[3] = 0x12;
        nDefaultPort = 13370;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 0;


        genesis = CreateGenesisBlock(1592001039 , 28269589, 0x1e0ffff0, 1, 0 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x0000098e30a3d29ee06c8f371e9e1fc516c8218b1be2615b7b0ec31649ed12e3"));
        assert(genesis.hashMerkleRoot == uint256S("0x7f572dcc0eae0471f168f6424b3247c1f5da22e7944b23e0cf06d39d57e2f352"));

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as a oneshot if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        vSeeds.emplace_back("s1.cspn.io");
        vSeeds.emplace_back("s2.cspn.io");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 28); // Wallet address starts with "C"
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 6);
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 46);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "cp";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        // long living quorum params
        consensus.llmqs[Consensus::LLMQ_50_60] = llmq50_60;
        consensus.llmqs[Consensus::LLMQ_400_60] = llmq400_60;
        consensus.llmqs[Consensus::LLMQ_400_85] = llmq400_85;
        consensus.llmqChainLocks = Consensus::LLMQ_400_60;
        consensus.llmqForInstantSend = Consensus::LLMQ_50_60;
        consensus.nLLMQActivationHeight = 50;

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        m_is_test_chain = false;
        fMiningRequiresPeers = true;
        fAllowMultiplePorts = true;
        nFulfilledRequestExpireTime = 60 * 60; // fulfilled requests expire in 1 hour

        vSporkAddresses = {"CQGjnKEWxF69yoo9chv6PwmEJLssnX1uZN"};
        nMinSporkKeys = 1;

        checkpointData = {
                {
                        {      1, uint256S("0x000000af1c83cdf16aba8e539aa86b90f8aa39afb1c9dcd489f02202c92b90a9")},
                        {   1000, uint256S("0x000000a687ae1b49630c53b78983bfa3e40248f811336480cc0454300b7ca0e4")},
                        {   1500, uint256S("0x00000027336a6d7cb44105e66b19b7c357a128f12ec097138b783b082c15f075")},
                        {  10000, uint256S("0xd4b13fe1ce11047cc04ef099bf85062a0bdc01913644a6e05ba4b6f44331bbc9")},
                        {  12167, uint256S("0x6a5b1939de7f55943aa73c02e033e5326f52fa07446c78db62c513f2e9bddafb")},
                        {  20000, uint256S("0xd2889eb5d03ae4cf77967c4f6dd5e0a3b25cd6c06b2ebe065b0399504b5f84ed")},
                        {  40000, uint256S("0x0a8c530d9b7a2a92fdf1deeb26f62b91bad189c880af1e5d51aa1aa60b5a0ab4")},
                        {  60000, uint256S("0xba66203533e488a13f7be8e8f459259a970997e921c337153aa6ab4498206094")},
                        {  80000, uint256S("0xf6469b539bfa8da20404024284a11ed5400152e47d38484ecb7f065b7ec538f6")},
                        { 100000, uint256S("0x9432475502579e22b27a93caf0314158159da4c979c0d75765944ed325588e9f")},
                        { 120000, uint256S("0xa55f1dfcc9f499235493a2ace0a4708c33d6b8b4017c4e2f11a3f136023b5563")},
                        { 140000, uint256S("0x098747dc5350c1b9c6dc00aa52d35383fbeb02539770873260e0b16b28b84ec8")},
                        { 160000, uint256S("0xe63bcee630d93e501372beb62e565ef67cde0345a80b88bd5ee29d0d2b736eea")},
                        { 180000, uint256S("0x3ade170816a5bd5a94980594e9e6c0974c6cf306cbf4f5f8a6e2bff7e06b1980")},
                        { 200000, uint256S("0x88761a6b98dd3d2dd83f995db8035788a599a068e183913ac88165fcf7b42d62")},
                        { 220000, uint256S("0x8b3ba3f0fa56d69272317630ffb352826bf3d1bbd6b00031d1a427d57e10e0df")},
                        { 240000, uint256S("0xe2a53ce2246a7f4ff51b68a04ded11ed68f7dcb7f916daeb54f475d10dbebc7d")},
                        { 260000, uint256S("0xf7bd7a739f08cd17a7be8fa3f378892bcfb56e553ba92b49f2b50c0542d6e3c4")},
                        { 275000, uint256S("0x66d05fdce53d656e8d9288e5043056b3b7a9fd948dddb12716c6dc92d388359b")}
                }};

        chainTxData = ChainTxData{
            // Data from rpc: getchaintxstats <nblock> <blockhash>
            // Data from RPC: getchaintxstats 70004 2da7cf773e5032a76aa4480b033c1ac6978ff64531f168c92d022c90f5bf7996
            /* nTime    */ 1592682533,
            /* nTxCount */ 29677,
            /* dTxRate  */ 0.04381794180445754};
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams
{
public:
    CTestNetParams()
    {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP16Exception = uint256();
        consensus.BIP34Height = 200;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 200;
        consensus.BIP66Height = 200;
        consensus.powLimit = uint256S("00000ffff0000000000000000000000000000000000000000000000000000000");
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 1 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nPosTargetSpacing = 2 * 60; // PoS: 2 minutes
        consensus.nPosTargetTimespan = 60 * 40;
        consensus.nStakeMinAge = 60 * 1;  // 1 minute
        consensus.nStakeMaxAge = 60 * 60; // 1 hour
        consensus.nModifierInterval = 60; // Modifier interval: time to elapse before new modifier is computed (1 minute)
        consensus.nLastPoWBlock = 200;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016;       // nPowTargetTimespan / nPowTargetSpacing
        consensus.nMasternodeMinimumConfirmations = 1;

        // Stake constants
        consensus.nStakeEnforcement = 200;
        consensus.nMinStakeAmount = 1 * COIN;
        consensus.nMinStakeHistory = 10;

        // Governance
        consensus.nSuperblockCycle = 24; // Superblocks can be issued hourly on testnet
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 500;
        consensus.nBudgetPaymentsStartBlock = 200;
        consensus.nBudgetPaymentsCycleBlocks = 50;
        consensus.nBudgetPaymentsWindowBlocks = 10;
        consensus.nSuperblockStartBlock = 300; // NOTE: Should satisfy nSuperblockStartBlock > nBudgetPaymentsStartBlock

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999;   // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1456790400; // March 1st, 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1493596800;   // May 1st, 2017

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1462060800; // May 1st 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1493596800;   // May 1st 2017

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        // InstantSend
        consensus.nInstantSendConfirmationsRequired = 2;
        consensus.nInstantSendKeepLock = 6;

        pchMessageStart[0] = 0xa3;
        pchMessageStart[1] = 0x6b;
        pchMessageStart[2] = 0xb0;
        pchMessageStart[3] = 0x4b;
        nDefaultPort = 113370;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 0;

        genesis = CreateGenesisBlock(1565017975, 21212214, 0x1e0ffff0, 1, 0 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        //assert(consensus.hashGenesisBlock == uint256S("0x00000546a6b03a54ae05f94119e37c55202e90a953058c35364d112d41ded06a"));
        //assert(genesis.hashMerkleRoot == uint256S("0x07cbcacfc822fba6bbeb05312258fa43b96a68fc310af8dfcec604591763f7cf"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 98);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 12);
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 108);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tbg";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        // long living quorum params
        consensus.llmqs[Consensus::LLMQ_50_60] = llmq50_60;
        consensus.llmqs[Consensus::LLMQ_400_60] = llmq400_60;
        consensus.llmqs[Consensus::LLMQ_400_85] = llmq400_85;
        consensus.llmqChainLocks = Consensus::LLMQ_50_60;
        consensus.llmqForInstantSend = Consensus::LLMQ_50_60;
        consensus.nLLMQActivationHeight = 50;

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        m_is_test_chain = true;
        fMiningRequiresPeers = true;
        fAllowMultiplePorts = false;
        nFulfilledRequestExpireTime = 5 * 60; // fulfilled requests expire in 5 minutes

        vSporkAddresses = {"CQGjnKEWxF69yoo9chv6PwmEJLssnX1uZN"};
        nMinSporkKeys = 1;

        checkpointData = {
            {}};

        chainTxData = ChainTxData{
            // Data from rpc: getchaintxstats <nblocks> <hash>
            /* nTime    */ 0,
            /* nTxCount */ 0,
            /* dTxRate  */ 0};
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams
{
public:
    explicit CRegTestParams(const ArgsManager& args)
    {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP16Exception = uint256();
        consensus.BIP34Height = consensus.nLastPoWBlock; // BIP34 activated on regtest (Used in functional tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = consensus.nLastPoWBlock; // BIP65 activated on regtest (Used in functional tests)
        consensus.BIP66Height = consensus.nLastPoWBlock; // BIP66 activated on regtest (Used in functional tests)
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nPosTargetSpacing = 2 * 60; // PoS: 2 minutes
        consensus.nPosTargetTimespan = 60 * 40;
        consensus.nStakeMinAge = 60 * 1;  // test net min age is 1 minute
        consensus.nStakeMaxAge = 60 * 10; // 10 minutes
        consensus.nModifierInterval = 60; // Modifier interval: time to elapse before new modifier is computed (1 minute)
        consensus.nLastPoWBlock = 1000;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144;       // Faster than normal for regtest (144 instead of 2016)
        consensus.nMasternodeMinimumConfirmations = 1;

        // Governance
        consensus.nSuperblockCycle = 10;
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 100;
        consensus.nBudgetPaymentsStartBlock = 1000;
        consensus.nBudgetPaymentsCycleBlocks = 50;
        consensus.nBudgetPaymentsWindowBlocks = 10;
        consensus.nSuperblockStartBlock = 1500; // NOTE: Should satisfy nSuperblockStartBlock > nBudgetPaymentsStartBlock

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        // InstantSend
        consensus.nInstantSendConfirmationsRequired = 2;
        consensus.nInstantSendKeepLock = 6;

        pchMessageStart[0] = 0xf2;
        pchMessageStart[1] = 0x90;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0x78;
        nDefaultPort = 213370;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        UpdateVersionBitsParametersFromArgs(args);

        genesis = CreateGenesisBlock(1565017975, 20542302, 0x207fffff, 1, 0 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        //assert(consensus.hashGenesisBlock == uint256S("0x100a3271b95d1a817101bcbd7045ad14c9799cb34e1cb6071973c8932ae48b6a"));
        //assert(genesis.hashMerkleRoot == uint256S("0x07cbcacfc822fba6bbeb05312258fa43b96a68fc310af8dfcec604591763f7cf"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = true;
        m_is_test_chain = true;
        fMiningRequiresPeers = false;
        fAllowMultiplePorts = true;
        nFulfilledRequestExpireTime = 5 * 60; // fulfilled requests expire in 5 minutes

        vSporkAddresses = {"CQGjnKEWxF69yoo9chv6PwmEJLssnX1uZN"};
        nMinSporkKeys = 1;

        checkpointData = {
            {}};

        chainTxData = ChainTxData{
            0,
            0,
            0};

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 98);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 12);
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 108);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "bgrt";

        // long living quorum params
        consensus.llmqs[Consensus::LLMQ_5_60] = llmq5_60;
        consensus.llmqs[Consensus::LLMQ_50_60] = llmq50_60;
        consensus.llmqChainLocks = Consensus::LLMQ_5_60;
        consensus.llmqForInstantSend = Consensus::LLMQ_5_60;
        consensus.nLLMQActivationHeight = 500;
    }

    /**
     * Allows modifying the Version Bits regtest parameters.
     */
    void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
    {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
    }
    void UpdateVersionBitsParametersFromArgs(const ArgsManager& args);
};

void CRegTestParams::UpdateVersionBitsParametersFromArgs(const ArgsManager& args)
{
    if (!args.IsArgSet("-vbparams")) return;

    for (const std::string& strDeployment : args.GetArgs("-vbparams")) {
        std::vector<std::string> vDeploymentParams;
        boost::split(vDeploymentParams, strDeployment, boost::is_any_of(":"));
        if (vDeploymentParams.size() != 3) {
            throw std::runtime_error("Version bits parameters malformed, expecting deployment:start:end");
        }
        int64_t nStartTime, nTimeout;
        if (!ParseInt64(vDeploymentParams[1], &nStartTime)) {
            throw std::runtime_error(strprintf("Invalid nStartTime (%s)", vDeploymentParams[1]));
        }
        if (!ParseInt64(vDeploymentParams[2], &nTimeout)) {
            throw std::runtime_error(strprintf("Invalid nTimeout (%s)", vDeploymentParams[2]));
        }
        bool found = false;
        for (int j = 0; j < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++j) {
            if (vDeploymentParams[0] == VersionBitsDeploymentInfo[j].name) {
                UpdateVersionBitsParameters(Consensus::DeploymentPos(j), nStartTime, nTimeout);
                found = true;
                LogPrintf("Setting version bits activation parameters for %s to start=%ld, timeout=%ld\n", vDeploymentParams[0], nStartTime, nTimeout);
                break;
            }
        }
        if (!found) {
            throw std::runtime_error(strprintf("Invalid deployment (%s)", vDeploymentParams[0]));
        }
    }
}

static std::unique_ptr<const CChainParams> globalChainParams;

const CChainParams& Params()
{
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<const CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams(gArgs));
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}
