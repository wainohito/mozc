// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MOZC_HAIKU_TOOL_DICTIONARY_STORE_H_
#define MOZC_HAIKU_TOOL_DICTIONARY_STORE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "client/client_interface.h"
#include "client/user_dictionary/user_dictionary_storage.h"
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {
namespace haiku {

// The user dictionaries, as edited by the tools. Every change is saved at
// once and the conversion engine is told to reload them, so that a word can
// be used as soon as it is added.
class DictionaryStore {
 public:
  using Dictionary = user_dictionary::UserDictionary;
  using Entry = user_dictionary::UserDictionary::Entry;

  DictionaryStore();
  ~DictionaryStore();

  // Loads the dictionaries; creates one if there is none. The returned
  // message, if any, is for the user.
  absl::Status Load();

  int size() const;
  const Dictionary& dictionary(int index) const;

  absl::Status AddEntry(int dictionary, const Entry& entry);
  absl::Status UpdateEntry(int dictionary, int index, const Entry& entry);
  absl::Status DeleteEntry(int dictionary, int index);
  absl::StatusOr<int> CreateDictionary(const std::string& name);
  absl::Status RenameDictionary(int dictionary, const std::string& name);
  absl::Status DeleteDictionary(int dictionary);
  // Imports the dictionary file of Mozc, MS-IME, ATOK or Kotoeri at `path`
  // into a new dictionary named `name`.
  absl::StatusOr<int> Import(const std::string& path, const std::string& name);
  absl::Status Export(int dictionary, const std::string& path) const;

  // The parts of speech, by their names as the user sees them.
  static const std::vector<std::string>& PosNames();
  static int DefaultPosIndex();

  // Fills `entry` from what the user typed; returns a message for the user
  // if it cannot be added.
  static absl::Status MakeEntry(const std::string& reading,
                                const std::string& word,
                                const std::string& pos,
                                const std::string& comment, Entry* entry);

 private:
  absl::Status Save();

  UserDictionaryStorage storage_;
  std::unique_ptr<client::ClientInterface> client_;
};

}  // namespace haiku
}  // namespace mozc

#endif  // MOZC_HAIKU_TOOL_DICTIONARY_STORE_H_
