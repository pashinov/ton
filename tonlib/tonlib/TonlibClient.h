/*
    This file is part of TON Blockchain Library.

    TON Blockchain Library is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    TON Blockchain Library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with TON Blockchain Library.  If not, see <http://www.gnu.org/licenses/>.

    Copyright 2017-2020 Telegram Systems LLP
*/
#pragma once

#include "TonlibCallback.h"

#include "tonlib/Config.h"
#include "tonlib/ExtClient.h"
#include "tonlib/ExtClientOutbound.h"
#include "tonlib/KeyStorage.h"
#include "tonlib/KeyValue.h"
#include "tonlib/LastBlockStorage.h"

#include "td/actor/actor.h"

#include "td/utils/CancellationToken.h"
#include "td/utils/optional.h"

#include "smc-envelope/ManualDns.h"

#include <future>
#include <map>

namespace tonlib {
namespace int_api {
struct GetAccountState;
struct GetPrivateKey;
struct GetLedgerKeys;
struct CheckLedgerKey;
struct CreateMessageBody;
struct GetDnsResolver;
struct SendMessage;
struct RemoteRunSmcMethod;
struct RemoteRunSmcMethodReturnType;

inline std::string to_string(const int_api::SendMessage&) {
  return "Send message";
}
}  // namespace int_api
class AccountState;
class Query;

td::Result<tonlib_api::object_ptr<tonlib_api::dns_EntryData>> to_tonlib_api(
    const ton::ManualDns::EntryData& entry_data);
td::Result<ton::ManualDns::EntryData> to_dns_entry_data(tonlib_api::dns_EntryData& entry_data);

class TonlibClient : public td::actor::Actor {
 public:
  template <class T>
  using object_ptr = tonlib_api::object_ptr<T>;

  template <class T>
  using future = std::future<td::Result<object_ptr<T>>>;

  explicit TonlibClient(td::unique_ptr<TonlibCallback> callback);
  void request(td::uint64 id, object_ptr<tonlib_api::Function> function);
  void request_async(object_ptr<tonlib_api::Function>&& function,
                     td::Promise<tonlib_api::object_ptr<tonlib_api::Object>>&& promise);
  void close();
  static object_ptr<tonlib_api::Object> static_request(object_ptr<tonlib_api::Function> function);

  ~TonlibClient();

  struct FullConfig {
    Config config;
    bool use_callbacks_for_network;
    LastBlockState last_state;
    std::string last_state_key;
    td::uint32 wallet_id;
    std::string rwallet_init_public_key;
  };

 private:
  enum class State { Uninited, Running, Closed } state_ = State::Uninited;
  td::unique_ptr<TonlibCallback> callback_;

  // Config
  Config config_;
  td::uint32 config_generation_{0};
  td::uint32 wallet_id_;
  std::string rwallet_init_public_key_;
  std::string last_state_key_;
  bool use_callbacks_for_network_{false};

  // KeyStorage
  std::shared_ptr<KeyValue> kv_;
  KeyStorage key_storage_;
  LastBlockStorage last_block_storage_;
  struct QueryContext {
    td::optional<ton::BlockIdExt> block_id;
  };
  QueryContext query_context_;

  // network
  td::actor::ActorOwn<ton::adnl::AdnlExtClient> raw_client_;
  td::actor::ActorId<ExtClientOutbound> ext_client_outbound_;
  td::actor::ActorOwn<LastBlock> raw_last_block_;
  td::actor::ActorOwn<LastConfig> raw_last_config_;
  ExtClient client_;

  td::CancellationTokenSource source_;

  std::map<td::int64, td::actor::ActorOwn<>> actors_;
  td::int64 actor_id_{1};

  ExtClientRef get_client_ref();
  void init_ext_client();
  void init_last_block(LastBlockState state);
  void init_last_config();

