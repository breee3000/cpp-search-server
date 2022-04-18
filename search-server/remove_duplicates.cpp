#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    set<string> comparison_set;
    set<int> ids_to_remove;

    for (const int document_id : search_server) {
        std::map<std::string, double> id_words = search_server.GetWordFrequencies(document_id);
        string words_to_compare = ""s;
        for (auto it = id_words.begin(); it != id_words.end(); ++it) {
            words_to_compare += it->first;
        }
        if (comparison_set.count(words_to_compare)) {
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
