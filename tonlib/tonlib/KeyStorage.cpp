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
#include "KeyStorage.h"

#include "tonlib/keys/bip39.h"
#include "tonlib/keys/Mnemonic.h"
#include "tonlib/keys/DecryptedKey.h"
#include "tonlib/keys/EncryptedKey.h"

#include "tonlib/TonlibError.h"

#include "td/utils/filesystem.h"
#include "td/utils/port/path.h"
#include "td/utils/crypto.h"
#include "td/utils/PathView.h"

#include "ftabi/Mnemonic.hpp"

namespace tonlib {
namespace {
std::string to_file_name_old(const KeyStorage::Key &key) {
  return td::buffer_to_hex(key.public_key);
}

std::string to_file_name(const KeyStorage::Key &key) {
  return td::buffer_to_hex(td::sha512(key.secret.as_slice()).substr(0, 32));
}

td::SecureString get_dummy_secret() {
  return td::SecureString("dummy secret of 32 bytes length!");
}

template <typename T>
td::Result<T> export_decrypted_key(KeyValue &kv, KeyStorage::InputKey input_key) {
  auto r_encrypted_data = kv.get(to_file_name(input_key.key));
  if (r_encrypted_data.is_error()) {
    r_encrypted_data = kv.get(to_file_name_old(input_key.key));
    if (r_encrypted_data.is_ok()) {
      LOG(WARNING) << "Restore private key from deprecated location " << to_file_name_old(input_key.key) << " --> "
                   << to_file_name(input_key.key);
      TRY_STATUS_PREFIX(kv.set(to_file_name(input_key.key), r_encrypted_data.ok()), TonlibError::Internal());
      kv.erase(to_file_name_old(input_key.key)).ignore();
    }
  }
  TRY_RESULT_PREFIX(encrypted_data, std::move(r_encrypted_data), TonlibError::KeyUnknown());
  EncryptedKey encrypted_key{std::move(encrypted_data), td::Ed25519::PublicKey(std::move(input_key.key.public_key)),
                             std::move(input_key.key.secret)};
  {
    auto r_decrypted_key = encrypted_key.decrypt<T>(input_key.local_password.copy(), true, true);
    if (r_decrypted_key.is_ok()) {
      LOG(WARNING) << "Restore private from deprecated encryption " << to_file_name(input_key.key);
      auto decrypted_key = r_decrypted_key.move_as_ok();
      auto key = KeyStorage::Key{encrypted_key.public_key.as_octet_string(), encrypted_key.secret.copy()};
      auto new_encrypted_key = decrypted_key.encrypt(input_key.local_password.copy(), encrypted_key.secret);
      CHECK(new_encrypted_key.public_key.as_octet_string() == encrypted_key.public_key.as_octet_string());
      CHECK(new_encrypted_key.secret == encrypted_key.secret);
      CHECK(new_encrypted_key.template decrypt<T>(input_key.local_password.copy()).ok().private_key.as_octet_string() ==
            decrypted_key.private_key.as_octet_string());
      kv.set(to_file_name(key), new_encrypted_key.encrypted_data);
      return std::move(decrypted_key);
    }
  }
  TRY_RESULT_PREFIX(decrypted_key, encrypted_key.decrypt<T>(std::move(input_key.local_password)),
                    TonlibError::KeyDecrypt());
  return std::move(decrypted_key);
}

template <typename T>
td::Result<KeyStorage::Key> save_key(KeyValue &kv, const T &decrypted_key, td::Slice local_password) {
  auto encrypted_key = decrypted_key.encrypt(local_password);

  KeyStorage::Key res;
  res.public_key = encrypted_key.public_key.as_octet_string();
  res.secret = std::move(encrypted_key.secret);
  TRY_STATUS_PREFIX(kv.set(to_file_name(res), encrypted_key.encrypted_data), TonlibError::Internal());
  return std::move(res);
}

}  // namespace

void KeyStorage::set_key_value(std::shared_ptr<KeyValue> kv) {
  kv_ = std::move(kv);
}

td::Result<KeyStorage::Key> KeyStorage::create_new_key(td::Slice local_password, td::Slice mnemonic_password,
                                                       td::Slice entropy) {
  Mnemonic::Options create_options;
  create_options.password = td::SecureString(mnemonic_password);
  create_options.entropy = td::SecureString(entropy);
  TRY_RESULT(mnemonic, Mnemonic::create_new(std::move(create_options)));

  return save_key(*kv_, DecryptedKey(mnemonic), local_password);
}

td::Result<KeyStorage::Key> KeyStorage::create_new_ftabi_key(td::Slice local_password, td::Slice derivation_path) {
  const auto dictionary = Mnemonic::normalize_and_split(td::SecureString(bip39_english()));
  TRY_RESULT(mnemonic, ftabi::mnemonic::generate_key(dictionary, derivation_path))

  return save_key(*kv_, DecryptedFtabiKey(std::move(mnemonic.first), derivation_path.str(), std::move(mnemonic.second)),
                  local_password);
}

td::Result<KeyStorage::ExportedKey> KeyStorage::export_key(InputKey input_key) {
  TRY_RESULT(decrypted_key, export_decrypted_key<DecryptedKey>(*kv_, std::move(input_key)))
  ExportedKey exported_key;
  exported_key.mnemonic_words = std::move(decrypted_key.mnemonic_words);
  return std::move(exported_key);
}

td::Result<KeyStorage::ExportedFtabiKey> KeyStorage::export_ftabi_key(InputKey input_key) {
  TRY_RESULT(decrypted_key, export_decrypted_key<DecryptedFtabiKey>(*kv_, std::move(input_key)))
  ExportedFtabiKey exported_key;
  exported_key.mnemonic_words = std::move(decrypted_key.mnemonic_words);
  return std::move(exported_key);
}

td::Result<KeyStorage::ExportedPemKey> KeyStorage::export_pem_key(InputKey input_key, td::Slice key_password) {
  TRY_RESULT(decrypted_key, export_decrypted_key<DecryptedKey>(*kv_, std::move(input_key)));
  TRY_RESULT_PREFIX(pem, decrypted_key.private_key.as_pem(key_password), TonlibError::Internal());
  return ExportedPemKey{std::move(pem)};
}

td::Result<KeyStorage::ExportedEncryptedKey> KeyStorage::export_encrypted_key(InputKeyType type, InputKey input_key,
                                                                              td::Slice key_password) {
  switch (type) {
    case InputKeyType::Fake:
    case InputKeyType::Original: {
      TRY_RESULT(decrypted_key, export_decrypted_key<DecryptedKey>(*kv_, std::move(input_key)));
      auto res = decrypted_key.encrypt(key_password, get_dummy_secret());
      return ExportedEncryptedKey{std::move(res.encrypted_data)};
    }
    case InputKeyType::Ftabi: {
      TRY_RESULT(decrypted_key, export_decrypted_key<DecryptedFtabiKey>(*kv_, std::move(input_key)));
      auto res = decrypted_key.encrypt(key_password, get_dummy_secret());
      return ExportedEncryptedKey{std::move(res.encrypted_data)};
    }
    default:
      UNREACHABLE();
  }
}

td::Result<KeyStorage::ExportedUnencryptedKey> KeyStorage::export_unencrypted_key(InputKeyType type,
                                                                                  InputKey input_key) {
  switch (type) {
    case InputKeyType::Fake:
    case InputKeyType::Original: {
      TRY_RESULT(decrypted_key, export_decrypted_key<DecryptedKey>(*kv_, std::move(input_key)));
      return ExportedUnencryptedKey{decrypted_key.private_key.as_octet_string()};
    }
    case InputKeyType::Ftabi: {
      TRY_RESULT(decrypted_key, export_decrypted_key<DecryptedFtabiKey>(*kv_, std::move(input_key)));
      return ExportedUnencryptedKey{decrypted_key.private_key.as_octet_string()};
    }
    default:
      UNREACHABLE();
  }
}

td::Result<KeyStorage::Key> KeyStorage::change_local_password(InputKeyType type, InputKey input_key,
                                                              td::Slice new_local_password) {
  auto old_name = to_file_name(input_key.key);
  switch (type) {
    case InputKeyType::Fake:
    case InputKeyType::Original: {
      TRY_RESULT(decrypted_key, export_decrypted_key<DecryptedKey>(*kv_, std::move(input_key)));
      return save_key(*kv_, decrypted_key, new_local_password);
    }
    case InputKeyType::Ftabi: {
      TRY_RESULT(decrypted_key, export_decrypted_key<DecryptedFtabiKey>(*kv_, std::move(input_key)));
      return save_key(*kv_, decrypted_key, new_local_password);
    }
    default:
      UNREACHABLE();
  }
}

td::Status KeyStorage::delete_key(const Key &key) {
  LOG(WARNING) << "Delete private key stored at " << to_file_name(key);
  return kv_->erase(to_file_name(key));
}

td::Status KeyStorage::delete_all_keys() {
  std::vector<std::string> keys;
  kv_->foreach_key([&](td::Slice key) {
    if (td::PathView(key).extension().empty()) {
      keys.push_back(key.str());
    }
  });
  td::Status status;
  for (const auto &key : keys) {
    LOG(WARNING) << "Delete private key stored at " << key;
    auto err = kv_->erase(key);
    if (err.is_error() && status.is_ok()) {
      status = std::move(err);
    }
  }
  return status;
}

td::Result<KeyStorage::Key> KeyStorage::import_key(td::Slice local_password, td::Slice mnemonic_password,
                                                   ExportedKey exported_key) {
  TRY_RESULT(mnemonic, Mnemonic::create(std::move(exported_key.mnemonic_words), td::SecureString(mnemonic_password)));
  if (!mnemonic.is_basic_seed()) {
    if (mnemonic_password.empty() && mnemonic.is_password_seed()) {
      return TonlibError::NeedMnemonicPassword();
    }
    return TonlibError::InvalidMnemonic();
  }
  return save_key(*kv_, DecryptedKey(mnemonic), local_password);
}

td::Result<KeyStorage::Key> KeyStorage::import_ftabi_key(td::Slice local_password, ExportedFtabiKey exported_key) {
  TRY_RESULT(key, ftabi::mnemonic::recover_key(exported_key.mnemonic_words, exported_key.derivation_path))
  return save_key(*kv_,
                  DecryptedFtabiKey(std::move(exported_key.mnemonic_words), std::move(exported_key.derivation_path),
                                    std::move(key)),
                  local_password);
}

td::Result<KeyStorage::Key> KeyStorage::import_pem_key(td::Slice local_password, td::Slice key_password,
                                                       ExportedPemKey exported_key) {
  TRY_RESULT_PREFIX(key, td::Ed25519::PrivateKey::from_pem(exported_key.pem, key_password),
                    TonlibError::InvalidPemKey());
  return save_key(*kv_, DecryptedKey({}, std::move(key)), local_password);
}

td::Result<KeyStorage::Key> KeyStorage::import_encrypted_key(td::Slice local_password, td::Slice key_password,
                                                             ExportedEncryptedKey exported_key) {
  EncryptedKey encrypted_key{std::move(exported_key.data), td::Ed25519::PublicKey(td::SecureString()),
                             get_dummy_secret()};
  TRY_RESULT_PREFIX(decrypted_key, encrypted_key.decrypt<DecryptedKey>(key_password, false), TonlibError::KeyDecrypt());
  return save_key(*kv_, decrypted_key, local_password);
}

td::Result<KeyStorage::Key> KeyStorage::import_unencrypted_key(td::Slice local_password,
                                                               ExportedUnencryptedKey exported_key) {
  RawDecryptedKey raw_key;
  raw_key.private_key = std::move(exported_key.data);
  DecryptedKey key(std::move(raw_key));
  return save_key(*kv_, key, local_password);
}

td::Result<KeyStorage::PrivateKey> KeyStorage::load_private_key(InputKeyType type, InputKey input_key) {
  PrivateKey private_key;
  switch (type) {
    case InputKeyType::Fake:
    case InputKeyType::Original: {
      if (is_fake_input_key(input_key)) {
        return fake_private_key();
      }

      TRY_RESULT(decrypted_key, export_decrypted_key<DecryptedKey>(*kv_, std::move(input_key)))
      private_key.private_key = decrypted_key.private_key.as_octet_string();
      break;
    }
    case InputKeyType::Ftabi: {
      TRY_RESULT(decrypted_key, export_decrypted_key<DecryptedFtabiKey>(*kv_, std::move(input_key)))
      private_key.private_key = decrypted_key.private_key.as_octet_string();
      break;
    }
    case InputKeyType::Ledger: {
      private_key.private_key = input_key.key.secret.copy();
      break;
    }
    default:
      UNREACHABLE();
  }

  return std::move(private_key);
}

KeyStorage::PrivateKey KeyStorage::fake_private_key() {
  return PrivateKey{td::SecureString(32, 0)};
}

KeyStorage::InputKey KeyStorage::fake_input_key() {
  return InputKey{{td::SecureString(32, 0), td::SecureString(32, 0)}, {}};
}

bool KeyStorage::is_fake_input_key(InputKey &input_key) {
  auto is_zero = [](td::Slice slice, size_t size) {
    if (slice.size() != size) {
      return false;
    }
    for (auto c : slice) {
      if (c != 0) {
        return false;
      }
    }
    return true;
  };
  return is_zero(input_key.local_password, 0) && is_zero(input_key.key.secret, 32) &&
         is_zero(input_key.key.public_key, 32);
}

}  // namespace tonlib