  bool is_closing_{false};
  td::uint32 ref_cnt_{1};
  void hangup_shared() override {
    auto it = actors_.find(get_link_token());
    if (it != actors_.end()) {
      actors_.erase(it);
    } else {
      ref_cnt_--;
    }
    try_stop();
  }
  void hangup() override;
  void try_stop() {
    if (is_closing_ && ref_cnt_ == 0 && actors_.empty()) {
      stop();
    }
  }

  void update_last_block_state(LastBlockState state, td::uint32 config_generation_);
  void update_sync_state(LastBlockSyncState state, td::uint32 config_generation);
  void on_result(td::uint64 id, object_ptr<tonlib_api::Object> response);
  void on_update(object_ptr<tonlib_api::Object> response);
  static bool is_static_request(td::int32 id);
  static bool is_uninited_request(td::int32 id);
  template <class T>
  static object_ptr<tonlib_api::Object> do_static_request(const T& request) {
    return tonlib_api::make_object<tonlib_api::error>(400, "Function can't be executed synchronously");
  }
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::runTests& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::getAccountAddress& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::packAccountAddress& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::unpackAccountAddress& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::convertIntoPacked& request);
  static object_ptr<tonlib_api::Object> do_static_request(tonlib_api::getBip39Hints& request);

  static object_ptr<tonlib_api::Object> do_static_request(tonlib_api::setLogStream& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::getLogStream& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::setLogVerbosityLevel& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::setLogTagVerbosityLevel& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::getLogVerbosityLevel& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::getLogTagVerbosityLevel& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::getLogTags& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::addLogMessage& request);

