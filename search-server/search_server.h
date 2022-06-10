#pragma once

#include <utility>
#include <map>
#include <set>
#include <cmath>
#include <numeric>
#include <execution>
#include <algorithm>
#include <string_view>
#include <deque>
#include <future>

#include "document.h"
#include "concurrent_map.h"
#include "log_duration.h"

using namespace std::string_literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(const std::string_view& stop_words_text);

    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, const std::string_view& raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, const std::string_view& raw_query) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;

    int GetDocumentCount() const;

    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    template <typename ExecutionPolicy>
    void RemoveDocument(const ExecutionPolicy& policy, int document_id);
    void RemoveDocument(int document_id);

    template <typename ExecutionPolicy, typename QueryType>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const ExecutionPolicy& policy, const QueryType& raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query, int document_id) const;
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_frequency_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    std::deque<std::string> words_;

    bool IsStopWord(const std::string_view& word) const;

    static bool IsValidWord(const std::string_view& word);

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
    QueryWord ParseQueryWord(const std::string_view& text) const;

    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };
    Query ParseQuery(const std::string_view& text) const;

    struct QueryParPolicy {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };
    QueryParPolicy ParseQueryParPolicy(const std::string_view& text) const;

    double ComputeWordInverseDocumentFreq(const std::string_view& word) const;

    template <typename DocumentPredicate, typename QueryType, typename ExecutionPolicy>
    std::vector<Document> FindAllDocuments(const ExecutionPolicy& policy, const QueryType& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate, typename QueryType>
    std::vector<Document> FindAllDocuments(const QueryType& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)){
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const {
    std::vector<Document> matched_documents;
    if constexpr (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>) {
        const auto query = ParseQuery(raw_query);
        matched_documents = FindAllDocuments(query, document_predicate);
    } else {
        const auto query = ParseQueryParPolicy(raw_query);
        matched_documents = FindAllDocuments(policy, query, document_predicate);
    }
    sort(matched_documents.begin(), matched_documents.end(),
         [](const Document& lhs, const Document& rhs) {
        const double EPSILON = 1e-6;
        return lhs.relevance > rhs.relevance
                || (std::abs(lhs.relevance - rhs.relevance) < EPSILON && lhs.rating > rhs.rating);
    });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, const std::string_view& raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query,
                            [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}
template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, const std::string_view& raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);

}

template <typename ExecutionPolicy>
void SearchServer::RemoveDocument(const ExecutionPolicy& policy, int document_id) {
    if constexpr (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>) {
        RemoveDocument(document_id);
    } else {
        if (documents_.count(document_id) != 0){
            std::vector<const std::string_view*> words_to_delete(document_to_word_frequency_.at(document_id).size());
            std::transform (policy,
                            document_to_word_frequency_[document_id].begin(), document_to_word_frequency_[document_id].end(),
                            words_to_delete.begin(),
                            [] (const auto& x) {
                return &x.first;
            });
            std::for_each(policy,
                          words_to_delete.begin(), words_to_delete.end(),
                          [this, &document_id](const auto word){
                word_to_document_freqs_[*word].erase(document_id);
            });
            document_to_word_frequency_.erase(document_id);
            documents_.erase(document_id);
            document_ids_.erase(document_id);
        }
    }
}

template <typename ExecutionPolicy, typename QueryType>
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const ExecutionPolicy& policy, const QueryType& raw_query, int document_id) const {
    if constexpr (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>) {
        return MatchDocument(raw_query, document_id);
    } else {
        const auto query = ParseQueryParPolicy(raw_query);
        std::vector<std::string_view> matched_words(query.plus_words.size());
        auto& status = documents_.at(document_id).status;
        const auto word_checker = [this, document_id](std::string_view word_view) {
            const auto& item = word_to_document_freqs_.find(word_view);
            return item != word_to_document_freqs_.end() && item->second.count(document_id);
        };
        if (any_of(policy, query.minus_words.begin(), query.minus_words.end(), word_checker)) {
            return make_tuple(matched_words, status);
        }
        auto matched_words_end = std::copy_if(policy,
                                              query.plus_words.begin(), query.plus_words.end(),
                                              matched_words.begin(),
                                              word_checker);
        matched_words.erase(matched_words_end, matched_words.end());

        sort(policy, matched_words.begin(), matched_words.end());
        auto last = unique(policy, matched_words.begin(), matched_words.end());
        matched_words.erase(last, matched_words.end());

        return make_tuple(matched_words, status);
    }
}

template <typename DocumentPredicate, typename QueryType, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindAllDocuments(const ExecutionPolicy& policy, const QueryType& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    if constexpr (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>) {
        for (const std::string_view& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
        for (const std::string_view& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
    } else {
        ConcurrentMap<int, double> c_document_to_relevance(1000);
        for_each(policy,
                 query.plus_words.begin(), query.plus_words.end(),
                 [this, &c_document_to_relevance, &document_predicate] (const auto& word) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    c_document_to_relevance[document_id].ref_to_value += (term_freq * inverse_document_freq);
                }
            }
        });
        document_to_relevance = c_document_to_relevance.BuildOrdinaryMap();
        for_each(policy,
                 query.minus_words.begin(), query.minus_words.end(),
                 [this, &document_to_relevance](const auto& word) {
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        });

    }
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template <typename DocumentPredicate, typename QueryType>
std::vector<Document> SearchServer::FindAllDocuments(const QueryType& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}
