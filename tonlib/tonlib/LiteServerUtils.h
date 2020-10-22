#pragma once

#include "tonlib/Stuff.h"

namespace tonlib {

auto parse_grams(td::Ref<vm::CellSlice>& grams) -> td::Result<std::string>;

auto parse_msg_anycast(td::Ref<vm::CellSlice>& anycast)
    -> td::Result<tonlib_api_ptr<tonlib_api::liteServer_messageAnycast>>;
auto parse_msg_address_ext(td::Ref<vm::CellSlice>& addr)
    -> td::Result<tonlib_api_ptr<tonlib_api::liteServer_MessageAddressExt>>;
auto parse_msg_address_int(td::Ref<vm::CellSlice>& addr)
    -> td::Result<tonlib_api_ptr<tonlib_api::liteServer_MessageAddressInt>>;

auto parse_message_info(td::Ref<vm::CellSlice>& msg) -> td::Result<tonlib_api_ptr<tonlib_api::liteServer_MessageInfo>>;
auto parse_message(td::Ref<vm::Cell>&& msg) -> td::Result<tonlib_api_ptr<tonlib_api::liteServer_message>>;

auto check_special_transaction(const tonlib_api_ptr<tonlib_api::liteServer_message>& msg_in,
                               const std::vector<tonlib_api_ptr<tonlib_api::liteServer_message>>& msgs_out)
    -> tonlib_api_ptr<tonlib_api::liteServer_TransactionAdditionalInfo>;
auto parse_stake_send_transaction(td::Ref<vm::CellSlice>&& msg_in, td::Ref<vm::CellSlice>&& msg_out)
    -> tonlib_api_ptr<tonlib_api::liteServer_transactionAdditionalInfoStakeSend>;
auto parse_stake_recover_transaction(td::Ref<vm::CellSlice>&& msg_in, td::Ref<vm::CellSlice>&& msg_out)
    -> tonlib_api_ptr<tonlib_api::liteServer_transactionAdditionalInfoStakeRecover>;

auto parse_transaction(int workchain, const td::Bits256& account, td::Ref<vm::Cell>&& list)
    -> td::Result<tonlib_api_ptr<tonlib_api::liteServer_transaction>>;

auto parse_account_state(const td::Ref<vm::CellSlice>& csr)
    -> td::Result<tonlib_api_ptr<tonlib_api::liteServer_AccountState>>;
auto parse_block_state_account(const td::Ref<vm::CellSlice>& csr)
    -> td::Result<tonlib_api_ptr<tonlib_api::liteServer_blockStateAccount>>;

auto parse_shard_state(const ton::BlockIdExt& blkid, const td::BufferSlice& data)
    -> td::Result<tonlib_api_ptr<tonlib_api::liteServer_blockState>>;

auto parse_config(const ton::BlockIdExt& blkid, td::Slice state_proof, td::Slice config_proof)
    -> td::Result<tonlib_api_ptr<tonlib_api::liteServer_configInfo>>;

}  // namespace tonlib
