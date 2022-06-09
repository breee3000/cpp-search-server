#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer::SearchServer(SplitIntoWords(stop_words_text)) {
}

SearchServer::SearchServer(const string_view& stop_words_text)
    : SearchServer::SearchServer(SplitIntoWordsView(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id, const string_view& document, DocumentStatus status,
                               const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string_view& word: words) {
        words_.push_back(string(word.begin(), word.size()));
        word_to_document_freqs_[words_.back()][document_id] += inv_word_count;
        document_to_word_frequency_[document_id][words_.back()] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query, DocumentStatus status) const {
    return FindTopDocuments(
                raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    auto result = document_to_word_frequency_.find(document_id);

    static const map<string_view, double> dummy_map;
    return result == document_to_word_frequency_.end() ? dummy_map : document_to_word_frequency_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    if (document_ids_.count(document_id) != 0) {
        for (auto& [word, freq] : document_to_word_frequency_[document_id]) {
            word_to_document_freqs_[word].erase(document_id);
        }
        documents_.erase(document_id);
        document_ids_.erase(document_id);
        document_to_word_frequency_.erase(document_id);
    }
}

void RemoveDuplicates(SearchServer& search_server) {
    set<string> comparison_set;
    set<int> ids_to_remove;
    for (const int document_id : search_server) {
        std::map<std::string_view, double> id_words = search_server.GetWordFrequencies(document_id);
        string words_to_compare = ""s;
        for (auto it = id_words.begin(); it != id_words.end(); ++it) {
            words_to_compare += it->first;
        }
        if (comparison_set.count(words_to_compare) != 0) {
            ids_to_remove.insert(document_id);
        } else {
            comparison_set.insert(words_to_compare);
        }
    }
    for (auto id: ids_to_remove) {
        cout << "Found duplicate document id "s << id << endl;
        search_server.RemoveDocument(id);
    }
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view& raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);
    vector<string_view> matched_words;
    for (const string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(string{word}).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(string{word}).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const string_view& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string_view& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view& text) const {
    vector<string_view> words;
    for (const string_view& word : SplitIntoWordsView(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + string{word} + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const string_view& text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + string(text) + " is invalid");
    }
    return {word, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(const string_view& text) const {
    Query result;
    for (const string_view& word : SplitIntoWordsView(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            query_word.is_minus ? result.minus_words.insert(query_word.data) : result.plus_words.insert(query_word.data);
        }
    }
    return result;
}

SearchServer::QueryParPolicy SearchServer::ParseQueryParPolicy(const std::string_view& text) const {
    QueryParPolicy result;
    for (const string_view& word : SplitIntoWordsView(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            query_word.is_minus ? result.minus_words.push_back(query_word.data) : result.plus_words.push_back(query_word.data);
        }
    }
    sort(std::execution::par, result.plus_words.begin(), result.plus_words.end());
    auto last_plus = unique(std::execution::par, result.plus_words.begin(), result.plus_words.end());
    result.plus_words.erase(last_plus, result.plus_words.end());

    sort(std::execution::par, result.minus_words.begin(), result.minus_words.end());
    auto last_minus = unique(std::execution::par, result.minus_words.begin(), result.minus_words.end());
    result.minus_words.erase(last_minus, result.minus_words.end());
    
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string_view& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