  static object_ptr<tonlib_api::Object> do_static_request(tonlib_api::generateKeyPair& request);

  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::encrypt& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::decrypt& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::kdf& request);

  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::msg_decryptWithProof& request);

  static object_ptr<tonlib_api::Object> do_static_request(tonlib_api::computeLastBlockIds& request);

  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::ftabi_computeFunctionId& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::ftabi_computeFunctionSignature& request);
  static object_ptr<tonlib_api::Object> do_static_request(tonlib_api::ftabi_createFunction& request);
  static object_ptr<tonlib_api::Object> do_static_request(tonlib_api::ftabi_getFunction& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::ftabi_decodeOutput& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::ftabi_decodeInput& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::ftabi_generateStateInit& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::ftabi_generateExtendedStateInit& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::ftabi_packIntoCell& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::ftabi_unpackFromCell& request);

  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::ftabi_packPublicKey& request);
  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::ftabi_unpackPublicKey& request);

  static object_ptr<tonlib_api::Object> do_static_request(const tonlib_api::ftabi_extractPublicKeyFromData& request);

  template <class P>
  td::Status do_request(const tonlib_api::runTests& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::getAccountAddress& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::packAccountAddress& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::unpackAccountAddress& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::convertIntoPacked& request, P&&);
  template <class P>
  td::Status do_request(tonlib_api::getBip39Hints& request, P&&);

  template <class P>
  td::Status do_request(tonlib_api::setLogStream& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::getLogStream& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::setLogVerbosityLevel& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::setLogTagVerbosityLevel& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::getLogVerbosityLevel& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::getLogTagVerbosityLevel& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::getLogTags& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::addLogMessage& request, P&&);

  template <class P>
  td::Status do_request(const tonlib_api::generateKeyPair& request, P&&);

  template <class P>
  td::Status do_request(const tonlib_api::encrypt& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::decrypt& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::kdf& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::msg_decryptWithProof& request, P&&);

  template <class P>
  td::Status do_request(const tonlib_api::computeLastBlockIds& request, P&&);

  template <class P>
  td::Status do_request(const tonlib_api::ftabi_computeFunctionId& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::ftabi_computeFunctionSignature& request, P&&);
  template <class P>
  td::Status do_request(tonlib_api::ftabi_createFunction& request, P&&);
  template <class P>
  td::Status do_request(tonlib_api::ftabi_getFunction& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::ftabi_decodeOutput& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::ftabi_decodeInput& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::ftabi_generateStateInit& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::ftabi_generateExtendedStateInit& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::ftabi_packIntoCell& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::ftabi_unpackFromCell& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::ftabi_packPublicKey& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::ftabi_unpackPublicKey& request, P&&);
  template <class P>
  td::Status do_request(const tonlib_api::ftabi_extractPublicKeyFromData& request, P&&);

  void make_any_request(tonlib_api::Function& function, QueryContext query_context,
                        td::Promise<tonlib_api::object_ptr<tonlib_api::Object>>&& promise);
  template <class T, class P>
  void make_request(T&& request, P&& promise) {
    td::Promise<typename std::decay_t<T>::ReturnType> new_promise = std::move(promise);

    auto status = do_request(std::forward<T>(request), std::move(new_promise));
    if (status.is_error()) {
      new_promise.operator()(std::move(status));
    }
  }

  td::Result<FullConfig> validate_config(tonlib_api::object_ptr<tonlib_api::config> config);
  void set_config(FullConfig config);
  td::Status do_request(const tonlib_api::init& request, td::Promise<object_ptr<tonlib_api::options_info>>&& promise);
  td::Status do_request(const tonlib_api::close& request, td::Promise<object_ptr<tonlib_api::ok>>&& promise);
  td::Status do_request(tonlib_api::options_validateConfig& request,
                        td::Promise<object_ptr<tonlib_api::options_configInfo>>&& promise);
  td::Status do_request(tonlib_api::options_setConfig& request,
                        td::Promise<object_ptr<tonlib_api::options_configInfo>>&& promise);

  td::Status do_request(const tonlib_api::raw_sendMessage& request, td::Promise<object_ptr<tonlib_api::ok>>&& promise);
  td::Status do_request(const tonlib_api::raw_createAndSendMessage& request,
                        td::Promise<object_ptr<tonlib_api::ok>>&& promise);
  td::Status do_request(const tonlib_api::raw_createQuery& request,
                        td::Promise<object_ptr<tonlib_api::query_info>>&& promise);
  td::Status do_request(const tonlib_api::raw_createQueryTvc& request,
                        td::Promise<object_ptr<tonlib_api::query_info>>&& promise);

  td::Status do_request(tonlib_api::raw_getAccountState& request,
                        td::Promise<object_ptr<tonlib_api::raw_fullAccountState>>&& promise);
  td::Status do_request(tonlib_api::raw_getTransactions& request,
                        td::Promise<object_ptr<tonlib_api::raw_transactions>>&& promise);

  td::Status do_request(const tonlib_api::getAccountState& request,
                        td::Promise<object_ptr<tonlib_api::fullAccountState>>&& promise);
  td::Status do_request(tonlib_api::guessAccountRevision& request,
                        td::Promise<object_ptr<tonlib_api::accountRevisionList>>&& promise);
  td::Status do_request(tonlib_api::guessAccount& request,
                        td::Promise<object_ptr<tonlib_api::accountRevisionList>>&& promise);
  td::Status do_request(const tonlib_api::findTransaction& request,
                        td::Promise<object_ptr<tonlib_api::transactionSearchResult>>&& promise);

  td::Status do_request(tonlib_api::sync& request, td::Promise<object_ptr<tonlib_api::ton_blockIdExt>>&& promise);

  td::Status do_request(const tonlib_api::createNewKey& request, td::Promise<object_ptr<tonlib_api::key>>&& promise);
  td::Status do_request(const tonlib_api::createNewFtabiKey& request,
                        td::Promise<object_ptr<tonlib_api::key>>&& promise);
  td::Status do_request(const tonlib_api::exportKey& request,
                        td::Promise<object_ptr<tonlib_api::ExportedKey>>&& promise);
  td::Status do_request(const tonlib_api::deleteKey& request, td::Promise<object_ptr<tonlib_api::ok>>&& promise);
  td::Status do_request(const tonlib_api::deleteAllKeys& request, td::Promise<object_ptr<tonlib_api::ok>>&& promise);
  td::Status do_request(const tonlib_api::importKey& request, td::Promise<object_ptr<tonlib_api::key>>&& promise);

  td::Status do_request(const tonlib_api::exportPemKey& request,
                        td::Promise<object_ptr<tonlib_api::exportedPemKey>>&& promise);
  td::Status do_request(const tonlib_api::importPemKey& request, td::Promise<object_ptr<tonlib_api::key>>&& promise);

  td::Status do_request(const tonlib_api::exportEncryptedKey& request,
                        td::Promise<object_ptr<tonlib_api::exportedEncryptedKey>>&& promise);
  td::Status do_request(const tonlib_api::importEncryptedKey& request,
                        td::Promise<object_ptr<tonlib_api::key>>&& promise);

  td::Status do_request(const tonlib_api::exportUnencryptedKey& request,
                        td::Promise<object_ptr<tonlib_api::exportedUnencryptedKey>>&& promise);
  td::Status do_request(const tonlib_api::importUnencryptedKey& request,
                        td::Promise<object_ptr<tonlib_api::key>>&& promise);

  td::Status do_request(const tonlib_api::changeLocalPassword& request,
                        td::Promise<object_ptr<tonlib_api::key>>&& promise);

  td::Status do_request(const tonlib_api::getLedgerKeys& request,
                        td::Promise<object_ptr<tonlib_api::keys>>&& promise);
  td::Status do_request(const tonlib_api::checkLedgerKey& request,
                        td::Promise<object_ptr<tonlib_api::ok>>&& promise);

  td::Status do_request(const tonlib_api::onLiteServerQueryResult& request,
                        td::Promise<object_ptr<tonlib_api::ok>>&& promise);
  td::Status do_request(const tonlib_api::onLiteServerQueryError& request,
                        td::Promise<object_ptr<tonlib_api::ok>>&& promise);

  td::int64 next_query_id_{0};
  std::map<td::int64, td::unique_ptr<Query>> queries_;
  td::int64 register_query(td::unique_ptr<Query> query);
  td::Result<tonlib_api::object_ptr<tonlib_api::query_info>> get_query_info(td::int64 id);
  void finish_create_query(td::Result<td::unique_ptr<Query>> r_query,
                           td::Promise<object_ptr<tonlib_api::query_info>>&& promise);
  void query_estimate_fees(td::int64 id, bool ignore_chksig, td::Result<LastConfigState> r_state,
                           td::Promise<object_ptr<tonlib_api::query_fees>>&& promise);

  void finish_get_ledger_keys(td::Result<object_ptr<tonlib_api::keys>> r_ledgerKeys,
                              td::Promise<object_ptr<tonlib_api::keys>>&& promise);
  void finish_check_ledger_key(td::Result<object_ptr<tonlib_api::ok>> r_status,
                              td::Promise<object_ptr<tonlib_api::ok>>&& promise);
  void finish_create_message_body(td::Result<object_ptr<tonlib_api::ftabi_messageBody>> r_ftabi_messageBody,
                                  td::Promise<object_ptr<tonlib_api::ftabi_messageBody>>&& promise);

  td::Status do_request(const tonlib_api::query_getInfo& request,
                        td::Promise<object_ptr<tonlib_api::query_info>>&& promise);
  td::Status do_request(const tonlib_api::liteServer_getTime& request,
                        td::Promise<object_ptr<tonlib_api::liteServer_currentTime>>&& promise);
  td::Status do_request(const tonlib_api::liteServer_getMasterchainInfo& masterchain_info,
                        td::Promise<object_ptr<tonlib_api::liteServer_masterchainInfo>>&& promise);
  td::Status do_request(const tonlib_api::liteServer_getState& request,
                        td::Promise<object_ptr<tonlib_api::liteServer_blockState>>&& promise);
  td::Status do_request(const tonlib_api::liteServer_getBlock& block_data,
                        td::Promise<object_ptr<tonlib_api::liteServer_block>>&& promise);
  td::Status do_request(const tonlib_api::liteServer_getBlockHeader& block_header,
                        td::Promise<object_ptr<tonlib_api::liteServer_blockHeader>>&& promise);
  td::Status do_request(tonlib_api::liteServer_getAccount& request,
                        td::Promise<object_ptr<tonlib_api::liteServer_account>>&& promise);
  td::Status do_request(tonlib_api::liteServer_getRawAccount& request,
                        td::Promise<object_ptr<tonlib_api::liteServer_rawAccount>>&& promise);
  td::Status do_request(const tonlib_api::liteServer_getOneTransaction& transaction_info,
                        td::Promise<object_ptr<tonlib_api::liteServer_transaction>>&& promise);
  td::Status do_request(const tonlib_api::liteServer_getTransactions& transaction_list,
                        td::Promise<object_ptr<tonlib_api::liteServer_transactionList>>&& promise);
  td::Status do_request(const tonlib_api::liteServer_getRawTransactions& transaction_list,
                        td::Promise<object_ptr<tonlib_api::liteServer_rawTransactionList>>&& promise);
  td::Status do_request(const tonlib_api::liteServer_lookupBlock& block_header,
                        td::Promise<object_ptr<tonlib_api::liteServer_block>>&& promise);
  td::Status do_request(const tonlib_api::liteServer_listBlockTransactions& block_transactions,
                        td::Promise<object_ptr<tonlib_api::liteServer_blockTransactions>>&& promise);
  td::Status do_request(const tonlib_api::liteServer_getBlockProof& request,
                        td::Promise<object_ptr<tonlib_api::liteServer_partialBlockProof>>&& promise);
  td::Status do_request(const tonlib_api::liteServer_getConfigAll& request,
                        td::Promise<object_ptr<tonlib_api::liteServer_configInfo>>&& promise);
  td::Status do_request(tonlib_api::liteServer_getConfigParams& request,
                        td::Promise<object_ptr<tonlib_api::liteServer_configInfo>>&& promise);
  td::Status do_request(tonlib_api::liteServer_getPastElections& request,
                        td::Promise<object_ptr<tonlib_api::liteServer_pastElections>>&& promise);

  td::Status do_request(const tonlib_api::query_estimateFees& request,
                        td::Promise<object_ptr<tonlib_api::query_fees>>&& promise);
  td::Status do_request(const tonlib_api::query_send& request, td::Promise<object_ptr<tonlib_api::ok>>&& promise);
  td::Status do_request(tonlib_api::query_forget& request, td::Promise<object_ptr<tonlib_api::ok>>&& promise);

  td::Status do_request(tonlib_api::createQuery& request, td::Promise<object_ptr<tonlib_api::query_info>>&& promise);

  td::Status do_request(tonlib_api::msg_decrypt& request,
                        td::Promise<object_ptr<tonlib_api::msg_dataDecryptedArray>>&& promise);

  td::int64 next_smc_id_{0};
  std::map<td::int64, td::unique_ptr<AccountState>> smcs_;

  td::int64 register_smc(td::unique_ptr<AccountState> smc);
  td::Result<tonlib_api::object_ptr<tonlib_api::smc_info>> get_smc_info(td::int64 id);
  void finish_load_smc(td::unique_ptr<AccountState> query, td::Promise<object_ptr<tonlib_api::smc_info>>&& promise);
  td::Status do_request(const tonlib_api::smc_load& request, td::Promise<object_ptr<tonlib_api::smc_info>>&& promise);
  td::Status do_request(const tonlib_api::smc_getCode& request,
                        td::Promise<object_ptr<tonlib_api::tvm_cell>>&& promise);
  td::Status do_request(const tonlib_api::smc_getData& request,
                        td::Promise<object_ptr<tonlib_api::tvm_cell>>&& promise);
  td::Status do_request(const tonlib_api::smc_getState& request,
                        td::Promise<object_ptr<tonlib_api::tvm_cell>>&& promise);

  td::Status do_request(const tonlib_api::smc_runGetMethod& request,
                        td::Promise<object_ptr<tonlib_api::smc_runResult>>&& promise);

  td::Status do_request(const tonlib_api::ftabi_createMessageBody& request,
                        td::Promise<object_ptr<tonlib_api::ftabi_messageBody>>&& promise);
  td::Status do_request(tonlib_api::ftabi_runLocal& request,
                        td::Promise<object_ptr<tonlib_api::ftabi_tvmOutput>>&& promise);
  td::Status do_request(const tonlib_api::ftabi_runLocalCached& request,
                        td::Promise<object_ptr<tonlib_api::ftabi_tvmOutput>>&& promise);
  td::Status do_request(const tonlib_api::ftabi_runLocalCachedSplit& request,
                        td::Promise<object_ptr<tonlib_api::ftabi_tvmOutput>>&& promise);

  td::Status do_request(const tonlib_api::dns_resolve& request,
                        td::Promise<object_ptr<tonlib_api::dns_resolved>>&& promise);

  td::Status do_request(tonlib_api::pchan_signPromise& request,
                        td::Promise<object_ptr<tonlib_api::pchan_promise>>&& promise);
  td::Status do_request(tonlib_api::pchan_validatePromise& request, td::Promise<object_ptr<tonlib_api::ok>>&& promise);

  td::Status do_request(tonlib_api::pchan_packPromise& request, td::Promise<object_ptr<tonlib_api::data>>&& promise);
  td::Status do_request(tonlib_api::pchan_unpackPromise& request,
                        td::Promise<object_ptr<tonlib_api::pchan_promise>>&& promise);

  void do_dns_request(std::string name, td::int32 category, td::int32 ttl, td::optional<ton::BlockIdExt> block_id,
                      block::StdAddress address, td::Promise<object_ptr<tonlib_api::dns_resolved>>&& promise);
  struct DnsFinishData {
    ton::BlockIdExt block_id;
    ton::SmartContract::State smc_state;
  };
  void finish_dns_resolve(std::string name, td::int32 category, td::int32 ttl, td::optional<ton::BlockIdExt> block_id,
                          DnsFinishData dns_finish_data, td::Promise<object_ptr<tonlib_api::dns_resolved>>&& promise);

  td::Status do_request(int_api::GetAccountState request, td::Promise<td::unique_ptr<AccountState>>&&);
  td::Status do_request(int_api::GetPrivateKey request, td::Promise<KeyStorage::PrivateKey>&&);
  td::Status do_request(int_api::GetLedgerKeys request, td::Promise<future<tonlib_api::keys>>&&);
  td::Status do_request(int_api::CheckLedgerKey request, td::Promise<future<tonlib_api::ok>>&&);
  td::Status do_request(int_api::CreateMessageBody request, td::Promise<future<tonlib_api::ftabi_messageBody>>&&);
  td::Status do_request(int_api::GetDnsResolver request, td::Promise<block::StdAddress>&&);
  td::Status do_request(int_api::RemoteRunSmcMethod request,
                        td::Promise<int_api::RemoteRunSmcMethodReturnType>&& promise);
  td::Status do_request(int_api::SendMessage request, td::Promise<td::Unit>&& promise);

  td::Status do_request(const tonlib_api::liteServer_getInfo& request,
                        td::Promise<object_ptr<tonlib_api::liteServer_info>>&& promise);

  td::Status do_request(tonlib_api::withBlock& request, td::Promise<object_ptr<tonlib_api::Object>>&& promise);

  void proxy_request(td::int64 query_id, std::string data);

  friend class TonlibQueryActor;
  struct Target {
    bool can_be_empty{true};
    bool can_be_uninited{false};
    block::StdAddress address;
    td::optional<td::Ed25519::PublicKey> public_key;
  };

  td::Status guess_revisions(std::vector<Target> targets,
                             td::Promise<object_ptr<tonlib_api::accountRevisionList>>&& promise);
};
}  // namespace tonlib
