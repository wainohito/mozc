// Copyright 2026 RenkuOS contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "haiku/tool/dictionary_store.h"

#include <UTF8.h>

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/file_util.h"
#include "client/client.h"
#include "client/user_dictionary/user_dictionary_importer.h"
#include "data_manager/pos_list_provider.h"
#include "dictionary/user_dictionary_util.h"
#include "dictionary/user_pos.h"
#include "haiku/tool/tool_strings.h"

namespace mozc {
namespace haiku {
namespace {

absl::Status UserError(const char* english) {
  return absl::InvalidArgumentError(Tr(english));
}

// Appends the UTF-8 form of a UTF-16 code unit sequence.
std::string Utf16ToUtf8(const std::string& data, bool big_endian) {
  std::string out;
  size_t i = 0;
  auto unit = [&](size_t at) -> uint32_t {
    const uint8_t a = data[at], b = data[at + 1];
    return big_endian ? (a << 8 | b) : (b << 8 | a);
  };
  while (i + 1 < data.size()) {
    uint32_t c = unit(i);
    i += 2;
    if (c >= 0xd800 && c < 0xdc00 && i + 1 < data.size()) {
      const uint32_t low = unit(i);
      if (low >= 0xdc00 && low < 0xe000) {
        c = 0x10000 + ((c - 0xd800) << 10) + (low - 0xdc00);
        i += 2;
      }
    }
    if (c < 0x80) {
      out += static_cast<char>(c);
    } else if (c < 0x800) {
      out += static_cast<char>(0xc0 | c >> 6);
      out += static_cast<char>(0x80 | (c & 0x3f));
    } else if (c < 0x10000) {
      out += static_cast<char>(0xe0 | c >> 12);
      out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
      out += static_cast<char>(0x80 | (c & 0x3f));
    } else {
      out += static_cast<char>(0xf0 | c >> 18);
      out += static_cast<char>(0x80 | ((c >> 12) & 0x3f));
      out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
      out += static_cast<char>(0x80 | (c & 0x3f));
    }
  }
  return out;
}

std::string ShiftJisToUtf8(const std::string& data) {
  std::string out;
  int32 state = 0;
  size_t done = 0;
  char buffer[4096];
  while (done < data.size()) {
    int32 source_length = static_cast<int32>(data.size() - done);
    int32 dest_length = sizeof(buffer);
    if (convert_to_utf8(B_SJIS_CONVERSION, data.data() + done,
                        &source_length, buffer, &dest_length,
                        &state) != B_OK ||
        source_length == 0) {
      break;
    }
    out.append(buffer, dest_length);
    done += source_length;
  }
  return out;
}

// Reads a dictionary file as UTF-8, whatever its encoding.
absl::StatusOr<std::string> ReadAsUtf8(const std::string& path) {
  absl::StatusOr<std::string> data = FileUtil::GetContents(path);
  if (!data.ok()) {
    return data.status();
  }
  if (data->size() >= 2 && static_cast<uint8_t>((*data)[0]) == 0xff &&
      static_cast<uint8_t>((*data)[1]) == 0xfe) {
    return Utf16ToUtf8(data->substr(2), false);
  }
  if (data->size() >= 2 && static_cast<uint8_t>((*data)[0]) == 0xfe &&
      static_cast<uint8_t>((*data)[1]) == 0xff) {
    return Utf16ToUtf8(data->substr(2), true);
  }
  switch (user_dictionary::GuessEncodingType(*data)) {
    case user_dictionary::SHIFT_JIS:
      return ShiftJisToUtf8(*data);
    case user_dictionary::UTF16:
      return Utf16ToUtf8(*data, false);
    default:
      return *std::move(data);
  }
}

}  // namespace

DictionaryStore::DictionaryStore()
    : client_(client::ClientFactory::NewClient()) {}

DictionaryStore::~DictionaryStore() { storage_.UnLock(); }

absl::Status DictionaryStore::Load() {
  if (!storage_.Lock()) {
    return UserError("Another program is editing the user dictionary.");
  }
  if (storage_.Exists().ok() && !storage_.Load().ok()) {
    return UserError("Cannot read the user dictionary.");
  }
  if (storage_.dictionaries_size() == 0) {
    if (!storage_.CreateDictionary(Tr("User dictionary")).ok()) {
      return UserError("Cannot save the user dictionary.");
    }
    return Save();
  }
  return absl::OkStatus();
}

int DictionaryStore::size() const {
  return static_cast<int>(storage_.dictionaries_size());
}

const DictionaryStore::Dictionary& DictionaryStore::dictionary(
    int index) const {
  return storage_.dictionaries(index);
}

absl::Status DictionaryStore::AddEntry(int dictionary, const Entry& entry) {
  Dictionary* dic = storage_.GetProto().mutable_dictionaries(dictionary);
  if (UserDictionaryStorage::IsDictionaryFull(*dic)) {
    return UserError("The dictionary is full.");
  }
  *dic->add_entries() = entry;
  return Save();
}

absl::Status DictionaryStore::UpdateEntry(int dictionary, int index,
                                          const Entry& entry) {
  *storage_.GetProto().mutable_dictionaries(dictionary)->mutable_entries(
      index) = entry;
  return Save();
}

absl::Status DictionaryStore::DeleteEntry(int dictionary, int index) {
  auto* entries =
      storage_.GetProto().mutable_dictionaries(dictionary)->mutable_entries();
  entries->erase(entries->begin() + index);
  return Save();
}

absl::StatusOr<int> DictionaryStore::CreateDictionary(const std::string& name) {
  absl::StatusOr<uint64_t> id = storage_.CreateDictionary(name);
  if (!id.ok()) {
    return id.status();
  }
  if (absl::Status status = Save(); !status.ok()) {
    return status;
  }
  return storage_.GetUserDictionaryIndex(*id);
}

absl::Status DictionaryStore::RenameDictionary(int dictionary,
                                               const std::string& name) {
  absl::Status status =
      storage_.RenameDictionary(this->dictionary(dictionary).id(), name);
  return status.ok() ? Save() : status;
}

absl::Status DictionaryStore::DeleteDictionary(int dictionary) {
  absl::Status status =
      storage_.DeleteDictionary(this->dictionary(dictionary).id());
  return status.ok() ? Save() : status;
}

absl::StatusOr<int> DictionaryStore::Import(const std::string& path,
                                            const std::string& name) {
  absl::StatusOr<std::string> data = ReadAsUtf8(path);
  if (!data.ok()) {
    return UserError("The file could not be imported.");
  }
  Dictionary imported;
  user_dictionary::StringTextLineIterator lines(*data);
  const absl::Status status = user_dictionary::ImportFromTextLineIterator(
      user_dictionary::IME_AUTO_DETECT, &lines, &imported);
  if (imported.entries_size() == 0) {
    return UserError("The file could not be imported.");
  }
  absl::StatusOr<int> index = CreateDictionary(name);
  if (!index.ok()) {
    return index;
  }
  storage_.GetProto().mutable_dictionaries(*index)->mutable_entries()->Swap(
      imported.mutable_entries());
  if (absl::Status saved = Save(); !saved.ok()) {
    return saved;
  }
  if (!status.ok()) {
    // The dictionary is there; tell what was left out.
    return absl::DataLossError(Tr("Some lines could not be imported."));
  }
  return index;
}

absl::Status DictionaryStore::Export(int dictionary,
                                     const std::string& path) const {
  return storage_.ExportDictionary(this->dictionary(dictionary).id(), path);
}

const std::vector<std::string>& DictionaryStore::PosNames() {
  static const std::vector<std::string>* names =
      new std::vector<std::string>(PosListProvider().GetPosList());
  return *names;
}

int DictionaryStore::DefaultPosIndex() {
  return PosListProvider().GetPosListDefaultIndex();
}

absl::Status DictionaryStore::MakeEntry(const std::string& reading,
                                        const std::string& word,
                                        const std::string& pos,
                                        const std::string& comment,
                                        Entry* entry) {
  entry->Clear();
  entry->set_key(user_dictionary::NormalizeReading(reading));
  entry->set_value(word);
  entry->set_pos(dictionary::UserPos::ToPosType(pos));
  entry->set_comment(comment);
  if (!user_dictionary::IsValidReading(entry->key())) {
    return UserError(
        "The reading is empty or contains characters that cannot be used.");
  }
  if (!user_dictionary::ValidateEntry(*entry).ok()) {
    return UserError("The word is empty or too long.");
  }
  return absl::OkStatus();
}

absl::Status DictionaryStore::Save() {
  if (!storage_.Save().ok()) {
    return UserError("Cannot save the user dictionary.");
  }
  // The engine loads the dictionaries again; new words work at once.
  client_->Reload();
  return absl::OkStatus();
}

}  // namespace haiku
}  // namespace mozc
